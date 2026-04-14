#include "dlgaddvenue.h"
#include "ui_dlgaddvenue.h"
#include <QMessageBox>

DlgAddVenue::DlgAddVenue(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgAddVenue)
{
    ui->setupUi(this);
    ui->lineEditName->setMinimumWidth(320);
    ui->lineEditAddress->setMinimumWidth(320);
    ui->lineEditPin->setMinimumWidth(320);
    setMinimumWidth(560);
}

DlgAddVenue::~DlgAddVenue()
{
    delete ui;
}

QString DlgAddVenue::venueName() const
{
    return ui->lineEditName->text().trimmed();
}

QString DlgAddVenue::venueAddress() const
{
    return ui->lineEditAddress->text().trimmed();
}

QString DlgAddVenue::venuePin() const
{
    return ui->lineEditPin->text().trimmed();
}

void DlgAddVenue::setVenueName(const QString &name)
{
    ui->lineEditName->setText(name);
}

void DlgAddVenue::setVenueAddress(const QString &address)
{
    ui->lineEditAddress->setText(address);
}

void DlgAddVenue::setVenuePin(const QString &pin)
{
    ui->lineEditPin->setText(pin);
}

void DlgAddVenue::setSubmitButtonText(const QString &text)
{
    ui->btnCreate->setText(text);
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
