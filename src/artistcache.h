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

#ifndef ARTISTCACHE_H
#define ARTISTCACHE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <spdlog/spdlog.h>
#include <spdlog/async_logger.h>
#include <spdlog/fmt/ostr.h>

std::ostream& operator<<(std::ostream& os, const QString& s);

class ArtistCache : public QObject
{
    Q_OBJECT

public:
    explicit ArtistCache(QObject *parent = nullptr);

    /// Look up an artist by filename pattern.
    /// Returns the artist name if known, or empty string if unknown.
    QString lookup(const QString &filenamePattern);

    /// Record an artist match for a filename pattern.
    void confirm(const QString &filenamePattern, const QString &artistName);

    /// Returns true if this pattern has been confirmed 3+ times (don't need to ask again).
    bool isSilent(const QString &filenamePattern);

    /// Returns a list of filename patterns that have never been confirmed.
    QStringList unknownPatterns();

    /// Load all known patterns into memory for fast lookup.
    void loadCache();

    /// Total cached entries count.
    int count();

private:
    std::string m_loggingPrefix{"[ArtistCache]"};
    std::shared_ptr<spdlog::logger> m_logger;

    /// In-memory cache: pattern → artist name
    QMap<QString, QString> m_cache;

    /// In-memory cache: pattern → auto_confirm flag (silent after 3+ matches)
    QMap<QString, bool> m_silent;

    void ensureTable();

signals:
};

#endif // ARTISTCACHE_H
