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
- `9b4cb8a1` — docs: add install and account guide

## Current status
- **Windows build is GREEN** ✅ (run `24900979790`)
- Linux build is GREEN ✅
- macOS build still queued/running
- Multi-platform CI workflow now exists alongside Windows-only workflow
- Roger shipped: release readiness cleanup, subscription tier in settings, auth header split, web dashboard command wiring, install guide
- Lisa synced local master to `origin/master` (`9b4cb8a1`)

## Doing now
- [ ] Decide: tag/release now that Windows + Linux are green?
- [ ] Lisa: continue autokj-pro playback backend work
- [ ] Roger: monitor macOS build, address if red

## Blocked
- None currently 🎉

## Next
- [ ] Paul: tag/release decision — builds are green
- [ ] Lisa: autokj-pro playback worker
- [ ] Roger: macOS CI if it fails

## Handoff notes
- **2026-04-20 10:43 UTC** — Lisa fixed Roger's accidental merge conflict markers in `autokjserverapi.cpp` and added missing Qt modules.
- **2026-04-21 11:03 UTC** — Lisa pivoting to autokj-pro. Windows build running with qtwebchannel via `extra` param.
- **2026-04-24 19:22 UTC** — Lisa synced to `9b4cb8a1`. Windows + Linux builds are GREEN. Roger shipped significant cleanup work. Ready for release decision.

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
