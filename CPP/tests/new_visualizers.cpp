#define main musik_application_main
#include "../src/main.cpp"
#undef main
#include <QEventLoop>

int main(int argc,char **argv){
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);
    Visual visual;visual.resize(640,360);
    QVector<float> waveform(512);
    for(int i=0;i<waveform.size();++i)waveform[i]=std::sin(i*.12f)*.7f;
    visual.setWaveform(waveform);
    visual.setSound({.7f,.4f,.3f,1.f});
    QImage fogImage;
    for(int effect:{10,11}){
        visual.setEffect(effect);visual.show();visual.beat();
        QEventLoop loop;QTimer::singleShot(400,&loop,&QEventLoop::quit);loop.exec();
        const auto image=visual.grabFramebuffer();
        if(!visual.context()||!visual.context()->isValid()||image.isNull())
            qFatal("Visualizer did not create an OpenGL framebuffer");
        bool changed=false;
        auto first=image.pixelColor(0,0);
        for(int y=0;y<image.height()&&!changed;y+=13)
            for(int x=0;x<image.width();x+=13)
                if(image.pixelColor(x,y)!=first){changed=true;break;}
        if(!changed)qFatal("Visualizer rendered a flat framebuffer");
        if(effect==10)fogImage=image;
        else {
            int differences=0;
            for(int y=0;y<image.height();y+=13)
                for(int x=0;x<image.width();x+=13)
                    if(image.pixelColor(x,y)!=fogImage.pixelColor(x,y))++differences;
            if(differences<100)qFatal("Crawler did not replace the fog_shS scene");
        }
    }
    qInfo("fog_shS and Crawler OpenGL smoke test passed");
    return 0;
}
