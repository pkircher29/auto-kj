#ifndef QRCODEWIDGET_H
#define QRCODEWIDGET_H

#include <QWidget>
#include <QString>

class QRCodeWidget : public QWidget {
    Q_OBJECT
public:
    explicit QRCodeWidget(QWidget *parent = nullptr);
    void setUrl(const QString &url);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_url;
};

#endif // QRCODEWIDGET_H
