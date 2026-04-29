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
- `a01b7a8e` — docs: clarify Auto-KJ uses Qt Widgets, no separate frontend

## Current status
- **RELEASE SHIPPED** — **v1.0.0-beta4** is live on GitHub
- Windows installer + portable artifacts attached (built from commit a01b7a8e)
- Release page: https://github.com/pkircher29/auto-kj/releases/tag/v1.0.0-beta4
- Singer App Phase 4 (Neon Stage, Discover, Setlist, Recordings, reactions, duets, venue presence) — deployed to VPS
- Auto-KJ website (auto-kj.com) — LIVE with Stripe checkout
- License server (api.auto-kj.com) — LIVE with Stripe webhook integration
- VPS currently unreachable (SSH timeout) — may need Paul to check/restart

## Doing now
- [x] Trigger Windows build on master (run 188 — GREEN)
- [x] Create v1.0.0-beta4 tag + GitHub release
- [x] Upload installer + portable artifacts
- [x] Update HANDOFF.md and TASKS.md

## Blocked
- VPS is unreachable from this server (SSH connection timeout)
  - Cannot currently re-deploy singer app or verify services
  - Paul may need to check VPS status or provide alternative access

## Next
- [ ] Deploy latest auto-kj-server code to VPS when it's reachable
- [ ] Update auto-kj-website on VPS with latest build if needed
- [ ] Continue autokj-pro work (Phase 1 core — playback pipeline, song scanner, CDG rendering)
- [ ] Set up email for paul@kircherentertainment.com for contact

## Handoff notes
- **2026-04-29 14:46 UTC** — Roger: v1.0.0-beta4 released. Windows build 188 (run 25115154263) GREEN on master (a01b7a8e). Artifacts: Auto-KJ-Windows-x64-Setup.exe (119MB) and Auto-KJ-Windows-x64.zip (161MB). VPS unreachable — service health unknown.

## Handoff rules
1. Update this file before switching context
2. Record the exact branch and latest safe commit
3. Keep notes short and factual
4. If a release or deploy happened, record what changed and how to roll back
5. Never say "it should be fine" when a concrete status is available
