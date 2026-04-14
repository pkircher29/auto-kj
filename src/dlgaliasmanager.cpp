#include "dlgaliasmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QHeaderView>
#include <QGroupBox>
#include <QFormLayout>

DlgAliasManager::DlgAliasManager(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Singer Alias Manager");
    resize(600, 480);

    auto *mainLayout = new QVBoxLayout(this);

    // Table
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Canonical Name", "Alias"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table);

    // Add row
    auto *addGroup = new QGroupBox("Add Alias", this);
    auto *formLayout = new QFormLayout(addGroup);
    m_editCanonical = new QLineEdit(this);
    m_editCanonical->setPlaceholderText("Canonical name (e.g. Bob)");
    m_editAlias = new QLineEdit(this);
    m_editAlias->setPlaceholderText("Alias name (e.g. Robert)");
    formLayout->addRow("Canonical:", m_editCanonical);
    formLayout->addRow("Alias:", m_editAlias);
    m_btnAdd = new QPushButton("Add Alias", this);
    formLayout->addRow(m_btnAdd);
    mainLayout->addWidget(addGroup);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    m_btnDelete = new QPushButton("Delete Selected", this);
    m_btnImport = new QPushButton("Import CSV", this);
    m_btnExport = new QPushButton("Export CSV", this);
    QPushButton *btnClose = new QPushButton("Close", this);
    btnLayout->addWidget(m_btnDelete);
    btnLayout->addWidget(m_btnImport);
    btnLayout->addWidget(m_btnExport);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    connect(m_btnAdd, &QPushButton::clicked, this, &DlgAliasManager::addAlias);
    connect(m_btnDelete, &QPushButton::clicked, this, &DlgAliasManager::deleteSelected);
    connect(m_btnExport, &QPushButton::clicked, this, &DlgAliasManager::exportCsv);
    connect(m_btnImport, &QPushButton::clicked, this, &DlgAliasManager::importCsv);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    loadAliases();
}

void DlgAliasManager::loadAliases() {
    m_table->setRowCount(0);
    QSqlQuery q("SELECT canonical_name, alias_name FROM singerAliases ORDER BY canonical_name, alias_name");
    while (q.next()) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(q.value(0).toString()));
        m_table->setItem(row, 1, new QTableWidgetItem(q.value(1).toString()));
    }
}

void DlgAliasManager::addAlias() {
    QString canonical = m_editCanonical->text().trimmed();
    QString alias = m_editAlias->text().trimmed();
    if (canonical.isEmpty() || alias.isEmpty()) {
        QMessageBox::warning(this, "Missing Field", "Both canonical name and alias are required.");
        return;
    }
    if (canonical.toLower() == alias.toLower()) {
        QMessageBox::warning(this, "Invalid", "Canonical name and alias cannot be the same.");
        return;
    }
    QSqlQuery q;
    q.prepare("INSERT INTO singerAliases (canonical_name, alias_name, first_seen, last_seen) "
              "VALUES (:c, :a, datetime('now'), datetime('now')) "
              "ON CONFLICT(alias_name) DO UPDATE SET canonical_name=:c, last_seen=datetime('now')");
    q.bindValue(":c", canonical);
    q.bindValue(":a", alias);
    if (!q.exec()) {
        QMessageBox::critical(this, "Error", "Failed to add alias: " + q.lastError().text());
        return;
    }
    m_editCanonical->clear();
    m_editAlias->clear();
    loadAliases();
}

void DlgAliasManager::deleteSelected() {
    int row = m_table->currentRow();
    if (row < 0) return;
    QString alias = m_table->item(row, 1)->text();
    QSqlQuery q;
    q.prepare("DELETE FROM singerAliases WHERE alias_name = :a COLLATE NOCASE");
    q.bindValue(":a", alias);
    q.exec();
    loadAliases();
}

void DlgAliasManager::exportCsv() {
    QString path = QFileDialog::getSaveFileName(this, "Export Aliases", {}, "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot open file for writing.");
        return;
    }
    QTextStream out(&file);
    out << "canonical_name,alias_name\n";
    for (int row = 0; row < m_table->rowCount(); ++row)
        out << "\"" << m_table->item(row, 0)->text() << "\",\""
            << m_table->item(row, 1)->text() << "\"\n";
    file.close();
}

void DlgAliasManager::importCsv() {
    QString path = QFileDialog::getOpenFileName(this, "Import Aliases", {}, "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot open file.");
        return;
    }
    QTextStream in(&file);
    QString header = in.readLine(); // skip header
    int imported = 0;
    QSqlQuery q;
    q.prepare("INSERT INTO singerAliases (canonical_name, alias_name, first_seen, last_seen) "
              "VALUES (:c, :a, datetime('now'), datetime('now')) "
              "ON CONFLICT(alias_name) DO UPDATE SET canonical_name=:c, last_seen=datetime('now')");
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(',');
        if (parts.size() < 2) continue;
        QString canonical = parts.at(0).trimmed().remove('"');
        QString alias = parts.at(1).trimmed().remove('"');
        if (canonical.isEmpty() || alias.isEmpty()) continue;
        q.bindValue(":c", canonical);
        q.bindValue(":a", alias);
        if (q.exec()) imported++;
    }
    file.close();
    QMessageBox::information(this, "Import Complete", QString("Imported %1 alias(es).").arg(imported));
    loadAliases();
}
