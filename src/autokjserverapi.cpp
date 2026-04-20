#include "autokjserverapi.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSslError>
#include <QSslSocket>
#include <QTime>
#include <QApplication>
#include <QMessageBox>

AutoKJServerAPI::AutoKJServerAPI(QObject *parent) : AutoKJServerClient(parent)
{
    qRegisterMetaType<OkjsVenues>("OkjsVenues");
    qRegisterMetaType<OkjsRequests>("OkjsRequests");
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_reconnectTimer = new QTimer(this);
    m_songSyncTimer = new QTimer(this);
    m_songSyncTimer->setSingleShot(false);
    m_songSyncTimer->setInterval(100); // send a chunk every 100ms
    m_songSyncTimeoutTimer = new QTimer(this);
    m_songSyncTimeoutTimer->setSingleShot(true);
    m_songSyncTimeoutTimer->setInterval(30000); // 30s stall timeout
    m_testTimer = new QTimer(this);
    m_testTimer->setSingleShot(true);
    m_testTimer->setInterval(10000); // 10s timeout for tests
    m_nam = new QNetworkAccessManager(this);


    connect(m_socket, &QWebSocket::connected,    this, &AutoKJServerAPI::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &AutoKJServerAPI::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &AutoKJServerAPI::onTextMessageReceived);
    connect(m_socket, QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors),
            this, &AutoKJServerAPI::onSslErrors);
    connect(m_reconnectTimer, &QTimer::timeout, this, &AutoKJServerAPI::onReconnectTimer);
    connect(m_songSyncTimer,  &QTimer::timeout, this, &AutoKJServerAPI::onSongSyncTimer);
    connect(m_songSyncTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_updateInProgress)
            return;
        m_songSyncTimer->stop();
        QString legacyError;
        if (tryLegacySongDbSync(&legacyError)) {
            m_songSyncTimeoutTimer->stop();
            m_updateInProgress = false;
            emit remoteSongDbUpdateDone();
            return;
        }
        m_updateInProgress = false;
        emit remoteSongDbUpdateFailed(
            legacyError.isEmpty()
                ? "Remote DB sync timed out waiting for server response."
                : "Remote DB sync timed out and legacy fallback failed: " + legacyError
        );
    });
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &AutoKJServerAPI::onSocketError);
    connect(m_testTimer, &QTimer::timeout, this, [this]() {
        if (m_testInProgress) {
            m_testInProgress = false;
            m_socket->abort();
            emit testFailed("Connection timed out after 10 seconds.");
        }
    });


    m_reconnectTimer->setInterval(m_reconnectIntervalSecs * 1000);
    m_settings.setPremiumAntiChaosAuthorized(false);

    if (m_settings.requestServerEnabled()) {
        refreshVenues();
        connectToServer();
    }
}

AutoKJServerAPI::~AutoKJServerAPI()
{
    m_songSyncTimer->stop();
    m_songSyncTimeoutTimer->stop();
    m_testTimer->stop();
    m_reconnectTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

// â”€â”€ Connection management â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::connectToServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        return;

    QString url = m_settings.requestServerUrl();
    if (url.isEmpty())
        return;

    // Convert http(s):// to ws(s)://
    if (url.startsWith("https://"))
        url.replace(0, 8, "wss://");
    else if (url.startsWith("http://"))
        url.replace(0, 7, "ws://");

    // Prevent confusing runtime failures on builds missing TLS runtime support.
    if (url.startsWith("wss://") && !QSslSocket::supportsSsl()) {
        const QString err = "SSL sockets are not supported on this platform. "
                            "Install the required OpenSSL runtime and restart Auto-KJ.";
        qWarning("[AutoKJ] %s", qPrintable(err));
        emit connectionStatusChanged(false);
        emit testFailed(err);
        return;
    }

    if (!url.endsWith("/ws/kj"))
        url += "/ws/kj";

    qDebug("[AutoKJ] Connecting to %s", qPrintable(url));
    m_socket->open(QUrl(url));
}

void AutoKJServerAPI::onConnected()
{
    qDebug("[AutoKJ] WebSocket connected");
    m_reconnectTimer->stop();
    authenticate();
}

void AutoKJServerAPI::onDisconnected()
{
    m_authenticated = false;
    m_settings.setPremiumAntiChaosAuthorized(false);
    emit connectionStatusChanged(false);
    qDebug("[AutoKJ] Disconnected â€” reconnecting in %ds", m_reconnectIntervalSecs);
    m_reconnectTimer->start();
}

void AutoKJServerAPI::onReconnectTimer()
{
    connectToServer();
}

void AutoKJServerAPI::setInterval(int intervalSecs)
{
    m_reconnectIntervalSecs = qMax(1, intervalSecs);
    m_reconnectTimer->setInterval(m_reconnectIntervalSecs * 1000);
}

// â”€â”€ Authentication â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::authenticate()
{
    QJsonObject data;
    data["token"]    = m_token.isEmpty() ? m_settings.requestServerToken() : m_token;
    data["venue_id"] = m_settings.requestServerVenue();
    
    // Send full config on auth so the server is always in sync
    data["kj_name"] = m_settings.kjName();
    data["max_singers_per_request"] = m_settings.maxSingersPerRequest();
    data["limit_one_song_per_singer"] = m_settings.limitOneSongPerSinger();
    data["block_repeat_songs"] = m_settings.blockRepeatSongs();
    data["kj_buffer_seconds"] = m_settings.kjBufferSeconds();
    data["no_show_timeout_seconds"] = m_settings.noShowTimeoutSeconds();
    data["line_jump_enabled"] = m_settings.lineJumpEnabled();
    data["geofence_enabled"] = m_settings.venueGeofenceEnabled();
    
    // Venue details
    data["lat"] = m_settings.venueLat();
    data["lon"] = m_settings.venueLon();
    data["geofence_radius_miles"] = m_settings.venueGeofenceRadius();
    data["geofence_enabled"] = m_settings.venueGeofenceEnabled();
    data["address"] = m_settings.venueAddress();
    data["kj_pin"] = m_settings.kjPin();

    sendEvent("kj:authenticate", data);
}

void AutoKJServerAPI::pushGigSettings()
{
    QJsonObject data;
    data["kj_name"] = m_settings.kjName();
    data["max_singers_per_request"] = m_settings.maxSingersPerRequest();
    data["limit_one_song_per_singer"] = m_settings.limitOneSongPerSinger();
    data["block_repeat_songs"] = m_settings.blockRepeatSongs();
    data["kj_buffer_seconds"] = m_settings.kjBufferSeconds();
    data["no_show_timeout_seconds"] = m_settings.noShowTimeoutSeconds();
    data["line_jump_enabled"] = m_settings.lineJumpEnabled();
    data["geofence_enabled"] = m_settings.venueGeofenceEnabled();
    sendEvent("kj:gig_settings", data);
}

void AutoKJServerAPI::pushVenueConfig()
{
    QJsonObject data;
    data["lat"] = m_settings.venueLat();
    data["lon"] = m_settings.venueLon();
    data["geofence_radius_miles"] = m_settings.venueGeofenceRadius();
    data["geofence_enabled"] = m_settings.venueGeofenceEnabled();
    data["address"] = m_settings.venueAddress();
    data["kj_pin"] = m_settings.kjPin();
    sendEvent("kj:venue_config", data);
}

// â”€â”€ Send/receive â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::sendEvent(const QString &event, const QJsonObject &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    QJsonObject msg;
    msg["event"] = event;
    msg["data"]  = data;
    m_socket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void AutoKJServerAPI::onTextMessageReceived(const QString &message)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject())
    {
        qWarning("[AutoKJ] Invalid message: %s", qPrintable(err.errorString()));
        return;
    }
    const auto event = doc.object().value("event").toString();
    const auto data  = doc.object().value("data").toObject();
    handleEvent(event, data);
}

void AutoKJServerAPI::handleEvent(const QString &event, const QJsonObject &data)
{
    if (event == "server:welcome")
    {
        qDebug("[AutoKJ] Server version: %s", qPrintable(data.value("version").toString()));
        return;
    }

    if (event == "server:authenticated")
    {
        m_authenticated = true;
        m_settings.setPremiumAntiChaosAuthorized(true);
        emit connectionStatusChanged(true);
        emit synchronized(QTime::currentTime());
        if (m_testInProgress) {
            m_testInProgress = false;
            m_testTimer->stop();
            emit testPassed();
        }

        qDebug("[AutoKJ] Authenticated â€” venue: %s", qPrintable(data.value("venueId").toString()));
        return;
    }

    if (event == "server:auth_error")
    {
        m_settings.setPremiumAntiChaosAuthorized(false);
        qWarning("[AutoKJ] Auth failed: %s", qPrintable(data.value("message").toString()));
        emit testFailed(data.value("message").toString());
        if (m_updateInProgress) {
            m_songSyncTimer->stop();
            m_songSyncTimeoutTimer->stop();
            m_updateInProgress = false;
            emit remoteSongDbUpdateFailed(data.value("message").toString());
        }
        m_testInProgress = false;
        m_testTimer->stop();
        return;

    }

    if (event == "server:new_request")
    {
        processNewRequest(data);
        return;
    }

    if (event == "server:pending_requests")
    {
        processPendingRequests(data.value("requests").toArray());
        return;
    }

    if (event == "server:request_cancel")
    {
        // A singer cancelled their own request â€” refresh the list
        const QString id = data.value("id").toString();
        m_requests.erase(std::remove_if(m_requests.begin(), m_requests.end(),
            [id](const OkjsRequest &r) { return r.requestId == id; }),
            m_requests.end());
        emit requestsChanged(m_requests);
        return;
    }

    if (event == "server:checkins")
    {
        emit checkinsFetched(data.value("checkins").toArray());
        return;
    }

    if (event == "server:songs_sync_ready")
    {
        qDebug("[AutoKJ] Server ready for song sync â€” starting chunks");
        m_syncChunkIndex = 0;
        m_songSyncTimeoutTimer->start();
        m_songSyncTimer->start();
        return;
    }

    if (event == "server:songs_chunk_ack")
    {
        // Acknowledged â€” nothing extra needed; timer sends next chunk
        if (m_updateInProgress)
            m_songSyncTimeoutTimer->start();
        return;
    }

    if (event == "server:songs_sync_complete")
    {
        m_songSyncTimer->stop();
        m_songSyncTimeoutTimer->stop();
        m_updateInProgress = false;
        emit remoteSongDbUpdateDone();
        return;
    }

    if (event == "server:kj_web_command")
    {
        // Commands from the KJ web tablet interface â€” will be wired to model methods in MainWindow
        // Emit as a signal or handle directly (future: connect to MainWindow slots)
        const QString action = data.value("action").toString();
        const QJsonObject payload = data.value("payload").toObject();
        Q_UNUSED(action)
        Q_UNUSED(payload)
        // TODO: emit kjWebCommand(action, payload) and handle in MainWindow
        return;
    }
}

// â”€â”€ Request management â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::processNewRequest(const QJsonObject &reqData)
{
    OkjsRequest req;
    req.requestId = reqData.value("id").toString();
    req.singer    = reqData.value("singer_name").toString();
    req.artist    = reqData.value("artist").toString();
    req.title     = reqData.value("title").toString();
    req.key       = reqData.value("key_change").toInt();
    req.time      = QDateTime::fromString(reqData.value("requested_at").toString(), Qt::ISODate).toSecsSinceEpoch();
    req.cosinger2 = reqData.value("cosinger2").toString();
    req.cosinger3 = reqData.value("cosinger3").toString();
    req.cosinger4 = reqData.value("cosinger4").toString();
    req.isSharedDevice = reqData.value("is_shared_device").toBool(false);
    req.noShowCount = reqData.value("no_show_count").toInt(0);
    req.removedCount = reqData.value("removed_count").toInt(0);

    if (!m_requests.contains(req))
    {
        m_requests.append(req);
        emit requestsChanged(m_requests);
    }
}

void AutoKJServerAPI::processPendingRequests(const QJsonArray &reqArray)
{
    OkjsRequests newRequests;
    for (const auto &entry : reqArray)
    {
        const QJsonObject obj = entry.toObject();
        OkjsRequest req;
        req.requestId = obj.value("id").toString();
        req.singer    = obj.value("singer_name").toString();
        req.artist    = obj.value("artist").toString();
        req.title     = obj.value("title").toString();
        req.key       = obj.value("key_change").toInt();
        req.time      = QDateTime::fromString(obj.value("requested_at").toString(), Qt::ISODate).toSecsSinceEpoch();
        req.cosinger2 = obj.value("cosinger2").toString();
        req.cosinger3 = obj.value("cosinger3").toString();
        req.cosinger4 = obj.value("cosinger4").toString();
        req.isSharedDevice = obj.value("is_shared_device").toBool(false);
        req.noShowCount = obj.value("no_show_count").toInt(0);
        req.removedCount = obj.value("removed_count").toInt(0);
        newRequests.append(req);
    }
    if (newRequests != m_requests)
    {
        m_requests = newRequests;
        emit requestsChanged(m_requests);
    }
}

void AutoKJServerAPI::removeRequest(const QString &requestId)
{
    QJsonObject data;
    data["requestId"] = requestId;
    data["action"]    = "accept";
    sendEvent("kj:request_handled", data);

    m_requests.erase(std::remove_if(m_requests.begin(), m_requests.end(),
        [requestId](const OkjsRequest &r) { return r.requestId == requestId; }),
        m_requests.end());
    emit requestsChanged(m_requests);
}

void AutoKJServerAPI::setAccepting(bool enabled, bool offerEndShowPrompt)
{
    const bool previous = m_accepting;
    m_accepting = enabled;

    const QString token = ensureToken();
    const int venueId = m_settings.requestServerVenue();
    if (token.isEmpty() || venueId <= 0) {
        qWarning("[AutoKJ] Cannot update accepting state: missing token or venue.");
        return;
    }

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + QString("/api/v1/kj/venues/%1").arg(venueId)));
    setAuthHeader(request, token);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["accepting"] = enabled;
    auto sendAcceptingPatch = [this, &request, &payload](QString *responseBody, QString *errorString) -> bool {
        QNetworkReply *reply = m_nam->sendCustomRequest(
            request,
            "PATCH",
            QJsonDocument(payload).toJson(QJsonDocument::Compact)
        );
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const QString body = QString::fromUtf8(reply->readAll());
        if (responseBody)
            *responseBody = body;
        if (errorString)
            *errorString = reply->errorString();
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        return ok;
    };

    QString errBody;
    QString errString;
    bool ok = sendAcceptingPatch(&errBody, &errString);

    // Auto-start path: when enabling requests on a venue with no active show,
    // start one automatically and retry once.
    if (!ok && enabled && errBody.contains("No active show found", Qt::CaseInsensitive)) {
        QString startErr;
        if (startNewShow(&startErr)) {
            ok = sendAcceptingPatch(&errBody, &errString);
        } else {
            errBody = startErr;
        }
    }

    if (!ok) {
        qWarning("[AutoKJ] Failed to set accepting=%d: %s %s",
                 enabled ? 1 : 0,
                 qPrintable(errString),
                 qPrintable(errBody));
        m_accepting = previous;

        if (enabled && errBody.contains("Concurrent show limit reached", Qt::CaseInsensitive)) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Upgrade Required");
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setText("Concurrent show limit reached for this account.");
            msgBox.setInformativeText(
                "To accept requests from another location at the same time, "
                "please upgrade your plan to add more concurrent web shows."
            );
            msgBox.exec();
        }
    }

    Q_UNUSED(offerEndShowPrompt);
    // Toggling Accept Requests off no longer ends the show — the show keeps
    // running until the KJ explicitly clicks End Show.

    if (ok) {
        refreshVenues(true);
    }

    // Keep websocket compatibility path for legacy server behavior.
    QJsonObject data;
    data["accepting"] = m_accepting;
    sendEvent("kj:accepting", data);
}

bool AutoKJServerAPI::getAccepting() const
{
    return m_accepting;
}

void AutoKJServerAPI::clearRequests()
{
    for (const auto &req : std::as_const(m_requests))
    {
        QJsonObject data;
        data["requestId"] = req.requestId;
        data["action"]    = "reject";
        sendEvent("kj:request_handled", data);
    }
    m_requests.clear();
    emit requestsChanged(m_requests);
}

// â”€â”€ Rotation / analytics push â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::pushRotationUpdate(const QJsonObject &rotationData)
{
    sendEvent("kj:rotation_update", rotationData);
}

void AutoKJServerAPI::notifySongPlayed(const QString &singerName, const QString &artist,
                                        const QString &title, const QStringList &cosingers,
                                        const QString &kjName)
{
    QJsonObject data;
    data["singerName"]   = singerName;
    data["artist"]       = artist;
    data["title"]        = title;
    data["cosingers"]    = QJsonArray::fromStringList(cosingers);
    data["performedAt"]  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!kjName.isEmpty())
        data["kjName"]   = kjName;
    sendEvent("kj:song_played", data);
}

void AutoKJServerAPI::pushNowPlaying(const QString &singer, const QString &artist,
                                      const QString &title, int durationSecs, int keyChange)
{
    QJsonObject data;
    data["singer"]       = singer;
    data["artist"]       = artist;
    data["title"]        = title;
    data["duration"]     = durationSecs;
    data["keyChange"]    = keyChange;
    sendEvent("kj:now_playing", data);
}

// â”€â”€ Song DB sync â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::updateSongDb()
{
    if (m_updateInProgress) {
        emit remoteSongDbUpdateFailed("Remote DB sync is already in progress.");
        return;
    }

    m_cancelUpdate = false;
    m_updateInProgress = true;
    m_songChunks.clear();
    m_syncChunkIndex = 0;
    emit remoteSongDbUpdateStart();

    // Build chunks
    QSqlQuery query;
    const int songsPerChunk = 500;
    if (!query.exec("SELECT DISTINCT artist,title FROM dbsongs "
                    "WHERE discid != '!!DROPPED!!' AND discid != '!!BAD!!' "
                    "ORDER BY artist ASC, title ASC"))
    {
        qWarning("[AutoKJ] Failed to query songs for sync");
        m_updateInProgress = false;
        emit remoteSongDbUpdateFailed("Failed to query local karaoke library for sync.");
        return;
    }

    QJsonArray chunk;
    int total = 0;
    while (query.next())
    {
        QJsonObject song;
        song["artist"] = query.value(0).toString();
        song["title"]  = query.value(1).toString();
        chunk.append(song);
        total++;
        if (chunk.size() >= songsPerChunk)
        {
            QJsonObject doc;
            doc["songs"] = chunk;
            m_songChunks.append(QJsonDocument(doc));
            chunk = QJsonArray();
        }
    }
    if (!chunk.isEmpty())
    {
        QJsonObject doc;
        doc["songs"] = chunk;
        m_songChunks.append(QJsonDocument(doc));
    }

    emit remoteSongDbUpdateNumDocs(m_songChunks.size());

    if (!m_authenticated) {
        QString legacyError;
        if (tryLegacySongDbSync(&legacyError)) {
            m_updateInProgress = false;
            emit remoteSongDbUpdateDone();
        } else {
            m_updateInProgress = false;
            emit remoteSongDbUpdateFailed(
                legacyError.isEmpty()
                    ? "Not connected to request server, and legacy song sync failed."
                    : "Not connected to request server. Legacy song sync failed: " + legacyError
            );
        }
        return;
    }

    // Tell server to clear and start accepting chunks
    sendEvent("kj:songs_sync_start", {});
    m_songSyncTimeoutTimer->start();
}

void AutoKJServerAPI::onSongSyncTimer()
{
    if (m_cancelUpdate)
    {
        m_songSyncTimer->stop();
        m_songSyncTimeoutTimer->stop();
        m_updateInProgress = false;
        return;
    }
    if (m_syncChunkIndex >= m_songChunks.size())
    {
        m_songSyncTimer->stop();
        sendEvent("kj:songs_sync_done", {});
        m_songSyncTimeoutTimer->start();
        return;
    }

    const QJsonDocument &chunkDoc = m_songChunks.at(m_syncChunkIndex);
    QJsonObject data = chunkDoc.object();
    sendEvent("kj:songs_sync_chunk", data);
    emit remoteSongDbUpdateProgress(m_syncChunkIndex + 1);
    m_songSyncTimeoutTimer->start();
    m_syncChunkIndex++;
}

void AutoKJServerAPI::dbUpdateCanceled()
{
    m_cancelUpdate = true;
    m_songSyncTimer->stop();
    m_songSyncTimeoutTimer->stop();
    m_updateInProgress = false;
}

bool AutoKJServerAPI::tryLegacySongDbSync(QString *errorOut)
{
    const QString token = ensureToken(errorOut);
    if (token.isEmpty())
        return false;

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (baseUrl.startsWith("wss://"))
        baseUrl.replace(0, 6, "https://");
    else if (baseUrl.startsWith("ws://"))
        baseUrl.replace(0, 5, "https://");
    if (!baseUrl.startsWith("http"))
        baseUrl = "https://" + baseUrl;

    const int systemId = m_settings.systemId();
    bool modernEndpointMissing = false;

    auto parseErrorFromReply = [&](QNetworkReply *reply, const QByteArray &body) -> QString {
        QString details = reply->errorString();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            const auto obj = doc.object();
            const QString detail = obj.value("detail").toString();
            if (!detail.isEmpty())
                details = detail;
            const QString errorString = obj.value("errorString").toString();
            if (!errorString.isEmpty())
                details = errorString;
        }
        return details;
    };

    auto postModern = [&](const QJsonObject &obj) -> bool {
        QNetworkRequest request(QUrl(baseUrl + "/api/v1/kj/songs/sync"));
        setAuthHeader(request, token);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = m_nam->post(request, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const QByteArray body = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            if (statusCode == 404) {
                modernEndpointMissing = true;
                reply->deleteLater();
                return false;
            }
            if (errorOut) *errorOut = parseErrorFromReply(reply, body);
            reply->deleteLater();
            return false;
        }

        reply->deleteLater();
        return true;
    };

    auto postLegacy = [&](const QJsonObject &obj, QJsonObject *outObj) -> bool {
        QNetworkRequest request(QUrl(baseUrl + "/api"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = m_nam->post(request, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            if (errorOut) *errorOut = parseErrorFromReply(reply, body);
            reply->deleteLater();
            return false;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        reply->deleteLater();
        if (!doc.isObject()) {
            if (errorOut) *errorOut = "Invalid response from legacy /api endpoint.";
            return false;
        }
        if (outObj) *outObj = doc.object();
        if (doc.object().value("error").toBool(false)) {
            if (errorOut) *errorOut = doc.object().value("errorString").toString("Legacy API returned an error.");
            return false;
        }
        return true;
    };

    // Prefer modern FastAPI endpoint first.
    if (postModern(QJsonObject{{"mode", "clear"}})) {
        for (int i = 0; i < m_songChunks.size(); ++i) {
            if (m_cancelUpdate) {
                if (errorOut) *errorOut = "Sync canceled.";
                return false;
            }
            QJsonObject req{
                {"mode", "add"},
                {"songs", m_songChunks.at(i).object().value("songs").toArray()},
            };
            if (!postModern(req))
                return false;
            emit remoteSongDbUpdateProgress(i + 1);
        }
        if (errorOut) errorOut->clear();
        return true;
    } else if (!modernEndpointMissing) {
        // Endpoint exists but failed for a real reason (auth/validation/server error).
        return false;
    }

    // Legacy AutoKJ /api fallback
    {
        QJsonObject req{
            {"command", "clearDatabase"},
            {"system_id", systemId},
        };
        QJsonObject ignored;
        if (!postLegacy(req, &ignored))
            return false;
    }

    // 2) Upload chunks
    for (int i = 0; i < m_songChunks.size(); ++i) {
        if (m_cancelUpdate) {
            if (errorOut) *errorOut = "Sync canceled.";
            return false;
        }
        QJsonObject req{
            {"command", "addSongs"},
            {"system_id", systemId},
            {"songs", m_songChunks.at(i).object().value("songs").toArray()},
        };
        QJsonObject ignored;
        if (!postLegacy(req, &ignored))
            return false;
        emit remoteSongDbUpdateProgress(i + 1);
    }

    if (errorOut) errorOut->clear();
    return true;
}

// â”€â”€ Venue list â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::refreshVenues(bool blocking)
{
    const QString token = ensureToken();
    if (token.isEmpty())
        return;

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/kj/venues"));
    setAuthHeader(request, token);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onVenuesReply(reply);
    });
    if (blocking) {
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

void AutoKJServerAPI::onVenuesReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qWarning("[AutoKJ] Venues fetch failed: %s", qPrintable(reply->errorString()));
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray())
        return;

    OkjsVenues venues;
    const int selectedVenueId = m_settings.requestServerVenue();
    for (const auto &entry : doc.array()) {
        QJsonObject obj = entry.toObject();
        OkjsVenue v;
        v.venueId  = obj.value("id").toInt();
        v.name     = obj.value("name").toString();
        v.urlName  = obj.value("url_name").toString();
        v.accepting = obj.value("accepting").toBool(true);
        v.hasActiveShow = obj.value("has_active_show").toBool(false);
        if (v.venueId == selectedVenueId)
            m_accepting = v.accepting;
        venues.append(v);
    }
    emit venuesChanged(venues);
}

// â”€â”€ Test connection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::test()
{
    m_testInProgress = true;

    // Preferred path for AutoKJ-Pro backend: validate token over HTTP first.
    // This avoids failing the test when /ws/kj is not implemented on the server.
    QString httpErr;
    if (testHttpAuth(&httpErr)) {
        m_testInProgress = false;
        m_testTimer->stop();
        emit testPassed();
        return;
    }

    // If API gave a concrete error, prefer that over a websocket fallback.
    // Fallback to websocket only for likely legacy servers without /api/v1/kj/venues.
    if (!httpErr.isEmpty()) {
        const bool mayBeLegacyServer =
            httpErr.contains("not found", Qt::CaseInsensitive) ||
            httpErr.contains("invalid response", Qt::CaseInsensitive);
        if (!mayBeLegacyServer) {
            m_testInProgress = false;
            m_testTimer->stop();
            emit testFailed(httpErr);
            return;
        }
    }

    // Guard websocket fallback when SSL sockets are unavailable.
    const QString baseUrl = m_settings.requestServerUrl();
    if ((baseUrl.startsWith("https://") || baseUrl.startsWith("wss://")) && !QSslSocket::supportsSsl()) {
        m_testInProgress = false;
        m_testTimer->stop();
        emit testFailed("SSL sockets are not supported on this platform. "
                        "Install the required OpenSSL runtime and restart Auto-KJ.");
        return;
    }

    if (m_authenticated)
    {
        m_testInProgress = false;
        m_testTimer->stop();
        emit testPassed();
        return;
    }
    m_testTimer->start();
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        authenticate();
    } else {
        connectToServer();
    }
    // testPassed/testFailed emitted from handleEvent when auth response arrives
}

bool AutoKJServerAPI::login(QString *errorOut)
{
    const QString email    = m_settings.requestServerEmail();
    const QString password = m_settings.requestServerPassword();
    if (email.isEmpty() || password.isEmpty()) {
        if (errorOut) *errorOut = "Email and password are required.";
        return false;
    }

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/auth/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["email"]    = email;
    body["password"] = password;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray responseBody = reply->readAll();
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = "Invalid login response from server.";
        return false;
    }

    const QString token = doc.object().value("access_token").toString();
    if (token.isEmpty()) {
        const QString detail = doc.object().value("detail").toString();
        if (errorOut) *errorOut = detail.isEmpty() ? "Login failed." : detail;
        return false;
    }

    m_token = token;
    m_settings.setRequestServerToken(token);

    const QString apiKey = doc.object().value("api_key").toString();
    if (!apiKey.isEmpty())
        m_settings.setRequestServerApiKey(apiKey);

    if (errorOut) errorOut->clear();
    return true;
}

QString AutoKJServerAPI::ensureToken(QString *errorOut)
{
    if (!m_token.isEmpty())
        return m_token;
    const QString stored = m_settings.requestServerToken();
    if (!stored.isEmpty()) {
        m_token = stored;
        return m_token;
    }
    if (!login(errorOut))
        return {};
    return m_token;
}

void AutoKJServerAPI::setAuthHeader(QNetworkRequest &request, const QString &token)
{
    // KJ desktop endpoints authenticate via X-Api-Key. The Bearer token is kept
    // for backwards compatibility with anything that still expects JWT.
    const QString apiKey = m_settings.requestServerApiKey();
    if (!apiKey.isEmpty())
        request.setRawHeader("X-Api-Key", apiKey.toUtf8());
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
}

bool AutoKJServerAPI::testHttpAuth(QString *errorOut)
{
    QString token = ensureToken(errorOut);
    if (token.isEmpty())
        return false;

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;

    if (baseUrl.startsWith("https://") && !QSslSocket::supportsSsl()) {
        if (errorOut) *errorOut = "TLS is not available. Install the required OpenSSL runtime and restart Auto-KJ.";
        return false;
    }

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/kj/venues"));
    setAuthHeader(request, token);
    QNetworkReply *reply = m_nam->get(request);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    // If 401, credentials may be expired — re-login once and retry
    if (status == 401) {
        m_token.clear();
        m_settings.setRequestServerToken({});
        m_settings.setRequestServerApiKey({});
        token = QString();
        if (!login(errorOut))
            return false;
        QNetworkRequest retryReq(QUrl(baseUrl + "/api/v1/kj/venues"));
        setAuthHeader(retryReq, m_token);
        QNetworkReply *retryReply = m_nam->get(retryReq);
        QEventLoop loop2;
        connect(retryReply, &QNetworkReply::finished, &loop2, &QEventLoop::quit);
        loop2.exec();
        const QByteArray retryBody = retryReply->readAll();
        retryReply->deleteLater();
        const QJsonDocument retryDoc = QJsonDocument::fromJson(retryBody);
        if (!retryDoc.isArray()) {
            if (errorOut) *errorOut = "Authentication failed after re-login.";
            return false;
        }
        if (errorOut) errorOut->clear();
        return true;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QString details = reply->errorString();
        if (doc.isObject()) {
            const QString d = doc.object().value("detail").toString();
            if (!d.isEmpty()) details = d;
        }
        if (errorOut) *errorOut = details;
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        if (errorOut) *errorOut = "Invalid response from /api/v1/kj/venues.";
        return false;
    }

    if (errorOut) errorOut->clear();
    return true;
}

void AutoKJServerAPI::onSocketError(QAbstractSocket::SocketError error)
{
    QString errStr = m_socket->errorString();
    qWarning("[AutoKJ] Socket error: %s", qPrintable(errStr));
    
    if (m_testInProgress) {
        m_testInProgress = false;
        m_testTimer->stop();
        emit testFailed(errStr);
    }
}


// â”€â”€ Venue URL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

QString AutoKJServerAPI::venueUrl() const
{
    QString base = m_settings.requestServerUrl();
    // Strip trailing /ws/kj if present
    if (base.endsWith("/ws/kj"))
        base.chop(6);
    if (!base.startsWith("http://") && !base.startsWith("https://"))
        base = "https://" + base;
    const QString slug = m_settings.requestServerVenueSlug().trimmed();
    if (slug.isEmpty())
        return base;
    return base + "/v/" + slug;
}

bool AutoKJServerAPI::isConnected() const
{
    return m_authenticated;
}

// â”€â”€ SSL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::onSslErrors(const QList<QSslError> &errors)
{
    // SECURITY: Only ignore specific expected errors (self-signed certs on local networks)
    // Never ignore all SSL errors blindly.
    QList<QSslError> expectedErrors;
    for (const auto &e : errors) {
        if (e.error() == QSslError::SelfSignedCertificate ||
            e.error() == QSslError::SelfSignedCertificateInChain ||
            e.error() == QSslError::CertificateUntrusted ||
            e.error() == QSslError::HostNameMismatch) {
            expectedErrors.append(e);
        }
    }

    // Only ignore if ALL errors are in the expected list AND user has opted in
    if (m_settings.requestServerIgnoreCertErrors() && errors.size() == expectedErrors.size() && !expectedErrors.isEmpty()) {
        m_socket->ignoreSslErrors(expectedErrors);
        return;
    }

    // Report errors that can't be safely ignored
    QString errorText;
    for (const auto &e : errors)
        errorText += e.errorString() + "\n";
    qWarning() << "SSL errors (not ignored):" << errorText;
    emit testSslError(errorText);
    emit sslError();
}


// â”€â”€ Check-ins â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AutoKJServerAPI::fetchCheckins()
{
    sendEvent("kj:get_checkins", {});
}

void AutoKJServerAPI::markCheckinAdded(const QString &checkinId)
{
    sendEvent("kj:checkin_added", {{"checkinId", checkinId}});
}

void AutoKJServerAPI::createVenue(const QString &name, const QString &address, const QString &pin)
{
    const QString token = ensureToken();
    if (token.isEmpty())
        return;

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/kj/venues"));
    setAuthHeader(request, token);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["name"] = name;
    obj["address"] = address;
    obj["kj_pin"] = pin;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(obj).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            refreshVenues();
        } else {
            qWarning("[AutoKJ] Venue creation failed: %s", qPrintable(reply->errorString()));
            emit testFailed("Venue creation failed: " + reply->errorString());
        }
    });
}

bool AutoKJServerAPI::startNewShow(QString *errorOut)
{
    const QString token = ensureToken(errorOut);
    if (token.isEmpty())
        return false;

    const int venueId = m_settings.requestServerVenue();
    if (venueId <= 0) {
        if (errorOut) *errorOut = "No venue selected.";
        return false;
    }

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + QString("/api/v1/kj/venues/%1/gigs/start").arg(venueId)));
    setAuthHeader(request, token);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(QJsonObject{}).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray body = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) {
        QString details = reply->errorString();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isObject()) {
            const QString apiDetail = doc.object().value("detail").toString();
            if (!apiDetail.isEmpty())
                details = apiDetail;
        }
        if (errorOut) *errorOut = details;
        reply->deleteLater();
        return false;
    }

    reply->deleteLater();
    if (errorOut) errorOut->clear();
    return true;
}

bool AutoKJServerAPI::endActiveShow(QString *errorOut)
{
    const QString token = ensureToken(errorOut);
    if (token.isEmpty())
        return false;

    const int venueId = m_settings.requestServerVenue();
    if (venueId <= 0) {
        if (errorOut) *errorOut = "No venue selected.";
        return false;
    }

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest listReq(QUrl(baseUrl + QString("/api/v1/kj/venues/%1/gigs").arg(venueId)));
    setAuthHeader(listReq, token);
    QNetworkReply *listReply = m_nam->get(listReq);
    QEventLoop listLoop;
    connect(listReply, &QNetworkReply::finished, &listLoop, &QEventLoop::quit);
    listLoop.exec();

    const QByteArray listBody = listReply->readAll();
    if (listReply->error() != QNetworkReply::NoError) {
        QString details = listReply->errorString();
        const QJsonDocument doc = QJsonDocument::fromJson(listBody);
        if (doc.isObject()) {
            const QString apiDetail = doc.object().value("detail").toString();
            if (!apiDetail.isEmpty())
                details = apiDetail;
        }
        if (errorOut) *errorOut = details;
        listReply->deleteLater();
        return false;
    }

    int activeGigId = -1;
    const QJsonDocument gigsDoc = QJsonDocument::fromJson(listBody);
    if (gigsDoc.isArray()) {
        for (const auto &entry : gigsDoc.array()) {
            const QJsonObject obj = entry.toObject();
            if (obj.value("is_active").toBool(false)) {
                activeGigId = obj.value("id").toInt(-1);
                break;
            }
        }
    }
    listReply->deleteLater();

    if (activeGigId <= 0) {
        if (errorOut) errorOut->clear();
        refreshVenues(true);
        return true;
    }

    QNetworkRequest endReq(QUrl(baseUrl + QString("/api/v1/kj/gigs/%1/end").arg(activeGigId)));
    setAuthHeader(endReq, token);
    endReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *endReply = m_nam->post(endReq, QJsonDocument(QJsonObject{}).toJson(QJsonDocument::Compact));
    QEventLoop endLoop;
    connect(endReply, &QNetworkReply::finished, &endLoop, &QEventLoop::quit);
    endLoop.exec();

    const QByteArray endBody = endReply->readAll();
    const bool ok = endReply->error() == QNetworkReply::NoError;
    if (!ok) {
        QString details = endReply->errorString();
        const QJsonDocument doc = QJsonDocument::fromJson(endBody);
        if (doc.isObject()) {
            const QString apiDetail = doc.object().value("detail").toString();
            if (!apiDetail.isEmpty())
                details = apiDetail;
        }
        if (errorOut) *errorOut = details;
        endReply->deleteLater();
        return false;
    }
    endReply->deleteLater();

    m_accepting = false;
    refreshVenues(true);
    if (errorOut) errorOut->clear();
    return true;
}

