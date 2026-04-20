# Desktop Release Checklist

Use this checklist for `auto-kj` desktop app releases.

## Rules
- Do not release from a dirty, unclear tree
- Do not tag until the Windows artifact is confirmed good
- Record the exact branch, commit, tag, and artifact names used
- If anything fails, stop and fix the first real failure instead of stacking guesses

## Pre-release
- [ ] Working tree understood and release-safe
- [ ] Branch confirmed
- [ ] Latest safe commit recorded in `HANDOFF.md`
- [ ] Version confirmed
- [ ] Release notes summary drafted

## Build validation
- [ ] Windows GitHub workflow triggered
- [ ] Workflow completed successfully
- [ ] Portable artifact exists
- [ ] Installer artifact exists
- [ ] Artifact names match expectations
- [ ] Installer launches
- [ ] Desktop app launches
- [ ] Update link/check behavior verified if relevant

## Release steps
- [ ] Commits pushed
- [ ] Tag created
- [ ] GitHub release created
- [ ] Installer binary uploaded/attached
- [ ] Portable zip uploaded/attached
- [ ] Release notes checked for stale links or wrong branding

## Post-release
- [ ] GitHub release page checked live
- [ ] Asset downloads tested
- [ ] `HANDOFF.md` updated
- [ ] `TASKS.md` updated with follow-up issues

## Artifact expectations
Expected outputs may include:
- `Auto-KJ-Windows-x64.zip`
- `Auto-KJ-Windows-x64-Setup.exe`
- workflow artifacts with similar naming from GitHub Actions

## Release note skeleton
```md
## Auto-KJ <version>

### Fixes
- Installer branding cleanup
- Desktop updater link fix
- Windows packaging/workflow improvements

### Notes
- Built from commit: <commit>
- Windows artifacts attached below
```
