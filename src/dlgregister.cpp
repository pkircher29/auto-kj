#include "dlgregister.h"
#include "ui_dlgregister.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

DlgRegister::DlgRegister(Settings &settings, QWidget *parent)
    : QDialog(parent), ui(new Ui::DlgRegister), m_settings(settings)
{
    ui->setupUi(this);
    ui->labelError->hide();
}

DlgRegister::~DlgRegister()
{
    if (m_pendingReply)
        m_pendingReply->abort();
    delete ui;
}

QString DlgRegister::registeredEmail() const
{
    return m_email;
}

QString DlgRegister::registeredPassword() const
{
    return m_password;
}
bool DlgRegister::passwordMeetsRequirements(const QString &password) const
{
    static const QRegularExpression re(QStringLiteral("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d).{8,}$"));
    return re.match(password).hasMatch();
}

void DlgRegister::updateInlineValidation()
{
    if (m_pendingReply)
        return;

    const QString password = ui->lineEditPassword->text();
    const QString confirm = ui->lineEditConfirm->text();

    if (password.isEmpty() && confirm.isEmpty()) {
        ui->labelError->hide();
        return;
    }

    if (!passwordMeetsRequirements(password)) {
        showError("Password must be at least 8 characters and include uppercase, lowercase, and a number.");
        return;
    }

    if (!confirm.isEmpty() && password != confirm) {
        showError("Passwords do not match.");
        return;
    }

    ui->labelError->hide();
}

void DlgRegister::on_lineEditPassword_textChanged(const QString &)
{
    updateInlineValidation();
}

void DlgRegister::on_lineEditConfirm_textChanged(const QString &)
{
    updateInlineValidation();
}

void DlgRegister::on_btnCreate_clicked()
{
    const QString name = ui->lineEditName->text().trimmed();
    const QString email = ui->lineEditEmail->text().trimmed();
    const QString password = ui->lineEditPassword->text();
    const QString confirm = ui->lineEditConfirm->text();

    if (name.isEmpty() || email.isEmpty() || password.isEmpty()) {
        showError("All fields are required.");
        return;
    }
    if (!passwordMeetsRequirements(password)) {
        showError("Password must be at least 8 characters and include uppercase, lowercase, and a number.");
        return;
    }
    if (password != confirm) {
        showError("Passwords do not match.");
        return;
    }
    if (m_pendingReply)
        return;

    if (m_pendingReply)
        return;

    setWorking(true);
    ui->labelError->hide();

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/auth/register"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["email"] = email;
    body["password"] = password;
    body["display_name"] = name;

    m_email = email;
    m_password = password;
    m_pendingReply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_pendingReply, &QNetworkReply::finished, this, &DlgRegister::onRegisterFinished);
}

void DlgRegister::onRegisterFinished()
{
    if (!m_pendingReply)
        return;

    QNetworkReply *reply = m_pendingReply;
    m_pendingReply.clear();

    const QByteArray responseBody = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    setWorking(false);

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);

    // The FastAPI /auth/register contract returns 201 Created on success.
    if (status == 201) {
        accept();
        return;
    }

    QString detail = "Registration failed.";
    if (doc.isObject()) {
        const QJsonValue d = doc.object().value("detail");
        if (d.isString())
            detail = d.toString();
        else if (d.isArray()) {
            QStringList msgs;
            for (const auto &v : d.toArray()) {
                if (v.isObject())
                    msgs << v.toObject().value("msg").toString();
            }
            if (!msgs.isEmpty())
                detail = msgs.join(", ");
        }
    }
    showError(detail);
}

void DlgRegister::setWorking(bool working)
{
    ui->btnCreate->setEnabled(!working);
    ui->btnCreate->setText(working ? "Creating…" : "Create Account");
    ui->btnCancel->setEnabled(!working);
    ui->lineEditName->setEnabled(!working);
    ui->lineEditEmail->setEnabled(!working);
    ui->lineEditPassword->setEnabled(!working);
    ui->lineEditConfirm->setEnabled(!working);
}

void DlgRegister::showError(const QString &msg)
{
    ui->labelError->setText(msg);
    ui->labelError->show();
}
