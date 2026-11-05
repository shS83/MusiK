#include "../src/seek_slider.h"
#include <QApplication>
#include <cstdlib>

static void check(bool ok,const char *message){if(!ok)qFatal("%s",message);}
static void click(SeekSlider &slider,const QPoint &point){
    QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&slider,&press);
    QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&slider,&release);
}
int main(int argc,char **argv){
    QApplication app(argc,argv);
    SeekSlider slider(Qt::Horizontal);
    slider.resize(400,30);
    slider.setRange(0,10000);
    slider.show();
    int count=0,requested=-1;
    int released=0;
    slider.trackClicked=[&](int value){++count;requested=value;};
    QObject::connect(&slider,&QSlider::sliderReleased,[&]{++released;});
    click(slider,{300,15});
    check(count==1 && released==0 && requested==slider.value() && requested>7000,"track click did not seek exactly once");
    click(slider,{300,15});
    check(count==1,"handle click triggered track seek");
    slider.setEnabled(false);
    click(slider,{100,15});
    check(count==1,"disabled timeline triggered seek");
    qInfo("Timeline click regression passed");
}
