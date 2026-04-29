#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSettings>
#include <QtTest>

#include "models/tablemodelrotation.h"
#include "rotationfairnessengine.h"
#include "songcanonicalizer.h"
#include "settings.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>

// ---------------------------------------------------------------------------
// Helper: set up an in-memory SQLite database with the minimum schema the
// RotationFairnessEngine and TableModelRotation need.  Every test gets a
// fresh database via init() / cleanup().
// ---------------------------------------------------------------------------
class RotationFairnessTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ---- Round advancement ------------------------------------------------
    void roundAdvancesWhenAllSingersHaveSung();
    void roundDoesNotAdvanceIfSingersPending();
    void roundNumberStartsAtOne();
    void multipleRoundAdvances();

    // ---- Has-sung-this-round tracking --------------------------------------
    void hasSungThisRoundReturnsFalseInitially();
    void hasSungThisRoundReturnsTrueAfterMarking();
    void hasSungThisRoundByNameMatchesSinger();
    void hasSungThisRoundByNameReturnsFalseForUnknownName();

    // ---- Duplicate turn warning --------------------------------------------
    void duplicateTurnWarningWhenAlreadySung();
    void duplicateTurnWarningEmptyWhenNotSung();
    void duplicateTurnWarningUsesResolvedAlias();

    // ---- Round violation check ---------------------------------------------
    void roundViolationEmptyWhenFairnessOff();
    void roundViolationEmptyWhenSingerNotSung();
    void roundViolationReturnsMessageWhenSingerAlreadySung();
    void roundViolationEmptyWhenNoSingersPending();

    // ---- Once-per-night song blocking --------------------------------------
    void songNotBlockedInitially();
    void songBlockedAfterRecord();
    void canonicalKeyBlocksVariantSpelling();
    void resetNightlyTrackingClearsRecords();
    void songNotBlockedWhenFairnessOff();

    // ---- Song canonicalization ---------------------------------------------
    void canonicalKeyStripsPunctuationAndCase();
    void canonicalKeyNormalizesAmpersand();
    void canonicalKeyStripsKaraokeQualifier();
    void canonicalKeyStripsBracketedTags();
    void differentVendorSameSongSameCanonicalKey();

    // ---- Co-singer fairness -----------------------------------------------
    void coSingersMarkedAsSungWhenPrimarySings();
    void unknownCoSingerIgnored();
    void coSingerAdvanceRound();

    // ---- Name resolution / alias ------------------------------------------
    void aliasResolvesToCanonicalSinger();
    void aliasInDuplicateTurnWarning();
    void noAliasFallsBackToDirectName();

    // ---- DJ override -------------------------------------------------------
    void overrideAllowsBlockedSongForSpecificSinger();
    void overrideDoesNotAllowForOtherSinger();
    void noOverrideReturnsFalse();
    void recordSongOverrideUpdatesExistingRecord();

    // ---- Edge cases --------------------------------------------------------
    void zeroSingersAdvanceRoundNoop();
    void oneSingerAdvancesRound();
    void singerRemovedMidRound();
    void emptyRotationCurrentRoundIsOne();
    void checkSongPlayedTonightMissingSongId();
    void singersPendingWithZeroSingers();

private:
    void setupDatabase();
    void seedSingers();
    void seedSongs();
    void seedAliases();

    TableModelRotation *m_rotModel{nullptr};
    RotationFairnessEngine *m_engine{nullptr};
    QTemporaryDir *m_tmpDir{nullptr};
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RotationFairnessTest::initTestCase()
{
    // Ensure a spdlog "logger" exists — the production code uses
    // spdlog::get("logger") everywhere.  A null sink keeps test output clean.
    if (!spdlog::get("logger")) {
        auto nullSink = std::make_shared<spdlog::sinks::null_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("logger", nullSink);
        logger->set_level(spdlog::level::off);
        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
    }
}

void RotationFairnessTest::cleanupTestCase()
{
    // spdlog remains registered for the process lifetime; no teardown needed.
}

void RotationFairnessTest::init()
{
    // Isolate QSettings to a temporary directory so we don't pollute the host.
    m_tmpDir = new QTemporaryDir();
    QVERIFY(m_tmpDir->isValid());
    QString iniPath = m_tmpDir->path() + "/test-auto-kj.ini";
    // Clean any prior QSettings object so a fresh one picks up our path.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmpDir->path());
    delete QSettings{}.organizationName();  // no-op; just ensure app identity
    QCoreApplication::setOrganizationName("Auto-KJ");
    QCoreApplication::setOrganizationDomain("auto-kj.com");
    QCoreApplication::setApplicationName("Auto-KJ");

    // Remove any existing default DB connection
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);

    setupDatabase();
    seedSingers();
    seedSongs();
    seedAliases();

    m_rotModel = new TableModelRotation();
    m_rotModel->loadData();
    m_engine = new RotationFairnessEngine(*m_rotModel);

    // Enable fairness for all tests by default.
    // fairnessEnabled() checks: settings value && antiChaosPremiumEnabled().
    // antiChaosPremiumEnabled() checks: requestServerEnabled && hasCredentials && premiumAntiChaosAuthorized.
    {
        QSettings settings;
        settings.setValue("requestServerEnabled", true);
        settings.setValue("requestServerEmail", QString("test@test.com"));
        settings.sync();
    }
    {
        Settings s;
        s.setPremiumAntiChaosAuthorized(true);
        s.setFairnessEnabled(true);
        s.setFairnessEnforceRound(true);
        s.setFairnessEnforceSongNight(true);
    }
}

void RotationFairnessTest::cleanup()
{
    delete m_engine;  m_engine  = nullptr;
    delete m_rotModel; m_rotModel = nullptr;
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    delete m_tmpDir; m_tmpDir = nullptr;
}

// ---------------------------------------------------------------------------
// Database setup
// ---------------------------------------------------------------------------

void RotationFairnessTest::setupDatabase()
{
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());

    QSqlQuery q;

    // Core rotation / singer tables
    q.exec("CREATE TABLE IF NOT EXISTS rotationSingers ("
           "singerid INTEGER PRIMARY KEY AUTOINCREMENT, "
           "name COLLATE NOCASE UNIQUE, "
           "position INTEGER NOT NULL, "
           "regular LOGICAL DEFAULT(0), "
           "regularid INTEGER, "
           "addts TIMESTAMP, "
           "sung_this_round INTEGER DEFAULT 0)");

    q.exec("CREATE TABLE IF NOT EXISTS rotation_meta ("
           "key TEXT PRIMARY KEY, value TEXT)");
    q.exec("INSERT OR IGNORE INTO rotation_meta (key, value) VALUES ('current_round', '1')");

    q.exec("CREATE TABLE IF NOT EXISTS songs_played_tonight ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "song_id INTEGER NOT NULL, "
           "canonical_song_key TEXT, "
           "singer_id INTEGER, "
           "singer_name TEXT NOT NULL, "
           "played_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
           "override_allowed_singer_id INTEGER DEFAULT NULL)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_spt_canonical_song_key "
           "ON songs_played_tonight(canonical_song_key)");

    q.exec("CREATE TABLE IF NOT EXISTS dbSongs ("
           "songid INTEGER PRIMARY KEY AUTOINCREMENT, "
           "Artist COLLATE NOCASE, "
           "Title COLLATE NOCASE, "
           "DiscId COLLATE NOCASE, "
           "Duration INTEGER, "
           "path VARCHAR(700) NOT NULL UNIQUE, "
           "filename COLLATE NOCASE, "
           "searchstring TEXT)");

    q.exec("CREATE TABLE IF NOT EXISTS singerAliases ("
           "aliasid INTEGER PRIMARY KEY AUTOINCREMENT, "
           "canonical_name TEXT NOT NULL COLLATE NOCASE, "
           "alias_name TEXT NOT NULL COLLATE NOCASE UNIQUE, "
           "first_seen TIMESTAMP, "
           "last_seen TIMESTAMP)");

    // Minimal tables that Settings / RotationSinger may reference
    q.exec("CREATE TABLE IF NOT EXISTS queueSongs ("
           "qsongid INTEGER PRIMARY KEY AUTOINCREMENT, "
           "singer INT, song INTEGER NOT NULL, "
           "artist INT, title INT, discid INT, "
           "path INT, keychg INT, "
           "played LOGICAL DEFAULT(0), position INT, "
           "cosinger1 TEXT DEFAULT NULL, "
           "cosinger2 TEXT DEFAULT NULL, "
           "cosinger3 TEXT DEFAULT NULL, "
           "cosinger4 TEXT DEFAULT NULL)");

    q.exec("CREATE TABLE IF NOT EXISTS regularSingers ("
           "regsingerid INTEGER PRIMARY KEY AUTOINCREMENT, "
           "Name COLLATE NOCASE UNIQUE, ph1 INT, ph2 INT, ph3 INT)");

    q.exec("CREATE TABLE IF NOT EXISTS regularSongs ("
           "regsongid INTEGER PRIMARY KEY AUTOINCREMENT, "
           "regsingerid INTEGER NOT NULL, songid INTEGER NOT NULL, "
           "keychg INTEGER, position INTEGER)");

    q.exec("CREATE TABLE IF NOT EXISTS historySingers ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE)");

    q.exec("CREATE TABLE IF NOT EXISTS historySongs ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "historySinger INT NOT NULL, filepath TEXT NOT NULL, "
           "artist TEXT, title TEXT, songid TEXT, "
           "keychange INT DEFAULT(0), plays INT DEFAULT(0), "
           "lastplay TIMESTAMP)");

    QCOMPARE(q.lastError().type(), QSqlError::NoError);
}

void RotationFairnessTest::seedSingers()
{
    QSqlQuery q;
    // 3 singers: Alice (id 1), Bob (id 2), Carol (id 3)
    q.exec("INSERT INTO rotationSingers (singerid, name, position, regular, addts, sung_this_round) "
           "VALUES (1, 'Alice', 0, 0, '2024-01-01 20:00:00', 0)");
    q.exec("INSERT INTO rotationSingers (singerid, name, position, regular, addts, sung_this_round) "
           "VALUES (2, 'Bob', 1, 0, '2024-01-01 20:01:00', 0)");
    q.exec("INSERT INTO rotationSingers (singerid, name, position, regular, addts, sung_this_round) "
           "VALUES (3, 'Carol', 2, 0, '2024-01-01 20:02:00', 0)");
}

void RotationFairnessTest::seedSongs()
{
    QSqlQuery q;
    // songId 10: "Don't Stop Believin'" by Journey
    q.exec("INSERT INTO dbSongs (songid, Artist, Title, path, filename) "
           "VALUES (10, 'Journey', \"Don't Stop Believin'\", '/karaoke/journey_dsb.cdg', 'journey_dsb.cdg')");
    // songId 11: Same song, different vendor spelling
    q.exec("INSERT INTO dbSongs (songid, Artist, Title, path, filename) "
           "VALUES (11, 'Journey', \"Don't Stop Believin' (Karaoke Version)\", "
           "'/karaoke/journey_dsb_kv.cdg', 'journey_dsb_kv.cdg')");
    // songId 12: "Sweet Caroline" by Neil Diamond
    q.exec("INSERT INTO dbSongs (songid, Artist, Title, path, filename) "
           "VALUES (12, 'Neil Diamond', 'Sweet Caroline', '/karaoke/neil_sc.cdg', 'neil_sc.cdg')");
    // songId 13: Same song with "&" instead of "and" in artist
    q.exec("INSERT INTO dbSongs (songid, Artist, Title, path, filename) "
           "VALUES (13, 'Neil Diamond & Band', 'Sweet Caroline', "
           "'/karaoke/neil_sc2.cdg', 'neil_sc2.cdg')");
}

void RotationFairnessTest::seedAliases()
{
    QSqlQuery q;
    // "Alicia" is an alias for "Alice"
    q.exec("INSERT INTO singerAliases (canonical_name, alias_name) "
           "VALUES ('Alice', 'Alicia')");
}

// ===========================================================================
// ROUND ADVANCEMENT
// ===========================================================================

void RotationFairnessTest::roundNumberStartsAtOne()
{
    QCOMPARE(m_engine->currentRound(), 1);
}

void RotationFairnessTest::roundAdvancesWhenAllSingersHaveSung()
{
    QCOMPARE(m_engine->currentRound(), 1);

    // Mark all 3 singers as having sung
    m_engine->onSongPlayed(1, {}); // Alice
    QCOMPARE(m_engine->currentRound(), 1); // Not yet — Bob & Carol pending
    m_engine->onSongPlayed(2, {}); // Bob
    QCOMPARE(m_engine->currentRound(), 1); // Carol still pending
    m_engine->onSongPlayed(3, {}); // Carol — now all have sung

    QCOMPARE(m_engine->currentRound(), 2);
    // All singers should have sung_this_round reset
    QCOMPARE(m_engine->hasSungThisRound(1), false);
    QCOMPARE(m_engine->hasSungThisRound(2), false);
    QCOMPARE(m_engine->hasSungThisRound(3), false);
}

void RotationFairnessTest::roundDoesNotAdvanceIfSingersPending()
{
    m_engine->onSongPlayed(1, {}); // Only Alice
    QCOMPARE(m_engine->currentRound(), 1);
    QCOMPARE(m_engine->singersPendingThisRound(), 2);
}

void RotationFairnessTest::multipleRoundAdvances()
{
    // Round 1: all sing
    m_engine->onSongPlayed(1, {});
    m_engine->onSongPlayed(2, {});
    m_engine->onSongPlayed(3, {});
    QCOMPARE(m_engine->currentRound(), 2);

    // Round 2: all sing again
    m_engine->onSongPlayed(1, {});
    m_engine->onSongPlayed(2, {});
    m_engine->onSongPlayed(3, {});
    QCOMPARE(m_engine->currentRound(), 3);

    QCOMPARE(m_engine->hasSungThisRound(1), false);
    QCOMPARE(m_engine->hasSungThisRound(2), false);
    QCOMPARE(m_engine->hasSungThisRound(3), false);
}

// ===========================================================================
// HAS-SUNG-THIS-ROUND TRACKING
// ===========================================================================

void RotationFairnessTest::hasSungThisRoundReturnsFalseInitially()
{
    QCOMPARE(m_engine->hasSungThisRound(1), false);
    QCOMPARE(m_engine->hasSungThisRound(2), false);
    QCOMPARE(m_engine->hasSungThisRound(3), false);
}

void RotationFairnessTest::hasSungThisRoundReturnsTrueAfterMarking()
{
    m_engine->onSongPlayed(1, {});
    QCOMPARE(m_engine->hasSungThisRound(1), true);
    QCOMPARE(m_engine->hasSungThisRound(2), false); // unchanged
}

void RotationFairnessTest::hasSungThisRoundByNameMatchesSinger()
{
    m_engine->onSongPlayed(1, {});
    QCOMPARE(m_engine->hasSungThisRoundByName("Alice"), true);
    QCOMPARE(m_engine->hasSungThisRoundByName("Bob"), false);
}

void RotationFairnessTest::hasSungThisRoundByNameReturnsFalseForUnknownName()
{
    QCOMPARE(m_engine->hasSungThisRoundByName("Nonexistent"), false);
}

// ===========================================================================
// DUPLICATE TURN WARNING
// ===========================================================================

void RotationFairnessTest::duplicateTurnWarningWhenAlreadySung()
{
    m_engine->onSongPlayed(1, {});
    QString warning = m_engine->duplicateTurnWarning("Alice");
    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains("Alice"));
    QVERIFY(warning.contains("already sung"));
}

void RotationFairnessTest::duplicateTurnWarningEmptyWhenNotSung()
{
    QString warning = m_engine->duplicateTurnWarning("Alice");
    QVERIFY(warning.isEmpty());
}

void RotationFairnessTest::duplicateTurnWarningUsesResolvedAlias()
{
    m_engine->onSongPlayed(1, {}); // Alice sang
    // "Alicia" is an alias for "Alice"
    QString warning = m_engine->duplicateTurnWarning("Alicia");
    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains("Alicia"));
}

// ===========================================================================
// ROUND VIOLATION CHECK
// ===========================================================================

void RotationFairnessTest::roundViolationEmptyWhenFairnessOff()
{
    // Turn fairness off
    {
        Settings s;
        s.setFairnessEnabled(false);
    }

    m_engine->onSongPlayed(1, {});
    // Even though Alice sang, no violation when fairness is off
    QString violation = m_engine->checkRoundViolation(1);
    QVERIFY(violation.isEmpty());
}

void RotationFairnessTest::roundViolationEmptyWhenSingerNotSung()
{
    // Bob hasn't sung yet
    QString violation = m_engine->checkRoundViolation(2);
    QVERIFY(violation.isEmpty());
}

void RotationFairnessTest::roundViolationReturnsMessageWhenSingerAlreadySung()
{
    m_engine->onSongPlayed(1, {}); // Alice sang
    QString violation = m_engine->checkRoundViolation(1);
    QVERIFY(!violation.isEmpty());
    QVERIFY(violation.contains("Alice"));
    QVERIFY(violation.contains("2 singer(s)")); // Bob and Carol pending
}

void RotationFairnessTest::roundViolationEmptyWhenNoSingersPending()
{
    // All singers sing → round advances → no one has sung in new round
    m_engine->onSongPlayed(1, {});
    m_engine->onSongPlayed(2, {});
    m_engine->onSongPlayed(3, {});
    QCOMPARE(m_engine->currentRound(), 2);

    // After round advance, sung_this_round is reset, so no violation
    QCOMPARE(m_engine->hasSungThisRound(1), false);
    QString violation = m_engine->checkRoundViolation(1);
    QVERIFY(violation.isEmpty());
}

// ===========================================================================
// ONCE-PER-NIGHT SONG BLOCKING
// ===========================================================================

void RotationFairnessTest::songNotBlockedInitially()
{
    auto result = m_engine->checkSongPlayedTonight(10);
    QCOMPARE(result.blocked, false);
    QVERIFY(result.singerName.isEmpty());
}

void RotationFairnessTest::songBlockedAfterRecord()
{
    m_engine->recordSongPlayed(10, 1, "Alice");
    auto result = m_engine->checkSongPlayedTonight(10);
    QCOMPARE(result.blocked, true);
    QCOMPARE(result.singerName, QString("Alice"));
}

void RotationFairnessTest::canonicalKeyBlocksVariantSpelling()
{
    // Record songId 10 (Journey - Don't Stop Believin')
    m_engine->recordSongPlayed(10, 1, "Alice");
    // SongId 11 is the same song but "(Karaoke Version)" — canonical key should match
    auto result = m_engine->checkSongPlayedTonight(11);
    QCOMPARE(result.blocked, true);
}

void RotationFairnessTest::resetNightlyTrackingClearsRecords()
{
    m_engine->recordSongPlayed(10, 1, "Alice");
    m_engine->resetNightlyTracking();
    auto result = m_engine->checkSongPlayedTonight(10);
    QCOMPARE(result.blocked, false);
}

void RotationFairnessTest::songNotBlockedWhenFairnessOff()
{
    {
        Settings s;
        s.setFairnessEnabled(false);
    }

    m_engine->recordSongPlayed(10, 1, "Alice");
    auto result = m_engine->checkSongPlayedTonight(10);
    QCOMPARE(result.blocked, false);
}

// ===========================================================================
// SONG CANONICALIZATION (unit-level: directly test okj::canonicalSongKey)
// ===========================================================================

void RotationFairnessTest::canonicalKeyStripsPunctuationAndCase()
{
    QString key1 = okj::canonicalSongKey("Journey", "Don't Stop Believin'");
    QString key2 = okj::canonicalSongKey("journey", "dont stop believin");
    QCOMPARE(key1, key2);
}

void RotationFairnessTest::canonicalKeyNormalizesAmpersand()
{
    QString key1 = okj::canonicalSongKey("Neil Diamond and Band", "Sweet Caroline");
    QString key2 = okj::canonicalSongKey("Neil Diamond & Band", "Sweet Caroline");
    QCOMPARE(key1, key2);
}

void RotationFairnessTest::canonicalKeyStripsKaraokeQualifier()
{
    QString key1 = okj::canonicalSongKey("Journey", "Don't Stop Believin'");
    QString key2 = okj::canonicalSongKey("Journey", "Don't Stop Believin' Karaoke");
    QCOMPARE(key1, key2);
}

void RotationFairnessTest::canonicalKeyStripsBracketedTags()
{
    QString key1 = okj::canonicalSongKey("Journey", "Don't Stop Believin'");
    QString key2 = okj::canonicalSongKey("Journey", "Don't Stop Believin' [Karaoke Version]");
    QCOMPARE(key1, key2);
}

void RotationFairnessTest::differentVendorSameSongSameCanonicalKey()
{
    // Via the engine: record songId 10, check songId 11
    m_engine->recordSongPlayed(10, 1, "Alice");
    auto result = m_engine->checkSongPlayedTonight(11);
    QCOMPARE(result.blocked, true);

    // Also verify the canonical keys generated by the engine match
    // (both songIds should map to the same canonical key via dbsongs)
    QSqlQuery q;
    q.prepare("SELECT artist, title FROM dbsongs WHERE songid = 10");
    QVERIFY(q.exec() && q.next());
    QString key10 = okj::canonicalSongKey(q.value(0).toString(), q.value(1).toString());

    q.prepare("SELECT artist, title FROM dbsongs WHERE songid = 11");
    QVERIFY(q.exec() && q.next());
    QString key11 = okj::canonicalSongKey(q.value(0).toString(), q.value(1).toString());

    QCOMPARE(key10, key11);
}

// ===========================================================================
// CO-SINGER FAIRNESS
// ===========================================================================

void RotationFairnessTest::coSingersMarkedAsSungWhenPrimarySings()
{
    // Alice sings with Bob as co-singer
    m_engine->onSongPlayed(1, QStringList{"Bob"});
    QCOMPARE(m_engine->hasSungThisRound(1), true); // Alice
    QCOMPARE(m_engine->hasSungThisRound(2), true); // Bob (co-singer)
    QCOMPARE(m_engine->hasSungThisRound(3), false); // Carol
}

void RotationFairnessTest::unknownCoSingerIgnored()
{
    // Alice sings with "Dave" (not in rotation) as co-singer
    m_engine->onSongPlayed(1, QStringList{"Dave"});
    QCOMPARE(m_engine->hasSungThisRound(1), true); // Alice
    // No crash, no effect on Dave (doesn't exist)
}

void RotationFairnessTest::coSingerAdvanceRound()
{
    // Alice + Bob sing, Carol still pending → no advance
    m_engine->onSongPlayed(1, QStringList{"Bob"});
    QCOMPARE(m_engine->currentRound(), 1);

    // Carol sings → all have sung → advance
    m_engine->onSongPlayed(3, {});
    QCOMPARE(m_engine->currentRound(), 2);
    QCOMPARE(m_engine->hasSungThisRound(1), false);
    QCOMPARE(m_engine->hasSungThisRound(2), false);
    QCOMPARE(m_engine->hasSungThisRound(3), false);
}

// ===========================================================================
// NAME RESOLUTION / ALIAS
// ===========================================================================

void RotationFairnessTest::aliasResolvesToCanonicalSinger()
{
    // "Alicia" is an alias for "Alice" (singerId 1)
    QCOMPARE(m_engine->hasSungThisRoundByName("Alicia"), false);
    m_engine->onSongPlayed(1, {});
    QCOMPARE(m_engine->hasSungThisRoundByName("Alicia"), true);
}

void RotationFairnessTest::aliasInDuplicateTurnWarning()
{
    m_engine->onSongPlayed(1, {});
    QString warning = m_engine->duplicateTurnWarning("Alicia");
    QVERIFY(!warning.isEmpty());
}

void RotationFairnessTest::noAliasFallsBackToDirectName()
{
    // "Bob" has no alias, but direct name match works
    QCOMPARE(m_engine->hasSungThisRoundByName("Bob"), false);
    m_engine->onSongPlayed(2, {});
    QCOMPARE(m_engine->hasSungThisRoundByName("Bob"), true);
}

// ===========================================================================
// DJ OVERRIDE
// ===========================================================================

void RotationFairnessTest::overrideAllowsBlockedSongForSpecificSinger()
{
    m_engine->recordSongPlayed(10, 1, "Alice"); // Blocked by Alice
    QVERIFY(m_engine->checkSongPlayedTonight(10).blocked);

    // Bob gets an override for this song
    m_engine->recordSongOverride(10, 2);
    QVERIFY(m_engine->hasSongOverrideForSinger(10, 2));
}

void RotationFairnessTest::overrideDoesNotAllowForOtherSinger()
{
    m_engine->recordSongPlayed(10, 1, "Alice");
    m_engine->recordSongOverride(10, 2); // Override for Bob only
    QVERIFY(!m_engine->hasSongOverrideForSinger(10, 3)); // Carol — no override
}

void RotationFairnessTest::noOverrideReturnsFalse()
{
    m_engine->recordSongPlayed(10, 1, "Alice");
    QVERIFY(!m_engine->hasSongOverrideForSinger(10, 2)); // No override recorded
}

void RotationFairnessTest::recordSongOverrideUpdatesExistingRecord()
{
    m_engine->recordSongPlayed(10, 1, "Alice");
    // Initially no override
    QVERIFY(!m_engine->hasSongOverrideForSinger(10, 2));

    // Record override
    m_engine->recordSongOverride(10, 2);
    QVERIFY(m_engine->hasSongOverrideForSinger(10, 2));

    // Override should also work via canonical key match (songId 11 = same canonical key)
    // After recording override for songId 10, the songs_played_tonight row has
    // canonical_song_key set, so checking via songId 11 should also find it
    QVERIFY(m_engine->hasSongOverrideForSinger(11, 2));
}

// ===========================================================================
// EDGE CASES
// ===========================================================================

void RotationFairnessTest::zeroSingersAdvanceRoundNoop()
{
    // Clear all singers from DB
    QSqlQuery q;
    q.exec("DELETE FROM rotationsingers");
    m_rotModel->loadData();

    // Current round should still be valid
    QCOMPARE(m_engine->currentRound(), 1);
    QCOMPARE(m_engine->singersPendingThisRound(), 0);

    // With 0 singers, "all have sung" (vacuously true), so onSongPlayed
    // on a non-existent singer shouldn't crash; but the round auto-advance
    // should happen since 0 pending = 0
    QCOMPARE(m_engine->singersPendingThisRound(), 0);
}

void RotationFairnessTest::oneSingerAdvancesRound()
{
    // Remove Bob and Carol, keep only Alice
    QSqlQuery q;
    q.exec("DELETE FROM rotationsingers WHERE singerid IN (2, 3)");
    m_rotModel->loadData();

    QCOMPARE(m_engine->currentRound(), 1);
    m_engine->onSongPlayed(1, {}); // Alice sings → 0 pending → round advances
    QCOMPARE(m_engine->currentRound(), 2);
    QCOMPARE(m_engine->hasSungThisRound(1), false); // Reset after advance
}

void RotationFairnessTest::singerRemovedMidRound()
{
    // Alice and Bob sing, then Carol is removed → should auto-advance
    m_engine->onSongPlayed(1, {}); // Alice
    m_engine->onSongPlayed(2, {}); // Bob
    QCOMPARE(m_engine->currentRound(), 1);
    QCOMPARE(m_engine->singersPendingThisRound(), 1); // Carol

    // Remove Carol from DB
    QSqlQuery q;
    q.exec("DELETE FROM rotationsingers WHERE singerid = 3");
    m_rotModel->loadData();

    // Now 0 pending → checkAndAdvanceRound happens on next onSongPlayed
    QCOMPARE(m_engine->singersPendingThisRound(), 0);
}

void RotationFairnessTest::emptyRotationCurrentRoundIsOne()
{
    QSqlQuery q;
    q.exec("DELETE FROM rotationsingers");
    m_rotModel->loadData();

    QCOMPARE(m_engine->currentRound(), 1);
}

void RotationFairnessTest::checkSongPlayedTonightMissingSongId()
{
    // SongId 999 doesn't exist in dbsongs → canonical key will be empty
    // Should still check by raw song_id
    m_engine->recordSongPlayed(999, 1, "Alice");
    auto result = m_engine->checkSongPlayedTonight(999);
    QCOMPARE(result.blocked, true);
}

void RotationFairnessTest::singersPendingWithZeroSingers()
{
    QSqlQuery q;
    q.exec("DELETE FROM rotationsingers");
    m_rotModel->loadData();

    QCOMPARE(m_engine->singersPendingThisRound(), 0);
}

QTEST_MAIN(RotationFairnessTest)
#include "test_rotation_fairness.moc"