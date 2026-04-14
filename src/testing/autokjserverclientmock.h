#ifndef AUTOKJSERVERCLIENTMOCK_H
#define AUTOKJSERVERCLIENTMOCK_H

#include "../autokjserverclient.h"

class AutoKJServerClientMock : public AutoKJServerClient
{
    Q_OBJECT
public:
    explicit AutoKJServerClientMock(QObject *parent = nullptr);

    void removeRequest(const QString &requestId) override;
    void setAccepting(bool enabled, bool offerEndShowPrompt = true) override;
    [[nodiscard]] bool getAccepting() const override;
    void clearRequests() override;
    void updateSongDb() override;
    void dbUpdateCanceled() override;
    void test() override;
    void setInterval(int intervalSecs) override;
    void authenticate() override;
    void refreshRequests() override;
    void refreshVenues(bool blocking = false) override;
    void createVenue(const QString &name, const QString &address, const QString &pin) override;
    bool startNewShow(QString *errorOut = nullptr) override;
    bool endActiveShow(QString *errorOut = nullptr) override;
    void triggerTestAdd() override;
    void pushRotationUpdate(const QJsonObject &rotationData) override;
    void notifySongPlayed(const QString &singerName, const QString &artist,
                          const QString &title, const QStringList &cosingers,
                          const QString &kjName = {}) override;
    void pushGigSettings() override;
    void pushVenueConfig() override;
    void pushNowPlaying(const QString &singer, const QString &artist,
                        const QString &title, int durationSecs, int keyChange) override;
    [[nodiscard]] QString venueUrl() const override;
    [[nodiscard]] bool isConnected() const override;
    void fetchCheckins() override;
    void markCheckinAdded(const QString &checkinId) override;

    void seedRequests(const OkjsRequests &requests);
    void seedVenues(const OkjsVenues &venues);
    void seedCheckins(const QJsonArray &checkins);
    void setConnected(bool connected);
    void setVenueUrl(const QString &url);

private:
    OkjsRequests m_requests;
    OkjsVenues m_venues;
    QJsonArray m_checkins;
    bool m_accepting{true};
    bool m_connected{true};
    bool m_hasActiveShow{true};
    int m_intervalSecs{5};
    QString m_venueUrl{"https://mock.autokj.local/venue/test"};
};

#endif // AUTOKJSERVERCLIENTMOCK_H
