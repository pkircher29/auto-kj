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

#include "artistcache.h"
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>

ArtistCache::ArtistCache(QObject *parent)
    : QObject(parent)
{
    m_logger = spdlog::get("logger");
    ensureTable();
    loadCache();
}

void ArtistCache::ensureTable()
{
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS artist_cache ("
               "filename_pattern TEXT PRIMARY KEY,"
               "artist_name TEXT NOT NULL,"
               "match_count INT DEFAULT 1,"
               "auto_confirm INT DEFAULT 0"
               ")");
    if (query.lastError().isValid()) {
        qWarning() << "[ArtistCache] Failed to create artist_cache table:" << query.lastError().text();
    }
}

void ArtistCache::loadCache()
{
    ensureTable();
    m_cache.clear();
    m_silent.clear();

    QSqlQuery query;
    query.exec("SELECT filename_pattern, artist_name, auto_confirm FROM artist_cache");
    while (query.next()) {
        QString pattern = query.value(0).toString();
        m_cache[pattern] = query.value(1).toString();
        m_silent[pattern] = (query.value(2).toInt() != 0);
    }
    m_logger->info("{} Loaded {} artist cache entries", m_loggingPrefix, m_cache.size());
}

QString ArtistCache::lookup(const QString &filenamePattern)
{
    return m_cache.value(filenamePattern, QString());
}

void ArtistCache::confirm(const QString &filenamePattern, const QString &artistName)
{
    ensureTable();
    m_cache[filenamePattern] = artistName;

    QSqlQuery query;
    query.prepare("INSERT INTO artist_cache (filename_pattern, artist_name, match_count, auto_confirm) "
                   "VALUES (:pattern, :name, 1, 0) "
                   "ON CONFLICT(filename_pattern) DO UPDATE SET "
                   "match_count = match_count + 1, "
                   "auto_confirm = CASE WHEN match_count + 1 >= 3 THEN 1 ELSE 0 END");
    query.bindValue(":pattern", filenamePattern);
    query.bindValue(":name", artistName);
    if (!query.exec()) {
        m_logger->error("{} Failed to confirm artist: {}", m_loggingPrefix, query.lastError().text().toStdString());
    }

    // Update silent cache
    m_silent[filenamePattern] = (query.exec() && query.numRowsAffected() >= 3);
    // Simplified: just check if it has been confirmed 3+ times
    QSqlQuery countQuery;
    countQuery.prepare("SELECT match_count >= 3 FROM artist_cache WHERE filename_pattern = :pattern");
    countQuery.bindValue(":pattern", filenamePattern);
    if (countQuery.exec() && countQuery.next()) {
        m_silent[filenamePattern] = countQuery.value(0).toBool();
    }
}

bool ArtistCache::isSilent(const QString &filenamePattern)
{
    ensureTable();
    QSqlQuery query;
    query.prepare("SELECT match_count >= 3 FROM artist_cache WHERE filename_pattern = :pattern");
    query.bindValue(":pattern", filenamePattern);
    if (query.exec() && query.next()) {
        return query.value(0).toBool();
    }
    return false;
}

QStringList ArtistCache::unknownPatterns()
{
    ensureTable();
    QStringList patterns;
    QSqlQuery query;
    query.exec("SELECT filename_pattern FROM artist_cache WHERE match_count < 3 ORDER BY match_count ASC");
    while (query.next()) {
        patterns << query.value(0).toString();
    }
    return patterns;
}

int ArtistCache::count()
{
    ensureTable();
    QSqlQuery query;
    query.exec("SELECT COUNT(*) FROM artist_cache");
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}
