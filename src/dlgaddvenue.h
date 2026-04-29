#ifndef DLGADDVENUE_H
#define DLGADDVENUE_H

#include <QDialog>
#include <QObject>
#include <QString>

class QWebEngineView;
class QWebChannel;
class QNetworkAccessManager;

namespace Ui {
class DlgAddVenue;
}

// JS ↔ C++ bridge object exposed to the Leaflet page.
class VenueMapBridge : public QObject
{
    Q_OBJECT
public:
    explicit VenueMapBridge(QObject *parent = nullptr) : QObject(parent) {}
signals:
    void mapClicked(double lat, double lon);
    void mapReady();
public slots:
    void onMapClicked(double lat, double lon) { emit mapClicked(lat, lon); }
    void ready() { emit mapReady(); }
};

class DlgAddVenue : public QDialog
{
    Q_OBJECT

public:
    explicit DlgAddVenue(QWidget *parent = nullptr);
    ~DlgAddVenue();

    QString venueName() const;
    QString venueAddress() const;

    double venueLat() const;     // returns NaN if no pin
    double venueLon() const;     // returns NaN if no pin
    bool   hasCoordinates() const;

    void setVenueName(const QString &name);
    void setVenueAddress(const QString &address);

    void setVenueCoordinates(double lat, double lon);
    void setSubmitButtonText(const QString &text);

private slots:
    void on_btnCreate_clicked();
    void on_btnCancel_clicked();
    void on_btnFindOnMap_clicked();
    void on_btnClearCoords_clicked();

    void onMapReady();
    void onMapClicked(double lat, double lon);

private:
    void updateLatLonLabel();
    void pushCoordinatesToMap(int zoom);

    Ui::DlgAddVenue *ui;
    QWebEngineView  *m_mapView    = nullptr;
    QWebChannel     *m_channel    = nullptr;
    VenueMapBridge  *m_bridge     = nullptr;
    QNetworkAccessManager *m_geocoder = nullptr;
    double m_lat = 0.0;
    double m_lon = 0.0;
    bool   m_hasCoords  = false;
    bool   m_mapReady   = false;
    bool   m_pendingCoords = false;   // queue setLocation until JS reports ready
};

#endif // DLGADDVENUE_H
