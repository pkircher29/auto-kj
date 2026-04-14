#include "dlgmanagedjs.h"
#include <QInputDialog>
#include <QMessageBox>

DlgManageDjs::DlgManageDjs(Settings &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle("Manage DJ Profiles");
    resize(400, 300);

    auto *mainLayout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    mainLayout->addWidget(m_list);

    auto *btnLayout = new QHBoxLayout();
    m_btnAdd = new QPushButton("Add DJ Profile", this);
    m_btnDelete = new QPushButton("Delete Selected", this);
    m_btnClose = new QPushButton("Close", this);

    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnDelete);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnClose);

    mainLayout->addLayout(btnLayout);

    connect(m_btnAdd, &QPushButton::clicked, this, &DlgManageDjs::onAddClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &DlgManageDjs::onDeleteClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);

    refreshList();
}

void DlgManageDjs::refreshList()
{
    m_list->clear();
    m_list->addItems(m_settings.djList());
}

void DlgManageDjs::onAddClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Add DJ", "DJ Name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QStringList djs = m_settings.djList();
        if (djs.contains(name.trimmed(), Qt::CaseInsensitive)) {
            QMessageBox::warning(this, "Duplicate", "A DJ with this name already exists.");
            return;
        }
        djs << name.trimmed();
        m_settings.setDjList(djs);
        refreshList();
    }
}

void DlgManageDjs::onDeleteClicked()
{
    auto *item = m_list->currentItem();
    if (!item) return;

    QString name = item->text();
    if (QMessageBox::question(this, "Confirm delete", QString("Are you sure you want to delete DJ profile '%1'?").arg(name)) == QMessageBox::Yes) {
        QStringList djs = m_settings.djList();
        djs.removeAll(name);
        m_settings.setDjList(djs);
        
        if (m_settings.activeDj() == name) {
            m_settings.setActiveDj("");
        }
        
        refreshList();
    }
}
