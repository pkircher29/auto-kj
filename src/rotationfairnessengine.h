#ifndef ROTATIONFAIRNESSENGINE_H
#define ROTATIONFAIRNESSENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "models/tablemodelrotation.h"
#include "settings.h"

class RotationFairnessEngine : public QObject {
    Q_OBJECT
public:
    // Rotation style constants
    enum RotationStyle {
        Classic = 0,
        Double = 1,
        Flex = 2
    };

    explicit RotationFairnessEngine(TableModelRotation &rotModel, Settings &settings, QObject *parent = nullptr);

    struct SongPlayedTonightResult {
        bool blocked;
        QString singerName;
    };

    // Set the rotation style for the current gig
    void setRotationStyle(int style);
    int rotationStyle() const;

    // Per-singer rotation style overrides (Flex mode)
    void setSingerRotationStyle(int singerId, RotationStyle style);
    RotationStyle singerRotationStyle(int singerId) const;

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

    // Get the effective rotation style for a singer (handles Flex per-singer overrides)
    int effectiveStyleForSinger(int singerId) const;

    // Check if a singer has completed their turn (for Double/Flex modes)
    bool isSingerTurnComplete(int singerId) const;

    // Reset turn counters (called when a singer completes their turn)
    void resetSingerTurnCounter(int singerId);

private:
    TableModelRotation &m_rotModel;
    Settings &m_settings;

    // Rotation style for the current gig
    int m_rotationStyle{Classic};

    // Per-singer rotation style overrides (Flex mode only)
    QMap<int, RotationStyle> m_singerRotationStyles;

    // Per-singer songs-sung-this-turn counter (for Double/Flex)
    QMap<int, int> m_singerSongsThisTurn;

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
