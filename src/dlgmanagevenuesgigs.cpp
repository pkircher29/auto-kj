#include "dlgmanagevenuesgigs.h"
#include "dlgaddvenue.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QTimeEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString dayName(int dayOfWeek)
{
    static const QStringList names = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    if (dayOfWeek < 1 || dayOfWeek > 7)
        return "N/A";
    return names.at(dayOfWeek - 1);
}

class ShowtimeDialog : public QDialog
{
public:
    explicit ShowtimeDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Showtime");
        resize(420, 320);
        auto *layout = new QVBoxLayout(this);

        m_weekly = new QRadioButton("Weekly repeating", this);
        m_oneTime = new QRadioButton("One-time date", this);
        m_weekly->setChecked(true);
        layout->addWidget(m_weekly);
        layout->addWidget(m_oneTime);

        auto *daysRow = new QHBoxLayout();
        for (int d = 1; d <= 7; ++d) {
            auto *cb = new QCheckBox(dayName(d), this);
            m_dayChecks.append(cb);
            daysRow->addWidget(cb);
        }
        layout->addLayout(daysRow);

        auto *form = new QFormLayout();
        m_date = new QDateEdit(QDate::currentDate(), this);
        m_date->setCalendarPopup(true);
        m_date->setEnabled(false);
        form->addRow("Date:", m_date);

        m_start = new QTimeEdit(QTime(20, 0), this);
        m_end = new QTimeEdit(QTime(0, 0), this);
        form->addRow("Start time:", m_start);
        form->addRow("End time:", m_end);
        layout->addLayout(form);

        // Rotation style selector
        auto *rotGroup = new QGroupBox("Rotation Style", this);
        auto *rotLayout = new QVBoxLayout(rotGroup);
        m_rotationStyle = new QComboBox(this);
        m_rotationStyle->addItem("Classic (1 per round)", 0);
        m_rotationStyle->addItem("Double (2 per round)", 1);
        m_rotationStyle->addItem("Flex (mixed)", 2);
        rotLayout->addWidget(m_rotationStyle);
        auto *rotHelp = new QLabel(
            "<small><b>Classic:</b> singers rotate one-at-a-time<br>"
            "<b>Double:</b> each singer gets 2 consecutive slots<br>"
            "<b>Flex:</b> mix 1-per-round and 2-per-round per singer</small>", this);
        rotHelp->setWordWrap(true);
        rotLayout->addWidget(rotHelp);
        layout->addWidget(rotGroup);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);

        connect(m_weekly, &QRadioButton::toggled, this, [this](bool checked) {
            for (auto *cb : m_dayChecks)
                cb->setEnabled(checked);
            m_date->setEnabled(!checked);
        });
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (m_weekly->isChecked()) {
                bool anyChecked = false;
                for (auto *cb : m_dayChecks)
                    anyChecked |= cb->isChecked();
                if (!anyChecked) {
                    QMessageBox::warning(this, "Showtime", "Select at least one day for weekly showtime.");
                    return;
                }
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void setFromJson(const QJsonObject &obj)
    {
        const QString type = obj.value("type").toString("weekly");
        if (type == "one_time") {
            m_oneTime->setChecked(true);
            m_date->setDate(QDate::fromString(obj.value("date").toString(), Qt::ISODate));
        } else {
            m_weekly->setChecked(true);
            const QJsonArray days = obj.value("days").toArray();
            QSet<int> daySet;
            for (const auto &d : days)
                daySet.insert(d.toInt());
            for (int i = 0; i < m_dayChecks.size(); ++i)
                m_dayChecks.at(i)->setChecked(daySet.contains(i + 1));
        }
        m_start->setTime(QTime::fromString(obj.value("start").toString("20:00"), "HH:mm"));
        m_end->setTime(QTime::fromString(obj.value("end").toString("00:00"), "HH:mm"));

        // Restore rotation style (defaults to Classic)
        int rotStyle = obj.value("rotation_style").toInt(0);
        int idx = m_rotationStyle->findData(rotStyle);
        if (idx >= 0)
            m_rotationStyle->setCurrentIndex(idx);
    }

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["start"] = m_start->time().toString("HH:mm");
        obj["end"] = m_end->time().toString("HH:mm");
        obj["rotation_style"] = m_rotationStyle->currentData().toInt();

        if (m_oneTime->isChecked()) {
            obj["type"] = "one_time";
            obj["date"] = m_date->date().toString(Qt::ISODate);
        } else {
            obj["type"] = "weekly";
            QJsonArray days;
            for (int i = 0; i < m_dayChecks.size(); ++i) {
                if (m_dayChecks.at(i)->isChecked())
                    days.append(i + 1);
            }
            obj["days"] = days;
        }
        return obj;
    }

    int rotationStyle() const { return m_rotationStyle->currentData().toInt(); }

private:
    QRadioButton *m_weekly{nullptr};
    QRadioButton *m_oneTime{nullptr};
    QList<QCheckBox *> m_dayChecks;
    QDateEdit *m_date{nullptr};
    QTimeEdit *m_start{nullptr};
    QTimeEdit *m_end{nullptr};
    QComboBox *m_rotationStyle{nullptr};
};

QString showtimeLabel(const QJsonObject &obj)
{
    const QString start = obj.value("start").toString("20:00");
    const QString end = obj.value("end").toString("00:00");
    const QString type = obj.value("type").toString("weekly");
    if (type == "one_time") {
        const QString date = obj.value("date").toString();
        return QString("One-time  %1  %2-%3").arg(date, start, end);
    }
    QStringList days;
    const QJsonArray dayArray = obj.value("days").toArray();
    for (const auto &d : dayArray)
        days << dayName(d.toInt());
    if (days.isEmpty())
        days << "No days";
    return QString("Weekly  [%1]  %2-%3").arg(days.join(", "), start, end);
}
} // namespace

DlgManageVenuesGigs::DlgManageVenuesGigs(Settings &settings, AutoKJServerClient &api, QWidget *parent)
    : QDialog(parent), m_settings(settings), m_api(api)
{
    setWindowTitle("Manage Venues & Showtimes");
    resize(860, 480);

    auto *mainLayout = new QVBoxLayout(this);
    auto *contentLayout = new QHBoxLayout();

    auto *venueColumn = new QVBoxLayout();
    venueColumn->addWidget(new QLabel("Venues", this));
    m_venueList = new QListWidget(this);
    m_venueList->setMinimumWidth(380);
    venueColumn->addWidget(m_venueList);

    auto *venueButtons = new QHBoxLayout();
    auto *btnVenueAdd = new QPushButton("Add", this);
    auto *btnVenueEdit = new QPushButton("Edit", this);
    auto *btnVenueRemove = new QPushButton("Remove", this);
    auto *btnVenueRefresh = new QPushButton("Refresh", this);
    venueButtons->addWidget(btnVenueAdd);
    venueButtons->addWidget(btnVenueEdit);
    venueButtons->addWidget(btnVenueRemove);
    venueButtons->addWidget(btnVenueRefresh);
    venueColumn->addLayout(venueButtons);

    auto *showtimeColumn = new QVBoxLayout();
    showtimeColumn->addWidget(new QLabel("Showtimes", this));
    m_showtimeList = new QListWidget(this);
    m_showtimeList->setMinimumWidth(380);
    showtimeColumn->addWidget(m_showtimeList);

    auto *showtimeButtons = new QHBoxLayout();
    auto *btnShowtimeAdd = new QPushButton("Add", this);
    auto *btnShowtimeEdit = new QPushButton("Edit", this);
    auto *btnShowtimeRemove = new QPushButton("Remove", this);
    auto *btnShowtimeRefresh = new QPushButton("Refresh", this);
    showtimeButtons->addWidget(btnShowtimeAdd);
    showtimeButtons->addWidget(btnShowtimeEdit);
    showtimeButtons->addWidget(btnShowtimeRemove);
    showtimeButtons->addWidget(btnShowtimeRefresh);
    showtimeColumn->addLayout(showtimeButtons);

    contentLayout->addLayout(venueColumn);
    contentLayout->addLayout(showtimeColumn);
    mainLayout->addLayout(contentLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    auto *closeRow = new QHBoxLayout();
    closeRow->addStretch();
    auto *closeBtn = new QPushButton("Close", this);
    closeRow->addWidget(closeBtn);
    mainLayout->addLayout(closeRow);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnVenueRefresh, &QPushButton::clicked, this, &DlgManageVenuesGigs::refreshAll);
    connect(btnShowtimeRefresh, &QPushButton::clicked, this, &DlgManageVenuesGigs::onVenueSelectionChanged);
    connect(m_venueList, &QListWidget::currentRowChanged, this, &DlgManageVenuesGigs::onVenueSelectionChanged);
    connect(btnVenueAdd, &QPushButton::clicked, this, &DlgManageVenuesGigs::addVenue);
    connect(btnVenueEdit, &QPushButton::clicked, this, &DlgManageVenuesGigs::editVenue);
    connect(btnVenueRemove, &QPushButton::clicked, this, &DlgManageVenuesGigs::removeVenue);
    connect(btnShowtimeAdd, &QPushButton::clicked, this, &DlgManageVenuesGigs::addShowtime);
    connect(btnShowtimeEdit, &QPushButton::clicked, this, &DlgManageVenuesGigs::editShowtime);
    connect(btnShowtimeRemove, &QPushButton::clicked, this, &DlgManageVenuesGigs::removeShowtime);

    refreshAll();
}

QString DlgManageVenuesGigs::baseHttpUrl() const
{
    QString base = m_settings.requestServerUrl();
    if (base.endsWith("/ws/kj"))
        base.chop(6);
    if (!base.startsWith("http://") && !base.startsWith("https://"))
        base = "http://" + base;
    return base;
}

QJsonDocument DlgManageVenuesGigs::requestJson(
    const QString &path,
    const QByteArray &method,
    const QJsonObject &payload,
    int *statusOut,
    QString *errorOut
)
{
    if (statusOut)
        *statusOut = 0;
    if (errorOut)
        errorOut->clear();

    const QString token = m_settings.requestServerToken();
    if (token.isEmpty()) {
        if (errorOut)
            *errorOut = "Not logged in. Please configure your email and password in settings.";
        return {};
    }

    QNetworkRequest request(QUrl(baseHttpUrl() + path));
    request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = nullptr;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (method == "GET")
        reply = m_network.get(request);
    else if (method == "POST")
        reply = m_network.post(request, body);
    else
        reply = m_network.sendCustomRequest(request, method, body);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray raw = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusOut)
        *statusOut = status;

    QJsonDocument parsed = QJsonDocument::fromJson(raw);
    if (reply->error() != QNetworkReply::NoError) {
        QString error = reply->errorString();
        if (parsed.isObject()) {
            const QString detail = parsed.object().value("detail").toString();
            if (!detail.isEmpty())
                error = detail;
        }
        if (errorOut)
            *errorOut = error;
    }

    reply->deleteLater();
    return parsed;
}

int DlgManageVenuesGigs::selectedVenueId() const
{
    auto *item = m_venueList->currentItem();
    if (!item)
        return -1;
    return item->data(Qt::UserRole).toInt();
}

int DlgManageVenuesGigs::selectedShowtimeIndex() const
{
    auto *item = m_showtimeList->currentItem();
    if (!item)
        return -1;
    return item->data(Qt::UserRole).toInt();
}

void DlgManageVenuesGigs::refreshAll()
{
    loadVenues();
    onVenueSelectionChanged();
}

void DlgManageVenuesGigs::loadVenues()
{
    int status = 0;
    QString error;
    const QJsonDocument doc = requestJson("/api/v1/venues", "GET", {}, &status, &error);
    if (status < 200 || status >= 300 || !doc.isArray()) {
        m_statusLabel->setText("Could not load venues: " + (error.isEmpty() ? "Unexpected response." : error));
        return;
    }

    const int prevVenue = selectedVenueId();
    m_venueList->blockSignals(true);
    m_venueList->clear();
    int rowToSelect = -1;

    const QJsonArray venues = doc.array();
    for (int i = 0; i < venues.size(); ++i) {
        const QJsonObject obj = venues.at(i).toObject();
        const int id = obj.value("id").toInt();
        const QString name = obj.value("name").toString();
        const QString address = obj.value("address").toString();
        const bool active = obj.value("has_active_show").toBool();
        const QString label = active ? QString("%1  [Active]").arg(name) : name;

        auto *item = new QListWidgetItem(label, m_venueList);
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, address);
        const QJsonValue latVal = obj.value("lat");
        const QJsonValue lonVal = obj.value("lon");
        item->setData(Qt::UserRole + 3, latVal.isDouble() ? latVal.toDouble() : QVariant());
        item->setData(Qt::UserRole + 4, lonVal.isDouble() ? lonVal.toDouble() : QVariant());
        if (id == prevVenue || (prevVenue <= 0 && i == 0))
            rowToSelect = i;
    }

    if (rowToSelect >= 0)
        m_venueList->setCurrentRow(rowToSelect);
    m_venueList->blockSignals(false);

    m_statusLabel->setText(QString("Loaded %1 venue(s).").arg(venues.size()));
}

void DlgManageVenuesGigs::loadShowtimesForVenue(int venueId)
{
    m_showtimeList->clear();
    if (venueId <= 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(m_settings.venueShowtimesJson(venueId).toUtf8());
    if (!doc.isArray())
        doc = QJsonDocument(QJsonArray());

    const QJsonArray arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject show = arr.at(i).toObject();
        auto *item = new QListWidgetItem(showtimeLabel(show), m_showtimeList);
        item->setData(Qt::UserRole, i);
        item->setData(Qt::UserRole + 1, show);
    }
}

void DlgManageVenuesGigs::onVenueSelectionChanged()
{
    loadShowtimesForVenue(selectedVenueId());
}

void DlgManageVenuesGigs::addVenue()
{
    DlgAddVenue dlg(this);
    dlg.setWindowTitle("Add Venue");
    if (dlg.exec() != QDialog::Accepted)
        return;

    QJsonObject payload;
    payload["name"] = dlg.venueName();
    payload["address"] = dlg.venueAddress();
    if (dlg.hasCoordinates()) {
        payload["lat"] = dlg.venueLat();
        payload["lon"] = dlg.venueLon();
    }

    int status = 0;
    QString error;
    requestJson("/api/v1/venues", "POST", payload, &status, &error);
    if (status < 200 || status >= 300) {
        QMessageBox::warning(this, "Add Venue Failed", error.isEmpty() ? "Unexpected response." : error);
        return;
    }

    refreshAll();
    m_api.refreshVenues();
}

void DlgManageVenuesGigs::editVenue()
{
    auto *item = m_venueList->currentItem();
    if (!item) {
        QMessageBox::information(this, "Edit Venue", "Select a venue first.");
        return;
    }
    const int venueId = item->data(Qt::UserRole).toInt();
    const QString oldName = item->text().replace("  [Active]", "");
    const QString oldAddress = item->data(Qt::UserRole + 1).toString();

    DlgAddVenue dlg(this);
    dlg.setWindowTitle("Edit Venue");
    dlg.setSubmitButtonText("Save Venue");
    dlg.setVenueName(oldName);
    dlg.setVenueAddress(oldAddress);
    const QVariant oldLat = item->data(Qt::UserRole + 3);
    const QVariant oldLon = item->data(Qt::UserRole + 4);
    if (oldLat.isValid() && oldLon.isValid())
        dlg.setVenueCoordinates(oldLat.toDouble(), oldLon.toDouble());
    if (dlg.exec() != QDialog::Accepted)
        return;

    QJsonObject payload;
    payload["name"] = dlg.venueName();
    payload["address"] = dlg.venueAddress();
    if (dlg.hasCoordinates()) {
        payload["lat"] = dlg.venueLat();
        payload["lon"] = dlg.venueLon();
    }

    int status = 0;
    QString error;
    requestJson(QString("/api/v1/venues/%1").arg(venueId), "PATCH", payload, &status, &error);
    if (status < 200 || status >= 300) {
        QMessageBox::warning(this, "Edit Venue Failed", error.isEmpty() ? "Unexpected response." : error);
        return;
    }

    refreshAll();
    m_api.refreshVenues();
}

void DlgManageVenuesGigs::removeVenue()
{
    const int venueId = selectedVenueId();
    if (venueId <= 0) {
        QMessageBox::information(this, "Remove Venue", "Select a venue first.");
        return;
    }

    if (QMessageBox::question(
            this,
            "Remove Venue",
            "Delete this venue?\n\nThis only works if the venue has no show history."
        ) != QMessageBox::Yes) {
        return;
    }

    int status = 0;
    QString error;
    requestJson(QString("/api/v1/venues/%1").arg(venueId), "DELETE", {}, &status, &error);
    if (status < 200 || status >= 300) {
        QMessageBox::warning(this, "Remove Venue Failed", error.isEmpty() ? "Unexpected response." : error);
        return;
    }

    refreshAll();
    m_api.refreshVenues();
}

void DlgManageVenuesGigs::addShowtime()
{
    const int venueId = selectedVenueId();
    if (venueId <= 0) {
        QMessageBox::information(this, "Showtimes", "Select a venue first.");
        return;
    }

    ShowtimeDialog dlg(this);
    dlg.setWindowTitle("Add Showtime");
    if (dlg.exec() != QDialog::Accepted)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(m_settings.venueShowtimesJson(venueId).toUtf8());
    QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray{};
    arr.append(dlg.toJson());
    m_settings.setVenueShowtimesJson(venueId, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    loadShowtimesForVenue(venueId);
}

void DlgManageVenuesGigs::editShowtime()
{
    const int venueId = selectedVenueId();
    const int idx = selectedShowtimeIndex();
    if (venueId <= 0 || idx < 0) {
        QMessageBox::information(this, "Showtimes", "Select a showtime first.");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(m_settings.venueShowtimesJson(venueId).toUtf8());
    QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray{};
    if (idx >= arr.size())
        return;

    ShowtimeDialog dlg(this);
    dlg.setWindowTitle("Edit Showtime");
    dlg.setFromJson(arr.at(idx).toObject());
    if (dlg.exec() != QDialog::Accepted)
        return;

    arr[idx] = dlg.toJson();
    m_settings.setVenueShowtimesJson(venueId, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    loadShowtimesForVenue(venueId);
}

void DlgManageVenuesGigs::removeShowtime()
{
    const int venueId = selectedVenueId();
    const int idx = selectedShowtimeIndex();
    if (venueId <= 0 || idx < 0) {
        QMessageBox::information(this, "Showtimes", "Select a showtime first.");
        return;
    }

    if (QMessageBox::question(this, "Remove Showtime", "Delete selected showtime?") != QMessageBox::Yes)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(m_settings.venueShowtimesJson(venueId).toUtf8());
    QJsonArray arr = doc.isArray() ? doc.array() : QJsonArray{};
    if (idx >= arr.size())
        return;
    arr.removeAt(idx);
    m_settings.setVenueShowtimesJson(venueId, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    loadShowtimesForVenue(venueId);
}

