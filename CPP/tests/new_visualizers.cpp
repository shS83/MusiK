#define main musik_application_main
#include "../src/main.cpp"
#undef main
#include <QEventLoop>
#include <QTemporaryDir>

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
    for(int effect:{10,11,12}){
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
        if(effect==12){
            auto press=[&](int key){QKeyEvent event(QEvent::KeyPress,key,Qt::NoModifier);QApplication::sendEvent(&visual,&event);};
            QImage last;
            for(int index=0;index<4;++index){
                press(Qt::Key_1+index);
                visual.setSound({.8f,.6f,.5f,1.f});visual.beat();
                QEventLoop frame;QTimer::singleShot(250,&frame,&QEventLoop::quit);frame.exec();
                const auto shot=visual.grabFramebuffer();
                if(shot.isNull()||(!last.isNull()&&shot==last))qFatal("Attractor selection failed");
                shot.save(QDir::tempPath()+QString("/musik-attractor-pack-cpp-%1.png").arg(index));
                last=shot;
            }
            // Text uses this pale color; the HUD must start absent and toggle both ways.
            auto hudPixels=[&](){
                const auto shot=visual.grabFramebuffer();int count=0;
                const qreal ratio=visual.devicePixelRatioF();
                for(int y=qRound(16*ratio);y<qRound(40*ratio);++y)
                    for(int x=qRound(24*ratio);x<qRound(500*ratio);++x)
                        if(shot.pixelColor(x,y)==QColor("#c8f4ff"))++count;
                return count;
            };
            if(hudPixels()>0)qFatal("HUD must start hidden");
            press(Qt::Key_H);if(hudPixels()==0)qFatal("H did not show HUD");
            press(Qt::Key_H);if(hudPixels()>0)qFatal("H did not hide HUD");
            press(Qt::Key_T);press(Qt::Key_N);press(Qt::Key_T);press(Qt::Key_A);
            const QString cwd=QDir::currentPath();QTemporaryDir shots;
            if(!shots.isValid()||!QDir::setCurrent(shots.path()))qFatal("Cannot create screenshot test directory");
            press(Qt::Key_S);visual.grabFramebuffer();
            if(QDir(shots.path()).entryList({"attractorpack_*.png"},QDir::Files).size()!=1)qFatal("Screenshot not saved");
            QDir::setCurrent(cwd);
            visual.resize(360,640);visual.setSound({});
            QEventLoop resized;QTimer::singleShot(100,&resized,&QEventLoop::quit);resized.exec();
            if(visual.grabFramebuffer().isNull())qFatal("AttractorPack resize failed");
        }
        if(effect==10)fogImage=image;
        else {
            int differences=0;
            for(int y=0;y<image.height();y+=13)
                for(int x=0;x<image.width();x+=13)
                    if(image.pixelColor(x,y)!=fogImage.pixelColor(x,y))++differences;
            if(differences<100)qFatal("New visualizer did not replace the fog_shS scene");
        }
    }
    qInfo("fog_shS, Crawler and AttractorPack OpenGL smoke test passed");
    return 0;
}
