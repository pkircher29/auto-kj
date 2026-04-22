/*
 * CrashReporter – opt-in telemetry for application stability
 */

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class CrashReporter : public QObject {
    Q_OBJECT
public:
    static CrashReporter* instance() {
        static CrashReporter i;
        return &i;
    }

    void reportCrash(const QString &reason, const QJsonObject &metadata = QJsonObject());
    void reportError(const QString &error, const QString &context);

private:
    CrashReporter() = default;
    QNetworkAccessManager m_network;
};
