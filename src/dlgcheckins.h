#ifndef DLGCHECKINS_H
#define DLGCHECKINS_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include "models/tablemodelrotation.h"
#include "settings.h"
#include "autokjserverclient.h"

class DlgCheckins : public QDialog {
    Q_OBJECT
public:
    explicit DlgCheckins(TableModelRotation &rotModel, AutoKJServerClient &api, QWidget *parent = nullptr);

private slots:
    void refresh();
    void addSelectedToRotation();
    void checkinsFetched(const QJsonArray &checkins);

private:
    TableModelRotation &m_rotModel;
    AutoKJServerClient &m_api;
    Settings m_settings;
    QListWidget *m_listWidget;
    QPushButton *m_btnAdd;
    QPushButton *m_btnRefresh;
    QLabel *m_labelStatus;
    QTimer m_refreshTimer;

    struct CheckinEntry {
        QString id;
        QString singerName;
        QString checkedInAt;
    };
    QList<CheckinEntry> m_checkins;
};

#endif // DLGCHECKINS_H


