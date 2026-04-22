/*
 * SecureCredentialStore – platform-native encrypted credential storage
 *
 * On Windows  → Windows Credential Manager (wincred.h / CredWrite/CredRead)
 * On macOS    → Keychain Services (Security.framework / SecKeychainItem)
 * On Linux    → libsecret via D-Bus (org.freedesktop.secrets)
 * Fallback    → QSettings (plain, same as before) with a runtime warning
 *
 * Usage:
 *   SecureCredentialStore store;
 *   store.store("AutoKJ/apiKey", apiKey);
 *   QString key = store.load("AutoKJ/apiKey");
 *   store.remove("AutoKJ/apiKey");
 *
 * The "target" string is the credential name / keychain label.
 * Keep it stable — changing it means migrating existing stored values.
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QSettings>
#include <QDebug>

class SecureCredentialStore {
public:
    SecureCredentialStore() = default;

    // Store a secret. Returns true on success.
    bool store(const QString &target, const QString &secret);

    // Load a secret. Returns empty string if not found or on error.
    QString load(const QString &target) const;

    // Delete a stored secret. Returns true if deleted or not present.
    bool remove(const QString &target);

private:
    // Fallback: QSettings under "SecureStore" group (NOT encrypted)
    void warnFallback(const QString &op) const;
    bool fallbackStore(const QString &target, const QString &secret);
    QString fallbackLoad(const QString &target) const;
    bool fallbackRemove(const QString &target);
};
