#include "dlgcheckins.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QDateTime>

DlgCheckins::DlgCheckins(TableModelRotation &rotModel, AutoKJServerClient &api, QWidget *parent)
    : QDialog(parent), m_rotModel(rotModel), m_api(api) {
    setWindowTitle("Singer Check-ins");
    resize(400, 500);

    auto *mainLayout = new QVBoxLayout(this);

    m_labelStatus = new QLabel("Showing pending check-ins. Click a name then 'Add to Rotation'.", this);
    m_labelStatus->setWordWrap(true);
    mainLayout->addWidget(m_labelStatus);

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    mainLayout->addWidget(m_listWidget);

    auto *btnLayout = new QHBoxLayout();
    m_btnRefresh = new QPushButton("Refresh", this);
    m_btnAdd = new QPushButton("Add to Rotation", this);
    m_btnAdd->setEnabled(false);
    btnLayout->addWidget(m_btnRefresh);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnAdd);
    mainLayout->addLayout(btnLayout);

    connect(m_btnRefresh, &QPushButton::clicked, this, &DlgCheckins::refresh);
    connect(m_btnAdd, &QPushButton::clicked, this, &DlgCheckins::addSelectedToRotation);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, [this]() {
        m_btnAdd->setEnabled(m_listWidget->currentRow() >= 0);
    });
    connect(&m_api, &AutoKJServerClient::checkinsFetched, this, &DlgCheckins::checkinsFetched);

    // Auto-refresh every 15 seconds while visible
    connect(&m_refreshTimer, &QTimer::timeout, this, &DlgCheckins::refresh);
    m_refreshTimer.setInterval(15000);
    m_refreshTimer.start();

    refresh();
}

void DlgCheckins::refresh() {
    m_api.fetchCheckins();
}

void DlgCheckins::checkinsFetched(const QJsonArray &checkins) {
    m_checkins.clear();
    m_listWidget->clear();
    for (const QJsonValue &val : checkins) {
        QJsonObject obj = val.toObject();
        if (obj.value("added_to_rotation").toBool())
            continue; // skip already-added
        CheckinEntry entry;
        entry.id = obj.value("id").toString();
        entry.singerName = obj.value("singer_name").toString();
        entry.checkedInAt = obj.value("checked_in_at").toString();
        m_checkins.append(entry);

        QDateTime ts = QDateTime::fromString(entry.checkedInAt, Qt::ISODateWithMs);
        QString displayText = entry.singerName;
        if (ts.isValid())
            displayText += "  (" + ts.toLocalTime().toString("h:mm ap") + ")";
        m_listWidget->addItem(displayText);
    }
    if (m_checkins.isEmpty())
        m_labelStatus->setText("No pending check-ins.");
    else
        m_labelStatus->setText(QString("%1 singer(s) checked in and waiting.").arg(m_checkins.size()));
}

void DlgCheckins::addSelectedToRotation() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_checkins.size()) return;

    const CheckinEntry &entry = m_checkins.at(row);
    if (m_rotModel.singerExists(entry.singerName)) {
        QMessageBox::warning(this, "Already in Rotation",
                             QString("'%1' is already in the rotation.").arg(entry.singerName));
        return;
    }
    m_rotModel.singerAdd(entry.singerName, TableModelRotation::ADD_BOTTOM);
    m_api.markCheckinAdded(entry.id);
    m_checkins.removeAt(row);
    delete m_listWidget->takeItem(row);
    m_labelStatus->setText(QString("%1 singer(s) checked in and waiting.").arg(m_checkins.size()));
}


