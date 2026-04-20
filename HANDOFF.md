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
- Check with `git log --oneline -n 1`
- When handing off, replace this line with the exact commit hash currently considered safe

## Current status
- Windows workflow exists in `.github/workflows/build-windows.yml`
- Additional manual artifact workflow exists in `.github/workflows/windows-artifact.yml`
- Recent fixes included installer branding cleanup and desktop updater link cleanup
- Release work should only continue from a clean, understood state

## Doing now
- [ ] Verify latest Windows GitHub Actions run result
- [ ] If green, prepare tag/release decision
- [ ] If red, fix the first failing step only

## Blocked
- [ ] None currently

## Next
- If Windows build passes, confirm artifact names and decide whether to tag/release
- If Windows build fails, capture the failing step and fix only that issue before retrying

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
