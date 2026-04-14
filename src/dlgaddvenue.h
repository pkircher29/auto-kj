#ifndef DLGADDVENUE_H
#define DLGADDVENUE_H

#include <QDialog>
#include <QString>

namespace Ui {
class DlgAddVenue;
}

class DlgAddVenue : public QDialog
{
    Q_OBJECT

public:
    explicit DlgAddVenue(QWidget *parent = nullptr);
    ~DlgAddVenue();

    QString venueName() const;
    QString venueAddress() const;
    QString venuePin() const;
    void setVenueName(const QString &name);
    void setVenueAddress(const QString &address);
    void setVenuePin(const QString &pin);
    void setSubmitButtonText(const QString &text);

private slots:
    void on_btnCreate_clicked();
    void on_btnCancel_clicked();

private:
    Ui::DlgAddVenue *ui;
};

#endif // DLGADDVENUE_H
