# TASKS

## Now
- [x] Verify latest Windows GitHub Actions build result — GREEN
- [x] Fix missing GStreamer plugins in Windows bundle (Lisa: 6f1785cd)
- [x] Set GST_PLUGIN_SYSTEM_PATH at runtime on Windows (Lisa: 6f1785cd)
- [x] Verify new build with GStreamer plugins bundles correctly (run 188 GREEN)
- [x] Download artifact and confirm lib/gstreamer-1.0/ is present
- [x] Tag and release desktop build — v1.0.0-beta4 LIVE on GitHub
- [x] Update HANDOFF.md and TASKS.md

## Next
- [ ] Deploy latest auto-kj-server to VPS (blocked: VPS unreachable)
- [ ] Set up email for paul@kircherentertainment.com or similar contact
- [ ] Marketing: start getting users (communities: r/karaoke, Karaoke Scene forums, Facebook KJ groups)
- [ ] Clean remaining stale old-domain links in desktop app if any remain

## Later
- [ ] Improve release automation so tag/release is less manual
- [ ] Add macOS build to release workflow
- [ ] Add clearer release notes routine

## Blocked
- [ ] VPS (74.208.181.52) unreachable — SSH timeout. Paul needs to check.

## Ownership guide
- Roger: build, release, CI/CD, packaging, debugging, marketing research
- Lisa: docs, organization, copy, planning, summaries, backend endpoints
- Paul: product/release decisions when needed
