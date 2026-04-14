#include "dlgaddsinger.h"
#include "ui_dlgaddsinger.h"
#include <QMessageBox>


DlgAddSinger::DlgAddSinger(TableModelRotation &rotationModel, QWidget *parent) :
        QDialog(parent),
        m_rotModel(rotationModel),
        ui(new Ui::DlgAddSinger) {
    ui->setupUi(this);
    ui->cbxPosition->addItems({"Fair", "Bottom", "Next"});
    ui->cbxPosition->setCurrentIndex(m_settings.lastSingerAddPositionType());
    connect(ui->cbxPosition, qOverload<int>(&QComboBox::currentIndexChanged), &m_settings, &Settings::setLastSingerAddPositionType);
    connect(ui->cbxPosition, qOverload<int>(&QComboBox::currentIndexChanged), this, &DlgAddSinger::updateWaitTimeLabel);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DlgAddSinger::addSinger);

}

DlgAddSinger::~DlgAddSinger() = default;

#include "singeridentitymanager.h"
#include <QPushButton>

void DlgAddSinger::addSinger() {
    QString newName = ui->lineEditName->text();
    if (newName.trimmed() == "") {
        QMessageBox::warning(this, "Missing required field", "You must enter a singer name.");
        return;
    }
    
    if (m_rotModel.singerExists(newName)) {
        QMessageBox::warning(this, "Unable to add singer", "A singer with the same name already exists.");
        return;
    }

    SingerIdentityManager identityManager(m_rotModel);
    auto match = m_settings.fairnessEnabled() && m_settings.fairnessDetectNameChange()
        ? identityManager.checkForDuplicate(newName)
        : SingerIdentityManager::MatchResult{false, "", -1, 0.0f, ""};

    if (match.isDuplicate) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Possible Duplicate Detected");
        msgBox.setText(QString("Warning: '%1' may be the same person as '%2'.")
                       .arg(newName, match.matchedName));
        msgBox.setIcon(QMessageBox::Warning);

        QPushButton *addAnywayBtn = msgBox.addButton("Add Anyway", QMessageBox::AcceptRole);
        QPushButton *mergeBtn = msgBox.addButton("Merge", QMessageBox::AcceptRole);
        QPushButton *aliasBtn = msgBox.addButton("Mark as Alias", QMessageBox::AcceptRole);
        msgBox.addButton(QMessageBox::Cancel);

        msgBox.exec();

        if (msgBox.clickedButton() == addAnywayBtn) {
            // Do nothing, proceed to add
        } else if (msgBox.clickedButton() == mergeBtn) {
            identityManager.mergeSingers(match.singerId, newName, match.matchedName);
            ui->lineEditName->clear();
            close();
            return;
        } else if (msgBox.clickedButton() == aliasBtn) {
            identityManager.recordAlias(match.matchedName, newName);
            // Alias means they want to add them as alias, which really just means we merge them.
            identityManager.mergeSingers(match.singerId, newName, match.matchedName);
            ui->lineEditName->clear();
            close();
            return;
        } else {
            // Cancelled
            return;
        }
    }

    int newSingerId = m_rotModel.singerAdd(newName, ui->cbxPosition->currentIndex());
    emit newSingerAdded(m_rotModel.getSinger(newSingerId).position);
    ui->lineEditName->clear();
    close();
}

void DlgAddSinger::showEvent(QShowEvent *event) {
    ui->lineEditName->setFocus();
    updateWaitTimeLabel();
    QDialog::showEvent(event);
}

void DlgAddSinger::updateWaitTimeLabel() {
    int n = static_cast<int>(m_rotModel.singerCount());
    if (n == 0) {
        ui->labelWaitTime->setText("First in rotation");
        return;
    }
    int waitSecs = m_rotModel.positionWaitTime(n); // estimated wait at end
    if (ui->cbxPosition->currentIndex() == TableModelRotation::ADD_NEXT)
        waitSecs = m_rotModel.positionWaitTime(m_rotModel.getSinger(m_rotModel.currentSinger()).position + 1);
    else if (ui->cbxPosition->currentIndex() == TableModelRotation::ADD_FAIR)
        waitSecs = m_rotModel.rotationDuration() / (n + 1);
    if (waitSecs <= 0) {
        ui->labelWaitTime->setText("—");
        return;
    }
    int mins = waitSecs / 60;
    int secs = waitSecs % 60;
    ui->labelWaitTime->setText(QString("~%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
}
