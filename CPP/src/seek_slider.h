#pragma once

#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <functional>

class SeekSlider : public QSlider {
public:
    explicit SeekSlider(Qt::Orientation orientation, QWidget *parent=nullptr)
        : QSlider(orientation,parent) {}

    std::function<void(int)> trackClicked;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton || !isEnabled() || maximum()<=minimum()){
            QSlider::mousePressEvent(event);
            return;
        }
        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect handle=style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderHandle,this);
        if(handle.contains(event->position().toPoint())){
            QSlider::mousePressEvent(event);
            return;
        }
        const QRect groove=style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderGroove,this);
        const int span=std::max(1,groove.width()-handle.width());
        const int offset=event->position().toPoint().x()-groove.x()-handle.width()/2;
        const int value=QStyle::sliderValueFromPosition(minimum(),maximum(),offset,span,option.upsideDown);
        setValue(value);
        if(trackClicked)trackClicked(value);
        event->accept();
    }
};
