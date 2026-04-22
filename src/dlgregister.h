#ifndef DLGREGISTER_H
#define DLGREGISTER_H

#include <QDialog>
#include <QNetworkAccessManager>
#include <QPointer>
#include "settings.h"

class QNetworkReply;
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
    void onRegisterFinished();
    void on_lineEditPassword_textChanged(const QString &);
    void on_lineEditConfirm_textChanged(const QString &);

private:
    Ui::DlgRegister *ui;
    Settings &m_settings;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_pendingReply;
    QString m_email;
    QString m_password;

    void setWorking(bool working);
    void showError(const QString &msg);
    void updateInlineValidation();
    bool passwordMeetsRequirements(const QString &password) const;
};

#endif // DLGREGISTER_H
