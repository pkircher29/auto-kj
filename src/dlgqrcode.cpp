#include "dlgqrcode.h"
#include "widgets/qrcodewidget.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QPixmap>
#include <QLabel>

DlgQRCode::DlgQRCode(const QString &venueUrl, QWidget *parent) : QDialog(parent), m_venueUrl(venueUrl) {
    setWindowTitle("Venue QR Code");
    
    auto layout = new QVBoxLayout(this);
    
    auto titleLabel = new QLabel(QString("Your Singer Request URL:\n%1").arg(venueUrl), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: bold;");
    layout->addWidget(titleLabel);

    m_qrWidget = new QRCodeWidget(this);
    m_qrWidget->setUrl(venueUrl);
    m_qrWidget->setMinimumSize(400, 400);
    layout->addWidget(m_qrWidget, 1, Qt::AlignCenter);

    auto btnLayout = new QHBoxLayout();
    auto btnSave = new QPushButton("Save as PNG...", this);
    auto btnClose = new QPushButton("Close", this);
    
    btnLayout->addStretch();
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnClose);
    
    layout->addLayout(btnLayout);

    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnSave, &QPushButton::clicked, this, &DlgQRCode::saveAsPng);

    resize(500, 550);
}

void DlgQRCode::saveAsPng() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save QR Code",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/venue_qr.png",
        "Images (*.png)");
        
    if (!fileName.isEmpty()) {
        QPixmap pixmap = m_qrWidget->grab();
        pixmap.save(fileName, "PNG");
    }
}
