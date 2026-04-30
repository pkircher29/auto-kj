#include "newsingeralert.h"

#include "settings.h"

#include <QApplication>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QWidget>

NewSingerAlert::NewSingerAlert(Settings &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
}

NewSingerAlert::~NewSingerAlert() = default;

void NewSingerAlert::ensureTrayIcon(QWidget *parentWindow)
{
    if (m_trayIcon || !QSystemTrayIcon::isSystemTrayAvailable())
        return;

    QIcon icon;
    if (parentWindow)
        icon = parentWindow->windowIcon();
    if (icon.isNull())
        icon = QIcon(":/Icons/autokjicon.svg");

    m_trayIcon = new QSystemTrayIcon(icon, this);
    m_trayIcon->setToolTip(QStringLiteral("Auto-KJ"));
    m_trayIcon->show();
}

void NewSingerAlert::notifyNewSinger(const QString &singerName, QWidget *parentWindow)
{
    if (!m_settings.newSingerAlertsEnabled())
        return;

    const QString title = QStringLiteral("New singer");
    const QString body = QStringLiteral("%1 just joined the rotation.").arg(singerName);

    ensureTrayIcon(parentWindow);
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, body, QSystemTrayIcon::Information, 5000);
    }

    QApplication::beep();
}
