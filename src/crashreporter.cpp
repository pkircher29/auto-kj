#include "crashreporter.h"
#include "settings.h"
#include "okjversion.h"
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QDebug>

extern Settings settings;

void CrashReporter::reportCrash(const QString &reason, const QJsonObject &metadata)
{
    if (!settings.crashReportingEnabled())
        return;

    QJsonObject payload;
    payload["type"] = "crash";
    payload["reason"] = reason;
    payload["version"] = OKJ_VERSION_STRING;
    payload["os"] = QSysInfo::prettyProductName();
    payload["arch"] = QSysInfo::currentCpuArchitecture();
    payload["metadata"] = metadata;

    QNetworkRequest request(QUrl("https://api.auto-kj.com/v1/telemetry/crash"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // We don't necessarily wait for a response on a crash report
    m_network.post(request, QJsonDocument(payload).toJson());
    qInfo() << "Crash report sent:" << reason;
}

void CrashReporter::reportError(const QString &error, const QString &context)
{
    if (!settings.crashReportingEnabled())
        return;

    QJsonObject payload;
    payload["type"] = "error";
    payload["message"] = error;
    payload["context"] = context;
    payload["version"] = OKJ_VERSION_STRING;

    QNetworkRequest request(QUrl("https://api.auto-kj.com/v1/telemetry/error"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    m_network.post(request, QJsonDocument(payload).toJson());
}
