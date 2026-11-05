// Exercise the actual C++ visualization with a real OpenGL framebuffer.
#define main musik_application_main
#include "../src/main.cpp"
#undef main
#include <QEventLoop>

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);
    Visual visual;
    visual.resize(640,360);
    visual.setEffect(9);
    visual.show();
    QEventLoop loop;
    QTimer::singleShot(900,&loop,&QEventLoop::quit);
    visual.beat();
    loop.exec();
    auto image=visual.grabFramebuffer();
    if(!visual.context() || !visual.context()->isValid() || image.isNull())
        qFatal("Circler failed to create an OpenGL framebuffer");
    auto center=image.pixelColor(image.width()/2,image.height()/2);
    auto corner=image.pixelColor(0,0);
    if(corner.red()==0 && corner.green()==0 && corner.blue()==0)
        qFatal("Circler background was not rendered");
    if(center.red()==corner.red() && center.green()==corner.green() && center.blue()==corner.blue())
        qFatal("Circler radial background is missing");
    bool tetraVisible=false;
    for(int y=image.height()/2-50;y<image.height()/2+50 && !tetraVisible;++y)
        for(int x=image.width()/2-50;x<image.width()/2+50;++x){
            auto pixel=image.pixelColor(x,y);
            if(pixel.red()>80 || pixel.green()>80 || pixel.blue()>80){tetraVisible=true;break;}
        }
    if(!tetraVisible)qFatal("Circler tetrahedra were not rendered");
    qInfo("Circler OpenGL smoke test passed");
    return 0;
}
