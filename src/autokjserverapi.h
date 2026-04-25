/*
 * AutoKJServerAPI — replaces OKJSongbookAPI
 *
 * Connects to the AutoKJ self-hosted server via plain WebSocket (no Socket.IO
 * framing required on the C++ side).
 *
 * Drop-in replacement: emits the same signals as OKJSongbookAPI so that
 * DlgRequests, MainWindow, and DlgSettings need minimal changes.
 *
 * Protocol: JSON messages  {"event": "event_name", "data": {...}}
 */

#ifndef AUTOKJSERVERAPI_H
#define AUTOKJSERVERAPI_H

#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <functional>
#include "autokjserverclient.h"
#include "settings.h"
#include "securecredentialstore.h"

class AutoKJServerAPI : public AutoKJServerClient
{
    Q_OBJECT
public:
    explicit AutoKJServerAPI(QObject *parent = nullptr);
    ~AutoKJServerAPI() override;
    void reconfigureFromSettings(bool force = false);

    // ── Public interface (mirrors OKJSongbookAPI) ─────────────────────────────

    /** Remove (accept/handle) a pending request */
    void removeRequest(const QString &requestId) override;

    /** Update accepting state on the server */
    void setAccepting(bool enabled, bool offerEndShowPrompt = true) override;

    /** True if this venue is accepting requests */
    [[nodiscard]] bool getAccepting() const override;

    /** Clear all pending requests */
    void clearRequests() override;

    /** Sync the local song database to the server (chunked) */
    void updateSongDb() override;

    /** Signal that the user cancelled the DB update */
    void dbUpdateCanceled() override;

    /** Run a connection test; emits testPassed() / testFailed() */
    void test() override;

    /** Change the reconnect interval */
    void setInterval(int intervalSecs) override;

    /** Re-send authentication (e.g. after venue change) */
    void authenticate() override;

    /** Change the logged-in user's password */
    bool changePassword(const QString &currentPassword, const QString &newPassword, QString *errorOut = nullptr);

    // ── Compatibility stubs (OKJSongbookAPI drop-in) ──────────────────────────

    /** No-op: requests arrive via push; kept for drop-in compatibility */
    void refreshRequests() override { /* push-based; no polling needed */ }

    /** Fetch venues for the current API key from the server */
    void refreshVenues() override;

    /** Create a new venue via the API */
    void createVenue(const QString &name, const QString &address, const QString &pin) override;

    /** Start a new active show (gig) for the selected venue. */
    void startNewShow() override;

    /** End the currently active show for the selected venue, if any. */
    void endActiveShow() override;

    /** No-op: testing stub */
    void triggerTestAdd() override { /* no equivalent in AutoKJ */ }

    // ── Rotation push (new — called by MainWindow on rotation changes) ────────

    /** Push the current rotation state to the server for display in PWAs */
    void pushRotationUpdate(const QJsonObject &rotationData) override;

    /** Notify the server a song just finished playing */
    void notifySongPlayed(const QString &singerName, const QString &artist,
                          const QString &title, const QStringList &cosingers,
                          const QString &kjName = {}) override;

    void pushGigSettings() override;
    /** Push venue-level configuration (GPS, address, etc.) */
    void pushVenueConfig() override;

    /** Push now-playing info */
    void pushNowPlaying(const QString &singer, const QString &artist,
                        const QString &title, int durationSecs, int keyChange) override;

    /** Returns the venue URL for QR code display */
    [[nodiscard]] QString venueUrl() const override;

    /** True if currently authenticated and connected */
    [[nodiscard]] bool isConnected() const override;

    /** Fetch pending check-ins from server; emits checkinsFetched() */
    void fetchCheckins() override;

    /** Mark a check-in as added to rotation on the server */
    void markCheckinAdded(const QString &checkinId) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onSslErrors(const QList<QSslError> &errors);
    void onReconnectTimer();
    void onSongSyncTimer();
    void onVenuesReply(QNetworkReply *reply);
    void onSocketError(QAbstractSocket::SocketError error);


private:
    QWebSocket *m_socket{nullptr};
    QTimer *m_reconnectTimer{nullptr};
    QTimer *m_songSyncTimer{nullptr};   // chunked song DB upload timer
    QTimer *m_songSyncTimeoutTimer{nullptr}; // watchdog for stalled sync
    QTimer *m_testTimer{nullptr};       // connection test timeout timer
    QNetworkAccessManager *m_nam{nullptr};


    Settings m_settings;
    bool m_accepting{false};
    bool m_authenticated{false};
    bool m_cancelUpdate{false};
    bool m_updateInProgress{false};
    bool m_testInProgress{false};
    bool m_reconnectEnabled{false};
    bool m_reconfigureInProgress{false};
    bool m_suppressReconnectOnce{false};
    int m_reconnectIntervalSecs{5};
    QString m_token;
    QString m_lastServerUrl;
    QString m_lastEmail;
    QString m_lastPassword;
    QString m_lastToken;
    int m_lastVenueId{-1};

    OkjsRequests m_requests;

    // Chunked song sync state
    QList<QJsonDocument> m_songChunks;
    int m_syncChunkIndex{0};

    SecureCredentialStore m_secureStore;

    void connectToServer();
    void disconnectFromServer(bool clearSessionState);
    void clearSessionState();
    void sendEvent(const QString &event, const QJsonObject &data = {});
    void handleEvent(const QString &event, const QJsonObject &data);
    void processNewRequest(const QJsonObject &reqData);
    void processPendingRequests(const QJsonArray &reqArray);
    QString normalizedBaseUrl() const;
    QString errorFromReply(QNetworkReply *reply, const QByteArray &body) const;
    bool login(QString *errorOut = nullptr);
    QString ensureToken(QString *errorOut = nullptr);
    void loginAsync(const std::function<void(bool, const QString &)> &callback);
    void ensureTokenAsync(const std::function<void(const QString &)> &onSuccess,
                          const std::function<void(const QString &)> &onFailure = {});
    void setAuthHeader(QNetworkRequest &request, const QString &token);
    void testHttpAuthAsync(const std::function<void(bool, const QString &, bool)> &callback);
    void tryLegacySongDbSyncAsync(const std::function<void(bool, const QString &)> &callback);
    void patchAcceptingState(bool enabled, const QString &token,
                             const std::function<void(bool, const QString &, const QString &)> &callback);
};

#endif // AUTOKJSERVERAPI_H
