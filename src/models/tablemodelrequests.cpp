/*
 * Copyright (c) 2013-2021 Thomas Isaac Lightburn
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

#include "tablemodelrequests.h"
#include <QDateTime>


TableModelRequests::TableModelRequests(AutoKJServerClient &songbookAPI, QObject *parent) :
        QAbstractTableModel(parent),
        songbookApi(songbookAPI) {
    m_logger = spdlog::get("logger");
    connect(&songbookApi, &AutoKJServerClient::requestsChanged, this, &TableModelRequests::requestsChanged);
    QString thm = (m_settings.theme() == 1) ? ":/theme/Icons/autokjbreeze-dark/" : ":/theme/Icons/autokjbreeze/";
    delete16 = QIcon(thm + "actions/16/edit-delete.svg");
    delete22 = QIcon(thm + "actions/22/edit-delete.svg");
}

void TableModelRequests::requestsChanged(const OkjsRequests &requests) {
    emit layoutAboutToBeChanged();
    m_requests.clear();
    for (const auto &request : requests) {
        m_requests << Request(
            request.requestId, 
            request.singer, 
            request.artist, 
            request.title, 
            request.time, 
            request.key, 
            request.cosinger2, 
            request.cosinger3, 
            request.cosinger4,
            request.isSharedDevice,
            request.noShowCount,
            request.removedCount
        );
    }
    emit layoutChanged();
}

int TableModelRequests::rowCount(const QModelIndex &parent) const {
    return m_requests.size();
}

int TableModelRequests::columnCount(const QModelIndex &parent) const {
    return 6;
}

QVariant TableModelRequests::data(const QModelIndex &index, int role) const {
    QSize sbSize(QFontMetrics(m_settings.applicationFont()).height(), QFontMetrics(m_settings.applicationFont()).height());
    if (!index.isValid())
        return {};

    if (index.row() >= m_requests.size() || index.row() < 0)
        return {};
    if ((index.column() == 5) && (role == Qt::DecorationRole)) {
        if (sbSize.height() > 18)
            return delete22.pixmap(sbSize);
        else
            return delete16.pixmap(sbSize);
    }
    if (role == Qt::TextAlignmentRole)
        switch (index.column()) {
            case KEYCHG:
                return Qt::AlignCenter;
            default:
                return Qt::AlignLeft;
        }
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        const Request &req = m_requests.at(index.row());
        switch (index.column()) {
            case SINGER: {
                if (role == Qt::ToolTipRole) {
                    QString tt = req.singer();
                    if (req.isSharedDevice()) tt += "\n[Shared Device]";
                    if (req.noShowCount() > 0) tt += QString("\n%1 No-shows").arg(req.noShowCount());
                    if (req.removedCount() > 0) tt += QString("\n%1 Removals").arg(req.removedCount());
                    return tt;
                }
                QString s = req.singer();
                if (req.isSharedDevice()) s = "[S] " + s;
                if (req.noShowCount() > 0) s += " ⚠️";
                if (req.removedCount() > 0) s += " 🚫";
                return s;
            }
            case ARTIST:
                return req.artist();
            case TITLE:
                return req.title();
            case KEYCHG:
                if (req.key() == 0)
                    return "";
                else if (req.key() > 0)
                    return "+" + QString::number(req.key());
                else
                    return QString::number(req.key());
            case TIMESTAMP:
                QDateTime ts;
                ts.setTime_t(req.timeStamp());
                return ts.toString("M-d-yy h:mm ap");
        }
    }
    if (role == Qt::UserRole)
        return m_requests.at(index.row()).requestId();
    return {};
}

QVariant TableModelRequests::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case SINGER:
                return "Singer";
            case ARTIST:
                return "Artist";
            case TITLE:
                return "Title";
            case KEYCHG:
                return "Key";
            case TIMESTAMP:
                return "Received";
            default:
                return "";
        }
    }
    return {};
}

Qt::ItemFlags TableModelRequests::flags(const QModelIndex &index) const {
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int TableModelRequests::count() {
    return m_requests.count();
}

int Request::key() const {
    return m_key;
}

Request::Request(const QString &requestId, const QString &Singer, const QString &Artist, const QString &Title, int ts, int key, const QString &cs2, const QString &cs3, const QString &cs4, bool shared, int noShows, int removals) {
    m_requestId = requestId;
    m_singer = Singer;
    m_artist = Artist;
    m_title = Title;
    m_timeStamp = ts;
    m_key = key;
    m_cosinger2 = cs2;
    m_cosinger3 = cs3;
    m_cosinger4 = cs4;
    m_isSharedDevice = shared;
    m_noShowCount = noShows;
    m_removedCount = removals;
}

QString Request::requestId() const {
    return m_requestId;
}

int Request::timeStamp() const {
    return m_timeStamp;
}

QString Request::artist() const {
    return m_artist;
}

void Request::setArtist(const QString &artist) {
    m_artist = artist;
}

QString Request::title() const {
    return m_title;
}

void Request::setTitle(const QString &title) {
    m_title = title;
}

QString Request::singer() const {
    return m_singer;
}

void Request::setSinger(const QString &singer) {
    m_singer = singer;
}






