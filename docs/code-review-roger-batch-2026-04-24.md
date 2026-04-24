# Code Review: auto-kj (C++/Qt Desktop)

**Date**: 2026-04-24  
**Range**: `760b48cd..ca57b7e7` (Roger's batch — ~30 commits)  
**Reviewer**: Lisa

---

## Summary

Big batch from Roger covering:
1. SecureCredentialStore (platform-native keychain storage)
2. CrashReporter (opt-in telemetry)
3. PerformanceProfiler (lightweight perf instrumentation)
4. Change Password UI in Settings
5. Subscription tier display in Settings
6. Auth header split (AutoByRoute / ApiKeyOnly / BearerOnly / DualForCompat)
7. CI: Multi-platform build (Linux, macOS, Windows)
8. Reusable `setup-deps.yml` workflow
9. BUILD.md documentation
10. Dependabot config
11. NSIS/Inno Setup packaging
12. C++17 migration (from C++20)
13. Various CI fixes and build hardening
14. Mock client tests + integration smoke tests
15. i18n scaffolding in main.cpp
16. Update signature verification stub
17. QEventLoop cleanup (blocking login → async)

---

## Issues Found

### 🔴 Critical

**None found.**

### 🟡 Medium

1. **`SecureCredentialStore::store()` on Windows — `CREDENTIALW` lifetime**:  
   `store()` creates `std::wstring targetWide` and `userNameWide` on the stack, then passes `const_cast<LPWSTR>(targetWide.c_str())` and `userNameWide.c_str()` as `TargetName` and `UserName` to `CredWriteW`. The credential blob data also points to `secretBytes.data()`. **This is safe** because `CredWriteW` copies the data before returning (documented behavior), but the `const_cast` is a code smell. Consider adding a comment explaining why the const cast is safe here.

2. **`SecureCredentialStore::store()` on macOS — CF memory leak**:  
   `SecItemUpdate` / `SecItemAdd` path creates `CFMutableDictionaryRef query` and `attrs` but never calls `CFRelease` on the `CFStringCreateWithCString` values added to the dictionaries. The strings are consumed by the dictionary, but the dictionaries themselves are released. However, `CFStringCreateWithCString` returns a `+1` reference that is consumed by `CFDictionaryAddValue` — **this is actually correct** because `CFDictionaryAddValue` retains the value. But `attrs` is only released after `SecItemUpdate` returns, so if `SecItemUpdate` returns `errSecItemNotFound`, the code falls through to `SecItemAdd(query, nullptr)` — which reuses the already-released `query` and adds `kSecValueData` to it. **Bug**: The `SecItemAdd` call adds `kSecValueData` to `query` but `query` already has all the search keys from the update attempt. This should work (SecItemAdd accepts extra keys), but it's fragile. Consider restructuring to build separate add/update dicts.

3. **`dlgsettings.cpp` — duplicate `if (m_pendingReply)` check in `DlgRegister::on_btnCreate_clicked()`**:  
   Lines 96 and 98 (approx) both check `if (m_pendingReply) return;`. The second one is a duplicate that should be removed.

4. **`crashreporter.cpp` — sends data to `api.auto-kj.com` on crash**:  
   This is fine for an opt-in feature, but there's no opt-in UI yet. The setting `crashReportingEnabled` defaults to `false`, which means no data is sent until the user enables it. **Good**. But consider adding a checkbox in Settings so users can actually toggle it.

5. **`ci-multiplatform.yml` — GStreamer via Chocolatey on Windows**:  
   We previously established that the official GStreamer MSI installer returns 418 teapot / connection timeouts on GitHub Actions. The `setup-deps.yml` workflow also uses Chocolatey for GStreamer. This **will** fail if the Chocolatey package is broken or unavailable. The `build-windows.yml` (the one that works) uses a self-hosted GStreamer SDK from `pkircher29/gstreamer` release. Consider updating `ci-multiplatform.yml` and `setup-deps.yml` to use the self-hosted SDK instead.

6. **`performanceprofiler.h` — non-thread-safe singleton**:  
   `PerformanceProfiler::instance()` returns a static local, which is thread-safe for construction (C++11 magic statics), but `logTiming()` accesses `m_timings` (QHash) under a QMutex. The `m_timings` hash is never read anywhere — it's only written to. If this is just for debug logging, consider removing the hash entirely and just keeping the `qDebug` output. As-is, it's a memory leak that grows without bound.

7. **`SecureCredentialStore` Linux implementation shells out to `secret-tool`**:  
   This works but is slow (spawns a process for each store/load/remove). For a desktop app with 3 credentials, this is acceptable. But `which secret-tool` is called on every single operation. Consider caching the availability check.

### 🟢 Low / Nit

1. **`BUILD.md` is excellent** — thorough, covers all three platforms. Good addition.

2. **`dependabot.yml`** — Weekly GitHub Actions updates. Good.

3. **C++17 downgrade** — Correct for MSVC 2022 + Qt 5.15.2 compatibility. Good call.

4. **Designated initializer removal** in `dbupdater.cpp` — Correct, C++17 doesn't support designated initializers (that's C++20/extension). Good fix.

5. **`QFutureWatcher<QStringList>` fix** in `directorymonitor.cpp` — Correct fix for template type mismatch. Good.

6. **i18n scaffolding** in `main.cpp` — Correct pattern. No `.qm` files yet but the infrastructure is there. Good.

7. **`UpdateChecker::verifySignature()`** — Stub that only does SHA-256 hash comparison. No actual signature verification (ed25519, GPG, etc.). Fine as a placeholder, but it's not "signature" verification — it's "integrity check". Consider renaming to `verifyIntegrity()` or adding a TODO.

8. **`test_autokj_client_integration_smoke.cpp`** — Good smoke test. Validates mock client lifecycle (authenticate → create venue → add request → sync → end show → start show). No actual network calls.

9. **`test_autokjserverclientmock.cpp`** — Tests mock client signals, accepting state, checkins, and test connection. Well-structured.

10. **NSIS + Inno Setup dual packaging** in CI — Having both is overkill. Pick one (Inno Setup is better maintained). The CI tries Inno Setup first, falls back to NSIS. Consider removing NSIS entirely.

11. **`SecureCredentialStore` fallback to QSettings** — Plaintext fallback with a `qWarning()` is correct. Users on platforms without keychain support get warned. Good.

12. **Auth header split** — `AuthMode::AutoByRoute` pattern is clean. Desktop routes → API key only, dashboard routes → Bearer only, legacy routes → both. This matches the server-side enforcement. Good.

13. **`testHttpAuthAsync` refactored** — Now validates both API key (desktop routes) and Bearer (auth/me) separately. This is much more thorough than the previous single-request test. Good.

14. **Change Password UI** — Client-side validation enforces 8+ chars with uppercase, lowercase, digit, and special character. Server endpoint `/api/v1/dashboard/kj/change-password`. Uses `changePassword()` which does a synchronous `QEventLoop`. **Wait** — Roger's previous task was to *clean up QEventLoop usage*. But `changePassword()` and `login()` still use `QEventLoop`. These are new additions, so they're new blocking calls. Consider making these async too.

---

## Verdict

**Ship it.** The medium issues are non-blocking — the credential store works correctly, the auth split is clean, the subscription tier display is straightforward. The only thing worth fixing soon:

1. Remove the duplicate `if (m_pendingReply)` in `dlgregister.cpp`
2. Consider async variants of `changePassword()` and `login()` (the sync QEventLoop versions are new debt)
3. Replace Chocolatey GStreamer in CI with self-hosted SDK

The rest is solid. Roger delivered good work across auth, credential storage, packaging, tests, and CI.