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

#ifndef AUTOZIPPER_H
#define AUTOZIPPER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <spdlog/spdlog.h>
#include <spdlog/async_logger.h>
#include <spdlog/fmt/ostr.h>

std::ostream& operator<<(std::ostream& os, const QString& s);

struct ExtractedFiles {
    QString sourceZip;
    QString destDir;
    QString cdgPath;
    QString audioPath;
    QString audioExt;
    bool valid{false};
};

class AutoZipper : public QObject
{
    Q_OBJECT

public:
    explicit AutoZipper(QObject *parent = nullptr);

    /// Extract .cdg and .mp3 from a karaoke zip to a temp import directory.
    /// Returns the extracted file info, or an invalid ExtractedFiles on failure.
    ExtractedFiles extract(const QString &sourceZipPath);

    /// Check if a zip has already been imported (by comparing source path + mtime against the DB).
    bool wasAlreadyImported(const QString &sourceZipPath);

    /// Register a successful import so it won't be re-processed.
    void markImported(const QString &sourceZipPath);

    /// Get the import working directory (AppData/Auto-KJ/imported/).
    static QString importDir();

    /// Remove loose extracted files (cdg + audio) from a previous import extraction.
    void cleanupExtracted(const ExtractedFiles &files);

    /// Remove original source zip after successful processing.
    void removeSourceZip(const QString &sourceZipPath);

private:
    std::string m_loggingPrefix{"[AutoImporter]"};
    std::shared_ptr<spdlog::logger> m_logger;

    /// Ensure the SQLite tables exist.
    static void ensureTables();

signals:
};

#endif // AUTOZIPPER_H
