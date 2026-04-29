# HANDOFF

## Current goal
Ship a stable Windows desktop build, verify the GitHub Windows artifact pipeline, and keep the repo in a release-safe state.

## Owners
- Roger: build pipeline, releases, installer, CI/CD, git hygiene, technical triage
- Lisa: docs, copy, organization, product messaging, planning, research summaries
- Paul: product decisions, approval on public messaging and release timing

## Canonical repo
- This repo is the canonical desktop app repo: `auto-kj`
- Do not treat scratch copies, backups, or comparison folders as source of truth

## Safe branch
- `master`

## Latest safe commit
- `6f1785cd` — fix: bundle GStreamer plugins, libexec, and GIO modules in Windows release

## Current status
- **Windows build was GREEN** ✅ (run `24900979790`) — but missing GStreamer plugins
- **New build in progress** (run `24912108249`) with GStreamer plugins fix
- Linux build is GREEN ✅
- macOS build still queued/running
- Multi-platform CI workflow now exists alongside Windows-only workflow
- Roger shipped: release readiness cleanup, subscription tier in settings, auth header split, web dashboard command wiring, install guide
- Lisa fixed: merge conflict markers, missing Qt modules, missing GStreamer plugins + env vars

## Doing now
- [ ] Verify new build `24912108249` includes GStreamer plugins in artifact
- [ ] If green, confirm `lib/gstreamer-1.0/` exists in portable zip
- [ ] Paul: tag/release decision once plugins verified
- [ ] Lisa: continue autokj-pro playback backend work

## Blocked
- None currently (pending build verification)

## Next
- [ ] Paul: tag/release decision — builds are green, plugins now bundled
- [ ] Lisa: autokj-pro playback worker
- [ ] Roger: macOS CI if it fails, release automation improvements

## Handoff notes
- **2026-04-20 10:43 UTC** — Lisa fixed Roger's accidental merge conflict markers in `autokjserverapi.cpp` and added missing Qt modules.
- **2026-04-21 11:03 UTC** — Lisa pivoting to autokj-pro. Windows build running with qtwebchannel via `extra` param.
- **2026-04-24 19:22 UTC** — Lisa synced to `9b4cb8a1`. Windows + Linux builds are GREEN. Roger shipped significant cleanup work.
- **2026-04-24 21:12 UTC** — Lisa found GStreamer plugins were missing from Windows bundle. Fixed CI to copy `lib/gstreamer-1.0/`, `libexec/`, `lib/gio/modules/`. Added `GST_PLUGIN_SYSTEM_PATH` env var in `main.cpp` for Windows. Build `24912108249` in progress.

## Handoff rules
1. Update this file before switching context
2. Record the exact branch and latest safe commit
3. Keep notes short and factual
4. If a release or deploy happened, record what changed and how to roll back
5. Never say "it should be fine" when a concrete status is available

## Handoff template
```md
### Handoff note
- Time:
- Owner handing off:
- Branch:
- Safe commit:
- What changed:
- What is verified:
- What is unverified:
- Next recommended action:
- Blockers:
```
