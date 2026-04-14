#ifndef DLGMANAGEDJS_H
#define DLGMANAGEDJS_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "settings.h"

class DlgManageDjs : public QDialog {
    Q_OBJECT
public:
    explicit DlgManageDjs(Settings &settings, QWidget *parent = nullptr);

private slots:
    void onAddClicked();
    void onDeleteClicked();

private:
    void refreshList();
    Settings &m_settings;
    QListWidget *m_list;
    QPushButton *m_btnAdd;
    QPushButton *m_btnDelete;
    QPushButton *m_btnClose;
};

#endif // DLGMANAGEDJS_H
