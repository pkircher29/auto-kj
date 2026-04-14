#ifndef DLGQRCODE_H
#define DLGQRCODE_H

#include <QDialog>
#include <QString>

class QRCodeWidget;

class DlgQRCode : public QDialog {
    Q_OBJECT
public:
    explicit DlgQRCode(const QString &venueUrl, QWidget *parent = nullptr);

private slots:
    void saveAsPng();

private:
    QRCodeWidget *m_qrWidget;
    QString m_venueUrl;
};

#endif // DLGQRCODE_H
