#include "spotifycontroller.h"

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpSocket>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSettings>

// ── PKCE helpers ─────────────────────────────────────────────────────────────

static QByteArray randomBase64Url(int bytes)
{
    QByteArray buf(bytes, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(buf.data()), (bytes + 3) / 4);
    return buf.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

static QByteArray sha256Base64Url(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

// ── Constructor ───────────────────────────────────────────────────────────────

SpotifyController::SpotifyController(QObject *parent)
    : StreamingController(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    // Attempt to restore saved tokens
    QSettings s;
    m_accessToken  = s.value("streaming/spotify_access_token").toString();
    m_refreshToken = s.value("streaming/spotify_refresh_token").toString();
    m_clientId     = s.value("streaming/spotify_client_id").toString();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &SpotifyController::onPollTimer);

    m_refreshTimer = new QTimer(this);
    // Refresh token 5 minutes before typical 1-hour expiry
    m_refreshTimer->setInterval(55 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &SpotifyController::refreshAccessToken);

    if (!m_accessToken.isEmpty() && !m_refreshToken.isEmpty()) {
        // We have a stored session — assume it needs a refresh to be safe
        refreshAccessToken();
    }
}

void SpotifyController::setClientId(const QString &clientId)
{
    m_clientId = clientId;
    QSettings().setValue("streaming/spotify_client_id", clientId);
}

// ── Auth ──────────────────────────────────────────────────────────────────────

void SpotifyController::connectService()
{
    if (m_clientId.isEmpty()) {
        emit errorOccurred(tr("Spotify Client ID not set. Enter it in Streaming Settings."));
        return;
    }
    startOAuthFlow();
}

void SpotifyController::disconnectService()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_pollTimer->stop();
    m_refreshTimer->stop();
    QSettings s;
    s.remove("streaming/spotify_access_token");
    s.remove("streaming/spotify_refresh_token");
    emit disconnected({});
}

void SpotifyController::startOAuthFlow()
{
    m_codeVerifier = randomBase64Url(32);
    QByteArray challenge = sha256Base64Url(m_codeVerifier.toUtf8());

    // Start local callback server
    if (!m_callbackServer) {
        m_callbackServer = new QTcpServer(this);
        connect(m_callbackServer, &QTcpServer::newConnection,
                this, &SpotifyController::onOAuthCallback);
    }
    if (!m_callbackServer->isListening()) {
        if (!m_callbackServer->listen(QHostAddress::LocalHost, kCallbackPort)) {
            emit errorOccurred(tr("Cannot open OAuth callback port %1.").arg(kCallbackPort));
            return;
        }
    }

    const QString scopes = "user-read-playback-state "
                           "user-modify-playback-state "
                           "playlist-read-private "
                           "playlist-read-collaborative";

    QUrlQuery q;
    q.addQueryItem("client_id",             m_clientId);
    q.addQueryItem("response_type",         "code");
    q.addQueryItem("redirect_uri",          QString("http://localhost:%1/callback").arg(kCallbackPort));
    q.addQueryItem("scope",                 scopes);
    q.addQueryItem("code_challenge_method", "S256");
    q.addQueryItem("code_challenge",        QString::fromUtf8(challenge));

    QUrl url("https://accounts.spotify.com/authorize");
    url.setQuery(q);
    QDesktopServices::openUrl(url);
}

void SpotifyController::onOAuthCallback()
{
    QTcpSocket *sock = m_callbackServer->nextPendingConnection();
    if (!sock) return;
    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        const QString request = QString::fromUtf8(sock->readAll());
        // Parse "GET /callback?code=XXX&... HTTP/1.1"
        static const QRegularExpression re(R"(GET /callback\?(\S+) HTTP)");
        auto m = re.match(request);
        if (m.hasMatch()) {
            QUrlQuery q(m.captured(1));
            const QString code  = q.queryItemValue("code");
            const QString error = q.queryItemValue("error");

            // Send a minimal HTTP response so the browser shows something
            const QByteArray body = "<html><body><h2>AutoKJ — Spotify connected!</h2>"
                                    "<p>You can close this tab.</p></body></html>";
            sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                        "Connection: close\r\n\r\n" + body);
            sock->flush();
            sock->disconnectFromHost();

            if (!code.isEmpty())
                exchangeCodeForToken(code);
            else
                emit errorOccurred(tr("Spotify auth failed: %1").arg(error));
        }
        sock->deleteLater();
        m_callbackServer->close();
    });
}

void SpotifyController::exchangeCodeForToken(const QString &code)
{
    QNetworkRequest req(QUrl("https://accounts.spotify.com/api/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("grant_type",    "authorization_code");
    body.addQueryItem("code",          code);
    body.addQueryItem("redirect_uri",  QString("http://localhost:%1/callback").arg(kCallbackPort));
    body.addQueryItem("client_id",     m_clientId);
    body.addQueryItem("code_verifier", m_codeVerifier);

    QNetworkReply *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        onTokenReply(reply);
    });
}

void SpotifyController::refreshAccessToken()
{
    if (m_refreshToken.isEmpty() || m_clientId.isEmpty()) return;

    QNetworkRequest req(QUrl("https://accounts.spotify.com/api/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("grant_type",    "refresh_token");
    body.addQueryItem("refresh_token", m_refreshToken);
    body.addQueryItem("client_id",     m_clientId);

    QNetworkReply *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        onTokenReply(reply);
    });
}

void SpotifyController::onTokenReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Spotify token error: %1").arg(reply->errorString()));
        return;
    }
    const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    m_accessToken = json["access_token"].toString();
    const QString newRefresh = json["refresh_token"].toString();
    if (!newRefresh.isEmpty())
        m_refreshToken = newRefresh;

    QSettings s;
    s.setValue("streaming/spotify_access_token",  m_accessToken);
    s.setValue("streaming/spotify_refresh_token", m_refreshToken);

    m_pollTimer->start();
    m_refreshTimer->start();
    emit connected();
    loadPlaylists();
    loadActiveDevice();
}

// ── API helpers ───────────────────────────────────────────────────────────────

void SpotifyController::sendPlayerCommand(const QString &endpoint,
                                          const QByteArray &body,
                                          const QByteArray &method)
{
    if (m_accessToken.isEmpty()) return;
    QNetworkRequest req(QUrl("https://api.spotify.com/v1" + endpoint));
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    if (!body.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->sendCustomRequest(req, method, body);
    connect(reply, &QNetworkReply::finished, this, [reply]() { reply->deleteLater(); });
}

void SpotifyController::loadActiveDevice()
{
    if (m_accessToken.isEmpty()) return;
    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/player/devices"));
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonArray devices = QJsonDocument::fromJson(reply->readAll())
                                       .object()["devices"].toArray();
        for (const auto &d : devices) {
            QJsonObject obj = d.toObject();
            if (obj["is_active"].toBool()) {
                m_activeDeviceId = obj["id"].toString();
                break;
            }
        }
        // Fallback: pick the first available device
        if (m_activeDeviceId.isEmpty() && !devices.isEmpty())
            m_activeDeviceId = devices.first().toObject()["id"].toString();

        if (!m_pendingPlaylistUri.isEmpty()) {
            playPlaylist({m_pendingPlaylistUri, {}, {}, 0, {}});
            m_pendingPlaylistUri.clear();
        }
        if (!m_pendingTrackUri.isEmpty()) {
            playTrack({m_pendingTrackUri, {}, {}, {}, {}});
            m_pendingTrackUri.clear();
        }
    });
}

// ── Browsing ──────────────────────────────────────────────────────────────────

void SpotifyController::loadPlaylists()
{
    if (m_accessToken.isEmpty()) return;
    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/playlists?limit=50"));
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("Failed to load Spotify playlists: %1")
                                   .arg(reply->errorString()));
            return;
        }
        QList<StreamPlaylist> playlists;
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll())
                                     .object()["items"].toArray();
        for (const auto &item : items) {
            QJsonObject obj = item.toObject();
            StreamPlaylist pl;
            pl.id          = obj["id"].toString();
            pl.name        = obj["name"].toString();
            pl.description = obj["description"].toString();
            pl.trackCount  = obj["tracks"].toObject()["total"].toInt();
            QJsonArray images = obj["images"].toArray();
            if (!images.isEmpty())
                pl.imageUrl = images.first().toObject()["url"].toString();
            playlists.append(pl);
        }
        emit playlistsLoaded(playlists);
    });
}

void SpotifyController::loadTracks(const StreamPlaylist &playlist)
{
    if (m_accessToken.isEmpty() || playlist.id.isEmpty()) return;
    const QUrl url = QUrl(QString("https://api.spotify.com/v1/playlists/%1/tracks?limit=100")
                              .arg(playlist.id));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, playlist]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("Failed to load Spotify tracks: %1")
                                   .arg(reply->errorString()));
            return;
        }
        QList<StreamTrack> tracks;
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll())
                                     .object()["items"].toArray();
        for (const auto &item : items) {
            QJsonObject track = item.toObject()["track"].toObject();
            if (track.isEmpty()) continue;
            StreamTrack t;
            t.id           = track["id"].toString();
            t.title        = track["name"].toString();
            t.playUrl      = track["uri"].toString();           // spotify:track:XXX
            t.durationSecs = track["duration_ms"].toInt() / 1000;
            QJsonArray artists = track["artists"].toArray();
            QStringList names;
            for (const auto &a : artists)
                names << a.toObject()["name"].toString();
            t.artist = names.join(", ");
            t.album  = track["album"].toObject()["name"].toString();
            tracks.append(t);
        }
        emit tracksLoaded(playlist.id, tracks);
    });
}

// ── Playback ──────────────────────────────────────────────────────────────────

void SpotifyController::playPlaylist(const StreamPlaylist &playlist)
{
    if (m_activeDeviceId.isEmpty()) {
        m_pendingPlaylistUri = playlist.id;
        loadActiveDevice();
        return;
    }
    QJsonObject body;
    // Spotify playlist URI format: spotify:playlist:<id>
    const QString uri = playlist.id.startsWith("spotify:")
                            ? playlist.id
                            : "spotify:playlist:" + playlist.id;
    body["context_uri"] = uri;
    if (!m_activeDeviceId.isEmpty())
        body["device_id"] = m_activeDeviceId;
    sendPlayerCommand("/me/player/play",
                      QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void SpotifyController::playTrack(const StreamTrack &track)
{
    if (m_activeDeviceId.isEmpty()) {
        m_pendingTrackUri = track.playUrl;
        loadActiveDevice();
        return;
    }
    QJsonObject body;
    body["uris"] = QJsonArray{track.playUrl.startsWith("spotify:")
                                  ? track.playUrl
                                  : "spotify:track:" + track.id};
    if (!m_activeDeviceId.isEmpty())
        body["device_id"] = m_activeDeviceId;
    sendPlayerCommand("/me/player/play",
                      QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void SpotifyController::pause()   { sendPlayerCommand("/me/player/pause", {}, "PUT"); }
void SpotifyController::resume()  { sendPlayerCommand("/me/player/play",  {}, "PUT"); }
void SpotifyController::next()    { sendPlayerCommand("/me/player/next",  {}, "POST"); }
void SpotifyController::previous(){ sendPlayerCommand("/me/player/previous", {}, "POST"); }

void SpotifyController::setVolume(int percent)
{
    sendPlayerCommand(QString("/me/player/volume?volume_percent=%1").arg(percent),
                      {}, "PUT");
}

// ── Now-playing polling ───────────────────────────────────────────────────────

void SpotifyController::onPollTimer()
{
    if (m_accessToken.isEmpty()) return;
    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/player/currently-playing"));
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QByteArray data = reply->readAll();
        if (data.isEmpty()) return;
        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject track = root["item"].toObject();
        if (track.isEmpty()) return;

        StreamTrack t;
        t.id           = track["id"].toString();
        t.title        = track["name"].toString();
        t.playUrl      = track["uri"].toString();
        t.durationSecs = track["duration_ms"].toInt() / 1000;
        QJsonArray artists = track["artists"].toArray();
        QStringList names;
        for (const auto &a : artists)
            names << a.toObject()["name"].toString();
        t.artist = names.join(", ");
        t.album  = track["album"].toObject()["name"].toString();

        emit nowPlayingChanged(t);
        emit playbackStateChanged(root["is_playing"].toBool());
    });
}

void SpotifyController::onRefreshTimer()
{
    refreshAccessToken();
}

void SpotifyController::onApiReply(QNetworkReply *) {}
