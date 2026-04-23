#ifndef DLGFORGOTPASSWORD_H
#define DLGFORGOTPASSWORD_H

#include <QDialog>
#include <QNetworkAccessManager>
#include <QPointer>
#include "settings.h"

class QNetworkReply;
namespace Ui { class DlgForgotPassword; }

class DlgForgotPassword : public QDialog
{
    Q_OBJECT
public:
    explicit DlgForgotPassword(Settings &settings, QWidget *parent = nullptr);
    ~DlgForgotPassword();

private slots:
    void on_btnSendReset_clicked();
    void onForgotPasswordFinished();
    void on_btnResetPassword_clicked();
    void onResetPasswordFinished();
    void on_lineEditNewPassword_textChanged(const QString &);
    void on_lineEditConfirm_textChanged(const QString &);

private:
    Ui::DlgForgotPassword *ui;
    Settings &m_settings;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_pendingReply;

    enum class Step { EnterEmail, EnterToken, Done };
    Step m_step = Step::EnterEmail;

    void setStep(Step step);
    void setWorking(bool working);
    void showError(const QString &msg);
    void showSuccess(const QString &msg);
    bool passwordMeetsRequirements(const QString &password) const;
    QString buildBaseUrl() const;
};

#endif // DLGFORGOTPASSWORD_H