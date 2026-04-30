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

#include "autozipper.h"
#include "mzarchive.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDateTime>
#include <QApplication>

AutoZipper::AutoZipper(QObject *parent)
    : QObject(parent)
{
    m_logger = spdlog::get("logger");
    ensureTables();
}

QString AutoZipper::importDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QDir::separator() + "imported";
    QDir().mkpath(dir);
    return dir;
}

bool AutoZipper::wasAlreadyImported(const QString &sourceZipPath)
{
    ensureTables();
    QSqlQuery query;
    query.prepare("SELECT last_modified FROM imported_files WHERE source_path = :path");
    query.bindValue(":path", sourceZipPath);
    if (query.exec() && query.next()) {
        QDateTime dbMtime = query.value(0).toDateTime();
        QFileInfo fi(sourceZipPath);
        QDateTime fileMtime = fi.lastModified();
        // If the file hasn't changed since we imported it, skip it
        if (dbMtime == fileMtime) {
            return true;
        }
        // File changed — delete the old record so we re-import
        QSqlQuery del;
        del.prepare("DELETE FROM imported_files WHERE source_path = :path");
        del.bindValue(":path", sourceZipPath);
        del.exec();
    }
    return false;
}

static void cleanupTempDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return;
    dir.removeRecursively();
}

void AutoZipper::markImported(const QString &sourceZipPath)
{
    ensureTables();
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO imported_files (source_path, last_modified, status) "
                  "VALUES (:path, :mtime, :status)");
    query.bindValue(":path", sourceZipPath);
    QFileInfo fi(sourceZipPath);
    query.bindValue(":mtime", fi.lastModified());
    query.bindValue(":status", "imported");
    if (!query.exec()) {
        m_logger->error("{} Failed to mark import: {}", m_loggingPrefix, query.lastError().text().toStdString());
    }
}

ExtractedFiles AutoZipper::extract(const QString &sourceZipPath)
{
    ExtractedFiles result;
    result.sourceZip = sourceZipPath;

    // Check if already imported with same mtime
    if (wasAlreadyImported(sourceZipPath)) {
        m_logger->info("{} Skipping already-imported zip: {}", m_loggingPrefix, sourceZipPath.toStdString());
        return result; // valid=false
    }

    QFileInfo fi(sourceZipPath);
    if (!fi.exists() || !fi.isFile()) {
        m_logger->error("{} Source zip does not exist: {}", m_loggingPrefix, sourceZipPath.toStdString());
        return result;
    }

    // Create a unique temp dir for this extraction
    QString baseName = fi.completeBaseName();
    QString destDir = importDir() + QDir::separator() + baseName + "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(destDir);

    MzArchive archive(sourceZipPath, this);

    if (!archive.isValidKaraokeFile()) {
        m_logger->error("{} Invalid karaoke zip: {} - {}", m_loggingPrefix, sourceZipPath.toStdString(), archive.getLastError().toStdString());
        cleanupTempDir(destDir);
        return result;
    }

    // Find audio extension from archive
    QString audioExt = archive.audioExtension();
    if (audioExt.isEmpty()) {
        m_logger->error("{} No audio file found in zip: {}", m_loggingPrefix, sourceZipPath.toStdString());
        cleanupTempDir(destDir);
        return result;
    }

    // Extract CDG
    QString cdgDest = baseName + ".cdg";
    if (!archive.extractCdg(destDir, cdgDest)) {
        m_logger->error("{} Failed to extract CDG from: {}", m_loggingPrefix, sourceZipPath.toStdString());
        cleanupTempDir(destDir);
        return result;
    }

    // Extract Audio
    QString audioDest = baseName + audioExt;
    if (!archive.extractAudio(destDir, audioDest)) {
        m_logger->error("{} Failed to extract audio from: {}", m_loggingPrefix, sourceZipPath.toStdString());
        cleanupTempDir(destDir);
        return result;
    }

    result.destDir = destDir;
    result.cdgPath = destDir + QDir::separator() + cdgDest;
    result.audioPath = destDir + QDir::separator() + audioDest;
    result.audioExt = audioExt;
    result.valid = true;

    m_logger->info("{} Extracted {} to {}", m_loggingPrefix, sourceZipPath.toStdString(), destDir.toStdString());
    return result;
}

void AutoZipper::cleanupExtracted(const ExtractedFiles &files)
{
    if (!files.valid)
        return;

    // Remove the .cdg and audio files
    if (!files.cdgPath.isEmpty()) {
        QFile::remove(files.cdgPath);
    }
    if (!files.audioPath.isEmpty()) {
        QFile::remove(files.audioPath);
    }

    // Remove the temp directory if empty
    QDir dir(files.destDir);
    if (dir.exists()) {
        dir.rmdir(".");
    }

    m_logger->info("{} Cleaned up temp files in {}", m_loggingPrefix, files.destDir.toStdString());
}

void AutoZipper::removeSourceZip(const QString &sourceZipPath)
{
    QFile::remove(sourceZipPath);
    m_logger->info("{} Removed source zip: {}", m_loggingPrefix, sourceZipPath.toStdString());
}

void AutoZipper::ensureTables()
{
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS imported_files ("
               "source_path TEXT PRIMARY KEY,"
               "dest_dir TEXT,"
               "last_modified DATETIME,"
               "status TEXT DEFAULT 'imported'"
               ")");
    if (query.lastError().isValid()) {
        qWarning() << "[AutoImporter] Failed to create imported_files table:" << query.lastError().text();
    }
}
