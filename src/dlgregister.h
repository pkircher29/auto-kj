#ifndef DLGREGISTER_H
#define DLGREGISTER_H

#include <QDialog>
#include <QNetworkAccessManager>
#include "settings.h"

namespace Ui { class DlgRegister; }

class DlgRegister : public QDialog
{
    Q_OBJECT
public:
    explicit DlgRegister(Settings &settings, QWidget *parent = nullptr);
    ~DlgRegister();

    QString registeredEmail() const;
    QString registeredPassword() const;

private slots:
    void on_btnCreate_clicked();
    void onRegistrationFinished();

private:
    Ui::DlgRegister *ui;
    Settings &m_settings;
    QNetworkAccessManager m_nam;
    QString m_email;
    QString m_password;

    void setWorking(bool working);
    void showError(const QString &msg);
};

#endif // DLGREGISTER_H
