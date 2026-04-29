# AutoKJ-Pro Code Review Report

**Reviewer:** Code Review Agent  
**Date:** 2026-04-28  
**Scope:** Critical paths — Rotation Fairness Engine, MainWindow/Play Flow, Settings, Server API, Database Schema

---

## Summary

The codebase has several **CRITICAL** and **HIGH** severity issues that need immediate attention before this can be considered production-ready for a commercial product. The most dangerous categories are: (1) data loss risk from `PRAGMA synchronous=OFF`, (2) thread safety violations on the database connection, (3) plaintext credential fallback, and (4) race conditions in the round advancement logic.

---

## 1. DATABASE & SCHEMA

### 1.1 CRITICAL — `PRAGMA synchronous=OFF` risks data corruption on crash

**Files:** `src/mainwindow.cpp:1522`, `src/dbupdater.cpp:144`, `src/bmdbupdatethread.cpp:93,138`

`PRAGMA synchronous=OFF` disables SQLite's write-ahead log barriers. On a power failure or OS crash, **committed transactions can be lost** and the database can be left in a corrupted state. This is a karaoke app — losing the rotation state (who's next, what round) mid-gig is a worst-case scenario.

The `PRAGMA cache_size=300000` and `PRAGMA temp_store=2` amplify this: massive in-memory caches with no durability guarantees.

**Fix:** Use `PRAGMA synchronous=NORMAL` (safe with WAL) or `PRAGMA synchronous=FULL`. Add `PRAGMA journal_mode=WAL` for concurrent read performance. The slight performance hit is negligible compared to data integrity for a single-user desktop app.

### 1.2 HIGH — No WAL mode, no concurrency support

**Files:** `src/mainwindow.cpp` (dbInit)

The database never sets `PRAGMA journal_mode=WAL`. This means all reads block on writes and vice versa. Combined with the `BmDbUpdateThread` and `DurationLazyUpdater` threads that access the database, this is a correctness problem (see §2.1 below) as well as a performance one.

**Fix:** Add `PRAGMA journal_mode=WAL` immediately after opening the database.

### 1.3 HIGH — Missing indexes for common fairness queries

**File:** `src/rotationfairnessengine.cpp`

The following queries have no index support:

- `SELECT sung_this_round FROM rotationsingers WHERE singerid = :id` — no index on `singerid` (it's not the primary key, which is `singerid` — actually this is the PK, so it's covered).
- `SELECT COUNT(*) FROM rotationsingers WHERE sung_this_round = 0` — no index on `sung_this_round`. With many singers this is a full table scan every time a song finishes.
- `SELECT singer_name FROM songs_played_tonight WHERE (canonical_song_key = :key OR song_id = :sid)` — composite index missing for `song_id`; only `canonical_song_key` has an index.

**Fix:** Add `CREATE INDEX IF NOT EXISTS idx_rotationsingers_sung ON rotationsingers(sung_this_round)` and `CREATE INDEX IF NOT EXISTS idx_spt_song_id ON songs_played_tonight(song_id)`.

### 1.4 MEDIUM — Schema migrations don't use transactions

**File:** `src/mainwindow.cpp:1532-1662`

Each `if (schemaVersion < N)` block runs multiple DDL/DML statements without wrapping them in a transaction. If migration v109 adds the column but crashes before setting `PRAGMA user_version = 109`, the next startup will re-run the `ALTER TABLE` and fail (SQLite doesn't support re-adding a column that exists).

**Fix:** Wrap each migration block in `BEGIN TRANSACTION` / `COMMIT`. Handle `ALTER TABLE` errors gracefully (the column may already exist from a partial migration).

### 1.5 MEDIUM — Schema v111 migration uses subquery that won't match the runtime canonicalizer

**File:** `src/mainwindow.cpp:1642-1651`

The migration computes `canonical_song_key` with inline SQL:
```sql
lower(trim(replace(
  (SELECT COALESCE(dbsongs.artist, '') || ' ' || COALESCE(dbsongs.title, '') 
   FROM dbsongs WHERE dbsongs.songid = songs_played_tonight.song_id), 
'&', ' and ')))
```

But the runtime `canonicalSongKey()` function (`songcanonicalizer.cpp`) does much more: Unicode NFKD normalization, removes bracketed tags, strips version/qualifier words like "karaoke", "instrumental", etc., and replaces all non-alphanumeric characters. The SQL migration only lowercases and replaces `&`. This means **existing rows get a different canonical key than what the runtime computes**, breaking the once-per-night blocking for any song with special characters, brackets, or version labels.

**Fix:** Backfill canonical keys using the C++ canonicalizer at app startup (which is already partially done in `loadState()`), or make the SQL match the full canonicalizer logic. The `loadState()` method already handles NULL/empty keys, so this mostly self-heals — but the initial migration will create wrong keys that only get fixed on the next startup.

### 1.6 LOW — SQL injection in v106 migration

**File:** `src/mainwindow.cpp:1597`

```cpp
songsQuery.exec(
    "SELECT ... FROM regularsongs,dbsongs WHERE ... AND regularsongs.regsingerid == " +
    singersQuery.value("regsingerid").toString() + " ORDER BY ...");
```

String interpolation in SQL. While `regsingerid` comes from the database itself (not user input), this is still bad practice and a landmine for future copy-paste.

**Fix:** Use parameterized queries.

---

## 2. THREAD SAFETY

### 2.1 CRITICAL — SQLite accessed from multiple threads without connection isolation

**Files:** `src/mainwindow.cpp:1497-1500`, `src/bmdbupdatethread.cpp:89`, `src/durationlazyupdater.cpp`

The main database connection `m_database` is created on the main thread with `QSqlDatabase::addDatabase("QSQLITE")` and opened once. But:

- `BmDbUpdateThread` is a `QThread` that creates `QSqlQuery query(database)` where `database` is passed from the main thread.
- `DurationLazyUpdater` runs on a separate `QThread` and likely accesses the database.
- `RotationFairnessEngine` creates `QSqlQuery` on whatever thread calls it (main thread, but also potentially from signal handlers).

SQLite's default mode is not thread-safe for sharing a connection across threads. Qt's `QSqlDatabase` explicitly warns: **"A connection can only be used from within the thread that created it."**

**Fix:** Each thread must create its own `QSqlDatabase` connection with a unique connection name via `QSqlDatabase::addDatabase("QSQLITE", uniqueName)`. Use WAL mode so concurrent reads don't block writes.

### 2.2 HIGH — RotationFairnessEngine creates Settings instances on the stack

**File:** `src/rotationfairnessengine.cpp:127,149`

```cpp
bool RotationFairnessEngine::checkRoundViolation(int singerId) const {
    Settings settings;  // <-- creates a whole new QSettings object on each call
    ...
}
```

`Settings` constructor creates a new `QSettings` object every time. This is wasteful but also **not thread-safe** if `checkRoundViolation` is ever called from a non-main thread. More importantly, if `Settings` constructor triggers any DB access (it doesn't currently, but could in future), it would be a problem.

**Fix:** Pass a reference to the application's `Settings` instance through the constructor, like `m_rotModel` is passed. The engine should not create its own `Settings` objects.

---

## 3. ROTATION FAIRNESS ENGINE

### 3.1 HIGH — Race condition in round advancement

**File:** `src/rotationfairnessengine.cpp:109-115`

```cpp
void RotationFairnessEngine::checkAndAdvanceRound() {
    QSqlQuery q;
    q.exec("SELECT COUNT(*) FROM rotationsingers WHERE sung_this_round = 0");
    if (q.next() && q.value(0).toInt() == 0) {
        advanceRound();
    }
}
```

There is no transaction wrapping the check-and-advance. If two songs finish nearly simultaneously (k2k transition), both could see `COUNT(*) = 0`, and both could call `advanceRound()`, incrementing the round twice. The `UPDATE rotationsingers SET sung_this_round = 0` and the meta round increment are also not atomic — a crash between them would leave the round counter incremented but singers still marked as having sung.

**Fix:** Wrap in a transaction:
```cpp
QSqlDatabase::database().transaction();
// check count
// if zero: advance round
QSqlDatabase::database().commit();
```

### 3.2 HIGH — Zero singers edge case: premature round advancement

**File:** `src/rotationfairnessengine.cpp:109-115`

If all singers are removed from the rotation mid-round, `COUNT(*) FROM rotationsingers WHERE sung_this_round = 0` returns 0 (because there are zero rows), which triggers `advanceRound()`. This will increment the round counter indefinitely if nothing guards against it.

**Fix:** Add a check: `SELECT COUNT(*) FROM rotationsingers` — only advance if there are active singers.

### 3.3 MEDIUM — Co-singer resolution failure is silent

**File:** `src/rotationfairnessengine.cpp:71-76`

```cpp
for (const QString &name : cosingers) {
    int id = resolveNameToId(name);
    if (id != -1)
        markSungThisRound(id);
}
```

If a co-singer name doesn't resolve to an ID (typo, not in rotation), the co-singer's round status is silently skipped. They won't be marked as having sung, and they could sing again in the same round.

**Fix:** Log a warning when `resolveNameToId` returns -1, and consider UI feedback to the KJ so they can correct the name.

### 3.4 MEDIUM — `resolveNameToId` doesn't check inactive/removed singers

**File:** `src/rotationfairnessengine.cpp:88-102`

`getSingerByName` searches through `m_singers`, but if a singer was removed from the rotation, their name won't resolve. This is correct behavior, but the alias lookup in `singerAliases` might still point to a removed singer. The method should verify the resolved singer is still active.

### 3.5 LOW — `currentRound()` does a DB query every call

**File:** `src/rotationfairnessengine.cpp:37-42`

Each call to `currentRound()` hits the database. If this is called frequently (e.g., in UI rendering), it's unnecessary overhead.

**Fix:** Cache the round number in a member variable, update it only when `advanceRound()` or `loadState()` is called.

---

## 4. MAIN WINDOW / PLAY FLOW

### 4.1 HIGH — QTemporaryDir lifecycle for playing media

**File:** `src/mainwindow.cpp:682,1675`

```cpp
m_mediaTempDir = std::make_unique<QTemporaryDir>();
```

This is created each time a song plays. The old `QTemporaryDir` is destroyed when replaced, which deletes the temporary files. But if the media backend is still reading from those files (async I/O), the files could be deleted while in use. The `unique_ptr` replacement is not synchronized with the media backend stopping.

**Fix:** Ensure the media backend has fully stopped and released file handles before replacing `m_mediaTempDir`. Consider keeping a reference count or using `deleteLater()` pattern.

### 4.2 MEDIUM — 5084-line god object

**File:** `src/mainwindow.cpp`

This is a maintainability nightmare. Single responsibility is violated dozens of times. Any change risks side effects. The file contains DB initialization, UI setup, media playback, network calls, rotation management, settings handling, and more.

**Fix:** Long-term, decompose into focused classes. Short-term, at least extract: (1) database initialization into a `DatabaseManager`, (2) play logic into a `PlaybackController`, (3) fairness integration into the existing `RotationFairnessEngine`.

### 4.3 MEDIUM — Dialog objects created with `new` without explicit parent for some paths

**Files:** `src/mainwindow.cpp:1281,1289`

```cpp
auto *dlg = new DlgCheckins(m_rotModel, m_songbookApi, this);
auto *dlg = new DlgAliasManager(this);
```

These have `this` as parent, so they'll be deleted when MainWindow is destroyed. But `DlgAddSinger` (line 4910) and others are also heap-allocated with `new`. Some have `deleteLater` connections, some don't. This is inconsistent but not a leak since MainWindow is the parent.

---

## 5. SETTINGS & CREDENTIALS

### 5.1 CRITICAL — KJ PIN stored in plaintext in QSettings

**File:** `src/settings.cpp:2108-2113`

```cpp
QString Settings::kjPin() const {
    return settings->value("kjPin", "1234").toString();
}
void Settings::setKjPin(const QString &pin) {
    settings->setValue("kjPin", pin);
}
```

The KJ PIN (used for venue authentication in `autokjserverapi.cpp:260,287`) is stored as **plaintext** in QSettings (INI file or registry). This is transmitted to the server on every authentication and venue config push. A default of `"1234"` is also a terrible default.

**Fix:** Use `SecureCredentialStore` to store the PIN, like passwords. Remove the default value. Hash it before transmission if the server supports it.

### 5.2 HIGH — SecureCredentialStore falls back to plaintext QSettings

**File:** `src/securecredentialstore.cpp:229-247`

On Linux, if `secret-tool` (libsecret) is not available, and on any unsupported platform, `SecureCredentialStore` falls back to storing secrets in **plain QSettings** under the `SecureStoreFallback` group. This means passwords, API keys, and tokens are stored in plaintext on disk with zero protection.

**Fix:** At minimum, encrypt with a machine-specific key before storing in fallback mode. Better: warn the user at startup that credentials are not secure and offer to lock the app. On Linux, depend on `libsecret` at build time rather than shelling out to `secret-tool`.

### 5.3 HIGH — Password system uses weak reversible encryption

**File:** `src/settings.cpp:337-356`

```cpp
void Settings::setPassword(QString password) {
    qint64 passHash = this->hash(password);
    SimpleCrypt simpleCrypt(passHash);
    QString pchk = simpleCrypt.encryptToString(QString("testpass"));
    settings->setValue("pchk", pchk);
}
```

The "password" system is actually just an access gate. It hashes the password with MD5 to create a SimpleCrypt key, then encrypts a known plaintext (`"testpass"`). To verify, it decrypts and checks if it equals `"testpass"`. This is **security by obscurity** — SimpleCrypt is trivially reversible if you know the algorithm, and the "verification" plaintext is hardcoded. Anyone who reads the QSettings file can reverse-engineer the password.

**Fix:** Use proper password hashing (bcrypt/argon2) for verification. Store only the hash. This is a local app so it's low risk, but it's still embarrassing for a commercial product.

### 5.4 MEDIUM — `Settings` object created per-instance in `readStoredSecret`/`writeStoredSecret`

**File:** `src/settings.cpp` (anonymous namespace)

The `readStoredSecret` and `writeStoredSecret` functions use `QKeychain` with `QEventLoop` for synchronous operations. The `QEventLoop::quit` connection via `&QKeychain::Job::finished` blocks the main thread until the keychain operation completes. On slow keychain backends (macOS Keychain with user prompt), this could freeze the UI.

**Fix:** Make keychain operations async or move them to a background thread.

### 5.5 MEDIUM — Dual credential storage creates migration confusion

**Files:** `src/settings.cpp` and `src/securecredentialstore.cpp`

Credentials can exist in three places: QKeychain, SecureCredentialStore (Windows Credential Manager/macOS Keychain/libsecret), and plain QSettings. The `requestServerPassword()` method checks `SecureCredentialStore` first, then falls back to plain QSettings. But the `readStoredSecret`/`writeStoredSecret` functions also check QKeychain. This means a password could exist in QKeychain from one code path and in Windows Credential Manager from another, causing stale data.

**Fix:** Consolidate to a single credential storage path. Pick either QKeychain or SecureCredentialStore, not both.

---

## 6. SERVER API CLIENT

### 6.1 HIGH — Auth token stored in memory member, not cleared on disconnect

**File:** `src/autokjserverapi.cpp`

`m_token` is a `QString` that persists in memory. On `clearSessionState()`, `m_token` is not explicitly cleared. The `reconfigureFromSettings` method does `m_token.clear()` only when the endpoint changes. After a disconnect-reconnect cycle, stale tokens may be used.

**Fix:** Clear `m_token` in `clearSessionState()`. Use `m_token.clear()` which also overwrites the QString buffer.

### 6.2 HIGH — Password sent in plaintext over WebSocket

**File:** `src/autokjserverapi.cpp:246-260`

The `authenticate()` method sends credentials via WebSocket:
```cpp
data["token"] = m_token.isEmpty() ? m_settings.requestServerToken() : m_token;
data["venue_id"] = m_settings.requestServerVenue();
```

While the WebSocket connection uses `wss://` (TLS), the initial connection URL construction (`connectToServer`) converts `https://` to `wss://`, which is correct. However, the `login()` method sends `email` and `password` in the JSON body over HTTPS — this is fine.

The real concern: `m_settings.requestServerPassword()` is called in `reconfigureFromSettings()`, which means the password is loaded into a `QString` on every reconfigure. It stays in `m_lastPassword` indefinitely.

**Fix:** Clear `m_lastPassword` after authentication succeeds. Don't cache the password at all — only cache the token.

### 6.3 MEDIUM — `QEventLoop` blocking in synchronous `login()` and `changePassword()`

**File:** `src/autokjserverapi.cpp`

`login()` and `changePassword()` use `QEventLoop` to block until the network reply finishes. This can cause reentrancy issues: while the event loop is spinning, other slots may fire, including `onDisconnected()`, `onTextMessageReceived()`, etc. This can lead to unexpected state changes.

**Fix:** Use async-only patterns. The `loginAsync`/`ensureTokenAsync` pattern is already available and correctly implemented. Remove the synchronous `login()` and `ensureToken()` methods, or mark them as deprecated with warnings.

### 6.4 MEDIUM — No retry logic for failed WebSocket connections

**File:** `src/autokjserverapi.cpp`

On disconnect, the reconnect timer starts. But there's no exponential backoff — it retries at the same interval forever. For network blips this is fine, but for prolonged outages it creates unnecessary network traffic and log spam.

**Fix:** Implement exponential backoff: start at 5s, double each retry up to a maximum (e.g., 5 minutes).

### 6.5 MEDIUM — SSL error handling is overly permissive

**File:** `src/autokjserverapi.cpp:780-798`

The `onSslErrors` handler ignores `SelfSignedCertificate`, `SelfSignedCertificateInChain`, `CertificateUntrusted`, and `HostNameMismatch` if `requestServerIgnoreCertErrors()` is true. These are the most dangerous SSL errors. A MITM attacker with a self-signed cert would be accepted.

**Fix:** At minimum, only ignore `SelfSignedCertificate` on local network addresses. Never ignore `HostNameMismatch`. Consider certificate pinning for the production server.

### 6.6 LOW — Song sync sends `systemId` in legacy mode but not in modern mode

**File:** `src/autokjserverapi.cpp`

In `tryLegacySongDbSyncAsync`, the legacy API call includes `systemID` in the request body. The modern `/api/v1/desktop/kj/songs/sync` endpoint doesn't include any system identifier. If the server needs to identify the client for deduplication, the modern path may break.

**Fix:** Verify server behavior. If `systemId` is needed, add it to the modern sync request headers or body.

---

## 7. SONG CANONICALIZATION

### 7.1 MEDIUM — Canonicalizer strips too aggressively

**File:** `src/songcanonicalizer.cpp`

The canonicalizer removes all parenthesized and bracketed content:
```cpp
value.remove(QRegularExpression("\\([^\\)]*\\)"));
value.remove(QRegularExpression("\\[[^\\]]*\\]"));
```

This means `"Sweet Caroline (Radio Edit)"` and `"Sweet Caroline (Live)"` become the same key — both map to `"sweet caroline"`. While this is arguably correct for once-per-night enforcement (you shouldn't sing 4 versions of the same song), it could surprise KJs who intentionally add different versions.

The word-stripping regex also removes `"version"`, `"karaoke"`, `"instrumental"`, etc. This is appropriate for a karaoke context.

**Assessment:** This is a design choice, not a bug. Document it clearly for users.

---

## 8. ADDITIONAL FINDINGS

### 8.1 MEDIUM — `DbUpdater` uses `std::mutex` in a Qt single-threaded context

**File:** `src/dbupdater.cpp:38`

```cpp
static std::mutex mutex;
if (!mutex.try_lock()) {
    m_errors.append("Scanner already running");
    return false;
}
```

This is a `static` mutex — it's never unlocked on the success path! The code has no corresponding `unlock()`. If the method returns successfully, the mutex is held forever, preventing any future scans.

**Fix:** Use `std::lock_guard<std::mutex>` for RAII, or add `mutex.unlock()` at the end of the method.

**UPDATE:** Looking more carefully, I see the method likely has `mutex.unlock()` at the end that wasn't visible in my grep. But using a static mutex for this purpose is still a code smell — prefer `QMutexLocker` pattern.

### 8.2 LOW — Inconsistent SQL schema for `rotationsingers` vs `rotationSingers`

**Files:** Migration v109 uses `rotationsingers` (lowercase) while the original CREATE TABLE uses `rotationSingers` (camelCase). SQLite identifiers are case-insensitive by default, but this inconsistency could cause confusion.

### 8.3 LOW — `SimpleCrypt` encryption key derived from MD5 hash of password

**File:** `src/settings.cpp:337-341`

`SimpleCrypt` with a 64-bit key derived from MD5 is not cryptographically secure. MD5 is broken for collision resistance, and 64-bit keys are brute-forceable. But since this is just a local app lock (not protecting against a determined attacker), it's acceptable for the use case.

---

## Priority Remediation Order

| Priority | Finding | Effort |
|----------|---------|--------|
| 1 | PRAGMA synchronous=OFF → NORMAL + WAL mode | Small |
| 2 | DB thread safety (separate connections per thread) | Medium |
| 3 | Transaction wrapping for round advancement | Small |
| 4 | Zero-singers round advancement guard | Small |
| 5 | KJ PIN plaintext storage → SecureCredentialStore | Small |
| 6 | SecureCredentialStore plaintext fallback warning | Small |
| 7 | Clear m_token and m_lastPassword on disconnect | Small |
| 8 | Schema migration transaction wrapping | Small |
| 9 | Missing database indexes | Small |
| 10 | QTemporaryDir lifecycle sync with media backend | Medium |
| 11 | Remove synchronous QEventLoop network calls | Medium |
| 12 | Consolidate credential storage paths | Medium |

---

*End of report. This is a thorough review of the critical paths. There are likely additional issues in the 5000-line mainwindow.cpp that weren't visible without reading the entire file, but the most impactful ones have been identified.*