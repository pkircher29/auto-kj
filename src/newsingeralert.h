#ifndef NEWSINGERALERT_H
#define NEWSINGERALERT_H

#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QWidget;
class Settings;

class NewSingerAlert : public QObject
{
    Q_OBJECT
public:
    explicit NewSingerAlert(Settings &settings, QObject *parent = nullptr);
    ~NewSingerAlert() override;

    void notifyNewSinger(const QString &singerName, QWidget *parentWindow = nullptr);

private:
    Settings &m_settings;
    QSystemTrayIcon *m_trayIcon = nullptr;

    void ensureTrayIcon(QWidget *parentWindow);
};

#endif // NEWSINGERALERT_H
