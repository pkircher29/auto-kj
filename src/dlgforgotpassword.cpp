#include "dlgforgotpassword.h"
#include "ui_dlgforgotpassword.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

DlgForgotPassword::DlgForgotPassword(Settings &settings, QWidget *parent)
    : QDialog(parent), ui(new Ui::DlgForgotPassword), m_settings(settings)
{
    ui->setupUi(this);
    setStep(Step::EnterEmail);
}

DlgForgotPassword::~DlgForgotPassword()
{
    if (m_pendingReply)
        m_pendingReply->abort();
    delete ui;
}

QString DlgForgotPassword::buildBaseUrl() const
{
    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;
    return baseUrl;
}

bool DlgForgotPassword::passwordMeetsRequirements(const QString &password) const
{
    static const QRegularExpression re(QStringLiteral("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d).{8,}$"));
    return re.match(password).hasMatch();
}

// ─── Step 1: Enter email ──────────────────────────────────────────

void DlgForgotPassword::on_btnSendReset_clicked()
{
    const QString email = ui->lineEditEmail->text().trimmed();
    if (email.isEmpty()) {
        showError("Please enter your email address.");
        return;
    }
    if (m_pendingReply)
        return;

    setWorking(true);
    ui->labelError->hide();

    QNetworkRequest request(QUrl(buildBaseUrl() + "/api/v1/auth/forgot-password"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["email"] = email;

    m_pendingReply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_pendingReply, &QNetworkReply::finished, this, &DlgForgotPassword::onForgotPasswordFinished);
}

void DlgForgotPassword::onForgotPasswordFinished()
{
    if (!m_pendingReply)
        return;

    QNetworkReply *reply = m_pendingReply;
    m_pendingReply.clear();
    reply->deleteLater();
    setWorking(false);

    // Server always returns 200 to prevent email enumeration
    showSuccess("If an account exists with that email, a reset link has been sent.\n\nCheck your inbox and paste the reset token below.");
    setStep(Step::EnterToken);
}

// ─── Step 2: Enter token + new password ───────────────────────────

void DlgForgotPassword::on_btnResetPassword_clicked()
{
    const QString token = ui->lineEditToken->text().trimmed();
    const QString newPassword = ui->lineEditNewPassword->text();
    const QString confirm = ui->lineEditConfirmPassword->text();

    if (token.isEmpty()) {
        showError("Please paste the reset token from your email.");
        return;
    }
    if (!passwordMeetsRequirements(newPassword)) {
        showError("Password must be at least 8 characters and include uppercase, lowercase, and a number.");
        return;
    }
    if (newPassword != confirm) {
        showError("Passwords do not match.");
        return;
    }
    if (m_pendingReply)
        return;

    setWorking(true);
    ui->labelError->hide();

    QNetworkRequest request(QUrl(buildBaseUrl() + "/api/v1/auth/reset-password"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["token"] = token;
    body["new_password"] = newPassword;

    m_pendingReply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_pendingReply, &QNetworkReply::finished, this, &DlgForgotPassword::onResetPasswordFinished);
}

void DlgForgotPassword::onResetPasswordFinished()
{
    if (!m_pendingReply)
        return;

    QNetworkReply *reply = m_pendingReply;
    m_pendingReply.clear();

    const QByteArray responseBody = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    setWorking(false);

    if (status == 200) {
        showSuccess("Password has been successfully reset.\nYou can now close this dialog and log in with your new password.");
        setStep(Step::Done);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    QString detail = "Reset failed.";
    if (doc.isObject()) {
        const QJsonValue d = doc.object().value("detail");
        if (d.isString())
            detail = d.toString();
    }
    showError(detail);
}

// ─── Inline validation ────────────────────────────────────────────

void DlgForgotPassword::on_lineEditNewPassword_textChanged(const QString &)
{
    if (m_step != Step::EnterToken || m_pendingReply)
        return;

    const QString password = ui->lineEditNewPassword->text();
    if (password.isEmpty()) {
        ui->labelError->hide();
        return;
    }

    if (!passwordMeetsRequirements(password)) {
        showError("Password must be at least 8 characters and include uppercase, lowercase, and a number.");
    } else {
        ui->labelError->hide();
    }
}

void DlgForgotPassword::on_lineEditConfirm_textChanged(const QString &)
{
    if (m_step != Step::EnterToken || m_pendingReply)
        return;

    const QString password = ui->lineEditNewPassword->text();
    const QString confirm = ui->lineEditConfirmPassword->text();

    if (confirm.isEmpty()) {
        ui->labelError->hide();
        return;
    }

    if (!password.isEmpty() && password != confirm) {
        showError("Passwords do not match.");
    } else {
        ui->labelError->hide();
    }
}

// ─── UI helpers ────────────────────────────────────────────────────

void DlgForgotPassword::setStep(Step step)
{
    m_step = step;

    ui->stackedWidget->setCurrentIndex(static_cast<int>(step));

    // Update dialog title
    switch (step) {
    case Step::EnterEmail:
        setWindowTitle("Forgot Password — Step 1 of 2");
        break;
    case Step::EnterToken:
        setWindowTitle("Forgot Password — Step 2 of 2");
        break;
    case Step::Done:
        setWindowTitle("Password Reset Complete");
        break;
    }
}

void DlgForgotPassword::setWorking(bool working)
{
    ui->btnSendReset->setEnabled(!working);
    ui->btnResetPassword->setEnabled(!working);
    ui->btnCancel->setEnabled(!working);

    if (m_step == Step::EnterEmail) {
        ui->btnSendReset->setText(working ? "Sending…" : "Send Reset Link");
        ui->lineEditEmail->setEnabled(!working);
    } else if (m_step == Step::EnterToken) {
        ui->btnResetPassword->setText(working ? "Resetting…" : "Reset Password");
        ui->lineEditToken->setEnabled(!working);
        ui->lineEditNewPassword->setEnabled(!working);
        ui->lineEditConfirmPassword->setEnabled(!working);
    }
}

void DlgForgotPassword::showError(const QString &msg)
{
    ui->labelError->setText(msg);
    ui->labelError->setStyleSheet("color: #ef2929; font-weight: bold;");
    ui->labelError->show();
}

void DlgForgotPassword::showSuccess(const QString &msg)
{
    ui->labelError->setText(msg);
    ui->labelError->setStyleSheet("color: #33d17a; font-weight: normal;");
    ui->labelError->show();
}