# Auto Song Importer — Build Plan

## Goal
KJ drops karaoke zips in a folder. Auto-KJ auto-extracts, normalizes volume, identifies/renames, assigns IDs, re-zips with standard naming, and cleans up. Zero manual steps.

## Pipeline (in order)

```
folder watcher
  → .zip appears
    → extract .cdg + .mp3 to temp
      → volume normalize .mp3 (GStreamer peak analysis + gain)
        → parse filename for artist/title/ID
          → artist cache check (prompt once if unknown)
            → if no ID: assign UN-# placeholder
              → rename files to KJ convention: {ID} - {Artist} - {Song}.cdg/.mp3
                → re-zip into clean archive: {ID} - {Artist} - {Song}.zip
                  → add to song DB
                    → delete loose .cdg + .mp3
                      → remove original source .zip (configurable)
```

## Existing Infrastructure

| Component | What it does | Status |
|-----------|-------------|--------|
| `DirectoryMonitor` | File watcher (debounced) | Done |
| `DbUpdater` | Scan loop, parse files, DB add/update/remove | Done |
| `MzArchive` | Extract .cdg + .mp3 from karaoke zips | Done |
| `miniz` | ZIP read/write library (bundled) | Done |
| `KaraokeFileInfo` | Parse filename → artist/title/song ID via patterns | Done |
| `KaraokeFilePatternResolver` | Naming pattern → regex mapping | Done |
| `TagReader` | ID3 metadata reading | Done |
| `AudioFader` | GStreamer volume control | Done (used for playback) |

## What To Build

### Phase 1: Core Pipeline (3-4 days)

**New files:**
- `src/autozipper.h / .cpp` — Zip auto-extract on discovery, track extracted files in DB table `imported_zips`
- `src/volumenormalizer.h / .cpp` — GStreamer peak analysis + gain adjustment on import
- `src/artistcache.h / .cpp` — Artist name learning, file → artist mapping table

**Modified files:**
- `src/dbupdater.h / .cpp` — Hook all the new steps into the scan pipeline
- `src/dlgsettings.h / .cpp` — Import settings tab (folders, normalization target, naming pattern)

### Phase 2: ID Placeholders + Re-zip + Cleanup (2 days)

**New files:**
- None — all in existing classes or minor additions

**Modified files:**
- `src/dbupdater.cpp` — ID assignment (UN-#), re-zip step, cleanup step
- `src/mzarchive.h / .cpp` — Add re-zip method (normalized files → new archive)

### Phase 3: Artist Cache Dialog (1 day)

**Modified files:**
- `src/artistcache.cpp` — Add prompt-on-unknown dialog
- `src/dlgsettings.cpp` — Artist cache management tab

## Build Order

1. **Phase 1 — Zip extraction + tracking** (dbupdater hook + autozipper)
2. **Phase 2 — Volume normalization** (volumenormalizer)
3. **Phase 3 — Artist identification** (artistcache + dialog)
4. **Phase 4 — ID placeholders** (UN-# assignment)
5. **Phase 5 — Re-zip + cleanup** (mzarchive rezip + file deletion)
6. **Phase 6 — Settings UI** (import settings tab in dlgsettings)

## Data Storage

New SQLite table:
```sql
CREATE TABLE IF NOT EXISTS imported_files (
    source_path TEXT PRIMARY KEY,
    original_name TEXT NOT NULL,
    dest_path TEXT NOT NULL,
    dest_zip TEXT,
    artist TEXT,
    title TEXT,
    discid TEXT,
    normalized INTEGER DEFAULT 0,
    imported_at INTEGER NOT NULL,
    status TEXT DEFAULT 'imported'
);

CREATE TABLE IF NOT EXISTS artist_cache (
    filename_pattern TEXT PRIMARY KEY,
    artist_name TEXT NOT NULL,
    match_count INTEGER DEFAULT 0,
    auto_confirm INTEGER DEFAULT 0
);
```

## Total Timeline
~6-8 days for a complete, ship-ready feature.
