#ifndef ROTATIONFAIRNESSENGINE_H
#define ROTATIONFAIRNESSENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "models/tablemodelrotation.h"
#include "settings.h"

class RotationFairnessEngine : public QObject {
    Q_OBJECT
public:
    explicit RotationFairnessEngine(TableModelRotation &rotModel, Settings &settings, QObject *parent = nullptr);

    struct SongPlayedTonightResult {
        bool blocked;
        QString singerName;
    };

    // Called when a song is marked as played
    void onSongPlayed(int primarySingerId, const QStringList &cosingers);

    // Check if singer has already sung this round
    bool hasSungThisRound(int singerId) const;
    bool hasSungThisRoundByName(const QString &name) const;

    // Returns a warning string if singer would use their turn twice this round; empty if fine
    QString duplicateTurnWarning(const QString &singerName) const;

    // Returns a warning if round enforcement is on and singer already sang; empty if ok
    QString checkRoundViolation(int singerId) const;

    // Count singers who haven't sung yet this round
    int singersPendingThisRound() const;

    // Once-per-night song tracking
    SongPlayedTonightResult checkSongPlayedTonight(int songId) const;
    void recordSongPlayed(int songId, int singerId, const QString &singerName);
    void resetNightlyTracking();

    // Per-singer DJ override for a blocked song
    bool hasSongOverrideForSinger(int songId, int singerId) const;
    void recordSongOverride(int songId, int singerId);

    // Current round number
    int currentRound() const;

    // Reload state from DB (call after DB is open)
    void loadState();

private:
    TableModelRotation &m_rotModel;
    Settings &m_settings;
    [[nodiscard]] QString canonicalSongKeyForSongId(int songId) const;

    // Mark a singer as having sung this round
    void markSungThisRound(int singerId);
    // Resolve a name (possibly alias) to a rotation singer id; returns -1 if not found
    int resolveNameToId(const QString &name) const;
    // Check if all active singers have sung; if so, advance round
    void checkAndAdvanceRound();
    // Advance round: reset all sung_this_round flags, increment round counter
    void advanceRound();
};

#endif // ROTATIONFAIRNESSENGINE_H
