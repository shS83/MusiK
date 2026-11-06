#include "../src/attractor_pack_scene.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLVersionFunctionsFactory>
#include <QElapsedTimer>
#include <ctime>
#include <cmath>

static void check(bool result,const char *message){if(!result)qFatal("%s",message);}

static void gpuChecks(bool benchmark){
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);
    QOpenGLContext context;context.setFormat(format);check(context.create(),"GPU context creation failed");
    QOffscreenSurface surface;surface.setFormat(context.format());surface.create();
    check(context.makeCurrent(&surface),"GPU context activation failed");
    auto *gl=QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(&context);
    check(gl&&gl->initializeOpenGLFunctions(),"OpenGL 3.3 unavailable");
    qInfo("GPU: %s",gl->glGetString(GL_RENDERER));
    QOpenGLFramebufferObject target(1280,720);
    check(target.isValid(),"GPU test framebuffer invalid");
    const int frames=benchmark?90:30,count=benchmark?250000:12000;
    for(int index=0;index<4;++index){
        AttractorPackScene gpu(count);gpu.select(index);gpu.key(Qt::Key_T);
        for(int i=0;i<30;++i)check(gpu.paint(1./60,1280,720,target.handle(),.8f,.6f,.5f),qPrintable(gpu.gpuError()));
        gl->glFinish();QElapsedTimer clock;clock.start();const auto cpuStart=std::clock();
        for(int i=0;i<frames;++i)check(gpu.paint(1./60,1280,720,target.handle(),.8f,.6f,.5f),qPrintable(gpu.gpuError()));
        gl->glFinish(); // test/benchmark only, never used by the interactive renderer
        const double wall=clock.nsecsElapsed()/1e6/frames,cpu=1000.*(std::clock()-cpuStart)/CLOCKS_PER_SEC/frames;
        const auto image=target.toImage();
        int lit=0;for(int y=0;y<image.height();y+=5)for(int x=0;x<image.width();x+=5)
            if(image.pixelColor(x,y)!=QColor(Qt::black))++lit;
        check(lit>100,"GPU attractor is blank");
        check(gl->glGetError()==GL_NO_ERROR,"GPU render generated an OpenGL error");
        if(benchmark){
            AttractorPackScene reference(count);reference.select(index);reference.key(Qt::Key_T);
            for(int i=0;i<10;++i)reference.render(1./60,1280,720,.8f,.6f,.5f);
            clock.restart();
            for(int i=0;i<frames;++i)reference.render(1./60,1280,720,.8f,.6f,.5f);
            qInfo("%s: CPU reference %.2f ms/frame | GPU wall %.2f ms/frame | GPU process CPU %.2f ms/frame",
                  qPrintable(gpu.name()),clock.nsecsElapsed()/1e6/frames,wall,cpu);
        }
        gpu.reset();gpu.release();
        check(gpu.paint(.02,1280,720,target.handle(),0,0,0),"GPU resources did not recreate");
        gpu.release();
    }
}

int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    if(app.arguments().contains("--benchmark")){gpuChecks(true);return 0;}
    AttractorPackScene timer(100);
    check(!timer.hudVisible()&&timer.autoSwitch(),"Expected hidden HUD and enabled auto switch");
    check(timer.key(Qt::Key_H)&&timer.hudVisible(),"H must show HUD");
    check(timer.key(Qt::Key_H)&&!timer.hudVisible(),"H must hide HUD");
    timer.advanceClock(14.99);check(timer.current()==0,"Switched before 15 seconds");
    timer.advanceClock(.02);check(timer.current()==1,"Did not switch after 15 seconds");
    check(std::abs(timer.switchElapsed()-.01)<1e-5,"Timer lost remainder");
    timer.advanceClock(45);check(timer.current()==0,"Timer did not wrap");
    timer.key(Qt::Key_T);timer.advanceClock(100);
    check(!timer.autoSwitch()&&timer.current()==0&&timer.switchElapsed()==0,"T did not disable timer");
    timer.key(Qt::Key_T);timer.advanceClock(14);timer.key(Qt::Key_4);
    check(timer.current()==3&&timer.switchElapsed()==0,"Direct selection did not restart slot");
    timer.advanceClock(14.99);check(timer.current()==3,"Manual selection got a short slot");
    timer.advanceClock(.02);check(timer.current()==0,"Manual slot did not expire");
    timer.key(Qt::Key_Space);timer.render(15.1,40,30,0,0,0);
    check(timer.current()==1,"Switch timer followed clamped simulation time or drift pause");
    timer.key(Qt::Key_N);check(timer.current()==2,"N did not select next attractor");
    check(!timer.key(Qt::Key_F11),"Scene consumed fullscreen key");

    QImage previous;
    for(int index=0;index<4;++index){
        AttractorPackScene quiet(6000),loud(6000);
        quiet.select(index);loud.select(index);loud.beat();
        QImage a,b;
        for(int i=0;i<90;++i){
            a=quiet.render(1./60,240,180,0,0,0);
            b=loud.render(1./60,240,180,1,.8f,.7f);
        }
        int changed=0,lit=0;
        for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x){
            if(a.pixel(x,y)!=b.pixel(x,y))++changed;
            if(a.pixel(x,y)!=qRgb(0,0,0))++lit;
        }
        check(lit>500,"Attractor is blank");
        check(changed>500,"Music did not affect the image");
        if(!previous.isNull())check(a!=previous,"Attractor selection did not change image");
        previous=a;
        AttractorPackScene control(6000);control.select(index);
        for(int i=0;i<90;++i)control.render(1./60,240,180,0,0,0);
        quiet.burst();
        check(quiet.render(.02,240,180,0,0,0)!=control.render(.02,240,180,0,0,0),"Click burst had no effect");
        for(int i=0;i<4;++i){
            quiet.key(Qt::Key_C);
            check(quiet.render(.02,4000,2000,.9f,.6f,.7f).size()==QSize(1100,550),"Fullscreen render budget exceeded");
        }
        quiet.key(Qt::Key_R);quiet.reset();
        check(quiet.render(.02,1,1,0,0,0).size()==QSize(1,1),"Tiny viewport failed");
        AttractorPackScene muted(1000),silent(1000);
        muted.select(index);silent.select(index);
        muted.key(Qt::Key_A);silent.key(Qt::Key_A);muted.beat();
        check(!muted.audioEnabled(),"A did not disable audio");
        check(muted.render(.02,200,140,1,1,1)==silent.render(.02,200,140,0,0,0),"Audio/beat affected muted scene");
        // Extended high-energy integration catches runaway ODE trajectories.
        for(int i=0;i<600;++i)loud.render(.05,80,60,1,1,1);
        qInfo("%s render, audio, controls and stability passed",qPrintable(quiet.name()));
    }
    gpuChecks(false);
    qInfo("AttractorPack defaults, switching, CPU reference and all four GPU attractors passed");
}
