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
#include <memory>

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
        tryLegacySongDbSyncAsync([this](bool ok, const QString &legacyError) {
            if (ok) {
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
    reconfigureFromSettings(true);
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
    if (!m_reconnectEnabled)
        return;
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

void AutoKJServerAPI::clearSessionState()
{
    const bool wasConnected = m_authenticated;
    m_authenticated = false;
    m_accepting = false;
    m_cancelUpdate = false;
    m_updateInProgress = false;
    m_testInProgress = false;
    m_songSyncTimer->stop();
    m_songSyncTimeoutTimer->stop();
    m_testTimer->stop();
    m_settings.setPremiumAntiChaosAuthorized(false);

    if (!m_requests.isEmpty()) {
        m_requests.clear();
        emit requestsChanged(m_requests);
    }

    if (wasConnected)
        emit connectionStatusChanged(false);
}

void AutoKJServerAPI::disconnectFromServer(bool clearSessionStateFlag)
{
    m_reconnectTimer->stop();
    if (clearSessionStateFlag)
        clearSessionState();

    if (m_socket->state() == QAbstractSocket::UnconnectedState)
        return;

    m_suppressReconnectOnce = true;
    m_reconfigureInProgress = true;
    m_socket->abort();
    m_reconfigureInProgress = false;
}

void AutoKJServerAPI::reconfigureFromSettings(bool force)
{
    const bool enabled = m_settings.requestServerEnabled();
    const QString serverUrl = m_settings.requestServerUrl().trimmed();
    const QString email = m_settings.requestServerEmail().trimmed();
    const QString password = m_settings.requestServerPassword();
    const QString token = m_settings.requestServerToken().trimmed();
    const int venueId = m_settings.requestServerVenue();

    if (!force && enabled == m_reconnectEnabled && serverUrl == m_lastServerUrl &&
        email == m_lastEmail && password == m_lastPassword && token == m_lastToken &&
        venueId == m_lastVenueId) {
        return;
    }

    const bool endpointChanged = force || serverUrl != m_lastServerUrl ||
        email != m_lastEmail || password != m_lastPassword || token != m_lastToken;
    const bool venueChanged = force || venueId != m_lastVenueId;

    m_reconnectEnabled = enabled;
    m_lastServerUrl = serverUrl;
    m_lastEmail = email;
    m_lastPassword = password;
    m_lastToken = token;
    m_lastVenueId = venueId;

    if (!enabled) {
        disconnectFromServer(true);
        return;
    }

    if (endpointChanged) {
        m_token.clear();
        disconnectFromServer(true);
    } else if (venueChanged) {
        clearSessionState();
    }

    refreshVenues();

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        authenticate();
    } else {
        connectToServer();
    }
}

void AutoKJServerAPI::onConnected()
{
    qDebug("[AutoKJ] WebSocket connected");
    m_reconnectTimer->stop();
    authenticate();
}

void AutoKJServerAPI::onDisconnected()
{
    clearSessionState();
    if (m_suppressReconnectOnce) {
        m_suppressReconnectOnce = false;
        return;
    }
    if (!m_reconnectEnabled || m_reconfigureInProgress)
        return;
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
    data["rotation_style"] = m_settings.rotationStyle();
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
        refreshVenues();
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
        // Commands from the KJ web tablet interface are handled by MainWindow.
        const QString action = data.value("action").toString();
        const QJsonObject payload = data.value("payload").toObject();
        if (!action.isEmpty())
            emit kjWebCommand(action, payload);
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

    const int venueId = m_settings.requestServerVenue();
    if (venueId <= 0) {
        qWarning("[AutoKJ] Cannot update accepting state: missing venue.");
        m_accepting = previous;
        emit acceptingSetFinished(false, enabled, "No venue selected.");
        return;
    }

    auto failAccepting = [this, previous, enabled](const QString &details) {
        qWarning("[AutoKJ] Failed to set accepting=%d: %s", enabled ? 1 : 0, qPrintable(details));
        m_accepting = previous;
        if (enabled && details.contains("Concurrent show limit reached", Qt::CaseInsensitive)) {
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
        emit acceptingSetFinished(false, enabled, details);
    };

    ensureTokenAsync(
        [this, enabled, offerEndShowPrompt, failAccepting](const QString &token) {
            if (token.isEmpty()) {
                failAccepting("Missing authentication.");
                return;
            }

            auto completeSuccess = [this, enabled, offerEndShowPrompt]() {
                refreshVenues();
                emit acceptingSetFinished(true, enabled, {});
                if (!enabled && offerEndShowPrompt) {
                    const auto btn = QMessageBox::question(
                        nullptr,
                        "End Show?",
                        "Requests are now turned off for this venue.\n\nDo you want to end the active show as well?",
                        QMessageBox::Yes | QMessageBox::No
                    );
                    if (btn == QMessageBox::Yes) {
                        QMetaObject::Connection *endConn = new QMetaObject::Connection;
                        *endConn = connect(this, &AutoKJServerClient::endActiveShowFinished, this,
                            [endConn](bool ok, const QString &endErr) {
                                disconnect(*endConn);
                                delete endConn;
                                if (!ok) {
                                    QMessageBox::warning(nullptr,
                                                         "End Show Failed",
                                                         endErr.isEmpty() ? "Could not end the active show." : endErr);
                                }
                            });
                        endActiveShow();
                    }
                }
            };

            patchAcceptingState(enabled, token,
                [this, enabled, token, completeSuccess, failAccepting](bool ok, const QString &details, const QString &responseBody) {
                    if (ok) {
                        QJsonObject data;
                        data["accepting"] = m_accepting;
                        sendEvent("kj:accepting", data);
                        completeSuccess();
                        return;
                    }

                    if (enabled && responseBody.contains("No active show found", Qt::CaseInsensitive)) {
                        QMetaObject::Connection *startConn = new QMetaObject::Connection;
                        *startConn = connect(this, &AutoKJServerClient::startNewShowFinished, this,
                            [this, startConn, enabled, token, completeSuccess, failAccepting](bool startOk, const QString &startErr) {
                                disconnect(*startConn);
                                delete startConn;
                                if (!startOk) {
                                    failAccepting(startErr);
                                    return;
                                }
                                patchAcceptingState(enabled, token,
                                    [this, completeSuccess, failAccepting](bool retryOk, const QString &retryDetails, const QString &) {
                                        if (retryOk) {
                                            QJsonObject data;
                                            data["accepting"] = m_accepting;
                                            sendEvent("kj:accepting", data);
                                            completeSuccess();
                                        } else {
                                            failAccepting(retryDetails);
                                        }
                                    });
                            });
                        startNewShow();
                        return;
                    }

                    failAccepting(details);
                });
        },
        [failAccepting](const QString &error) {
            failAccepting(error);
        }
    );
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
        tryLegacySongDbSyncAsync([this](bool ok, const QString &legacyError) {
            m_updateInProgress = false;
            if (ok) {
                emit remoteSongDbUpdateDone();
            } else {
                emit remoteSongDbUpdateFailed(
                    legacyError.isEmpty()
                        ? "Not connected to request server, and legacy song sync failed."
                        : "Not connected to request server. Legacy song sync failed: " + legacyError
                );
            }
        });
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

void AutoKJServerAPI::tryLegacySongDbSyncAsync(const std::function<void(bool, const QString &)> &callback)
{
    ensureTokenAsync(
        [this, callback](const QString &token) {
            if (token.isEmpty()) {
                callback(false, "Missing authentication.");
                return;
            }

            const QString baseUrl = normalizedBaseUrl();
            const int systemId = m_settings.systemId();

            auto sendLegacyClear = std::make_shared<std::function<void()>>();
            auto sendLegacyChunk = std::make_shared<std::function<void(int)>>();
            auto sendModernClear = std::make_shared<std::function<void()>>();
            auto sendModernChunk = std::make_shared<std::function<void(int)>>();

            *sendLegacyChunk = [this, callback, baseUrl, systemId, sendLegacyChunk](int index) {
                if (m_cancelUpdate) {
                    callback(false, "Sync canceled.");
                    return;
                }
                if (index >= m_songChunks.size()) {
                    callback(true, {});
                    return;
                }

                QJsonObject req{{"method", "songbookUpdate"}, {"systemID", systemId},
                                {"songbook", m_songChunks.at(index).object().value("songs").toArray()},
                                {"append", true}};
                QNetworkRequest request(QUrl(baseUrl + "/api"));
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QNetworkReply *reply = m_nam->post(request, QJsonDocument(req).toJson(QJsonDocument::Compact));
                connect(reply, &QNetworkReply::finished, this, [this, reply, callback, sendLegacyChunk, index]() {
                    const QByteArray body = reply->readAll();
                    const QJsonDocument doc = QJsonDocument::fromJson(body);
                    const QString details = errorFromReply(reply, body);
                    const bool ok = reply->error() == QNetworkReply::NoError && doc.isObject() && !doc.object().value("error").toBool(false);
                    reply->deleteLater();
                    if (!ok) {
                        callback(false, details.isEmpty() ? "Legacy API returned an error." : details);
                        return;
                    }
                    emit remoteSongDbUpdateProgress(index + 1);
                    (*sendLegacyChunk)(index + 1);
                });
            };

            *sendLegacyClear = [this, callback, baseUrl, systemId, sendLegacyChunk]() {
                QJsonObject req{{"method", "songbookUpdate"}, {"systemID", systemId}, {"songbook", QJsonArray()}, {"append", false}};
                QNetworkRequest request(QUrl(baseUrl + "/api"));
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QNetworkReply *reply = m_nam->post(request, QJsonDocument(req).toJson(QJsonDocument::Compact));
                connect(reply, &QNetworkReply::finished, this, [this, reply, callback, sendLegacyChunk]() {
                    const QByteArray body = reply->readAll();
                    const QJsonDocument doc = QJsonDocument::fromJson(body);
                    const QString details = errorFromReply(reply, body);
                    const bool ok = reply->error() == QNetworkReply::NoError && doc.isObject() && !doc.object().value("error").toBool(false);
                    reply->deleteLater();
                    if (!ok) {
                        callback(false, details.isEmpty() ? "Legacy API returned an error." : details);
                        return;
                    }
                    (*sendLegacyChunk)(0);
                });
            };

            *sendModernChunk = [this, callback, baseUrl, token, sendLegacyClear, sendModernChunk](int index) {
                if (m_cancelUpdate) {
                    callback(false, "Sync canceled.");
                    return;
                }
                if (index >= m_songChunks.size()) {
                    callback(true, {});
                    return;
                }

                QJsonObject req{{"mode", "add"}, {"songs", m_songChunks.at(index).object().value("songs").toArray()}};
                QNetworkRequest request(QUrl(baseUrl + "/api/v1/desktop/kj/songs/sync"));
                setAuthHeader(request, token);
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QNetworkReply *reply = m_nam->post(request, QJsonDocument(req).toJson(QJsonDocument::Compact));
                connect(reply, &QNetworkReply::finished, this, [this, reply, callback, sendLegacyClear, sendModernChunk, index]() {
                    const QByteArray body = reply->readAll();
                    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    const QString details = errorFromReply(reply, body);
                    const bool ok = reply->error() == QNetworkReply::NoError;
                    reply->deleteLater();
                    if (!ok) {
                        if (statusCode == 404) {
                            (*sendLegacyClear)();
                            return;
                        }
                        callback(false, details);
                        return;
                    }
                    emit remoteSongDbUpdateProgress(index + 1);
                    (*sendModernChunk)(index + 1);
                });
            };

            *sendModernClear = [this, callback, baseUrl, token, sendLegacyClear, sendModernChunk]() {
                QNetworkRequest request(QUrl(baseUrl + "/api/v1/desktop/kj/songs/sync"));
                setAuthHeader(request, token);
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QNetworkReply *reply = m_nam->post(request, QJsonDocument(QJsonObject{{"mode", "clear"}}).toJson(QJsonDocument::Compact));
                connect(reply, &QNetworkReply::finished, this, [this, reply, callback, sendLegacyClear, sendModernChunk]() {
                    const QByteArray body = reply->readAll();
                    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    const QString details = errorFromReply(reply, body);
                    const bool ok = reply->error() == QNetworkReply::NoError;
                    reply->deleteLater();
                    if (!ok) {
                        if (statusCode == 404) {
                            (*sendLegacyClear)();
                            return;
                        }
                        callback(false, details);
                        return;
                    }
                    (*sendModernChunk)(0);
                });
            };

            (*sendModernClear)();
        },
        [callback](const QString &error) {
            callback(false, error);
        }
    );
}

void AutoKJServerAPI::refreshVenues()
{
    ensureTokenAsync(
        [this](const QString &token) {
            if (token.isEmpty()) {
                emit venuesRefreshFailed("Missing authentication.");
                return;
            }
            QNetworkRequest request(QUrl(normalizedBaseUrl() + "/api/v1/desktop/kj/venues"));
            setAuthHeader(request, token);
            QNetworkReply *reply = m_nam->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                onVenuesReply(reply);
            });
        },
        [this](const QString &error) {
            emit venuesRefreshFailed(error);
        }
    );
}
void AutoKJServerAPI::onVenuesReply(QNetworkReply *reply)
{
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        const QString details = errorFromReply(reply, body);
        qWarning("[AutoKJ] Venues fetch failed: %s", qPrintable(details));
        emit venuesRefreshFailed(details);
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        emit venuesRefreshFailed("Invalid response from /api/v1/desktop/kj/venues.");
        return;
    }

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

    testHttpAuthAsync([this](bool ok, const QString &httpErr, bool mayBeLegacyServer) {
        if (ok) {
            m_testInProgress = false;
            m_testTimer->stop();
            emit testPassed();
            return;
        }

        if (!httpErr.isEmpty() && !mayBeLegacyServer) {
            m_testInProgress = false;
            m_testTimer->stop();
            emit testFailed(httpErr);
            return;
        }

        const QString baseUrl = m_settings.requestServerUrl();
        if ((baseUrl.startsWith("https://") || baseUrl.startsWith("wss://")) && !QSslSocket::supportsSsl()) {
            m_testInProgress = false;
            m_testTimer->stop();
            emit testFailed("SSL sockets are not supported on this platform. Install the required OpenSSL runtime and restart Auto-KJ.");
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
    });
}

QString AutoKJServerAPI::normalizedBaseUrl() const
{
    QString baseUrl = m_settings.requestServerUrl().trimmed();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (baseUrl.startsWith("wss://"))
        baseUrl.replace(0, 6, "https://");
    else if (baseUrl.startsWith("ws://"))
        baseUrl.replace(0, 5, "http://");
    else if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;
    return baseUrl;
}

QString AutoKJServerAPI::errorFromReply(QNetworkReply *reply, const QByteArray &body) const
{
    QString details = reply->errorString();
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString detail = obj.value("detail").toString();
        if (!detail.isEmpty())
            details = detail;
        const QString errorString = obj.value("errorString").toString();
        if (!errorString.isEmpty())
            details = errorString;
    }
    return details;
}

void AutoKJServerAPI::loginAsync(const std::function<void(bool, const QString &)> &callback)
{
    const QString email = m_settings.requestServerEmail();
    const QString password = m_settings.requestServerPassword();
    if (email.isEmpty() || password.isEmpty()) {
        callback(false, "Email and password are required.");
        return;
    }

    QNetworkRequest request(QUrl(normalizedBaseUrl() + "/api/v1/auth/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["email"] = email;
    body["password"] = password;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        const QByteArray responseBody = reply->readAll();
        const QString details = errorFromReply(reply, responseBody);
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();

        if (!ok) {
            callback(false, details);
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
        if (!doc.isObject()) {
            callback(false, "Invalid login response from server.");
            return;
        }

        const QString token = doc.object().value("access_token").toString();
        if (token.isEmpty()) {
            callback(false, details.isEmpty() ? "Login failed." : details);
            return;
        }

        m_token = token;
        m_settings.setRequestServerToken(token);

        // Auto-save API key from login response (server auto-generates one if missing)
        // api_key from login response is deprecated; JWT-only auth

        const QString subscriptionTier = doc.object().value("subscription_tier").toString();
        if (!subscriptionTier.isEmpty())
            m_settings.setRequestServerSubscriptionTier(subscriptionTier);

        callback(true, {});
    });
}

void AutoKJServerAPI::ensureTokenAsync(const std::function<void(const QString &)> &onSuccess,
                                       const std::function<void(const QString &)> &onFailure)
{
    if (!m_token.isEmpty()) {
        onSuccess(m_token);
        return;
    }

    const QString stored = m_settings.requestServerToken();
    if (!stored.isEmpty()) {
        m_token = stored;
        onSuccess(m_token);
        return;
    }

    if (m_settings.requestServerEmail().trimmed().isEmpty() || m_settings.requestServerPassword().trimmed().isEmpty()) {
        if (onFailure)
            onFailure("Email and password are required.");
        return;
    }

    loginAsync([this, onSuccess, onFailure](bool ok, const QString &error) {
        if (ok) {
            onSuccess(m_token);
            return;
        }
        if (onFailure)
            onFailure(error);
    });
}

void AutoKJServerAPI::setAuthHeader(QNetworkRequest &request, const QString &token)
{
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
}

bool AutoKJServerAPI::login(QString *errorOut)
{
    const QString email    = m_settings.requestServerEmail();
    const QString password = m_settings.requestServerPassword();
    if (email.isEmpty() || password.isEmpty()) {
        if (errorOut) *errorOut = "Email and password are required.";
        return false;
    }

    QString baseUrl = normalizedBaseUrl();

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
    const QString details = errorFromReply(reply, responseBody);
    const bool ok = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();

    if (!ok) {
        if (errorOut) *errorOut = details;
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = "Invalid login response from server.";
        return false;
    }

    const QString token = doc.object().value("access_token").toString();
    if (token.isEmpty()) {
        if (errorOut) *errorOut = details.isEmpty() ? "Login failed." : details;
        return false;
    }

    m_token = token;
    m_settings.setRequestServerToken(token);

    // api_key from login response is deprecated; JWT-only auth

    const QString subscriptionTier = doc.object().value("subscription_tier").toString();
    if (!subscriptionTier.isEmpty())
        m_settings.setRequestServerSubscriptionTier(subscriptionTier);

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

bool AutoKJServerAPI::changePassword(const QString &currentPassword, const QString &newPassword, QString *errorOut)
{
    QString token = ensureToken(errorOut);
    if (token.isEmpty())
        return false;

    QString baseUrl = normalizedBaseUrl();

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/dashboard/kj/change-password"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    setAuthHeader(request, token);

    QJsonObject body;
    body["current_password"] = currentPassword;
    body["new_password"] = newPassword;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();
    reply->deleteLater();

    if (status == 200) {
        if (errorOut) errorOut->clear();
        return true;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    QString details = reply->errorString();
    if (doc.isObject()) {
        const QString d = doc.object().value("detail").toString();
        if (!d.isEmpty()) details = d;
    }
    if (errorOut) *errorOut = details;
    return false;
}

void AutoKJServerAPI::testHttpAuthAsync(const std::function<void(bool, const QString &, bool)> &callback)
{
    const QString baseUrl = normalizedBaseUrl();
    if (baseUrl.startsWith("https://") && !QSslSocket::supportsSsl()) {
        callback(false, "TLS is not available. Install the required OpenSSL runtime and restart Auto-KJ.", false);
        return;
    }

    ensureTokenAsync(
        [this, baseUrl, callback](const QString &token) {
            if (token.isEmpty()) {
                callback(false, "Missing authentication.", false);
                return;
            }

            QNetworkRequest request(QUrl(baseUrl + "/api/v1/auth/me"));
            setAuthHeader(request, token);
            QNetworkReply *reply = m_nam->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, callback, token, baseUrl]() {
                const QByteArray body = reply->readAll();
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QString details = errorFromReply(reply, body);
                const bool ok = reply->error() == QNetworkReply::NoError;
                reply->deleteLater();

                if (status == 401) {
                    // Token expired or invalid — try re-login
                    m_token.clear();
                    m_settings.setRequestServerToken({});
                    loginAsync([this, baseUrl, callback](bool loginOk, const QString &error) {
                        if (!loginOk) {
                            callback(false, error, false);
                            return;
                        }
                        // Re-validate with fresh token
                        QNetworkRequest req2(QUrl(baseUrl + "/api/v1/auth/me"));
                        setAuthHeader(req2, m_token);
                        QNetworkReply *reply2 = m_nam->get(req2);
                        connect(reply2, &QNetworkReply::finished, this, [this, reply2, callback]() {
                            const QByteArray body2 = reply2->readAll();
                            const QString details2 = errorFromReply(reply2, body2);
                            const bool ok2 = reply2->error() == QNetworkReply::NoError;
                            reply2->deleteLater();
                            if (!ok2) {
                                callback(false, details2.isEmpty() ? "Authentication failed." : details2, false);
                                return;
                            }
                            callback(true, {}, false);
                        });
                    });
                    return;
                }

                if (!ok) {
                    callback(false, details.isEmpty() ? "Auth validation failed." : details, false);
                    return;
                }

                callback(true, {}, false);
            });
        },
        [callback](const QString &error) {
            callback(false, error, false);
        }
    );
}

void AutoKJServerAPI::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    QString errStr = m_socket->errorString();
    qWarning("[AutoKJ] Socket error: %s", qPrintable(errStr));

    if (m_testInProgress) {
        m_testInProgress = false;
        m_testTimer->stop();
        emit testFailed(errStr);
    }
}

QString AutoKJServerAPI::venueUrl() const
{
    QString base = m_settings.requestServerUrl();
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

void AutoKJServerAPI::createVenue(const QString &name, const QString &address)
{
    ensureTokenAsync(
        [this, name, address](const QString &token) {
            if (token.isEmpty()) {
                emit testFailed("Venue creation failed: Missing authentication.");
                return;
            }

            QNetworkRequest request(QUrl(normalizedBaseUrl() + "/api/v1/desktop/kj/venues"));
            setAuthHeader(request, token);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            QJsonObject obj;
            obj["name"] = name;
            obj["address"] = address;

            QNetworkReply *reply = m_nam->post(request, QJsonDocument(obj).toJson());
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                const QByteArray body = reply->readAll();
                const QString details = errorFromReply(reply, body);
                const bool ok = reply->error() == QNetworkReply::NoError;
                reply->deleteLater();
                if (ok) {
                    refreshVenues();
                } else {
                    qWarning("[AutoKJ] Venue creation failed: %s", qPrintable(details));
                    emit testFailed("Venue creation failed: " + details);
                }
            });
        },
        [this](const QString &error) {
            emit testFailed("Venue creation failed: " + error);
        }
    );
}

void AutoKJServerAPI::startNewShow()
{
    ensureTokenAsync(
        [this](const QString &token) {
            if (token.isEmpty()) {
                emit startNewShowFinished(false, "Missing authentication.");
                return;
            }

            const int venueId = m_settings.requestServerVenue();
            if (venueId <= 0) {
                emit startNewShowFinished(false, "No venue selected.");
                return;
            }

            QNetworkRequest request(QUrl(normalizedBaseUrl() + QString("/api/v1/desktop/kj/venues/%1/gigs/start").arg(venueId)));
            setAuthHeader(request, token);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QNetworkReply *reply = m_nam->post(request, QJsonDocument(QJsonObject{}).toJson(QJsonDocument::Compact));
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                const QByteArray body = reply->readAll();
                const QString details = errorFromReply(reply, body);
                const bool ok = reply->error() == QNetworkReply::NoError;
                reply->deleteLater();
                emit startNewShowFinished(ok, ok ? QString() : details);
            });
        },
        [this](const QString &error) {
            emit startNewShowFinished(false, error);
        }
    );
}

void AutoKJServerAPI::endActiveShow()
{
    ensureTokenAsync(
        [this](const QString &token) {
            if (token.isEmpty()) {
                emit endActiveShowFinished(false, "Missing authentication.");
                return;
            }

            const int venueId = m_settings.requestServerVenue();
            if (venueId <= 0) {
                emit endActiveShowFinished(false, "No venue selected.");
                return;
            }

            QNetworkRequest listReq(QUrl(normalizedBaseUrl() + QString("/api/v1/desktop/kj/venues/%1/gigs").arg(venueId)));
            setAuthHeader(listReq, token);
            QNetworkReply *listReply = m_nam->get(listReq);
            connect(listReply, &QNetworkReply::finished, this, [this, listReply, token]() {
                const QByteArray listBody = listReply->readAll();
                const QString listDetails = errorFromReply(listReply, listBody);
                const bool listOk = listReply->error() == QNetworkReply::NoError;
                if (!listOk) {
                    listReply->deleteLater();
                    emit endActiveShowFinished(false, listDetails);
                    return;
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
                    m_accepting = false;
                    refreshVenues();
                    emit endActiveShowFinished(true, {});
                    return;
                }

                QNetworkRequest endReq(QUrl(normalizedBaseUrl() + QString("/api/v1/desktop/kj/gigs/%1/end").arg(activeGigId)));
                setAuthHeader(endReq, token);
                endReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QNetworkReply *endReply = m_nam->post(endReq, QJsonDocument(QJsonObject{}).toJson(QJsonDocument::Compact));
                connect(endReply, &QNetworkReply::finished, this, [this, endReply]() {
                    const QByteArray endBody = endReply->readAll();
                    const QString endDetails = errorFromReply(endReply, endBody);
                    const bool ok = endReply->error() == QNetworkReply::NoError;
                    endReply->deleteLater();
                    if (ok) {
                        m_accepting = false;
                        refreshVenues();
                    }
                    emit endActiveShowFinished(ok, ok ? QString() : endDetails);
                });
            });
        },
        [this](const QString &error) {
            emit endActiveShowFinished(false, error);
        }
    );
}

void AutoKJServerAPI::patchAcceptingState(bool enabled, const QString &token,
                                          const std::function<void(bool, const QString &, const QString &)> &callback)
{
    const int venueId = m_settings.requestServerVenue();
    if (venueId <= 0) {
        callback(false, "No venue selected.", {});
        return;
    }

    QNetworkRequest request(QUrl(normalizedBaseUrl() + QString("/api/v1/desktop/kj/venues/%1").arg(venueId)));
    setAuthHeader(request, token);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["accepting"] = enabled;
    QNetworkReply *reply = m_nam->sendCustomRequest(
        request,
        "PATCH",
        QJsonDocument(payload).toJson(QJsonDocument::Compact)
    );
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        const QByteArray body = reply->readAll();
        const QString details = errorFromReply(reply, body);
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        callback(ok, details, QString::fromUtf8(body));
    });
}
