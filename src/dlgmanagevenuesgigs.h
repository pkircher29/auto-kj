#ifndef DLGMANAGEVENUESGIGS_H
#define DLGMANAGEVENUESGIGS_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include "settings.h"
#include "autokjserverclient.h"

class DlgManageVenuesGigs : public QDialog
{
    Q_OBJECT

public:
    explicit DlgManageVenuesGigs(Settings &settings, AutoKJServerClient &api, QWidget *parent = nullptr);

private slots:
    void refreshAll();
    void onVenueSelectionChanged();
    void addVenue();
    void editVenue();
    void removeVenue();
    void addShowtime();
    void editShowtime();
    void removeShowtime();

private:
    QString baseHttpUrl() const;
    QJsonDocument requestJson(
        const QString &path,
        const QByteArray &method,
        const QJsonObject &payload,
        int *statusOut,
        QString *errorOut
    );
    int selectedVenueId() const;
    int selectedShowtimeIndex() const;
    void loadVenues();
    void loadShowtimesForVenue(int venueId);

    Settings &m_settings;
    AutoKJServerClient &m_api;
    QNetworkAccessManager m_network;
    QListWidget *m_venueList{nullptr};
    QListWidget *m_showtimeList{nullptr};
    QLabel *m_statusLabel{nullptr};
};

#endif // DLGMANAGEVENUESGIGS_H


