#ifndef DLGALIASMANAGER_H
#define DLGALIASMANAGER_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include "settings.h"

class DlgAliasManager : public QDialog {
    Q_OBJECT
public:
    explicit DlgAliasManager(QWidget *parent = nullptr);

private slots:
    void loadAliases();
    void addAlias();
    void deleteSelected();
    void exportCsv();
    void importCsv();

private:
    QTableWidget *m_table;
    QLineEdit *m_editCanonical;
    QLineEdit *m_editAlias;
    QPushButton *m_btnAdd;
    QPushButton *m_btnDelete;
    QPushButton *m_btnExport;
    QPushButton *m_btnImport;
    Settings m_settings;
};

#endif // DLGALIASMANAGER_H
