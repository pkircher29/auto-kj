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

#ifndef VOLUMENORMALIZER_H
#define VOLUMENORMALIZER_H

#include <QObject>
#include <QString>
#include <spdlog/spdlog.h>
#include <spdlog/async_logger.h>
#include <spdlog/fmt/ostr.h>
#include <gst/gst.h>

std::ostream& operator<<(std::ostream& os, const QString& s);

class VolumeNormalizer : public QObject
{
    Q_OBJECT

public:
    explicit VolumeNormalizer(QObject *parent = nullptr);

    /// Normalize the audio file to the target peak level.
    /// Returns the path to the normalized file (with "_normalized" suffix),
    /// or empty string on failure.
    /// @param audioPath  Path to the input audio file
    /// @param targetPeak Target peak level in dBFS (default -1.0)
    QString normalize(const QString &audioPath, double targetPeak = -1.0);

    /// Check if a file has already been normalized (contains _normalized suffix).
    static bool isNormalized(const QString &audioPath);

    /// GStreamer pad-added callback for decodebin → audioconvert linking.
    static void padAddedCallback(GstElement *src, GstPad *newPad, gpointer userData);

private:
    std::string m_loggingPrefix{"[VolumeNormalizer]"};
    std::shared_ptr<spdlog::logger> m_logger;

    /// Scan audio file with GStreamer to find peak sample level in dBFS.
    /// Returns -HUGE_VAL on failure.
    double analyzePeak(const QString &audioPath);

    /// Apply gain to the audio file and write a new normalized file.
    bool applyGain(const QString &inputPath, const QString &outputPath, double gainDb);

signals:
};

#endif // VOLUMENORMALIZER_H
