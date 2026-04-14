#pragma once

#include "streamingcontroller.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTimer>

/*
 * YouTubeMusicController — launches YouTube Music in the user's default
 * browser and controls playback via OS-level media transport commands.
 *
 * Transport control per platform:
 *   Windows : SendInput with VK_MEDIA_PLAY_PAUSE / VK_MEDIA_NEXT_TRACK /
 *             VK_MEDIA_PREV_TRACK
 *   macOS   : osascript to send media key events
 *   Linux   : D-Bus MPRIS2 (targets the first browser media session found)
 *
 * Playlist browsing requires a YouTube Data API v3 key stored in settings
 * under "streaming/youtube_api_key". Without a key, the user can still open
 * any YouTube Music playlist URL manually.
 *
 * "Now playing" is read from the OS media session:
 *   Windows : Windows.Media.Control WinRT (polled via helper)
 *   macOS   : osascript query to Now Playing info
 *   Linux   : MPRIS GetMetadata over D-Bus
 */

class YouTubeMusicController : public StreamingController
{
    Q_OBJECT
public:
    explicit YouTubeMusicController(QObject *parent = nullptr);

    Service service()     const override { return Service::YouTubeMusic; }
    QString serviceName() const override { return QStringLiteral("YouTube Music"); }
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

    void setApiKey(const QString &key);
    void setClientId(const QString &clientId);

private slots:
    void onOAuthCallback();
    void onTokenReply(QNetworkReply *reply);
    void onPollTimer();
    void onRefreshTimer();

private:
    void sendMediaKey(int key);       // platform-specific dispatch
    void sendMediaKeyWindows(int key);
    void sendMediaKeyMac(int key);
    void sendMediaKeyLinux(int key);

    void pollNowPlayingWindows();
    void pollNowPlayingMac();
    void pollNowPlayingLinux();

    void openUrl(const QUrl &url);
    void startOAuthFlow();
    void exchangeCodeForToken(const QString &code);
    void refreshAccessToken();

    QNetworkAccessManager *m_nam{nullptr};
    QTcpServer            *m_callbackServer{nullptr};
    QTimer                *m_pollTimer{nullptr};
    QTimer                *m_refreshTimer{nullptr};
    QString                m_apiKey;       // Used for public API calls if desired
    QString                m_clientId;     // Google OAuth2 Client ID
    QString                m_accessToken;
    QString                m_refreshToken;
    QString                m_codeVerifier;
    bool                   m_playing{false};

    static constexpr int kCallbackPort = 8889; // Different from Spotify's 8888
    static constexpr int kPollIntervalMs = 4000;

    // OS media key virtual-key codes (Windows subset; re-used as enum across platforms)
    static constexpr int kKeyPlayPause = 0;
    static constexpr int kKeyNext      = 1;
    static constexpr int kKeyPrev      = 2;
    static constexpr int kKeyStop      = 3;
};
