#include "dlgaddvenue.h"
#include "ui_dlgaddvenue.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEngineView>
#include <cmath>

DlgAddVenue::DlgAddVenue(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgAddVenue)
{
    ui->setupUi(this);
    ui->lineEditName->setMinimumWidth(320);
    ui->lineEditAddress->setMinimumWidth(280);
    setMinimumWidth(620);

    // Build the embedded map and bridge.
    m_mapView = new QWebEngineView(ui->mapContainer);
    auto *layout = new QVBoxLayout(ui->mapContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_mapView);

    m_bridge  = new VenueMapBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject("bridge", m_bridge);
    m_mapView->page()->setWebChannel(m_channel);

    connect(m_bridge, &VenueMapBridge::mapReady,   this, &DlgAddVenue::onMapReady);
    connect(m_bridge, &VenueMapBridge::mapClicked, this, &DlgAddVenue::onMapClicked);

    m_mapView->load(QUrl("qrc:/html/html/venuemap.html"));

    m_geocoder = new QNetworkAccessManager(this);

    updateLatLonLabel();
}

DlgAddVenue::~DlgAddVenue()
{
    delete ui;
}

QString DlgAddVenue::venueName()    const { return ui->lineEditName->text().trimmed(); }
QString DlgAddVenue::venueAddress() const { return ui->lineEditAddress->text().trimmed(); }
double  DlgAddVenue::venueLat()     const { return m_hasCoords ? m_lat : std::nan(""); }
double  DlgAddVenue::venueLon()     const { return m_hasCoords ? m_lon : std::nan(""); }
bool    DlgAddVenue::hasCoordinates() const { return m_hasCoords; }

void DlgAddVenue::setVenueName(const QString &name)       { ui->lineEditName->setText(name); }
void DlgAddVenue::setVenueAddress(const QString &address) { ui->lineEditAddress->setText(address); }
void DlgAddVenue::setSubmitButtonText(const QString &t)   { ui->btnCreate->setText(t); }

void DlgAddVenue::setVenueCoordinates(double lat, double lon)
{
    m_lat = lat;
    m_lon = lon;
    m_hasCoords = true;
    updateLatLonLabel();
    if (m_mapReady) pushCoordinatesToMap(15);
    else            m_pendingCoords = true;
}

void DlgAddVenue::onMapReady()
{
    m_mapReady = true;
    if (m_pendingCoords) {
        pushCoordinatesToMap(15);
        m_pendingCoords = false;
    }
}

void DlgAddVenue::onMapClicked(double lat, double lon)
{
    m_lat = lat;
    m_lon = lon;
    m_hasCoords = true;
    updateLatLonLabel();
}

void DlgAddVenue::updateLatLonLabel()
{
    if (!m_hasCoords) {
        ui->labelLatLon->setText("(none — click the map)");
        return;
    }
    ui->labelLatLon->setText(QString("%1, %2")
                                 .arg(m_lat, 0, 'f', 6)
                                 .arg(m_lon, 0, 'f', 6));
}

void DlgAddVenue::pushCoordinatesToMap(int zoom)
{
    if (!m_mapView) return;
    const QString js = QString("setLocation(%1, %2, %3);")
                           .arg(m_lat, 0, 'f', 8)
                           .arg(m_lon, 0, 'f', 8)
                           .arg(zoom);
    m_mapView->page()->runJavaScript(js);
}

void DlgAddVenue::on_btnFindOnMap_clicked()
{
    const QString address = venueAddress();
    if (address.isEmpty()) {
        QMessageBox::information(this, "Find on Map", "Enter an address first.");
        return;
    }
    ui->btnFindOnMap->setEnabled(false);
    ui->btnFindOnMap->setText("Searching…");

    QUrl url("https://nominatim.openstreetmap.org/search");
    QUrlQuery q;
    q.addQueryItem("format", "json");
    q.addQueryItem("limit",  "1");
    q.addQueryItem("q",      address);
    url.setQuery(q);

    QNetworkRequest req(url);
    // Nominatim policy requires a real UA identifying the app.
    req.setRawHeader("User-Agent", "AutoKJ/0.5 (https://auto-kj.com)");
    QNetworkReply *reply = m_geocoder->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        ui->btnFindOnMap->setEnabled(true);
        ui->btnFindOnMap->setText("Find on Map");

        const QByteArray body = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray() || doc.array().isEmpty()) {
            QMessageBox::warning(this, "Find on Map",
                                 "Couldn't find that address. Try a more specific one or click the map directly.");
            return;
        }
        const QJsonObject hit = doc.array().at(0).toObject();
        const double lat = hit.value("lat").toString().toDouble();
        const double lon = hit.value("lon").toString().toDouble();
        if (lat == 0.0 && lon == 0.0) {
            QMessageBox::warning(this, "Find on Map", "Couldn't parse coordinates from the response.");
            return;
        }
        m_lat = lat;
        m_lon = lon;
        m_hasCoords = true;
        updateLatLonLabel();
        if (m_mapReady) pushCoordinatesToMap(16);
        else            m_pendingCoords = true;
    });
}

void DlgAddVenue::on_btnClearCoords_clicked()
{
    m_hasCoords = false;
    m_lat = m_lon = 0.0;
    updateLatLonLabel();
    if (m_mapView) m_mapView->page()->runJavaScript("clearLocation();");
}

void DlgAddVenue::on_btnCreate_clicked()
{
    if (venueName().isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please enter a venue name.");
        return;
    }
    accept();
}

void DlgAddVenue::on_btnCancel_clicked()
{
    reject();
}
