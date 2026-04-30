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

## COMPLETED FEATURES

### ✅ Auto Song Importer (#14) — done
Full pipeline: watch → extract → volume normalize → artist cache → ID placeholder → rename → re-zip → delete loose → delete source

### ✅ New Singer Alerts (#9) — done (PR #2)
Desktop tray notification + system beep when new singer joins rotation. Merged to master.

---

## YOUR NEXT JOB: Rotation Style (#7)

### #7 — Rotation Style
- **3 rotation types** configurable per gig:
  - **Classic (1-per-round):** Each singer rotates one-at-a-time
  - **Double (2-per-round):** Each singer gets 2 consecutive slots each rotation
  - **Flex (mixed):** DJ can mix 1-per-round and 2-per-round singers
- **Backend** (`auto-kj-server` repo): `rotation_style` field on `GigCreate` + `Gig` schema
- **Desktop app** (`auto-kj` repo): rotation style dropdown in gig creation/edit dialog
- **KJ Dashboard** (`auto-kj-server` frontend): rotation style display + per-singer override

### Where to work:
- Desktop C++ code: in the `auto-kj` repo
- Backend Python/FastAPI + DJ dashboard: in the `auto-kj-server` repo

### Key files for desktop:
- `src/dlgmanagevenuesgigs.cpp/.h` — gig creation/edit dialog
- `src/models/tablemodelkaraokesourcedirs.h` / `SourceDir` — existing settings model
- `src/rotationfairnessengine.h/.cpp` — existing rotation logic, this is where the style behavior lives
- `src/mainwindow.cpp` — main app logic

### Key files for server:
- `backend/app/schemas/venue.py` — GigCreate, Gig schemas
- `backend/app/db/models.py` — Gig SQLAlchemy model
- `frontend/apps/dj-dashboard/src/App.tsx` — DJ dashboard UI

---

## FEATURE QUEUE (build in this order)

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
