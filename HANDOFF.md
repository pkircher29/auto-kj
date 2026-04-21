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
- `adbe9f9d` — fix: resolve merge conflict in autokjserverapi.cpp and add missing Qt WebChannel/WebEngineWidgets

## Current status
- Windows workflow exists in `.github/workflows/build-windows.yml`
- **Fixed**: merge conflict markers left in `autokjserverapi.cpp` by Roger (lines 965-972)
- **Fixed**: missing Qt WebChannel/WebEngineWidgets modules in CMakeLists.txt
- Build re-triggered: run `24662134822` (push) + `24662137786` (manual)
- Release work should only continue from a clean, understood state

## Doing now
- [x] Lisa synced to origin/master (commit `01c15268`)
- [x] Lisa: fixed merge conflict + missing Qt modules
- [x] Lisa: re-triggered Windows build
- [x] Lisa: added qtwebchannel via `extra` param (build `24718824821` in progress)
- [ ] Monitor build run `24718824821`
- [ ] If green, prepare tag/release decision
- [ ] If red, fix the first failing step only

## Blocked
- [ ] Waiting on Qt WebChannel module availability in CI

## Next
- [x] Lisa: pivot back to autokj-pro playback backend work
- [ ] Paul/Roger: monitor Windows build, merge any fix branches
- [ ] Target by Thursday: released auto-kj + pro on deck

## Handoff notes
- **2026-04-20 10:43 UTC** — Lisa fixed Roger's accidental merge conflict markers in `autokjserverapi.cpp` and added missing Qt modules. Build in progress.
- **2026-04-21 11:03 UTC** — Lisa pivoting back to autokj-pro. Windows build `24718824821` running with qtwebchannel via `extra` param. Paul/Roger to monitor.

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
