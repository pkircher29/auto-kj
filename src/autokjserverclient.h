#ifndef AUTOKJSERVERCLIENT_H
#define AUTOKJSERVERCLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QTime>

class OkjsRequest
{
public:
    QString requestId;
    QString singer;
    QString artist;
    QString title;
    int key;
    int time;
    QString cosinger2;
    QString cosinger3;
    QString cosinger4;
    bool isSharedDevice{false};
    int noShowCount{0};
    int removedCount{0};
    bool operator == (const OkjsRequest& r) const {
        return requestId == r.requestId;
    }
};

typedef QList<OkjsRequest> OkjsRequests;

class OkjsVenue
{
public:
    int venueId;
    QString name;
    QString urlName;
    bool accepting;
    bool hasActiveShow{false};
    bool operator == (const OkjsVenue& v) const {
        return venueId == v.venueId;
    }
};

typedef QList<OkjsVenue> OkjsVenues;

class AutoKJServerClient : public QObject
{
    Q_OBJECT
public:
    explicit AutoKJServerClient(QObject *parent = nullptr) : QObject(parent) {}
    ~AutoKJServerClient() override = default;

    virtual void removeRequest(const QString &requestId) = 0;
    virtual void setAccepting(bool enabled, bool offerEndShowPrompt = true) = 0;
    [[nodiscard]] virtual bool getAccepting() const = 0;
    virtual void clearRequests() = 0;
    virtual void updateSongDb() = 0;
    virtual void dbUpdateCanceled() = 0;
    virtual void test() = 0;
    virtual void setInterval(int intervalSecs) = 0;
    virtual void authenticate() = 0;
    virtual bool changePassword(const QString &currentPassword, const QString &newPassword, QString *errorOut = nullptr) = 0;

    virtual void refreshRequests() {}
    virtual void refreshVenues() = 0;
    virtual void createVenue(const QString &name, const QString &address) = 0;
    virtual void startNewShow() = 0;
    virtual void endActiveShow() = 0;
    virtual void triggerTestAdd() {}

    virtual void pushRotationUpdate(const QJsonObject &rotationData) = 0;
    virtual void notifySongPlayed(const QString &singerName, const QString &artist,
                                  const QString &title, const QStringList &cosingers,
                                  const QString &kjName = {}) = 0;
    virtual void pushGigSettings() = 0;
    virtual void pushVenueConfig() = 0;
    virtual void pushNowPlaying(const QString &singer, const QString &artist,
                                const QString &title, int durationSecs, int keyChange) = 0;
    [[nodiscard]] virtual QString venueUrl() const = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;
    virtual void fetchCheckins() = 0;
    virtual void markCheckinAdded(const QString &checkinId) = 0;

signals:
    void requestsChanged(OkjsRequests requests);
    void venuesChanged(const OkjsVenues &venues);
    void venuesRefreshFailed(QString error);
    void startNewShowFinished(bool ok, QString error);
    void endActiveShowFinished(bool ok, QString error);
    void acceptingSetFinished(bool ok, bool enabled, QString error);
    void synchronized(QTime time);
    void remoteSongDbUpdateProgress(int progress);
    void remoteSongDbUpdateNumDocs(int numDocs);
    void remoteSongDbUpdateStart();
    void remoteSongDbUpdateDone();
    void remoteSongDbUpdateFailed(QString error);
    void delayError(int secondsSinceLastSync);
    void testPassed();
    void testFailed(QString error);
    void testSslError(QString error);
    void sslError();
    void alertReceived(QString title, QString message);
    void entitledSystemCountChanged(int count);
    void connectionStatusChanged(bool connected);
    void checkinsFetched(const QJsonArray &checkins);
    void kjWebCommand(QString action, QJsonObject payload);
};

#endif // AUTOKJSERVERCLIENT_H
