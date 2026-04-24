#include "testing/autokjserverclientmock.h"

#include <QSignalSpy>
#include <QtTest>

Q_DECLARE_METATYPE(OkjsRequests)
Q_DECLARE_METATYPE(OkjsVenues)

class AutoKJServerClientMockTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<OkjsRequests>("OkjsRequests");
        qRegisterMetaType<OkjsVenues>("OkjsVenues");
    }

    void requestLifecycleEmitsCurrentRequests()
    {
        AutoKJServerClientMock client;
        QSignalSpy requestsChanged(&client, &AutoKJServerClientMock::requestsChanged);
        QVERIFY(requestsChanged.isValid());

        client.triggerTestAdd();
        QCOMPARE(requestsChanged.count(), 1);

        const auto added = qvariant_cast<OkjsRequests>(requestsChanged.takeFirst().at(0));
        QCOMPARE(added.size(), 1);
        QCOMPARE(added.first().requestId, QStringLiteral("mock-1"));
        QCOMPARE(added.first().singer, QStringLiteral("Mock Singer"));

        client.removeRequest(QStringLiteral("mock-1"));
        QCOMPARE(requestsChanged.count(), 1);

        const auto removed = qvariant_cast<OkjsRequests>(requestsChanged.takeFirst().at(0));
        QVERIFY(removed.isEmpty());
    }

    void acceptingStateCanBeToggled()
    {
        AutoKJServerClientMock client;
        QSignalSpy acceptingFinished(&client, &AutoKJServerClientMock::acceptingSetFinished);
        QVERIFY(acceptingFinished.isValid());

        client.setAccepting(false);
        QCOMPARE(client.getAccepting(), false);
        QCOMPARE(acceptingFinished.count(), 1);
        QCOMPARE(acceptingFinished.first().at(0).toBool(), true);
        QCOMPARE(acceptingFinished.first().at(1).toBool(), false);
    }

    void connectionAndShowStateDriveTestResult()
    {
        AutoKJServerClientMock client;
        QSignalSpy passed(&client, &AutoKJServerClientMock::testPassed);
        QSignalSpy failed(&client, &AutoKJServerClientMock::testFailed);
        QVERIFY(passed.isValid());
        QVERIFY(failed.isValid());

        client.test();
        QCOMPARE(passed.count(), 1);
        QCOMPARE(failed.count(), 0);

        client.setConnected(false);
        client.test();
        QCOMPARE(failed.count(), 1);
        QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("disconnected")));

        client.setConnected(true);
        client.endActiveShow();
        client.test();
        QCOMPARE(failed.count(), 2);
        QVERIFY(failed.at(1).at(0).toString().contains(QStringLiteral("no active show")));
    }

    void checkinsCanBeMarkedAdded()
    {
        AutoKJServerClientMock client;
        QSignalSpy checkinsFetched(&client, &AutoKJServerClientMock::checkinsFetched);
        QVERIFY(checkinsFetched.isValid());

        QJsonObject checkin;
        checkin["id"] = QStringLiteral("checkin-1");
        checkin["added_to_rotation"] = false;
        client.seedCheckins(QJsonArray{checkin});

        client.markCheckinAdded(QStringLiteral("checkin-1"));
        QCOMPARE(checkinsFetched.count(), 1);

        const auto checkins = checkinsFetched.first().at(0).toJsonArray();
        QCOMPARE(checkins.size(), 1);
        QCOMPARE(checkins.first().toObject().value("added_to_rotation").toBool(), true);
    }
};

QTEST_MAIN(AutoKJServerClientMockTests)
#include "test_autokjserverclientmock.moc"
