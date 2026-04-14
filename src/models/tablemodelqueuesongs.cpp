#include "tablemodelqueuesongs.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QTime>
#include <QMimeData>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QSvgRenderer>
#include <spdlog/fmt/ostr.h>

std::ostream & operator<<(std::ostream& os, const QString& s);

TableModelQueueSongs::TableModelQueueSongs(TableModelKaraokeSongs &karaokeSongsModel, QObject *parent)
        : QAbstractTableModel(parent), m_karaokeSongsModel(karaokeSongsModel) {
    m_logger = spdlog::get("logger");
    setFont(m_settings.applicationFont());
}

QVariant TableModelQueueSongs::headerData(int section, Qt::Orientation orientation, int role) const {
    switch (role) {
        case Qt::SizeHintRole:
            if (orientation == Qt::Horizontal)
                return getColumnSizeHint(section);
            return {};
        case Qt::FontRole:
            return m_headerFont;
        case Qt::DisplayRole:
            if (orientation == Qt::Horizontal)
                return getColumnName(section);
            return {};
        default:
            return {};
    }
}

QString TableModelQueueSongs::getColumnName(int section) {
    switch (section) {
        case COL_ID:
            return "ID";
        case COL_DBSONGID:
            return "DBSongId";
        case COL_SINGER1:
            return "Singer 1";
        case COL_SINGER2:
            return "Singer 2";
        case COL_SINGER3:
            return "Singer 3";
        case COL_SINGER4:
            return "Singer 4";
        case COL_ARTIST:
            return "Artist";
        case COL_TITLE:
            return "Title";
        case COL_SONGID:
            return "SongID";
        case COL_KEY:
            return "Key";
        case COL_DURATION:
            return "Time";
        default:
            return {};
    }
}

int TableModelQueueSongs::rowCount([[maybe_unused]]const QModelIndex &parent) const {
    return static_cast<int>(m_songs.size());
}

int TableModelQueueSongs::columnCount([[maybe_unused]]const QModelIndex &parent) const {
    return 12;
}

QVariant TableModelQueueSongs::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return {};
    switch (role) {
        case Qt::FontRole:
            if (m_songs.at(index.row()).played)
                return m_itemFontStrikeout;
            return m_itemFont;
        case Qt::ForegroundRole:
            if (m_songs.at(index.row()).played)
                return QColor("darkGrey");
            return {};
        case Qt::TextAlignmentRole:
            return getColumnTextAlignmentRoleData(index.column());
        case Qt::UserRole: {
            QVariant retVal;
            retVal.setValue(m_songs.at(index.row()));
            return retVal;
        }
        case Qt::DisplayRole:
            return getItemDisplayRoleData(index);
        default:
            return {};
    }
}

bool TableModelQueueSongs::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || (role != Qt::EditRole))
        return false;

    auto &song = m_songs.at(index.row());
    QString dbColumn;
    switch (index.column()) {
        case COL_SINGER2:
            song.cosinger2 = value.toString();
            dbColumn = "cosinger2";
            break;
        case COL_SINGER3:
            song.cosinger3 = value.toString();
            dbColumn = "cosinger3";
            break;
        case COL_SINGER4:
            song.cosinger4 = value.toString();
            dbColumn = "cosinger4";
            break;
        default:
            return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE queuesongs SET " + dbColumn + " = :val WHERE qsongid = :id");
    query.bindValue(":val", value.toString());
    query.bindValue(":id", song.id);
    if (!query.exec()) {
        m_logger->error("{} Failed to update co-singer: {}", m_loggingPrefix, query.lastError().text());
        return false;
    }

    emit dataChanged(index, index);
    emit queueModified(m_curSingerId);
    return true;
}

QVariant TableModelQueueSongs::getColumnTextAlignmentRoleData(int column) {
    switch (column) {
        case COL_KEY:
            return Qt::AlignHCenter + Qt::AlignVCenter;
        case COL_DURATION:
            return Qt::AlignRight + Qt::AlignVCenter;
        default:
            return {};
    }
}

QVariant TableModelQueueSongs::getItemDisplayRoleData(const QModelIndex &index) const {
    switch (index.column()) {
        case COL_ID:
            return m_songs.at(index.row()).id;
        case COL_DBSONGID:
            return m_songs.at(index.row()).dbSongId;
        case COL_SINGER1:
            // Singer 1 is always the primary singer name
            return m_songs.at(index.row()).singerName; // Wait, I need to add singerName to QueueSong or get it from rotation
        case COL_SINGER2:
            return m_songs.at(index.row()).cosinger2;
        case COL_SINGER3:
            return m_songs.at(index.row()).cosinger3;
        case COL_SINGER4:
            return m_songs.at(index.row()).cosinger4;
        case COL_ARTIST:
            return m_songs.at(index.row()).artist;
        case COL_TITLE:
            return m_songs.at(index.row()).title;
        case COL_SONGID:
            if (m_songs.at(index.row()).songId == "!!DROPPED!!")
                return {};
            return m_songs.at(index.row()).songId;
        case COL_KEY:
            if (m_songs.at(index.row()).keyChange == 0)
                return {};
            else if (m_songs.at(index.row()).keyChange > 0)
                return "+" + QString::number(m_songs.at(index.row()).keyChange);
            else
                return m_songs.at(index.row()).keyChange;
        case COL_DURATION:
            if (m_songs.at(index.row()).duration < 1)
                return {};
            return QTime(0, 0, 0, 0).addMSecs(m_songs.at(index.row()).duration).toString("m:ss");
        case COL_PATH:
            return m_songs.at(index.row()).path;
        default:
            return {};
    }
}

void TableModelQueueSongs::loadSinger(const int singerId) {
    m_logger->debug("{} loadSinger({}) fired", m_loggingPrefix, singerId);
    emit layoutAboutToBeChanged();
    m_songs.clear();
    m_songs.shrink_to_fit();
    m_curSingerId = singerId;
    QSqlQuery query;
    query.prepare("SELECT queuesongs.qsongid, queuesongs.singer, queuesongs.song, queuesongs.played, "
                  "queuesongs.keychg, queuesongs.position, rotationsingers.name, dbsongs.artist, "
                  "dbsongs.title, dbsongs.discid, dbsongs.duration, dbsongs.path, "
                  "queuesongs.cosinger2, queuesongs.cosinger3, queuesongs.cosinger4 FROM queuesongs "
                  "INNER JOIN rotationsingers ON rotationsingers.singerid = queuesongs.singer "
                  "INNER JOIN dbsongs ON dbsongs.songid = queuesongs.song WHERE queuesongs.singer = :singerId "
                  "ORDER BY queuesongs.position");
    query.bindValue(":singerId", singerId);
    query.exec();
    if (auto error = query.lastError(); error.type() != QSqlError::NoError)
        m_logger->error("{} DB error: {}", m_loggingPrefix, error.text());
    else
        m_logger->debug("{} Query returned {} rows", m_loggingPrefix, query.size());
    while (query.next()) {
        m_songs.emplace_back(okj::QueueSong{
                query.value(0).toInt(),
                query.value(1).toInt(),
                query.value(2).toInt(),
                query.value(3).toBool(),
                query.value(4).toInt(),
                query.value(5).toInt(),
                query.value(7).toString(),
                query.value(8).toString(),
                query.value(9).toString(),
                query.value(10).toInt(),
                query.value(11).toString(),
                query.value(6).toString(), // singerName
                query.value(12).toString(), // cosinger2
                query.value(13).toString(), // cosinger3
                query.value(14).toString()  // cosinger4
        });
    }
    emit layoutChanged();
}

int TableModelQueueSongs::getPosition(const int songId) {
    auto it = std::find_if(m_songs.begin(), m_songs.end(), [&songId](okj::QueueSong &song) {
        return (song.id == songId);
    });
    if (it == m_songs.end())
        return -1;
    return it->position;
}

bool TableModelQueueSongs::getPlayed(const int songId) {
    auto it = std::find_if(m_songs.begin(), m_songs.end(), [&songId](okj::QueueSong &song) {
        return (song.id == songId);
    });
    if (it == m_songs.end())
        return false;
    return it->played;
}

int TableModelQueueSongs::getKey(const int songId) {
    auto it = std::find_if(m_songs.begin(), m_songs.end(), [&songId](okj::QueueSong &song) {
        return (song.id == songId);
    });
    if (it == m_songs.end())
        return 0;
    return it->keyChange;
}

void TableModelQueueSongs::move(const int oldPosition, const int newPosition) {
    if (oldPosition == newPosition)
        return;
    emit layoutAboutToBeChanged();
    if (oldPosition > newPosition) {
        // moving up
        std::for_each(m_songs.begin(), m_songs.end(), [&oldPosition, &newPosition](okj::QueueSong &song) {
            if (song.position == oldPosition)
                song.position = newPosition;
            else if (song.position >= newPosition && song.position < oldPosition)
                song.position++;
        });
    } else {
        // moving down
        std::for_each(m_songs.begin(), m_songs.end(), [&oldPosition, &newPosition](okj::QueueSong &song) {
            if (song.position == oldPosition)
                song.position = newPosition;
            else if (song.position > oldPosition && song.position <= newPosition)
                song.position--;
        });
    }
    std::sort(m_songs.begin(), m_songs.end(), [](okj::QueueSong &a, okj::QueueSong &b) {
        return (a.position < b.position);
    });
    emit layoutChanged();
    commitChanges();
    emit queueModified(m_curSingerId);
}

void TableModelQueueSongs::moveSongId(const int songId, const int newPosition) {
    int oldPosition = getPosition(songId);
    if (oldPosition == -1)
        return;
    move(oldPosition, newPosition);
}

int TableModelQueueSongs::add(const int songId, const QStringList &cosingers) {
    int limit = m_settings.maxQueuePerSinger();
    if (limit > 0) {
        int unplayed = static_cast<int>(std::count_if(m_songs.begin(), m_songs.end(),
                                                       [](const okj::QueueSong &s) { return !s.played; }));
        if (unplayed >= limit) {
            m_logger->warn("{} Singer {} already has {} unplayed song(s); queue limit is {}",
                           m_loggingPrefix, m_curSingerId, unplayed, limit);
            return -1;
        }
    }
    okj::KaraokeSong ksong = m_karaokeSongsModel.getSong(songId);
    QSqlQuery query;
    query.prepare("INSERT INTO queuesongs (singer,song,artist,title,discid,path,keychg,played,position,cosinger2,cosinger3,cosinger4) "
                  "VALUES (:singerId,:songId,:songId,:songId,:songId,:songId,:key,:played,:position,:cosinger2,:cosinger3,:cosinger4)");
    query.bindValue(":singerId", m_curSingerId);
    query.bindValue(":songId", songId);
    query.bindValue(":key", 0);
    query.bindValue(":played", false);
    query.bindValue(":position", (int) m_songs.size());
    query.bindValue(":cosinger2", cosingers.size() > 0 ? cosingers.at(0) : "");
    query.bindValue(":cosinger3", cosingers.size() > 1 ? cosingers.at(1) : "");
    query.bindValue(":cosinger4", cosingers.size() > 2 ? cosingers.at(2) : "");
    query.exec();
    auto queueSongId = query.lastInsertId().toInt();
    emit layoutAboutToBeChanged();
    m_songs.emplace_back(okj::QueueSong{
            queueSongId,
            m_curSingerId,
            songId,
            false,
            0,
            (int) m_songs.size() - 1, // position is index
            ksong.artist,
            ksong.title,
            ksong.songid,
            ksong.duration,
            ksong.path,
            m_settings.kjName(), // Just a placeholder or empty? Actually singerName should be fetched
            "",
            "",
            ""
    });
    // Fix singerName after insertion if possible, or just reload singer
    loadSinger(m_curSingerId);
    emit layoutChanged();
    emit queueModified(m_curSingerId);
    return queueSongId;
}

void TableModelQueueSongs::insert(const int songId, const int position, const QStringList &cosingers) {
    add(songId, cosingers);
    move(static_cast<int>(m_songs.size()) - 1, position);
}

void TableModelQueueSongs::remove(const int songId) {
    emit layoutAboutToBeChanged();
    auto it = std::remove_if(m_songs.begin(), m_songs.end(), [&songId](okj::QueueSong &song) {
        return (song.id == songId);
    });
    m_songs.erase(it, m_songs.end());
    int pos{0};
    std::for_each(m_songs.begin(), m_songs.end(), [&pos](okj::QueueSong &song) {
        song.position = pos++;
    });
    emit layoutChanged();
    commitChanges();
    emit queueModified(m_curSingerId);
}

void TableModelQueueSongs::setKey(const int songId, const int semitones) {
    QSqlQuery query;
    query.prepare("UPDATE queuesongs SET keychg = :key WHERE qsongid = :id");
    query.bindValue(":id", songId);
    query.bindValue(":key", semitones);
    query.exec();
    auto it = std::find_if(m_songs.begin(), m_songs.end(), [&songId](okj::QueueSong &song) {
        return (song.id == songId);
    });
    if (it == m_songs.end())
        return;
    it->keyChange = semitones;
    emit dataChanged(this->index(it->position, COL_KEY), this->index(it->position, COL_KEY),
                     QVector<int>{Qt::DisplayRole});
}

void TableModelQueueSongs::setPlayed(const int songId, const bool played) {
    m_logger->debug("{} Setting songId {} to played", m_loggingPrefix, songId);
    QSqlQuery query;
    query.prepare("UPDATE queuesongs SET played = :played WHERE qsongid = :id");
    query.bindValue(":id", songId);
    query.bindValue(":played", played);
    query.exec();
    auto it = std::find_if(m_songs.begin(), m_songs.end(), [&songId](okj::QueueSong &song) {
        return (song.id == songId);
    });
    if (it == m_songs.end())
        return;
    it->played = played;
    emit dataChanged(this->index(it->position, 0), this->index(it->position, columnCount() - 1),
                     QVector<int>{Qt::FontRole, Qt::BackgroundRole, Qt::ForegroundRole});
    emit queueModified(m_curSingerId);
}

void TableModelQueueSongs::removeAll() {
    emit layoutAboutToBeChanged();
    QSqlQuery query;
    query.prepare("DELETE FROM queuesongs WHERE singer = :singerId");
    query.bindValue(":singerId", m_curSingerId);
    query.exec();
    m_songs.clear();
    m_songs.shrink_to_fit();
    emit layoutChanged();
    emit queueModified(m_curSingerId);
}

void TableModelQueueSongs::commitChanges() {
    QSqlQuery query;
    query.exec("BEGIN TRANSACTION");
    query.prepare("DELETE FROM queuesongs WHERE singer = :singerId");
    query.bindValue(":singerId", m_curSingerId);
    query.exec();
    query.prepare("INSERT INTO queuesongs (qsongid,singer,song,artist,title,discid,path,keychg,played,position) "
                  "VALUES(:id,:singerId,:songId,:songId,:songId,:songId,:songId,:key,:played,:position)");
    std::for_each(m_songs.begin(), m_songs.end(), [&](okj::QueueSong &song) {
        query.bindValue(":id", song.id);
        query.bindValue(":singerId", song.singerId);
        query.bindValue(":songId", song.dbSongId);
        query.bindValue(":key", song.keyChange);
        query.bindValue(":played", song.played);
        query.bindValue(":position", song.position);
        query.exec();
    });
    query.exec("COMMIT");
}

void TableModelQueueSongs::songAddSlot(int songId, int singerId, int keyChg, const QStringList &cosingers) {
    if (singerId == m_curSingerId) {
        int queueSongId = add(songId, cosingers);
        setKey(queueSongId, keyChg);
    } else {
        int newPos{0};
        okj::KaraokeSong ksong = m_karaokeSongsModel.getSong(songId);
        QSqlQuery query;
        // Check queue limit for the target singer
        int limit = m_settings.maxQueuePerSinger();
        if (limit > 0) {
            query.prepare("SELECT COUNT(qsongid) FROM queuesongs WHERE singer = :singerId AND played = 0");
            query.bindValue(":singerId", singerId);
            query.exec();
            if (query.first() && query.value(0).toInt() >= limit) {
                m_logger->warn("{} Singer {} already at queue limit ({})", m_loggingPrefix, singerId, limit);
                return;
            }
        }
        query.prepare("SELECT COUNT(qsongid) FROM queuesongs WHERE singer = :singerId");
        query.bindValue(":singerId", singerId);
        query.exec();
        if (auto error = query.lastError(); error.type() != QSqlError::NoError)
            m_logger->error("{} DB error: {}", m_loggingPrefix, error.text());
        if (query.first())
            newPos = query.value(0).toInt();
        query.prepare("INSERT INTO queuesongs (singer,song,artist,title,discid,path,keychg,played,position,cosinger2,cosinger3,cosinger4) "
                      "VALUES (:singerId,:songId,:songId,:songId,:songId,:songId,:key,:played,:position,:cosinger2,:cosinger3,:cosinger4)");
        query.bindValue(":singerId", singerId);
        query.bindValue(":songId", songId);
        query.bindValue(":key", keyChg);
        query.bindValue(":played", false);
        query.bindValue(":position", newPos);
        query.bindValue(":cosinger2", cosingers.size() > 0 ? cosingers.at(0) : "");
        query.bindValue(":cosinger3", cosingers.size() > 1 ? cosingers.at(1) : "");
        query.bindValue(":cosinger4", cosingers.size() > 2 ? cosingers.at(2) : "");
        query.exec();
        if (auto error = query.lastError(); error.type() != QSqlError::NoError)
            m_logger->error("{} DB error: {}", m_loggingPrefix, error.text());
    }
}


bool TableModelQueueSongs::isQueueFull(int singerId) const {
    int limit = m_settings.maxQueuePerSinger();
    if (limit <= 0) return false;
    QSqlQuery q;
    q.prepare("SELECT COUNT(qsongid) FROM queuesongs WHERE singer = :id AND played = 0");
    q.bindValue(":id", singerId);
    if (q.exec() && q.next())
        return q.value(0).toInt() >= limit;
    return false;
}

QStringList TableModelQueueSongs::mimeTypes() const {
    QStringList types;
    types << "integer/songid";
    types << "text/queueitems";
    return types;
}

QMimeData *TableModelQueueSongs::mimeData(const QModelIndexList &indexes) const {
    auto mimeData = new QMimeData();
    if (indexes.size() > 1) {
        QJsonArray jArr;
        std::for_each(indexes.begin(), indexes.end(), [&](QModelIndex index) {
            // Just using COL_ARTIST here because it's the first column that's included in the index list
            if (index.column() != COL_ARTIST)
                return;
            jArr.append(index.sibling(index.row(), 0).data().toInt());
        });
        QJsonDocument jDoc(jArr);
        mimeData->setData("text/queueitems", jDoc.toJson());
    }
    return mimeData;
}

bool TableModelQueueSongs::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                           const QModelIndex &parent) const {
    if ((data->hasFormat("integer/songid")) || (data->hasFormat("text/queueitems")) ||
        data->hasFormat("text/uri-list")) {
        return true;
    }
    return false;
}

bool TableModelQueueSongs::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                        const QModelIndex &parent) {
    if (getSingerId() == -1) {
        emit songDroppedWithoutSinger();
        return false;
    }
    if (action == Qt::MoveAction && data->hasFormat("text/queueitems")) {
        QJsonDocument jDoc = QJsonDocument::fromJson(data->data("text/queueitems"));
        QJsonArray jArr = jDoc.array();
        auto ids = jArr.toVariantList();
        int dropRow{0};
        if (parent.row() >= 0)
            dropRow = parent.row();
        else if (row >= 0)
            dropRow = row;
        else
            dropRow = rowCount() - 1;
        if (ids.empty())
            return false;
        if (getPosition(ids.at(0).toInt()) > dropRow)
            std::reverse(ids.begin(), ids.end());
        std::for_each(ids.begin(), ids.end(), [&](auto val) {
            int oldPosition = getPosition(val.toInt());
            if (oldPosition < dropRow && dropRow != rowCount() - 1)
                moveSongId(val.toInt(), dropRow - 1);
            else
                moveSongId(val.toInt(), dropRow);
        });
        if (dropRow == rowCount() - 1) {
            // moving to bottom
            emit qSongsMoved(dropRow - ids.size() + 1, 0, rowCount() - 1, columnCount() - 1);
        } else if (getPosition(ids.at(0).toInt()) < dropRow) {
            // moving down
            emit qSongsMoved(dropRow - ids.size(), 0, dropRow - 1, columnCount() - 1);
        } else {
            // moving up
            emit qSongsMoved(dropRow, 0, dropRow + ids.size() - 1, columnCount() - 1);
        }
        commitChanges();
        return true;
    }
    if (data->hasFormat("integer/songid")) {
        unsigned int dropRow;
        if (parent.row() >= 0)
            dropRow = parent.row();
        else if (row >= 0)
            dropRow = row;
        else
            dropRow = rowCount();
        int songid;
        QByteArray bytedata = data->data("integer/songid");
        songid = QString(bytedata.data()).toInt();
        insert(songid, static_cast<int>(dropRow));
        commitChanges();
        return true;
    } else if (data->hasFormat("text/uri-list")) {
        QList<QUrl> items = data->urls();
        if (!items.empty()) {
            unsigned int droprow;
            if (parent.row() >= 0) {
                droprow = parent.row();
            } else if (row >= 0) {
                droprow = row;
            } else {
                droprow = rowCount();
            }
            emit filesDroppedOnSinger(items, m_curSingerId, static_cast<int>(droprow));
        }
        commitChanges();
        return true;
    }
    return false;
}

Qt::DropActions TableModelQueueSongs::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags TableModelQueueSongs::flags(const QModelIndex &index) const {
    auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    if (index.column() == COL_SINGER2 || index.column() == COL_SINGER3 || index.column() == COL_SINGER4)
        flags |= Qt::ItemIsEditable;
    return flags;
}

void TableModelQueueSongs::sort(int column, Qt::SortOrder order) {
    emit layoutAboutToBeChanged();
    if (order == Qt::AscendingOrder) {
        std::sort(m_songs.begin(), m_songs.end(), [&column](okj::QueueSong &a, okj::QueueSong &b) {
            switch (column) {
                case COL_ARTIST:
                    return (a.artist < b.artist);
                case COL_TITLE:
                    return (a.title < b.title);
                case COL_SONGID:
                    return (a.songId < b.songId);
                case COL_DURATION:
                    return (a.duration < b.duration);
                case COL_KEY:
                    return (a.keyChange < b.keyChange);
                default:
                    return (a.position < b.position);
            }
        });
    } else {
        std::sort(m_songs.rbegin(), m_songs.rend(), [&column](okj::QueueSong &a, okj::QueueSong &b) {
            switch (column) {
                case COL_ARTIST:
                    return (a.artist < b.artist);
                case COL_TITLE:
                    return (a.title < b.title);
                case COL_SONGID:
                    return (a.songId < b.songId);
                case COL_DURATION:
                    return (a.duration < b.duration);
                case COL_KEY:
                    return (a.keyChange < b.keyChange);
                default:
                    return (a.position < b.position);
            }
        });
    }
    int pos{0};
    std::for_each(m_songs.begin(), m_songs.end(), [&pos](okj::QueueSong &song) {
        song.position = pos++;
    });
    emit layoutChanged();
    commitChanges();
}

void TableModelQueueSongs::setFont(const QFont &font) {
    m_itemFont = font;
    m_itemFontStrikeout = font;
    m_itemFontStrikeout.setStrikeOut(true);
    m_itemHeight = m_itemFontMetrics.height() + 6;
    m_headerFont = font;
    m_headerFont.setBold(true);
}

QSize TableModelQueueSongs::getColumnSizeHint(int section) const {
    switch (section) {
        case COL_ID:
            return QSize(m_itemFontMetrics.size(Qt::TextSingleLine, "_ID").width(), m_itemHeight);
        case COL_ARTIST:
            return QSize(m_itemFontMetrics.size(Qt::TextSingleLine, "_Artist").width(), m_itemHeight);
        case COL_TITLE:
            return QSize(m_itemFontMetrics.size(Qt::TextSingleLine, "_Title").width(), m_itemHeight);
        case COL_SONGID:
            return QSize(m_itemFontMetrics.size(Qt::TextSingleLine, "XXXX0000000-01-00").width(), m_itemHeight);
        case COL_KEY:
            return QSize(m_itemFontMetrics.size(Qt::TextSingleLine, "_Key_").width(), m_itemHeight);
        case COL_DURATION:
            return QSize(m_itemFontMetrics.size(Qt::TextSingleLine, "_00:00").width(), m_itemHeight);
        case COL_PATH:
            // This is actually where the delete icon is displayed, reused the column since we don't actually show path info
        default:
            return QSize(m_itemHeight + 6, m_itemHeight);
    }
}

void ItemDelegateQueueSongs::resizeIconsForFont(const QFont &font) {
    QString thm = (m_settings.theme() == 1) ? ":/theme/Icons/autokjbreeze-dark/" : ":/theme/Icons/autokjbreeze/";
    m_curFontHeight = QFontMetrics(font).height();
    m_iconDelete = QImage(m_curFontHeight, m_curFontHeight, QImage::Format_ARGB32);
    m_iconDelete.fill(Qt::transparent);
    QPainter painterDelete(&m_iconDelete);
    QSvgRenderer svgrndrDelete(thm + "actions/16/edit-delete.svg");
    svgrndrDelete.render(&painterDelete);
}

ItemDelegateQueueSongs::ItemDelegateQueueSongs(QObject *parent) :
        QItemDelegate(parent) {
    resizeIconsForFont(m_settings.applicationFont());
}

void ItemDelegateQueueSongs::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (index.column() == TableModelQueueSongs::COL_PATH) {
        int topPad = (option.rect.height() - m_curFontHeight) / 2;
        int leftPad = (option.rect.width() - m_curFontHeight) / 2;
        painter->drawImage(QRect(option.rect.x() + leftPad, option.rect.y() + topPad, m_curFontHeight, m_curFontHeight),
                           m_iconDelete);
        return;
    }
    QItemDelegate::paint(painter, option, index);
}
