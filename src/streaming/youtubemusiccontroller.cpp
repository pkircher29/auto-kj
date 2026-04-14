#include "youtubemusiccontroller.h"

#include <QDesktopServices>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QSettings>
#include <QUrlQuery>
#include <QTcpSocket>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>

#if defined(Q_OS_WIN)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#if defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
#  include <QDBusInterface>
#include <QDBusReply>
#endif

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

YouTubeMusicController::YouTubeMusicController(QObject *parent)
    : StreamingController(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    QSettings s;
    m_accessToken  = s.value("streaming/youtube_access_token").toString();
    m_refreshToken = s.value("streaming/youtube_refresh_token").toString();
    m_clientId     = s.value("streaming/youtube_client_id").toString();
    m_apiKey       = s.value("streaming/youtube_api_key").toString();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &YouTubeMusicController::onPollTimer);

    m_refreshTimer = new QTimer(this);
    // Google tokens usually last 1 hour; refresh at 55 min
    m_refreshTimer->setInterval(55 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &YouTubeMusicController::onRefreshTimer);

    if (!m_accessToken.isEmpty() && !m_refreshToken.isEmpty()) {
        refreshAccessToken();
    }
}

void YouTubeMusicController::setApiKey(const QString &key)
{
    m_apiKey = key;
    QSettings().setValue("streaming/youtube_api_key", key);
}

void YouTubeMusicController::setClientId(const QString &clientId)
{
    m_clientId = clientId;
    QSettings().setValue("streaming/youtube_client_id", clientId);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void YouTubeMusicController::connectService()
{
    if (m_clientId.isEmpty()) {
        emit errorOccurred(tr("Google Client ID not set. Enter it in Streaming Settings."));
        return;
    }
    startOAuthFlow();
}

void YouTubeMusicController::disconnectService()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_pollTimer->stop();
    m_refreshTimer->stop();
    QSettings s;
    s.remove("streaming/youtube_access_token");
    s.remove("streaming/youtube_refresh_token");
    emit disconnected({});
}

void YouTubeMusicController::startOAuthFlow()
{
    m_codeVerifier = randomBase64Url(32);
    QByteArray challenge = sha256Base64Url(m_codeVerifier.toUtf8());

    if (!m_callbackServer) {
        m_callbackServer = new QTcpServer(this);
        connect(m_callbackServer, &QTcpServer::newConnection,
                this, &YouTubeMusicController::onOAuthCallback);
    }
    if (!m_callbackServer->isListening()) {
        if (!m_callbackServer->listen(QHostAddress::LocalHost, kCallbackPort)) {
            emit errorOccurred(tr("Cannot open OAuth callback port %1.").arg(kCallbackPort));
            return;
        }
    }

    const QString scopes = "https://www.googleapis.com/auth/youtube.readonly";

    QUrlQuery q;
    q.addQueryItem("client_id",             m_clientId);
    q.addQueryItem("response_type",         "code");
    q.addQueryItem("redirect_uri",          QString("http://localhost:%1/callback").arg(kCallbackPort));
    q.addQueryItem("scope",                 scopes);
    q.addQueryItem("code_challenge_method", "S256");
    q.addQueryItem("code_challenge",        QString::fromUtf8(challenge));

    QUrl url("https://accounts.google.com/o/oauth2/v2/auth");
    url.setQuery(q);
    openUrl(url);
}

void YouTubeMusicController::onOAuthCallback()
{
    QTcpSocket *sock = m_callbackServer->nextPendingConnection();
    if (!sock) return;
    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        const QString request = QString::fromUtf8(sock->readAll());
        static const QRegularExpression re(R"(GET /callback\?(\S+) HTTP)");
        auto m = re.match(request);
        if (m.hasMatch()) {
            QUrlQuery q(m.captured(1));
            const QString code  = q.queryItemValue("code");
            const QString error = q.queryItemValue("error");

            const QByteArray body = "<html><body><h2>AutoKJ — YouTube Music connected!</h2>"
                                    "<p>You can close this tab.</p></body></html>";
            sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                        "Connection: close\r\n\r\n" + body);
            sock->flush();
            sock->disconnectFromHost();

            if (!code.isEmpty())
                exchangeCodeForToken(code);
            else
                emit errorOccurred(tr("YouTube auth failed: %1").arg(error));
        }
        sock->deleteLater();
        m_callbackServer->close();
    });
}

void YouTubeMusicController::exchangeCodeForToken(const QString &code)
{
    QNetworkRequest req(QUrl("https://oauth2.googleapis.com/token"));
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

void YouTubeMusicController::refreshAccessToken()
{
    if (m_refreshToken.isEmpty() || m_clientId.isEmpty()) return;

    QNetworkRequest req(QUrl("https://oauth2.googleapis.com/token"));
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

void YouTubeMusicController::onTokenReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("YouTube token error: %1").arg(reply->errorString()));
        return;
    }
    const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    m_accessToken = json["access_token"].toString();
    const QString newRefresh = json["refresh_token"].toString();
    if (!newRefresh.isEmpty())
        m_refreshToken = newRefresh;

    QSettings s;
    s.setValue("streaming/youtube_access_token",  m_accessToken);
    s.setValue("streaming/youtube_refresh_token", m_refreshToken);

    m_pollTimer->start();
    m_refreshTimer->start();
    emit connected();
    loadPlaylists();
}

void YouTubeMusicController::onRefreshTimer()
{
    refreshAccessToken();
}

// ── Browsing ──────────────────────────────────────────────────────────────────

void YouTubeMusicController::loadPlaylists()
{
    // YouTube Music's API (YouTube Data API v3) lists user playlists.
    // Requires the user's YouTube API key and they must be signed in via the
    // browser opened in connectService().
    if (m_accessToken.isEmpty()) return;

    QUrl url("https://www.googleapis.com/youtube/v3/playlists");
    QUrlQuery q;
    q.addQueryItem("part",       "snippet,contentDetails");
    q.addQueryItem("mine",       "true");
    q.addQueryItem("maxResults", "50");
    if (!m_apiKey.isEmpty())
        q.addQueryItem("key",     m_apiKey); // Still helpful for quota but not required with Bearer
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("YouTube playlist fetch failed: %1")
                                   .arg(reply->errorString()));
            return;
        }
        QList<StreamPlaylist> playlists;
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll())
                                     .object()["items"].toArray();
        for (const auto &item : items) {
            QJsonObject obj = item.toObject();
            StreamPlaylist pl;
            pl.id         = obj["id"].toString();
            pl.name       = obj["snippet"].toObject()["title"].toString();
            pl.description= obj["snippet"].toObject()["description"].toString();
            pl.trackCount = obj["contentDetails"].toObject()["itemCount"].toInt();
            const QJsonObject thumbs = obj["snippet"].toObject()["thumbnails"].toObject();
            pl.imageUrl = thumbs.contains("medium")
                              ? thumbs["medium"].toObject()["url"].toString()
                              : thumbs["default"].toObject()["url"].toString();
            playlists.append(pl);
        }
        emit playlistsLoaded(playlists);
    });
}

void YouTubeMusicController::loadTracks(const StreamPlaylist &playlist)
{
    if (m_accessToken.isEmpty() || playlist.id.isEmpty()) return;

    QUrl url("https://www.googleapis.com/youtube/v3/playlistItems");
    QUrlQuery q;
    q.addQueryItem("part",       "snippet");
    q.addQueryItem("playlistId", playlist.id);
    q.addQueryItem("maxResults", "50");
    if (!m_apiKey.isEmpty())
        q.addQueryItem("key",     m_apiKey);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, playlist]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("YouTube track fetch failed: %1")
                                   .arg(reply->errorString()));
            return;
        }
        QList<StreamTrack> tracks;
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll())
                                     .object()["items"].toArray();
        for (const auto &item : items) {
            QJsonObject snip    = item.toObject()["snippet"].toObject();
            QJsonObject resId   = snip["resourceId"].toObject();
            const QString vidId = resId["videoId"].toString();
            if (vidId.isEmpty()) continue;

            StreamTrack t;
            t.id      = vidId;
            t.title   = snip["title"].toString();
            t.playUrl = "https://music.youtube.com/watch?v=" + vidId;
            // YouTube API doesn't return duration in playlist items; omit it
            t.durationSecs = 0;
            // YTM doesn't separate artist from title in the playlist API —
            // the title field often contains "Artist - Title" format
            const int sep = t.title.indexOf(" - ");
            if (sep > 0) {
                t.artist = t.title.left(sep).trimmed();
                t.title  = t.title.mid(sep + 3).trimmed();
            }
            tracks.append(t);
        }
        emit tracksLoaded(playlist.id, tracks);
    });
}

// ── Playback ──────────────────────────────────────────────────────────────────

void YouTubeMusicController::playPlaylist(const StreamPlaylist &playlist)
{
    const QUrl url("https://music.youtube.com/playlist?list=" + playlist.id);
    openUrl(url);
    m_playing = true;
    emit playbackStateChanged(true);
}

void YouTubeMusicController::playTrack(const StreamTrack &track)
{
    openUrl(QUrl(track.playUrl));
    m_playing = true;
    emit playbackStateChanged(true);
}

void YouTubeMusicController::pause()
{
    sendMediaKey(kKeyPlayPause);
    m_playing = false;
    emit playbackStateChanged(false);
}

void YouTubeMusicController::resume()
{
    sendMediaKey(kKeyPlayPause);
    m_playing = true;
    emit playbackStateChanged(true);
}

void YouTubeMusicController::next()     { sendMediaKey(kKeyNext); }
void YouTubeMusicController::previous() { sendMediaKey(kKeyPrev); }

void YouTubeMusicController::setVolume(int /*percent*/)
{
    // OS-level volume control is outside our scope; the user adjusts their
    // system or browser volume independently.
}

// ── Media key dispatch ────────────────────────────────────────────────────────

void YouTubeMusicController::sendMediaKey(int key)
{
#if defined(Q_OS_WIN)
    sendMediaKeyWindows(key);
#elif defined(Q_OS_MACOS)
    sendMediaKeyMac(key);
#elif defined(Q_OS_LINUX)
    sendMediaKeyLinux(key);
#else
    Q_UNUSED(key)
#endif
}

#if defined(Q_OS_WIN)
void YouTubeMusicController::sendMediaKeyWindows(int key)
{
    WORD vk = 0;
    switch (key) {
    case kKeyPlayPause: vk = VK_MEDIA_PLAY_PAUSE; break;
    case kKeyNext:      vk = VK_MEDIA_NEXT_TRACK; break;
    case kKeyPrev:      vk = VK_MEDIA_PREV_TRACK; break;
    case kKeyStop:      vk = VK_MEDIA_STOP;        break;
    default: return;
    }
    INPUT inputs[2] = {};
    inputs[0].type       = INPUT_KEYBOARD;
    inputs[0].ki.wVk     = vk;
    inputs[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    inputs[1].type       = INPUT_KEYBOARD;
    inputs[1].ki.wVk     = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}
#else
void YouTubeMusicController::sendMediaKeyWindows(int) {}
#endif

#if defined(Q_OS_MACOS)
void YouTubeMusicController::sendMediaKeyMac(int key)
{
    // Use AppleScript to send a synthetic key event (NX_KEYTYPE_PLAY = 16,
    // NX_KEYTYPE_NEXT = 17, NX_KEYTYPE_PREVIOUS = 18).
    int keyCode = 0;
    switch (key) {
    case kKeyPlayPause: keyCode = 16; break;
    case kKeyNext:      keyCode = 17; break;
    case kKeyPrev:      keyCode = 18; break;
    case kKeyStop:      keyCode = 19; break;
    default: return;
    }
    const QString script = QString(
        "tell application \"System Events\"\n"
        "  key code %1 using {}\n"
        "end tell"
    ).arg(keyCode);
    QProcess::startDetached("osascript", {"-e", script});
}
#else
void YouTubeMusicController::sendMediaKeyMac(int) {}
#endif

#if defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
void YouTubeMusicController::sendMediaKeyLinux(int key)
{
    // Send via MPRIS2 D-Bus to the first browser (chromium / chrome / firefox)
    // that exposes a media session, or fall back to xdotool.
    const QStringList candidates = {
        "org.mpris.MediaPlayer2.chromium",
        "org.mpris.MediaPlayer2.chrome",
        "org.mpris.MediaPlayer2.firefox",
        "org.mpris.MediaPlayer2.brave",
    };
    QString method;
    switch (key) {
    case kKeyPlayPause: method = "PlayPause"; break;
    case kKeyNext:      method = "Next";      break;
    case kKeyPrev:      method = "Previous";  break;
    case kKeyStop:      method = "Stop";      break;
    default: return;
    }
    for (const auto &service : candidates) {
        QDBusInterface iface(service,
                             "/org/mpris/MediaPlayer2",
                             "org.mpris.MediaPlayer2.Player",
                             QDBusConnection::sessionBus());
        if (iface.isValid()) {
            iface.call(method);
            return;
        }
    }
    // Fallback: xdotool key
    const QMap<int, QString> keyNames = {
        {kKeyPlayPause, "XF86AudioPlay"},
        {kKeyNext,      "XF86AudioNext"},
        {kKeyPrev,      "XF86AudioPrev"},
        {kKeyStop,      "XF86AudioStop"},
    };
    if (keyNames.contains(key))
        QProcess::startDetached("xdotool", {"key", keyNames[key]});
}
#else
void YouTubeMusicController::sendMediaKeyLinux(int) {}
#endif

// ── Now-playing polling ───────────────────────────────────────────────────────

void YouTubeMusicController::onPollTimer()
{
#if defined(Q_OS_WIN)
    pollNowPlayingWindows();
#elif defined(Q_OS_MACOS)
    pollNowPlayingMac();
#elif defined(Q_OS_LINUX)
    pollNowPlayingLinux();
#endif
}

void YouTubeMusicController::pollNowPlayingWindows()
{
#if defined(Q_OS_WIN)
    // Use a helper PowerShell one-liner to read the Windows SMTC session.
    // This avoids linking WinRT directly and works on Windows 10+.
    const QString ps = R"(
$mgr = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager,Windows.Media,ContentType=WindowsRuntime]
$session = $mgr::RequestAsync().GetAwaiter().GetResult().GetCurrentSession()
if ($session) {
    $info = $session.TryGetMediaPropertiesAsync().GetAwaiter().GetResult()
    Write-Output ("TITLE=" + $info.Title)
    Write-Output ("ARTIST=" + $info.Artist)
} else { Write-Output "NONE" }
)";
    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();
        if (out.contains("NONE")) return;
        StreamTrack t;
        for (const auto &line : out.split('\n')) {
            if (line.startsWith("TITLE="))
                t.title  = line.mid(6).trimmed();
            else if (line.startsWith("ARTIST="))
                t.artist = line.mid(7).trimmed();
        }
        if (!t.title.isEmpty())
            emit nowPlayingChanged(t);
    });
    proc->start("powershell", {"-NoProfile", "-NonInteractive", "-Command", ps});
#endif
}

void YouTubeMusicController::pollNowPlayingMac()
{
#if defined(Q_OS_MACOS)
    const QString script =
        "tell application \"System Events\"\n"
        "  set nowPlaying to (do shell script \"osascript -e 'tell application \\\"Music\\\" "
        "to get {name of current track, artist of current track}'\")\n"
        "  return nowPlaying\n"
        "end tell";
    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (out.isEmpty()) return;
        const QStringList parts = out.split(", ");
        if (parts.size() >= 1) {
            StreamTrack t;
            t.title  = parts.value(0).trimmed();
            t.artist = parts.value(1).trimmed();
            emit nowPlayingChanged(t);
        }
    });
    proc->start("osascript", {"-e", script});
#endif
}

void YouTubeMusicController::pollNowPlayingLinux()
{
#if defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
    const QStringList candidates = {
        "org.mpris.MediaPlayer2.chromium",
        "org.mpris.MediaPlayer2.chrome",
        "org.mpris.MediaPlayer2.firefox",
        "org.mpris.MediaPlayer2.brave",
    };
    for (const auto &service : candidates) {
        QDBusInterface iface(service,
                             "/org/mpris/MediaPlayer2",
                             "org.mpris.MediaPlayer2.Player",
                             QDBusConnection::sessionBus());
        if (!iface.isValid()) continue;
        QDBusReply<QVariantMap> reply =
            iface.call("org.freedesktop.DBus.Properties.GetAll",
                       "org.mpris.MediaPlayer2.Player");
        if (reply.isValid()) {
            QVariantMap meta = reply.value()["Metadata"].toMap();
            StreamTrack t;
            t.title  = meta["xesam:title"].toString();
            t.artist = meta["xesam:artist"].toStringList().join(", ");
            if (!t.title.isEmpty())
                emit nowPlayingChanged(t);
        }
        break;
    }
#endif
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void YouTubeMusicController::openUrl(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}
