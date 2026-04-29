#include "testing/autokjserverclientmock.h"

#include <QSignalSpy>
#include <QtTest>

Q_DECLARE_METATYPE(OkjsRequests)
Q_DECLARE_METATYPE(OkjsVenues)

class AutoKJClientIntegrationSmokeTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<OkjsRequests>("OkjsRequests");
        qRegisterMetaType<OkjsVenues>("OkjsVenues");
    }

    void mockClientSupportsBasicShowWorkflow()
    {
        AutoKJServerClientMock client;
        QSignalSpy synchronized(&client, &AutoKJServerClientMock::synchronized);
        QSignalSpy venuesChanged(&client, &AutoKJServerClientMock::venuesChanged);
        QSignalSpy requestsChanged(&client, &AutoKJServerClientMock::requestsChanged);
        QSignalSpy updateDone(&client, &AutoKJServerClientMock::remoteSongDbUpdateDone);
        QSignalSpy showEnded(&client, &AutoKJServerClientMock::endActiveShowFinished);
        QSignalSpy showStarted(&client, &AutoKJServerClientMock::startNewShowFinished);

        QVERIFY(synchronized.isValid());
        QVERIFY(venuesChanged.isValid());
        QVERIFY(requestsChanged.isValid());
        QVERIFY(updateDone.isValid());
        QVERIFY(showEnded.isValid());
        QVERIFY(showStarted.isValid());

        client.authenticate();
        QCOMPARE(synchronized.count(), 1);
        QVERIFY(client.isConnected());

        client.createVenue(QStringLiteral("Main Room"), QStringLiteral("123 Example Ave"), QStringLiteral("1234"));
        QCOMPARE(venuesChanged.count(), 1);
        const auto venues = qvariant_cast<OkjsVenues>(venuesChanged.first().at(0));
        QCOMPARE(venues.size(), 1);
        QCOMPARE(venues.first().name, QStringLiteral("Main Room"));
        QCOMPARE(venues.first().urlName, QStringLiteral("main-room"));

        client.triggerTestAdd();
        QCOMPARE(requestsChanged.count(), 1);
        const auto requests = qvariant_cast<OkjsRequests>(requestsChanged.first().at(0));
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.first().title, QStringLiteral("Mock Song"));

        client.updateSongDb();
        QCOMPARE(updateDone.count(), 1);

        client.endActiveShow();
        QCOMPARE(showEnded.count(), 1);
        QCOMPARE(showEnded.first().at(0).toBool(), true);

        client.startNewShow();
        QCOMPARE(showStarted.count(), 1);
        QCOMPARE(showStarted.first().at(0).toBool(), true);
    }
};

QTEST_MAIN(AutoKJClientIntegrationSmokeTests)
#include "test_autokj_client_integration_smoke.moc"
