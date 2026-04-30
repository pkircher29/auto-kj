/*
 * Copyright (c) 2025 Thomas Isaac Lightburn
 *
 *
 * This file is part of Auto-KJ.
 *
 * Auto-KJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "volumenormalizer.h"
#include <QFileInfo>
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/pbutils/pbutils.h>

VolumeNormalizer::VolumeNormalizer(QObject *parent)
    : QObject(parent)
{
    m_logger = spdlog::get("logger");
}

bool VolumeNormalizer::isNormalized(const QString &audioPath)
{
    return audioPath.contains("_normalized");
}

double VolumeNormalizer::analyzePeak(const QString &audioPath)
{
    // Build a pipeline: filesrc → decodebin → audioconvert → level → fakesink
    GstElement *pipeline = gst_pipeline_new("peak-analyzer");
    if (!pipeline) {
        m_logger->error("{} Failed to create GStreamer pipeline for peak analysis", m_loggingPrefix);
        return -HUGE_VAL;
    }

    GstElement *src = gst_element_factory_make("filesrc", "src");
    GstElement *decode = gst_element_factory_make("decodebin", "decode");
    GstElement *convert = gst_element_factory_make("audioconvert", "convert");
    GstElement *level = gst_element_factory_make("level", "level");
    GstElement *sink = gst_element_factory_make("fakesink", "sink");

    if (!src || !decode || !convert || !level || !sink) {
        m_logger->error("{} Failed to create GStreamer elements for peak analysis", m_loggingPrefix);
        gst_object_unref(pipeline);
        return -HUGE_VAL;
    }

    g_object_set(src, "location", audioPath.toUtf8().constData(), nullptr);
    g_object_set(level, "peak-ttl", (guint64)GST_SECOND, nullptr);

    gst_bin_add_many(GST_BIN(pipeline), src, decode, convert, level, sink, nullptr);

    if (!gst_element_link(src, decode)) {
        m_logger->error("{} Failed to link src → decodebin", m_loggingPrefix);
        gst_object_unref(pipeline);
        return -HUGE_VAL;
    }

    // Connect decodebin's pad-added to link to audioconvert
    auto padAddedCb = [](GstElement *src, GstPad *newPad, gpointer userData) {
        GstElement *convert = GST_ELEMENT(userData);
        GstPad *sinkPad = gst_element_get_static_pad(convert, "sink");
        if (gst_pad_is_linked(sinkPad)) {
            gst_object_unref(sinkPad);
            return;
        }
        GstCaps *caps = gst_pad_get_current_caps(newPad);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        if (gst_structure_has_name(structure, "audio/x-raw")) {
            gst_pad_link(newPad, sinkPad);
        }
        gst_caps_unref(caps);
        gst_object_unref(sinkPad);
    };
    g_signal_connect(decode, "pad-added", G_CALLBACK(padAddedCb), convert);

    if (!gst_element_link_many(convert, level, sink, nullptr)) {
        m_logger->error("{} Failed to link convert → level → sink", m_loggingPrefix);
        gst_object_unref(pipeline);
        return -HUGE_VAL;
    }

    // Set up a bus to watch for messages
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Process the bus until we get EOS or ERROR
    double peak = -HUGE_VAL;
    bool done = false;
    while (!done) {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        if (!msg)
            break;

        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            done = true;
            break;
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            m_logger->error("{} Peak analysis error: {}", m_loggingPrefix, err->message);
            g_error_free(err);
            g_free(dbg);
            done = true;
            break;
        }
        case GST_MESSAGE_ELEMENT: {
            const GstStructure *s = gst_message_get_structure(msg);
            if (gst_structure_has_name(s, "level")) {
                const GValue *peakVal = gst_structure_get_value(s, "peak");
                if (peakVal && GST_VALUE_HOLDS_LIST(peakVal)) {
                    guint nChannels = gst_value_list_get_size(peakVal);
                    for (guint ch = 0; ch < nChannels; ch++) {
                        const GValue *chVal = gst_value_list_get_value(peakVal, ch);
                        if (G_VALUE_HOLDS_DOUBLE(chVal)) {
                            double chPeak = g_value_get_double(chVal);
                            if (chPeak > peak)
                                peak = chPeak;
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
        }
        gst_message_unref(msg);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);

    return peak;
}

bool VolumeNormalizer::applyGain(const QString &inputPath, const QString &outputPath, double gainDb)
{
    // Build pipeline: filesrc → decodebin → audioconvert → audioamplify → encode → filesink
    GstElement *pipeline = gst_pipeline_new("gain-applier");
    if (!pipeline) {
        m_logger->error("{} Failed to create GStreamer pipeline for gain application", m_loggingPrefix);
        return false;
    }

    GstElement *src = gst_element_factory_make("filesrc", "src");
    GstElement *decode = gst_element_factory_make("decodebin", "decode");
    GstElement *convert = gst_element_factory_make("audioconvert", "convert");
    GstElement *amplify = gst_element_factory_make("audioamplify", "amplify");
    GstElement *encoder = nullptr;
    GstElement *sink = gst_element_factory_make("filesink", "sink");

    // Determine encoder based on input file extension
    QFileInfo fi(inputPath);
    QString ext = fi.suffix().toLower();
    if (ext == "mp3") {
        encoder = gst_element_factory_make("lamemp3enc", "encoder");
        if (!encoder)
            encoder = gst_element_factory_make("avenc_mp3", "encoder");
    } else if (ext == "wav") {
        encoder = gst_element_factory_make("wavenc", "encoder");
    } else if (ext == "ogg") {
        encoder = gst_element_factory_make("vorbisenc", "encoder");
    } else if (ext == "flac") {
        encoder = gst_element_factory_make("flacenc", "encoder");
    } else {
        // Default to MP3
        encoder = gst_element_factory_make("lamemp3enc", "encoder");
        if (!encoder)
            encoder = gst_element_factory_make("avenc_mp3", "encoder");
    }

    if (!src || !decode || !convert || !amplify || !encoder || !sink) {
        m_logger->error("{} Failed to create GStreamer elements for gain application", m_loggingPrefix);
        gst_object_unref(pipeline);
        return false;
    }

    g_object_set(src, "location", inputPath.toUtf8().constData(), nullptr);
    g_object_set(sink, "location", outputPath.toUtf8().constData(), nullptr);

    // Set gain as linear factor from dB
    double gainFactor = pow(10.0, gainDb / 20.0);
    g_object_set(amplify, "amplification", gainFactor, nullptr);

    gst_bin_add_many(GST_BIN(pipeline), src, decode, convert, amplify, encoder, sink, nullptr);

    if (!gst_element_link(src, decode)) {
        m_logger->error("{} Failed to link src → decodebin for gain", m_loggingPrefix);
        gst_object_unref(pipeline);
        return false;
    }

    // Connect decodebin pad-added to audioconvert
    struct CbData {
        GstElement *convert;
        std::shared_ptr<spdlog::logger> logger;
        std::string prefix;
    };
    CbData *data = new CbData{convert, m_logger, m_loggingPrefix};

    auto padAddedCb = [](GstElement *src, GstPad *newPad, gpointer userData) {
        auto *data = static_cast<CbData*>(userData);
        GstPad *sinkPad = gst_element_get_static_pad(data->convert, "sink");
        if (gst_pad_is_linked(sinkPad)) {
            gst_object_unref(sinkPad);
            return;
        }
        GstCaps *caps = gst_pad_get_current_caps(newPad);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        if (gst_structure_has_name(structure, "audio/x-raw")) {
            gst_pad_link(newPad, sinkPad);
        } else {
            data->logger->warn("{} decodebin produced non-audio pad: {}", data->prefix, gst_structure_get_name(structure));
        }
        gst_caps_unref(caps);
        gst_object_unref(sinkPad);
    };
    g_signal_connect_data(decode, "pad-added", G_CALLBACK(padAddedCb), data,
                          (GClosureNotify)([](gpointer d) { delete static_cast<CbData*>(d); }), (GConnectFlags)0);

    if (!gst_element_link_many(convert, amplify, encoder, sink, nullptr)) {
        m_logger->error("{} Failed to link convert → amplify → encoder → sink", m_loggingPrefix);
        gst_object_unref(pipeline);
        return false;
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    bool success = true;
    bool finished = false;
    while (!finished) {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        if (!msg)
            break;

        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            finished = true;
            break;
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            m_logger->error("{} Gain application error: {} - {}", m_loggingPrefix, err->message, dbg ? dbg : "");
            g_error_free(err);
            g_free(dbg);
            success = false;
            finished = true;
            break;
        }
        default:
            break;
        }
        gst_message_unref(msg);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);

    return success;
}

QString VolumeNormalizer::normalize(const QString &audioPath, double targetPeak)
{
    if (isNormalized(audioPath)) {
        m_logger->info("{} File already normalized, skipping: {}", m_loggingPrefix, audioPath.toStdString());
        return audioPath;
    }

    QFileInfo fi(audioPath);
    if (!fi.exists()) {
        m_logger->error("{} Audio file not found: {}", m_loggingPrefix, audioPath.toStdString());
        return {};
    }

    double currentPeak = analyzePeak(audioPath);
    if (currentPeak == -HUGE_VAL || currentPeak >= 0.0) {
        // Either analysis failed or already clipping
        if (currentPeak >= 0.0) {
            m_logger->warn("{} Audio already clipping (peak {} dBFS): {}", m_loggingPrefix, currentPeak, audioPath.toStdString());
        } else {
            m_logger->error("{} Peak analysis failed: {}", m_loggingPrefix, audioPath.toStdString());
        }
        return {};
    }

    // Calculate gain needed: targetPeak - currentPeak (both in dBFS, negative)
    // e.g. currentPeak = -6.0, targetPeak = -1.0 => gain = 5.0 dB
    double gainDb = targetPeak - currentPeak;
    if (gainDb <= 0.0) {
        // File is already loud enough (peak >= target)
        m_logger->info("{} Audio already meets target level ({:.1f} dBFS peak) - no gain needed: {}",
                       m_loggingPrefix, currentPeak, audioPath.toStdString());
        return audioPath;
    }

    QString outputPath = fi.absolutePath() + QDir::separator()
                         + fi.completeBaseName() + "_normalized." + fi.suffix();

    m_logger->info("{} Normalizing {} (peak {:.1f} dBFS → target {:.1f} dBFS, gain {:.1f} dB)",
                   m_loggingPrefix, audioPath.toStdString(), currentPeak, targetPeak, gainDb);

    if (!applyGain(audioPath, outputPath, gainDb)) {
        m_logger->error("{} Failed to apply gain to: {}", m_loggingPrefix, audioPath.toStdString());
        return {};
    }

    m_logger->info("{} Normalized file written: {}", m_loggingPrefix, outputPath.toStdString());
    return outputPath;
}
