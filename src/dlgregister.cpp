#include "dlgregister.h"
#include "ui_dlgregister.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>

DlgRegister::DlgRegister(Settings &settings, QWidget *parent)
    : QDialog(parent), ui(new Ui::DlgRegister), m_settings(settings)
{
    ui->setupUi(this);
    ui->labelError->hide();
}

DlgRegister::~DlgRegister()
{
    delete ui;
}

QString DlgRegister::registeredEmail() const    { return m_email; }
QString DlgRegister::registeredPassword() const { return m_password; }

void DlgRegister::on_btnCreate_clicked()
{
    const QString name     = ui->lineEditName->text().trimmed();
    const QString email    = ui->lineEditEmail->text().trimmed();
    const QString password = ui->lineEditPassword->text();
    const QString confirm  = ui->lineEditConfirm->text();

    if (name.isEmpty() || email.isEmpty() || password.isEmpty()) {
        showError("All fields are required.");
        return;
    }
    if (password.length() < 8) {
        showError("Password must be at least 8 characters.");
        return;
    }
    if (password != confirm) {
        showError("Passwords do not match.");
        return;
    }

    setWorking(true);

    QString baseUrl = m_settings.requestServerUrl();
    if (baseUrl.endsWith("/ws/kj"))
        baseUrl.chop(6);
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://"))
        baseUrl = "https://" + baseUrl;

    QNetworkRequest request(QUrl(baseUrl + "/api/v1/auth/register"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["email"]        = email;
    body["password"]     = password;
    body["display_name"] = name;

    QNetworkReply *reply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray responseBody = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    setWorking(false);

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);

    if (status == 201 || (status == 200 && doc.isObject() && doc.object().contains("access_token"))) {
        m_email    = email;
        m_password = password;
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
            for (const auto &v : d.toArray())
                if (v.isObject()) msgs << v.toObject().value("msg").toString();
            if (!msgs.isEmpty()) detail = msgs.join(", ");
        }
    }
    showError(detail);
}

void DlgRegister::setWorking(bool working)
{
    ui->btnCreate->setEnabled(!working);
    ui->btnCreate->setText(working ? "Creating…" : "Create Account");
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
