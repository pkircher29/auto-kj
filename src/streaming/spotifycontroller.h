#pragma once

#include "streamingcontroller.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTimer>
#include <QByteArray>

/*
 * SpotifyController — controls the Spotify desktop client via Spotify Connect
 * (Spotify Web API).
 *
 * Auth: OAuth 2.0 PKCE — no client secret stored in the binary.
 * Redirect URI: http://localhost:8888/callback
 *
 * The user must supply a Spotify Client ID (created at
 * https://developer.spotify.com/dashboard). Store it in settings under
 * "streaming/spotify_client_id".
 *
 * Playback is delegated to the Spotify app running on the same device or any
 * Spotify Connect device on the user's account; audio is NOT routed through
 * GStreamer.
 */

class SpotifyController : public StreamingController
{
    Q_OBJECT
public:
    explicit SpotifyController(QObject *parent = nullptr);

    Service service()     const override { return Service::Spotify; }
    QString serviceName() const override { return QStringLiteral("Spotify"); }
    bool    isConnected() const override { return !m_accessToken.isEmpty(); }

    void connectService()    override;
    void disconnectService() override;

    void loadPlaylists()                             override;
    void loadTracks(const StreamPlaylist &playlist)  override;

    void playPlaylist(const StreamPlaylist &playlist) override;
    void playTrack(const StreamTrack &track)          override;
    void pause()    override;
    void resume()   override;
    void next()     override;
    void previous() override;
    void setVolume(int percent) override;

    void setClientId(const QString &clientId);

private slots:
    void onOAuthCallback();
    void onTokenReply(QNetworkReply *reply);
    void onApiReply(QNetworkReply *reply);
    void onPollTimer();
    void onRefreshTimer();

private:
    void startOAuthFlow();
    void exchangeCodeForToken(const QString &code);
    void refreshAccessToken();
    void sendPlayerCommand(const QString &endpoint,
                           const QByteArray &body = {},
                           const QByteArray &method = "PUT");
    void loadActiveDevice();

    QString m_clientId;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_codeVerifier;
    QString m_activeDeviceId;

    QNetworkAccessManager *m_nam{nullptr};
    QTcpServer            *m_callbackServer{nullptr};
    QTimer                *m_pollTimer{nullptr};   // now-playing polling
    QTimer                *m_refreshTimer{nullptr};// token refresh

    QString m_pendingPlaylistUri;  // play after device is resolved
    QString m_pendingTrackUri;

    static constexpr int kCallbackPort = 8888;
    static constexpr int kPollIntervalMs = 5000;
};
