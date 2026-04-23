#include "autokjserverclientmock.h"
#include <QDateTime>

AutoKJServerClientMock::AutoKJServerClientMock(QObject *parent)
    : AutoKJServerClient(parent)
{
}

void AutoKJServerClientMock::removeRequest(const QString &requestId)
{
    for (int i = 0; i < m_requests.size(); ++i) {
        if (m_requests.at(i).requestId == requestId) {
            m_requests.removeAt(i);
            break;
        }
    }
    emit requestsChanged(m_requests);
}

void AutoKJServerClientMock::setAccepting(bool enabled, bool offerEndShowPrompt)
{
    Q_UNUSED(offerEndShowPrompt)
    m_accepting = enabled;
    emit acceptingSetFinished(true, enabled, {});
}

bool AutoKJServerClientMock::getAccepting() const
{
    return m_accepting;
}

void AutoKJServerClientMock::clearRequests()
{
    m_requests.clear();
    emit requestsChanged(m_requests);
}

void AutoKJServerClientMock::updateSongDb()
{
    emit remoteSongDbUpdateStart();
    emit remoteSongDbUpdateNumDocs(100);
    emit remoteSongDbUpdateProgress(25);
    emit remoteSongDbUpdateProgress(75);
    emit remoteSongDbUpdateProgress(100);
    emit remoteSongDbUpdateDone();
}

void AutoKJServerClientMock::dbUpdateCanceled()
{
    emit remoteSongDbUpdateFailed("Mock update canceled");
}

void AutoKJServerClientMock::test()
{
    if (m_connected && m_hasActiveShow) {
        emit testPassed();
    } else if (!m_connected) {
        emit testFailed("Mock server disconnected");
    } else {
        emit testFailed("Mock server has no active show");
    }
}

void AutoKJServerClientMock::setInterval(int intervalSecs)
{
    m_intervalSecs = intervalSecs;
    Q_UNUSED(m_intervalSecs)
}

void AutoKJServerClientMock::authenticate()
{
    emit synchronized(QTime::currentTime());
}

void AutoKJServerClientMock::refreshRequests()
{
    emit requestsChanged(m_requests);
}

void AutoKJServerClientMock::refreshVenues()
{
    emit venuesChanged(m_venues);
}

void AutoKJServerClientMock::createVenue(const QString &name, const QString &address, const QString &pin)
{
    Q_UNUSED(address)
    Q_UNUSED(pin)
    OkjsVenue venue;
    venue.venueId = m_venues.size() + 1;
    venue.name = name;
    venue.urlName = name.toLower().replace(" ", "-");
    venue.accepting = m_accepting;
    venue.hasActiveShow = m_hasActiveShow;
    m_venues.append(venue);
    emit venuesChanged(m_venues);
}

void AutoKJServerClientMock::startNewShow()
{
    m_hasActiveShow = true;
    emit startNewShowFinished(true, {});
}

void AutoKJServerClientMock::endActiveShow()
{
    m_hasActiveShow = false;
    emit endActiveShowFinished(true, {});
}

void AutoKJServerClientMock::triggerTestAdd()
{
    OkjsRequest req;
    req.requestId = QString("mock-%1").arg(m_requests.size() + 1);
    req.singer = "Mock Singer";
    req.artist = "Mock Artist";
    req.title = "Mock Song";
    req.time = QDateTime::currentSecsSinceEpoch();
    m_requests.append(req);
    emit requestsChanged(m_requests);
}

void AutoKJServerClientMock::pushRotationUpdate(const QJsonObject &rotationData)
{
    Q_UNUSED(rotationData)
}

void AutoKJServerClientMock::notifySongPlayed(const QString &singerName, const QString &artist,
                                              const QString &title, const QStringList &cosingers,
                                              const QString &kjName)
{
    Q_UNUSED(singerName)
    Q_UNUSED(artist)
    Q_UNUSED(title)
    Q_UNUSED(cosingers)
    Q_UNUSED(kjName)
}

void AutoKJServerClientMock::pushGigSettings()
{
}

void AutoKJServerClientMock::pushVenueConfig()
{
}

void AutoKJServerClientMock::pushNowPlaying(const QString &singer, const QString &artist,
                                            const QString &title, int durationSecs, int keyChange)
{
    Q_UNUSED(singer)
    Q_UNUSED(artist)
    Q_UNUSED(title)
    Q_UNUSED(durationSecs)
    Q_UNUSED(keyChange)
}

QString AutoKJServerClientMock::venueUrl() const
{
    return m_venueUrl;
}

bool AutoKJServerClientMock::isConnected() const
{
    return m_connected;
}

void AutoKJServerClientMock::fetchCheckins()
{
    emit checkinsFetched(m_checkins);
}

void AutoKJServerClientMock::markCheckinAdded(const QString &checkinId)
{
    for (int i = 0; i < m_checkins.size(); ++i) {
        QJsonObject obj = m_checkins.at(i).toObject();
        if (obj.value("id").toString() == checkinId) {
            obj["added_to_rotation"] = true;
            m_checkins[i] = obj;
            break;
        }
    }
    emit checkinsFetched(m_checkins);
}

void AutoKJServerClientMock::seedRequests(const OkjsRequests &requests)
{
    m_requests = requests;
}

void AutoKJServerClientMock::seedVenues(const OkjsVenues &venues)
{
    m_venues = venues;
}

void AutoKJServerClientMock::seedCheckins(const QJsonArray &checkins)
{
    m_checkins = checkins;
}

void AutoKJServerClientMock::setConnected(bool connected)
{
    m_connected = connected;
    emit connectionStatusChanged(m_connected);
}

void AutoKJServerClientMock::setVenueUrl(const QString &url)
{
    m_venueUrl = url;
}
