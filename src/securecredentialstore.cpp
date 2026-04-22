/*
 * SecureCredentialStore – implementation
 *
 * Each platform block is guarded by the same macros CMake / qmake exposes:
 *   Q_OS_WIN   – Windows (MSVC / MinGW)
 *   Q_OS_MACOS – macOS
 *   Q_OS_LINUX – Linux (libsecret)
 */

#include "securecredentialstore.h"

// ── Windows ──────────────────────────────────────────────────────────────────
#if defined(Q_OS_WIN)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <wincred.h>
#   pragma comment(lib, "Advapi32.lib")

static QByteArray toUtf8Bytes(const QString &s) { return s.toUtf8(); }

bool SecureCredentialStore::store(const QString &target, const QString &secret)
{
    QByteArray secretBytes = toUtf8Bytes(secret);
    QByteArray targetBytes = target.toLocal8Bit();

    CREDENTIALW cred = {};
    cred.Type                   = CRED_TYPE_GENERIC;
    cred.TargetName             = reinterpret_cast<LPWSTR>(target.toStdWString().data());
    cred.CredentialBlobSize     = static_cast<DWORD>(secretBytes.size());
    cred.CredentialBlob         = reinterpret_cast<LPBYTE>(secretBytes.data());
    cred.Persist                = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName               = L"autokj";

    if (!CredWriteW(&cred, 0)) {
        qWarning() << "[SecureCredentialStore] CredWriteW failed, HRESULT:" << GetLastError();
        return fallbackStore(target, secret);
    }
    return true;
}

QString SecureCredentialStore::load(const QString &target) const
{
    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(target.toStdWString().c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        DWORD err = GetLastError();
        if (err != ERROR_NOT_FOUND)
            qWarning() << "[SecureCredentialStore] CredReadW failed:" << err;
        return fallbackLoad(target);
    }
    QString result = QString::fromUtf8(
        reinterpret_cast<const char *>(pCred->CredentialBlob),
        static_cast<int>(pCred->CredentialBlobSize));
    CredFree(pCred);
    return result;
}

bool SecureCredentialStore::remove(const QString &target)
{
    if (!CredDeleteW(target.toStdWString().c_str(), CRED_TYPE_GENERIC, 0)) {
        DWORD err = GetLastError();
        if (err == ERROR_NOT_FOUND) return true; // already gone
        qWarning() << "[SecureCredentialStore] CredDeleteW failed:" << err;
        return fallbackRemove(target);
    }
    return true;
}

// ── macOS ────────────────────────────────────────────────────────────────────
#elif defined(Q_OS_MACOS)
#   include <Security/Security.h>

bool SecureCredentialStore::store(const QString &target, const QString &secret)
{
    QByteArray tgt = target.toUtf8();
    QByteArray sec = secret.toUtf8();

    // Try to update existing item first
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass,            kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService,      CFStringCreateWithCString(nullptr, tgt.constData(), kCFStringEncodingUTF8));
    CFDictionaryAddValue(query, kSecAttrAccount,      CFSTR("autokj"));

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(attrs, kSecValueData,
        CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(sec.constData()), sec.size()));

    OSStatus status = SecItemUpdate(query, attrs);
    if (status == errSecItemNotFound) {
        CFDictionaryAddValue(query, kSecValueData,
            CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(sec.constData()), sec.size()));
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(query);
    CFRelease(attrs);

    if (status != errSecSuccess) {
        qWarning() << "[SecureCredentialStore] Keychain store failed, status:" << status;
        return fallbackStore(target, secret);
    }
    return true;
}

QString SecureCredentialStore::load(const QString &target) const
{
    QByteArray tgt = target.toUtf8();

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass,           kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService,     CFStringCreateWithCString(nullptr, tgt.constData(), kCFStringEncodingUTF8));
    CFDictionaryAddValue(query, kSecAttrAccount,     CFSTR("autokj"));
    CFDictionaryAddValue(query, kSecReturnData,      kCFBooleanTrue);
    CFDictionaryAddValue(query, kSecMatchLimit,      kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);

    if (status != errSecSuccess || !result) {
        if (status != errSecItemNotFound)
            qWarning() << "[SecureCredentialStore] Keychain load failed, status:" << status;
        return fallbackLoad(target);
    }

    CFDataRef data = static_cast<CFDataRef>(result);
    QString secret = QString::fromUtf8(
        reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
        static_cast<int>(CFDataGetLength(data)));
    CFRelease(result);
    return secret;
}

bool SecureCredentialStore::remove(const QString &target)
{
    QByteArray tgt = target.toUtf8();

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass,       kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService, CFStringCreateWithCString(nullptr, tgt.constData(), kCFStringEncodingUTF8));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("autokj"));

    OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return (status == errSecSuccess || status == errSecItemNotFound);
}

// ── Linux (libsecret via D-Bus) ───────────────────────────────────────────────
#elif defined(Q_OS_LINUX)
#include <QProcess>

// We shell out to `secret-tool` (part of libsecret-tools) to avoid a direct
// C dependency on libsecret. If secret-tool is unavailable, we fall back.
static bool secretToolAvailable()
{
    return QProcess::execute("which", {"secret-tool"}) == 0;
}

bool SecureCredentialStore::store(const QString &target, const QString &secret)
{
    if (!secretToolAvailable()) return fallbackStore(target, secret);

    QProcess p;
    p.start("secret-tool", {"store", "--label", target, "service", "autokj", "target", target});
    p.write(secret.toUtf8());
    p.closeWriteChannel();
    p.waitForFinished(5000);
    if (p.exitCode() != 0) {
        qWarning() << "[SecureCredentialStore] secret-tool store failed";
        return fallbackStore(target, secret);
    }
    return true;
}

QString SecureCredentialStore::load(const QString &target) const
{
    if (!secretToolAvailable()) return fallbackLoad(target);

    QProcess p;
    p.start("secret-tool", {"lookup", "service", "autokj", "target", target});
    p.waitForFinished(5000);
    if (p.exitCode() != 0) return fallbackLoad(target);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

bool SecureCredentialStore::remove(const QString &target)
{
    if (!secretToolAvailable()) return fallbackRemove(target);

    QProcess p;
    p.start("secret-tool", {"clear", "service", "autokj", "target", target});
    p.waitForFinished(5000);
    return true;
}

// ── Fallback (unsupported platform) ──────────────────────────────────────────
#else
bool SecureCredentialStore::store(const QString &target, const QString &secret)
{
    warnFallback("store");
    return fallbackStore(target, secret);
}
QString SecureCredentialStore::load(const QString &target) const
{
    warnFallback("load");
    return fallbackLoad(target);
}
bool SecureCredentialStore::remove(const QString &target)
{
    warnFallback("remove");
    return fallbackRemove(target);
}
#endif

// ── Fallback helpers (always compiled) ───────────────────────────────────────

void SecureCredentialStore::warnFallback(const QString &op) const
{
    qWarning() << "[SecureCredentialStore]" << op
               << "— platform keychain unavailable; falling back to plain QSettings (NOT encrypted)";
}

bool SecureCredentialStore::fallbackStore(const QString &target, const QString &secret)
{
    QSettings s;
    s.beginGroup("SecureStoreFallback");
    s.setValue(target, secret);
    s.endGroup();
    return true;
}

QString SecureCredentialStore::fallbackLoad(const QString &target) const
{
    QSettings s;
    s.beginGroup("SecureStoreFallback");
    QString val = s.value(target, QString{}).toString();
    s.endGroup();
    return val;
}

bool SecureCredentialStore::fallbackRemove(const QString &target)
{
    QSettings s;
    s.beginGroup("SecureStoreFallback");
    s.remove(target);
    s.endGroup();
    return true;
}
