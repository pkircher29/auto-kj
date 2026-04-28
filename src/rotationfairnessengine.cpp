#include "rotationfairnessengine.h"
#include "settings.h"
#include "songcanonicalizer.h"
#include <QSqlQuery>
#include <QSqlError>
#include <spdlog/spdlog.h>

RotationFairnessEngine::RotationFairnessEngine(TableModelRotation &rotModel, Settings &settings, QObject *parent)
    : QObject(parent), m_rotModel(rotModel), m_settings(settings) {}

void RotationFairnessEngine::loadState() {
    // Ensure rotation_meta table has the current_round row
    QSqlQuery q;
    q.exec("INSERT OR IGNORE INTO rotation_meta (key, value) VALUES ('current_round', '1')");

    // Backfill any missing canonical keys so cross-version blocking applies to existing rows.
    QSqlQuery missing;
    missing.prepare("SELECT id, song_id FROM songs_played_tonight "
                    "WHERE canonical_song_key IS NULL OR canonical_song_key = ''");
    if (missing.exec()) {
        while (missing.next()) {
            const int rowId = missing.value(0).toInt();
            const int songId = missing.value(1).toInt();
            const QString canonicalKey = canonicalSongKeyForSongId(songId);
            if (canonicalKey.isEmpty())
                continue;
            QSqlQuery update;
            update.prepare("UPDATE songs_played_tonight SET canonical_song_key = :song_key WHERE id = :id");
            update.bindValue(":song_key", canonicalKey);
            update.bindValue(":id", rowId);
            update.exec();
        }
    }
}

int RotationFairnessEngine::currentRound() const {
    QSqlQuery q;
    q.exec("SELECT value FROM rotation_meta WHERE key = 'current_round'");
    if (q.next())
        return q.value(0).toInt();
    return 1;
}

bool RotationFairnessEngine::hasSungThisRound(int singerId) const {
    QSqlQuery q;
    q.prepare("SELECT sung_this_round FROM rotationsingers WHERE singerid = :id");
    q.bindValue(":id", singerId);
    if (q.exec() && q.next())
        return q.value(0).toInt() != 0;
    return false;
}

bool RotationFairnessEngine::hasSungThisRoundByName(const QString &name) const {
    int id = resolveNameToId(name);
    if (id == -1) return false;
    return hasSungThisRound(id);
}

QString RotationFairnessEngine::duplicateTurnWarning(const QString &singerName) const {
    if (hasSungThisRoundByName(singerName))
        return QString("%1 has already sung this round.").arg(singerName);
    return {};
}

void RotationFairnessEngine::onSongPlayed(int primarySingerId, const QStringList &cosingers) {
    markSungThisRound(primarySingerId);

    for (const QString &name : cosingers) {
        int id = resolveNameToId(name);
        if (id != -1)
            markSungThisRound(id);
    }

    checkAndAdvanceRound();
}

void RotationFairnessEngine::markSungThisRound(int singerId) {
    QSqlQuery q;
    q.prepare("UPDATE rotationsingers SET sung_this_round = 1 WHERE singerid = :id");
    q.bindValue(":id", singerId);
    if (!q.exec())
        spdlog::get("logger")->error("[FairnessEngine] Failed to mark singer {}: {}",
                                     singerId, q.lastError().text().toStdString());
    emit m_rotModel.rotationModified();
}

int RotationFairnessEngine::resolveNameToId(const QString &name) const {
    // Check aliases first
    QSqlQuery aliasQ;
    aliasQ.prepare("SELECT canonical_name FROM singerAliases WHERE alias_name = :n COLLATE NOCASE");
    aliasQ.bindValue(":n", name);
    QString lookupName = name;
    if (aliasQ.exec() && aliasQ.next())
        lookupName = aliasQ.value(0).toString();

    const auto &singer = m_rotModel.getSingerByName(lookupName);
    if (singer.isValid())
        return singer.id;

    // Try direct name match too if alias lookup returned something different
    if (lookupName != name) {
        const auto &direct = m_rotModel.getSingerByName(name);
        if (direct.isValid())
            return direct.id;
    }
    return -1;
}

void RotationFairnessEngine::checkAndAdvanceRound() {
    // Don't advance if there are no singers in the rotation at all
    QSqlQuery countQ;
    countQ.exec("SELECT COUNT(*) FROM rotationsingers");
    int totalSingers = 0;
    if (countQ.next())
        totalSingers = countQ.value(0).toInt();
    if (totalSingers == 0)
        return;

    // Check if all singers in the rotation have sung_this_round = 1
    QSqlQuery q;
    q.exec("SELECT COUNT(*) FROM rotationsingers WHERE sung_this_round = 0");
    if (q.next() && q.value(0).toInt() == 0) {
        advanceRound();
    }
}

void RotationFairnessEngine::advanceRound() {
    QSqlQuery q;
    q.exec("UPDATE rotationsingers SET sung_this_round = 0");
    q.exec("UPDATE rotation_meta SET value = CAST(CAST(value AS INTEGER) + 1 AS TEXT) WHERE key = 'current_round'");
    spdlog::get("logger")->info("[FairnessEngine] Round advanced to {}", currentRound());
    emit m_rotModel.rotationModified();
}

QString RotationFairnessEngine::checkRoundViolation(int singerId) const {
    if (!m_settings.fairnessEnabled() || !m_settings.fairnessEnforceRound())
        return {};
    if (!hasSungThisRound(singerId))
        return {};
    int pending = singersPendingThisRound();
    if (pending == 0)
        return {};
    const auto &singer = m_rotModel.getSinger(singerId);
    return QString("%1 has already sung this round. %2 singer(s) still haven't had a turn.")
        .arg(singer.name).arg(pending);
}

int RotationFairnessEngine::singersPendingThisRound() const {
    QSqlQuery q;
    q.exec("SELECT COUNT(*) FROM rotationsingers WHERE sung_this_round = 0");
    if (q.next())
        return q.value(0).toInt();
    return 0;
}

RotationFairnessEngine::SongPlayedTonightResult RotationFairnessEngine::checkSongPlayedTonight(int songId) const {
    if (!m_settings.fairnessEnabled() || !m_settings.fairnessEnforceSongNight())
        return {false, {}};
    const QString canonicalKey = canonicalSongKeyForSongId(songId);
    QSqlQuery q;
    q.prepare("SELECT singer_name FROM songs_played_tonight "
              "WHERE ((canonical_song_key = :song_key AND :song_key <> '') OR song_id = :sid) "
              "LIMIT 1");
    q.bindValue(":song_key", canonicalKey);
    q.bindValue(":sid", songId);
    if (q.exec() && q.next())
        return {true, q.value(0).toString()};
    return {false, {}};
}

void RotationFairnessEngine::recordSongPlayed(int songId, int singerId, const QString &singerName) {
    const QString canonicalKey = canonicalSongKeyForSongId(songId);
    QSqlQuery q;
    q.prepare("INSERT INTO songs_played_tonight (song_id, canonical_song_key, singer_id, singer_name) "
              "VALUES (:song_id, :song_key, :singer_id, :singer_name)");
    q.bindValue(":song_id", songId);
    q.bindValue(":song_key", canonicalKey);
    q.bindValue(":singer_id", singerId > 0 ? QVariant(singerId) : QVariant());
    q.bindValue(":singer_name", singerName);
    if (!q.exec())
        spdlog::get("logger")->error("[FairnessEngine] Failed to record song played tonight: {}",
                                     q.lastError().text().toStdString());
}

void RotationFairnessEngine::resetNightlyTracking() {
    QSqlQuery q;
    q.exec("DELETE FROM songs_played_tonight");
    spdlog::get("logger")->info("[FairnessEngine] Nightly song tracking reset");
}

bool RotationFairnessEngine::hasSongOverrideForSinger(int songId, int singerId) const {
    const QString canonicalKey = canonicalSongKeyForSongId(songId);
    QSqlQuery q;
    q.prepare("SELECT 1 FROM songs_played_tonight "
              "WHERE ((canonical_song_key = :song_key AND :song_key <> '') OR song_id = :song_id) "
              "AND override_allowed_singer_id = :singer_id");
    q.bindValue(":song_key", canonicalKey);
    q.bindValue(":song_id", songId);
    q.bindValue(":singer_id", singerId);
    return q.exec() && q.next();
}

void RotationFairnessEngine::recordSongOverride(int songId, int singerId) {
    const QString canonicalKey = canonicalSongKeyForSongId(songId);
    QSqlQuery q;
    q.prepare("UPDATE songs_played_tonight SET override_allowed_singer_id = :singer_id "
              "WHERE ((canonical_song_key = :song_key AND :song_key <> '') OR song_id = :song_id) "
              "AND override_allowed_singer_id IS NULL");
    q.bindValue(":singer_id", singerId);
    q.bindValue(":song_key", canonicalKey);
    q.bindValue(":song_id", songId);
    if (!q.exec())
        spdlog::get("logger")->error("[FairnessEngine] Failed to record song override: {}",
                                     q.lastError().text().toStdString());
}

QString RotationFairnessEngine::canonicalSongKeyForSongId(int songId) const {
    QSqlQuery q;
    q.prepare("SELECT artist, title FROM dbsongs WHERE songid = :sid LIMIT 1");
    q.bindValue(":sid", songId);
    if (q.exec() && q.next()) {
        return okj::canonicalSongKey(q.value(0).toString(), q.value(1).toString());
    }
    return {};
}
