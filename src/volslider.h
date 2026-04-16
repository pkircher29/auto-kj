#ifndef VOLSLIDER_H
#define VOLSLIDER_H

#include <QSlider>
#include <QWheelEvent>

// Minimal QSlider subclass that emits sliderMoved on wheel events
// instead of valueChanged, preventing feedback loops in volume control
class VolSlider : public QSlider
{
    Q_OBJECT
public:
    explicit VolSlider(QWidget *parent = nullptr) : QSlider(parent) {}

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        QAbstractSlider::wheelEvent(event);
        emit sliderMoved(value());
    }
};

#endif // VOLSLIDER_H