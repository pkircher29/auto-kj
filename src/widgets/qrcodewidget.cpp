#include "qrcodewidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <qrencode.h>

QRCodeWidget::QRCodeWidget(QWidget *parent) : QWidget(parent) {}

void QRCodeWidget::setUrl(const QString &url) {
    m_url = url;
    update();
}

void QRCodeWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (m_url.isEmpty()) return;

    QRcode *qrcode = QRcode_encodeString(m_url.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int w = this->width();
    int h = this->height();
    int size = std::min(w, h);
    
    // Draw white background
    painter.fillRect(0, 0, w, h, Qt::white);

    // Calculate scale
    int margin = 4;
    int codeSize = qrcode->width;
    double scale = static_cast<double>(size) / (codeSize + 2 * margin);

    // Draw QR code centered
    double offsetX = (w - size) / 2.0;
    double offsetY = (h - size) / 2.0;

    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);

    for (int y = 0; y < codeSize; ++y) {
        for (int x = 0; x < codeSize; ++x) {
            unsigned char b = qrcode->data[y * codeSize + x];
            if (b & 1) {
                painter.drawRect(offsetX + (x + margin) * scale, offsetY + (y + margin) * scale, scale, scale);
            }
        }
    }

    QRcode_free(qrcode);
}
