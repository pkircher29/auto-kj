/*
 * Copyright (c) 2013-2016 Thomas Isaac Lightburn
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


#ifndef MZARCHIVE_H
#define MZARCHIVE_H

#include <QObject>
#include <QStringList>
#include <okarchive.h>
#include <spdlog/spdlog.h>
#include <spdlog/async_logger.h>
#include <spdlog/fmt/ostr.h>

std::ostream& operator<<(std::ostream& os, const QString& s);


class MzArchive : public QObject
{
    Q_OBJECT
public:
    explicit MzArchive(const QString &ArchiveFile, QObject *parent = nullptr);
    explicit MzArchive(QObject *parent = nullptr);
    int getSongDuration();
    void setArchiveFile(const QString &value);
    bool checkCDG();
    bool checkAudio();
    QString audioExtension();
    bool extractAudio(const QString& destPath, const QString& destFile);
    bool extractCdg(const QString& destPath, const QString& destFile);
    bool isValidKaraokeFile();
    QString getLastError();

    /// Create a new zip archive from a list of files.
    /// @param sourceDir   Directory containing the files to zip (paths are relative to this).
    /// @param outputPath  Full path for the output .zip file.
    /// @param files       List of filenames (basename only, relative to sourceDir) to include.
    /// @param compressionLevel 0 (store) to 9 (max), default 6.
    /// @return true on success, false on failure.
    bool createArchive(const QString &sourceDir, const QString &outputPath, const QStringList &files, int compressionLevel = 6);

private:
    QString archiveFile;
    QString audioExt;
    QString lastError;
    bool findCDG();
    bool findAudio();
    unsigned int m_audioFileIndex{0};
    unsigned int m_cdgFileIndex{0};
    int m_cdgSize{0};
    unsigned int m_audioSize{0};
    bool m_audioSupportedCompression{false};
    bool m_cdgSupportedCompression{false};
    bool m_cdgFound{false};
    bool m_audioFound{false};
    bool findEntries();
    QStringList audioExtensions;
    OkArchive oka;
    std::string m_loggingPrefix{"[MZArchive]"};
    std::shared_ptr<spdlog::logger> m_logger;

signals:

public slots:
};

#endif // MZARCHIVE_H
