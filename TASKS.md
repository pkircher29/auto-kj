# TASKS

## Now
- [x] Verify latest Windows GitHub Actions build result — **GREEN** ✅
- [x] Fix missing GStreamer plugins in Windows bundle (Lisa: `6f1785cd`)
- [x] Set GST_PLUGIN_SYSTEM_PATH at runtime on Windows (Lisa: `6f1785cd`)
- [ ] Verify new build with GStreamer plugins bundles correctly (run `24912108249` in progress)
- [ ] If green, download artifact and confirm `lib/gstreamer-1.0/` is present
- [ ] If red, fix the first failing build step only

## Next
- [ ] Tag and release desktop build once artifacts are confirmed good AND plugins verified
- [ ] Clean remaining stale old-domain links in desktop app
- [ ] Review updater flow end-to-end after release

## Later
- [ ] Improve release automation so tag/release is less manual
- [ ] Reduce dirty-tree release risk with cleaner branch discipline
- [ ] Add clearer release notes routine

## Blocked
- [ ] None currently

## Ownership guide
- Roger: build, release, CI/CD, packaging, debugging
- Lisa: docs, organization, copy, planning, summaries
- Paul: product/release decisions when needed
