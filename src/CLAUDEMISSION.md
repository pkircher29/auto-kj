# CLAUDE MISSION — Auto-KJ Feature Builds

You are building features for **Auto-KJ**, a C++ Qt5/GStreamer karaoke software. Paul is the founder/KJ with 25 years experience. Build fast, ship working code.

---

## Repos

| Repo | What | URL |
|------|------|-----|
| auto-kj | C++ Qt5 desktop app (main product) | `github.com/pkircher29/auto-kj` |
| auto-kj-server | Python FastAPI backend + frontend apps | `github.com/pkircher29/auto-kj-server` |
| autokj-pro | Rust/Tauri prototype (ignore this) | `github.com/pkircher29/autokj-pro` |

Work in the **auto-kj repo** unless a feature specifically needs the server.

---

## YOUR FIRST JOB: Finish Auto Song Importer

The pipeline (in order):

```
watched folder → .zip appears → extract .cdg+.mp3 → GStreamer volume normalize
→ parse filename → artist cache check → ID placeholder → rename to KJ convention
→ re-zip into clean archive → add to DB → delete loose files → delete source .zip
```

### Already built (3 files exist at src/):

1. **`autozipper.h/.cpp`** — Extract zips to `AppData/Auto-KJ/imported/`, tracks in `imported_files` SQLite table, cleanup/remove methods
2. **`volumenormalizer.h/.cpp`** — GStreamer peak analysis via `level` element + gain application via `audioamplify`
3. **`artistcache.h/.cpp`** — DB-backed artist name learning (`artist_cache` table), auto-confirm after 3 matches

### What still needs to be done:

**A) Hook into dbupdater.cpp**
- Add includes for `autozipper.h`, `volumenormalizer.h`, `artistcache.h`
- In `addFilesToDatabase()`, before the existing .zip validation loop:
  - If a file is `.zip` and has `.cdg+.mp3` inside: run the full pipeline
  - After processing, point the DB path to the new re-zipped file
  - Add `AutoZipper`, `VolumeNormalizer`, `ArtistCache` as member references or instantiate per-scan

**B) ID Placeholder assignment**
- If `KaraokeFileInfo::getSongId()` returns empty: query `SELECT MAX(CAST(SUBSTR(discid, 4) AS INTEGER)) FROM dbSongs WHERE discid LIKE 'UN-%'`
- Assign `UN-{next}` (or start at 1 if none exist)
- Store assigned ID in the `discid` field when adding to DB

**C) Re-zip step**
- Add `createArchive()` method to `mzarchive.h/.cpp`:
  ```cpp
  bool createArchive(const QString &sourceDir, const QString &outputPath, const QStringList &files);
  ```
  Uses bundled miniz `mz_zip_writer` API (already in the project as an include).
- After normalization + rename: re-zip the `.cdg` + `.mp3` into a clean archive named `{ID} - {Artist} - {Song}.zip`

**D) Auto-rename to KJ convention**
- After extracting + normalizing, rename files to: `{discid} - {artist} - {title}.cdg` / `.mp3`
- Use existing `KaraokeFilePatternResolver` infrastructure
- Fall back to original base name if parsing fails

**E) Settings UI**
- Modify `dlgsettings.h/.cpp` to add an "Import Settings" tab:
  - Enable/disable auto-extract
  - Volume normalization toggle + target level slider
  - Artist cache management (view/clear)
  - Naming pattern selector (reuses existing pattern system)

**F) Artist cache dialog**
- When `ArtistCache::lookup()` returns empty for a pattern during import:
  - Show a dialog: "Found new song: [filename]. Who is this artist?"
  - Pre-populate artist from filename parse (if `KaraokeFileInfo::getArtist()` has a value)
  - Text field + "Confirm" button
  - Optionally: "Always this artist" checkbox (auto-silent immediately)
  - Store confirmed mapping via `ArtistCache::confirm()`

**G) Add new files to CMakeLists.txt**
- Add `autozipper`, `volumenormalizer`, `artistcache` to the `SOURCE_FILES` list (around line 491-740)

---

## AFTER AUTO SONG IMPORTER: Continue Through Paul's Wishlist

Build in this exact order:

### #14 (done ✅) — Auto Song Importer (your first job above)

### #9 — New Singer Integration
- When a new singer requests for the first time, send a push notification to the KJ
- Desktop app: tray notification + sound alert
- Singer PWA shows welcome animation
- Backend endpoint: `POST /venues/{id}/gigs/active/new-singer-alert`
- Desktop app connects to server via WebSocket or SSE for real-time alerts

### #7 — Rotation Style
- 3 rotation types configurable per gig:
  - **Classic (1-per-round):** Each singer rotates one-at-a-time
  - **Double (2-per-round):** Each singer gets 2 consecutive slots each rotation
  - **Flex (mixed):** DJ can mix 1-per-round and 2-per-round singers
- Backend: `rotation_style` field on `GigCreate` + `Gig` schema
- Desktop app: rotation style dropdown in gig creation/edit
- KJ Dashboard: rotation style display + per-singer override

### #1 — Tip/Payment
- Singers can tip the KJ or pay per song via the Singer PWA
- Stripe integration (already exists in server for licensing)
- Singer PWA: "Tip the KJ" button with preset amounts ($1/$2/$5)
- Desktop app: shows incoming tips in real-time with sound effect
- Backend: `tips` table, `POST /api/v1/singers/{id}/tip`, Stripe webhook for tips
- Optional: "Song Payment" mode where KJ charges per song

### #2 — Tip Shout-out
- When a singer tips, show the tip + a shout-out message on a secondary display
- Desktop app: "Tip Shout-Out" overlay window
- Singer writes a shout-out message when tipping
- Display shows: "🎉 [Singer Name] tipped $5! '[message]'"
- Configurable: toggle on/off, display duration, sound effect
- Designed to encourage more tips (social proof)

### #15 — Priority Track Selection (enhance existing)
- Already in brainstorm but NOT built: priority-based track selection
- Manufacturer priority list (SC > PHM > KV > SBI)
- Auto-selects best version when multiple tracks with same artist/title exist
- Right-click override with sticky checkbox
- UI: priority list editor in settings
- Backend: not needed (pure desktop feature)

### #10 — Break Music
- Auto-play music between singers
- Playlist mode: drag-drop or auto from folder
- GStreamer handles crossfade between songs
- Configurable: playlist folder, crossfade duration (0-5s), volume level
- Settings tab in desktop app

### #11 — Speed Slider
- Real-time playback speed control (0.5x - 2.0x) using GStreamer `pitch` element
- Slider in the main playback toolbar
- Resets to 1.0x when changing songs
- Save last speed per-song (optional)

### #5 — Show Reports
- Generate end-of-night report: songs played, singer stats, tips earned
- Export as CSV/PDF
- Desktop app: report dialog with preview
- Backend: report generation endpoint
- Stats: total songs, unique singers, most popular songs, total tips, show duration

### #4 — Singer Notes
- KJ can add notes to any singer profile
- Notes stored in backend: `singer_notes` table
- Desktop app: notes field when clicking a singer
- Notes persist across shows
- Example uses: "Allergic to peanuts — bring snacks", "Only drinks Diet Coke"

### #3 — Blacklist/Whitelist
- **Blacklist:** songs the KJ never wants played (blocked)
- **Whitelist:** songs the KJ only wants played (everything else blocked)
- Toggle per-gig: normal / whitelist-only / blacklist-only
- Desktop app: song list context menu → "Add to Blacklist" / "Add to Whitelist"
- Singer PWA: if whitelist mode, only show whitelisted songs; show "blocked" status for blacklisted

### #12 — Show Intro
- Startup screen/show intro when gig starts
- Full-screen display: venue name, "Start Singing!", QR code for singer PWA
- Configurable: background image/color, text content, duration
- Desktop app: intro window on secondary display
- Automatically shows when gig starts

### #13 — Song Vote
- Singers can upvote/downvote songs in the rotation
- Singer PWA: thumbs up/down next to each song in queue
- Desktop app: shows vote count next to each queue entry
- KJ can sort queue by votes (higher = earlier)
- Optional: only the rotating singer can vote (prevents vote manipulation)

### #6 — Sub KJ Mode
- Second KJ can log in as sub on an existing show
- Sub KJ gets limited controls: rotation management, song search, break music
- Full KJ retains master control
- Backend: `sub_kj_pin` or invitation system
- Desktop app: sub KJ login dialog, role-restricted UI

---

## CRITICAL RULES

1. **All features go into auto-kj (C++ Qt) first.** Do not gate behind "Pro". No feature is Pro-only.
2. **Push to GitHub after each completed feature.** Commit message format: `feature: [Feature Name] — brief description`
3. **No Pro gating, no pricing discussions.** Just build the product.
4. **If something breaks, fix it before moving on.**
5. **Test compilation only** — don't test runtime (no Qt SDK on build machine). Check for syntax errors.
6. **Artist cache dialog** should be a lightweight modal, not a blocking dialog. KJ should be able to close it and continue.
7. **Volume normalization** should NOT be mandatory — configurable toggle with sensible defaults.
8. **After each feature**, push `git add -A && git commit -m "feature: [name]" && git push origin master`
9. **If you run out of tokens**, leave a clear `// TODO: [next step]` comment at the top of the file you were working on.

---

## CODEBASE FACTS

- **Language:** C++17, Qt 5.15, GStreamer 1.0
- **Build system:** CMake (project file at `/CMakeLists.txt`)
- **Source files:** `/home/paul/.openclaw/workspace/auto-kj/src/` (185 files, ~22K lines)
- **Database:** SQLite via Qt SQL module (`QSqlQuery`)
- **SQL table `dbSongs`:** `songid (INT PK), discid (TEXT), artist (TEXT), title (TEXT), path (TEXT), filename (TEXT), duration (INT), searchstring (TEXT)`
- **ZIP library:** miniz (bundled, MIT) — `src/miniz/miniz.h` and `src/miniz/miniz.c`
- **GUI patterns:** Qt Designer forms `.ui` files in `src/`, or hand-coded widgets. Use existing dialogs as reference.
- **Logging:** spdlog via `m_logger->info()`, also `qInfo()`, `qWarning()`
- **File naming:** Classes in `lowercasewithouthyphens.h/.cpp` (e.g., `dbupdater.cpp`, `dlgsettings.cpp`)
- **SQLite migration:** Add `CREATE TABLE IF NOT EXISTS` in constructors or setup methods. No formal migration system.
- **API server:** Python FastAPI on port 3002 at `api.auto-kj.com`, Node.js license server on port 3001
- **VPS:** 74.208.181.52 (IONOS, Ubuntu 24.04), SSH key at `~/.ssh/id_rsa_autokj`
