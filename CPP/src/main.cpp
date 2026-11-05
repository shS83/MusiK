#include <QTabWidget>
#include "audio_output.h"
#include "circler_scene.h"
#include "fog_scene.h"
#include "crawler_scene.h"
#include "seek_slider.h"
#include <QToolTip>
#include <QApplication>
#include <QMainWindow>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QElapsedTimer>
#include <QPainter>
#include <QRandomGenerator>
#include <QSlider>
#include <QLabel>
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QToolBar>
#include <QSurfaceFormat>
#include <QLibraryInfo>
#include <QDateTime>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <memory>
#include <QUrl>
#include <QMouseEvent>
#include <QMenu>
#include <QMediaMetaData>
#include <QDirIterator>
#include <QSet>
#include <QProgressBar>
#include <QFrame>
#include <QGroupBox>
#include <QStatusBar>
#include <QFontMetrics>
#include <QToolTip>
#include <QPainterPath>
#include <QShortcut>
#include <QKeySequence>
#include <QDialog>
#include "track_information.h"
#include <QFont>
#include <QPointer>
#include <QGraphicsDropShadowEffect>
#include <complex>
#include <array>
#include <QSpinBox>
#include <QFormLayout>
#include <QLinearGradient>
#include <QLineEdit>
#include <cstddef>
#include <QCheckBox>
#include <QColorDialog>
#include <QStyle>
#include <QGraphicsOpacityEffect>
#include <QFontDatabase>
#include <QScreen>
#include <cstdio>
#include <QElapsedTimer>
#include <QDebug>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct SoundState { float energy=0,mid=0,treble=0,pulse=0; };
static QList<QPair<QString,QString>> availableGlBackends(){
    QList<QPair<QString,QString>> choices{{"auto","Automatic (use launch environment)"}};
#ifdef Q_OS_LINUX
    const QString plugins=QLibraryInfo::path(QLibraryInfo::PluginsPath);
    if(!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && QFile::exists(plugins+"/platforms/libqwayland.so"))
        choices.append(qMakePair(QStringLiteral("wayland_egl"),QStringLiteral("Wayland / EGL")));
    if(!qEnvironmentVariableIsEmpty("DISPLAY") && QFile::exists(plugins+"/platforms/libqxcb.so")){
        if(QFile::exists(plugins+"/xcbglintegrations/libqxcb-glx-integration.so")){
            choices.append(qMakePair(QStringLiteral("x11_glx"),QStringLiteral("X11 / GLX")));
            if(QFile::exists("/usr/lib/dri/swrast_dri.so") || QFile::exists("/usr/lib64/dri/swrast_dri.so"))
                choices.append(qMakePair(QStringLiteral("software_glx"),QStringLiteral("X11 / GLX (Mesa software)")));
        }
        if(QFile::exists(plugins+"/xcbglintegrations/libqxcb-egl-integration.so"))
            choices.append(qMakePair(QStringLiteral("x11_egl"),QStringLiteral("X11 / EGL")));
    }
#endif
    return choices;
}
static void applyGlBackend(const QString &key){
    if(key=="auto")return;
    if(key=="wayland_egl"){
        qputenv("QT_QPA_PLATFORM","wayland");
        qunsetenv("QT_XCB_GL_INTEGRATION");
    }else if(key=="x11_glx" || key=="x11_egl" || key=="software_glx"){
        qputenv("QT_QPA_PLATFORM","xcb");
        qputenv("QT_XCB_GL_INTEGRATION",key=="x11_egl"?"xcb_egl":"xcb_glx");
    }else return;
    if(key=="software_glx")qputenv("LIBGL_ALWAYS_SOFTWARE","1");
    else qunsetenv("LIBGL_ALWAYS_SOFTWARE");
}
static constexpr float pi=3.14159265358979323846f;
// C++ port of musik/lasers.py LaserScene (five distinct variants).
class LaserScene {
public:
    explicit LaserScene(int variant=0):variant(variant),random(8100+variant) { resetParameters(); }
    void resetParameters(){
        static int counts[]={5,15,14,15,36}, per[]={32,48,48,180,150},sizes[]={12,12,27,18,36};
        static const char *chars[]={"•","•","?","O","´ "};
        static double sp[]={1,1,2.3,4.3,2.3};
        rays=counts[variant];perRay=per[variant];fontSize=sizes[variant];glyph=QString::fromUtf8(chars[variant]);spin=spinTarget=sp[variant];
    }
    void reset(){particles.clear();trails=QImage();emission=0;}
    void setPartyMode(bool on){partyMode=on;}
    void setPaletteBeats(int count){paletteBeats=std::max(1,count);}
    void onBeat(){if(++beatsSinceChange>=paletteBeats){beatsSinceChange=0;advancePalette();}}
    void advancePalette(){from=currentColor();++paletteIndex;to=paletteColor(paletteIndex);mix=0;elapsedSincePalette=0;}
    void phrase(){
        if(variant==4)return; // Preserve v4.3 glyph and geometry EXACTLY.
        if(!partyMode)return;
        QVector<int> options=variant<2?QVector<int>{5,8,12,16}:QVector<int>{8,12,16,20,28};
        rays=options[int(random()%options.size())];
        QVector<int> lengths={24,36,48,64,80};perRay=lengths[int(random()%lengths.size())];
        spinTarget=(random()%2?1:-1)*(.7+double(random()%4100)/1000.0);
        // Preserve each variant’s original glyph and font size even in Party Mode.
        from=currentColor();to=paletteColor(int(random()%16));mix=0;
    }
    void paint(QWidget *widget,float dt,const SoundState &sound){
        const double scale=std::min({1.,1280./std::max(1,widget->width()),800./std::max(1,widget->height())});
        const int w=std::max(80,int(std::round(widget->width()*scale))),h=std::max(60,int(std::round(widget->height()*scale)));
        if(trails.size()!=QSize(w,h))reset();
        elapsed+=dt;elapsedSincePalette+=dt;
        if(elapsedSincePalette >= 8.0)advancePalette();
        spin+=(spinTarget-spin)*(1-std::exp(-dt/.4));
        double drive=variant==4?1.0:(.08+1.1*sound.energy+.45*sound.mid+.30*sound.treble);
        phase+=dt*spin*drive;orbit+=dt*(variant==4?.65:spin*(.025+.55*sound.energy+.25*sound.mid));
        distance+=dt*(variant==4?180.0:(8+180*sound.energy+60*sound.pulse));mix=std::min(1.,mix+dt*.5);
        const double radius=variant==4?50.0*std::min(w/800.,h/600.):std::min(w,h)*(variant==2?.23:.085);
        QPointF target(w/2.+radius*std::cos(orbit),h/2.+radius*std::sin(orbit));
        const double perimeter=2.*(w-20+h-20);
        QImage layer(w,h,QImage::Format_ARGB32_Premultiplied);layer.fill(Qt::transparent);
        QPainter painter(&layer);painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);painter.setBrush(Qt::white);
        if(variant<2){
            // Closed, overlapping antialiased ribbons. Unlike sparse glyphs, filled
            // shapes have no perforated interior and remain smooth at all rotations.
            // Neighboring ribbons overlap with a soft gradient to suppress black noise.
            painter.setRenderHint(QPainter::Antialiasing,true);
            const int strands=variant==0?36:48;
            for(int ray=0;ray<strands;++ray){
                double a=2*pi*ray/strands + phase*(variant==0?.055:.09);
                QPointF edge(target.x()+std::cos(a)*std::hypot(w,h),target.y()+std::sin(a)*std::hypot(w,h));
                QPointF tangent(-std::sin(a),std::cos(a));
                QPainterPath ribbon;
                const int steps=44;
                auto at=[&](double t,double side){
                    double taper=std::pow(t,.62);
                    double wiggle=std::sin(t*8.+phase*.48+ray*.28)*(variant==0?9.:16.)*taper*(.35+sound.mid);
                    double halfWidth=(2.4+(variant==0?24.:19.)*taper)*(1.+sound.pulse*.14);
                    return QPointF(target.x()+(edge.x()-target.x())*t+tangent.x()*(wiggle+side*halfWidth),
                                   target.y()+(edge.y()-target.y())*t+tangent.y()*(wiggle+side*halfWidth));
                };
                ribbon.moveTo(at(0,1));
                for(int k=1;k<=steps;k++)ribbon.lineTo(at(double(k)/steps,1));
                for(int k=steps;k>=0;k--)ribbon.lineTo(at(double(k)/steps,-1));
                ribbon.closeSubpath();
                QLinearGradient grad(target,edge);grad.setColorAt(0,QColor(255,255,255,245));
                grad.setColorAt(.55,QColor(255,255,255,175));grad.setColorAt(1,QColor(255,255,255,105));
                painter.fillPath(ribbon,grad);
            }
        }else{
            if(cache.isEmpty())buildCache();
            for(int ray=0;ray<rays;ray++){
                QPointF start=edgePosition(ray*perimeter/rays+distance,w,h);
                double base=std::atan2(target.y()-start.y(),target.x()-start.x());
                for(int j=0;j<perRay;j++){
                    double t=(j+.25)/(perRay+.7);
                    QPointF at=start+(target-start)*t;
                    double wave=phase-j*.32+ray*.24;
                    double angle=base+(variant==2? -elapsed*2.3+ray*.13 : wave);
                    if(variant==3)at+=QPointF(std::sin(elapsed*2.1+j*.10+ray)*sound.mid*11.0, std::cos(elapsed*1.7+j*.13+ray)*sound.treble*9.0);
                    int k=(int(std::round(angle/(2*pi)*72))%72+72)%72;
                    const QImage &g=cache[k];painter.drawImage(QPointF(std::round(at.x()-g.width()/2.),std::round(at.y()-g.height()/2.)),g);
                }
            }
        }
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);painter.fillRect(layer.rect(),currentColor());painter.end();
        QImage *out=&layer;
        if(variant==4){
            if(trails.isNull()){trails=QImage(w,h,QImage::Format_ARGB32_Premultiplied);trails.fill(Qt::transparent);}
            QPainter feedback(&trails);feedback.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            feedback.fillRect(trails.rect(),QColor(255,255,255,int(std::round(255*std::pow(.5,dt/.15)))));
            if(variant==4){feedback.setCompositionMode(QPainter::CompositionMode_SourceIn);feedback.fillRect(trails.rect(),currentColor());}
            feedback.setCompositionMode(QPainter::CompositionMode_SourceOver);feedback.drawImage(0,0,layer);feedback.end();out=&trails;
        }
        QPainter display(widget);display.fillRect(widget->rect(),Qt::black);display.setRenderHint(QPainter::SmoothPixmapTransform);
        display.drawImage(widget->rect(),*out);
        // Avoid full-screen white flashes: they hide the laser surface details.
    }
private:
    struct Particle{QPointF start,captured;double progress;};
    static QPointF edgePosition(double distance,int w,int h){
        double a=std::max(1,w-20),b=std::max(1,h-20),d=std::fmod(distance,2*(a+b));
        if(d<a)return {10+d,10};if(d<a+b)return {double(w-10),10+d-a};
        if(d<2*a+b)return {w-10-(d-a-b),double(h-10)};return {10.,h-10-(d-2*a-b)};
    }
    static QColor paletteColor(int n){static const std::array<QColor,16> colors={QColor(255,0,0),QColor(128,0,128),QColor(255,255,0),QColor(0,255,0),QColor(0,0,255),QColor(0,255,255),QColor(255,0,255),QColor(128,128,0),QColor(0,128,128),QColor(255,128,128),QColor(128,0,0),QColor(128,128,128),QColor(255,255,255),QColor(128,255,128),QColor(255,128,255),QColor(128,255,255)};return colors[size_t((n%16+16)%16)];}
    QColor currentColor()const{double t=mix*mix*(3-2*mix);return QColor(int(std::round(from.red()+(to.red()-from.red())*t)),int(std::round(from.green()+(to.green()-from.green())*t)),int(std::round(from.blue()+(to.blue()-from.blue())*t)));}
    void buildCache(){
        cache.clear();QFont font("DejaVu Sans Mono");font.setPixelSize(fontSize);
        QPainterPath path;path.addText(0,0,font,glyph);auto bounds=path.boundingRect();
        QTransform transform;transform.translate(-bounds.center().x(),-bounds.center().y());path=transform.map(path);
        int side=int(std::ceil(std::hypot(bounds.width(),bounds.height())))+6;
        for(int k=0;k<72;k++){
            QImage img(side,side,QImage::Format_ARGB32_Premultiplied);img.fill(Qt::transparent);
            QPainter paint(&img);paint.setRenderHint(QPainter::Antialiasing);paint.translate(side/2.,side/2.);paint.rotate(k*5.);paint.fillPath(path,Qt::white);cache.push_back(img);
        }
    }
    int variant,rays=5,perRay=32,fontSize=12;QString glyph;double spin=1,spinTarget=1,phase=0,orbit=0,distance=0,elapsed=0,emission=0,mix=0,elapsedSincePalette=0;int paletteIndex=0,paletteBeats=16,beatsSinceChange=0;bool partyMode=false;
    QColor from=QColor(255,0,0),to=QColor(128,0,128);QImage trails;QVector<QImage> cache;
    std::vector<Particle> particles;std::mt19937 random;
};

class Visual : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    struct FireRocket{QVector2D pos,vel,start;QColor color;float fuse=0,gravity=190.f,hover=0.f;bool hoverAtApex=false,waiting=false,active=false,finale=false;int style=0;};
    struct FireSpark{QVector2D pos,vel;QColor color;float life=0,maxLife=1,size=2,drag=.98f,angle=0,rotation=0,gravity=70;int shape=0;bool active=false;};
    struct CrackleBurst{QVector2D pos,vel;QColor color;float delay=0;};
    struct Comet { QVector2D origin, velocity; QColor color; float start, scale; };
public:
    explicit Visual(QWidget *p=nullptr):QOpenGLWidget(p),rng(1731) {
        setWindowFlag(Qt::Window,true); resize(1280,720);
        timer.setTimerType(Qt::PreciseTimer);timer.setInterval(6); QObject::connect(&timer,&QTimer::timeout,this,[this]{update();});
        clock.start(); last=clock.elapsed();
    }
    static int laserVariant(int value){return value==4?1:(value==5?3:4);}
    void setEffect(int value) { if(effect!=value && value==9)circler.reset(); if(effect!=value && value==10)fog.reset(); if(effect!=value && value==11)crawler.reset(); effect=value; if(effect>=4 && effect<=6){laser=std::make_unique<LaserScene>(laserVariant(effect));laser->setPartyMode(partyMode);laser->setPaletteBeats(paletteBeats);} setWindowTitle(QString("μSIK · %1 · F11 fullscreen · Esc close").arg(names().value(effect))); cleanup(); update(); }
    static QStringList names(){return {"Chrome sphere","PsyTunnel","Hiss","Matrixia","Orbit Streams","Glyph Spin","Glyph Trails","Biolume","Fireworks","Circler Tetra","fog_shS","Crawler"};}
    void setSound(SoundState s){ sound=s; }
    void setWaveform(const QVector<float> &samples){fog.setWaveform(samples);}
    void changeLaserPhrase(){if(laser)laser->phrase();}
    void beat(){if(laser)laser->onBeat();if(effect==9)circler.beat();if(effect==10)fog.beat();if(effect==11)crawler.beat();}
    void setPartyMode(bool enabled){partyMode=enabled;if(laser)laser->setPartyMode(enabled);}
    void setPaletteBeats(int beats){paletteBeats=beats;if(laser)laser->setPaletteBeats(beats);}
protected:
#ifdef Q_OS_WIN
    void printDisplayMode(const char *reason){
        HWND hwnd = reinterpret_cast<HWND>(winId());
        HMONITOR monitor = MonitorFromWindow(hwnd ? hwnd : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW info{}; info.cbSize = sizeof(info);
        DEVMODEW dm{}; dm.dmSize = sizeof(dm);
        if(GetMonitorInfoW(monitor, &info) && EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &dm)){
            std::fprintf(stderr, "[MusiK] %s -> %lu x %lu, %u bpp, %lu Hz\n",
                         reason,
                         static_cast<unsigned long>(dm.dmPelsWidth),
                         static_cast<unsigned long>(dm.dmPelsHeight),
                         static_cast<unsigned>(dm.dmBitsPerPel),
                         static_cast<unsigned long>(dm.dmDisplayFrequency));
            std::fflush(stderr);
        }
    }
#else
    void printDisplayMode(const char *reason){
        if(QScreen *s = screen()){
            std::fprintf(stderr, "[MusiK] %s -> %d x %d, %d bpp, %.2f Hz\n",
                         reason, s->geometry().width(), s->geometry().height(), 32, s->refreshRate());
            std::fflush(stderr);
        }
    }
#endif
    void showEvent(QShowEvent *e) override {QOpenGLWidget::showEvent(e);timer.start();}
    void hideEvent(QHideEvent *e) override {timer.stop();QOpenGLWidget::hideEvent(e);}
    void toggleBorderless(){
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(winId());
        if (!hwnd) return;
        printDisplayMode(borderless ? "before restore" : "before borderless");
        if (!borderless) {
            nativeStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
            nativeExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            nativePlacement.length = sizeof(WINDOWPLACEMENT);
            if (!GetWindowPlacement(hwnd, &nativePlacement)) return;
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (!GetMonitorInfoW(monitor, &info)) return;
            const RECT &r = info.rcMonitor;
            LONG_PTR style = nativeStyle;
            style &= ~(static_cast<LONG_PTR>(WS_MAXIMIZE) | static_cast<LONG_PTR>(WS_CAPTION) | static_cast<LONG_PTR>(WS_THICKFRAME));
            style &= ~(static_cast<LONG_PTR>(WS_MINIMIZEBOX) | static_cast<LONG_PTR>(WS_MAXIMIZEBOX) | static_cast<LONG_PTR>(WS_SYSMENU));
            LONG_PTR exStyle = nativeExStyle;
            exStyle &= ~(static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME) | static_cast<LONG_PTR>(WS_EX_CLIENTEDGE) | static_cast<LONG_PTR>(WS_EX_STATICEDGE) | static_cast<LONG_PTR>(WS_EX_WINDOWEDGE));
            // A fully opaque, monitor-sized OpenGL window can be promoted by
            // the driver to fullscreen scanout even without a mode-set API.
            // Require desktop blending before resizing (254/255 opacity).
            // Keep the existing HWND/context; do not use Qt::WindowFullScreen.
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            if (!SetLayeredWindowAttributes(hwnd, 0, 254, LWA_ALPHA)) {
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, nativeExStyle);
                qWarning("Could not enable desktop-composited borderless window");
                return;
            }
            SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_VISIBLE);
            if (!SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right-r.left, r.bottom-r.top,
                              SWP_FRAMECHANGED | SWP_SHOWWINDOW)) {
                SetWindowLongPtrW(hwnd, GWL_STYLE, nativeStyle);
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, nativeExStyle);
                return;
            }
            borderless = true;
        } else {
            SetWindowLongPtrW(hwnd, GWL_STYLE, nativeStyle);
            SetWindowPlacement(hwnd, &nativePlacement);
            // Remove the blending guard only after restoring the window size.
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, nativeExStyle);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            borderless = false;
        }
#else
        if (!borderless) {
            savedGeometry = geometry();
            savedFlags = windowFlags();
            QScreen *display = screen();
            if (!display) return;
            setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
            showNormal();
            setGeometry(display->geometry());
            show();
            borderless = true;
        } else {
            setWindowFlags(savedFlags);
            showNormal();
            setGeometry(savedGeometry);
            show();
            borderless = false;
        }
#endif
        raise();
        activateWindow();
        QTimer::singleShot(250, this, [this](){ printDisplayMode(borderless ? "after borderless" : "after restore"); });
    }
    void keyPressEvent(QKeyEvent *e) override {
        if(effect==11 && crawler.key(e->key())){update();e->accept();return;}
        if(effect==8 && (e->key()==Qt::Key_section || e->text()==QStringLiteral("§"))){
            fireworksDebug=!fireworksDebug;
            QToolTip::showText(mapToGlobal(QPoint(width()/2,28)),fireworksDebug?"DEBUG MODE ON":"DEBUG MODE OFF",this);
            e->accept();return;
        }
        if(effect==8 && fireworksDebug && e->key()>=Qt::Key_1 && e->key()<=Qt::Key_9){
            if(!fireworksReady)resetFireworks();
            FireRocket rocket;
            rocket.pos={bioRand(.28f,.72f)*width(),bioRand(.20f,.45f)*height()};
            rocket.color=fireworksColor();rocket.finale=e->key()==Qt::Key_6;
            explodeRocket(rocket,e->key()-Qt::Key_0);update();e->accept();return;
        }
        if(e->key()==Qt::Key_F11){toggleBorderless();e->accept();return;}
        if(e->key()==Qt::Key_Escape){if(borderless)toggleBorderless();else close();e->accept();return;}
        QOpenGLWidget::keyPressEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {if(effect==3){color=(color+1)%7;glyphCache.clear();update();}QOpenGLWidget::mousePressEvent(e);}
    void initializeGL() override {initializeOpenGLFunctions();glGenVertexArrays(1,&vao);glBindVertexArray(vao);glDisable(GL_DEPTH_TEST);}
    void cleanup() {if(!context()){program.reset();cube.reset();return;}makeCurrent();program.reset();cube.reset();circler.release();fog.release();crawler.release();if(texture){glDeleteTextures(1,&texture);texture=0;}if(fbo){glDeleteFramebuffers(1,&fbo);fbo=0;}doneCurrent();}
    static QByteArray read(const QString &name){QFile f(QCoreApplication::applicationDirPath()+"/shaders/"+name);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
    std::unique_ptr<QOpenGLShaderProgram> shader(const QString &frag){
        auto p=std::make_unique<QOpenGLShaderProgram>();auto vert=read("scene.vert"),fs=read(frag);
        if(vert.isEmpty()||fs.isEmpty()||!p->addShaderFromSourceCode(QOpenGLShader::Vertex,vert)||!p->addShaderFromSourceCode(QOpenGLShader::Fragment,fs)||!p->link()){
            error="Shader error ("+frag+"): "+p->log();return {};
        }return p;
    }
    void render(QOpenGLShaderProgram &p){p.bind();glDrawArrays(GL_TRIANGLES,0,3);p.release();}
    void setup(){
        error.clear();
        if(effect==0)program=shader("scene.frag");
        else if(effect==1)program=shader("tunnel.frag");
        else if(effect==2){cube=shader("hiss_scene.frag");program=shader("hiss.frag");}
        else if(effect==7)program=shader("biolume.frag");
    }
    void ensureFbo(int w,int h){
        if(fbo && fw==w && fh==h)return;
        if(texture)glDeleteTextures(1,&texture);
        if(fbo)glDeleteFramebuffers(1,&fbo);
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());fw=w;fh=h;
    }
    void glyphs(){
        glyphCache.clear();const QString chars=QString::fromUtf8("アイウエオカキクケコサシスセソタチツテトナニヌネノ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        QStringList palette={"#38ff75","#4414ff","#ffb448","#ff55bb","#b394ff","#fafafa","#ff5757"};
        QFont font("Yu Gothic");font.setPixelSize(20);
        for(auto tint:{palette[color],QString("#edfff4")}){
            std::vector<QPixmap> pix;
            for(auto ch:chars){QPixmap p(qRound(24*devicePixelRatioF()),qRound(27*devicePixelRatioF()));p.setDevicePixelRatio(devicePixelRatioF());p.fill(Qt::transparent);QPainter a(&p);a.setFont(font);a.setPen(QColor(tint));a.drawText(0,21,QString(ch));pix.push_back(p);}glyphCache.push_back(pix);
        }
        pixelRatio=devicePixelRatioF();
    }
    struct Glyph{int row,index;};
    struct Stream{double y,speed;std::vector<Glyph> chars;};
    void matrix(float dt){
        if(glyphCache.empty()||pixelRatio!=devicePixelRatioF())glyphs();
        int cols=std::max(1,width()/24+1);std::uniform_real_distribution<double> speed(.6,1.5),start(-height(),0);
        while(int(streams.size())<cols)streams.push_back({start(rng),speed(rng),{}});
        streams.resize(cols);
        float rate=8+190*energy+100*sound.mid+100*sound.treble+260*sound.pulse;
        QPainter painter(this);painter.fillRect(rect(),QColor("#020604"));
        for(int x=0;x<cols;x++){
            auto &s=streams[x];double next=s.y+dt*rate*s.speed;
            int from=int(std::floor(s.y/27.0)),to=int(std::floor(next/27.0));
            for(int r=from+1;r<=to;r++)s.chars.insert(s.chars.begin(),{r,int(rng()%glyphCache[0].size())});
            if(s.chars.size()>22)s.chars.resize(22);
            s.y=next;
            for(size_t i=0;i<s.chars.size();i++){
                auto g=s.chars[i];int y=g.row*27;if(y< -27||y>height())continue;
                painter.setOpacity(std::pow(1.0-double(i)/22.0,1.6));
                painter.drawPixmap(x*24,y,glyphCache[i==0?1:0][g.index]);
            }
            if(s.y>height()+27*22){std::uniform_real_distribution<double> restart(27,std::max(28.,height()*.4));s.y=-restart(rng);s.chars.clear();}
        }
        painter.end();
    }
    // Predator/prey simulation. Coordinates are normalized by viewport height.
    // No flies spawn in view: respawns enter from just beyond the four edges.
    float bioRand(float lo,float hi){return lo+(hi-lo)*float(rng()%100000)/99999.f;}
    QVector2D bioEdgePoint(bool cell=false){
        const float aspect=float(std::max(1,width()))/float(std::max(1,height()));
        const float margin=cell?.14f:.07f;
        switch(rng()%4){
            case 0:return QVector2D(-.5f*aspect-margin,bioRand(-.55f,.55f));
            case 1:return QVector2D( .5f*aspect+margin,bioRand(-.55f,.55f));
            case 2:return QVector2D(bioRand(-.55f*aspect,.55f*aspect),-.5f-margin);
            default:return QVector2D(bioRand(-.55f*aspect,.55f*aspect),.5f+margin);
        }
    }
    bool burstBiolume(int cell){
        int group=-1;
        for(int g=0;g<2;g++){
            bool active=false;
            for(int i=g*36;i<(g+1)*36;i++)if(bioFragments[i].w()>0.f){active=true;break;}
            if(!active){group=g;break;}
        }
        if(group<0)return false;
        const float aspect=float(std::max(1,width()))/float(std::max(1,height()));
        QVector2D center(bioCells[cell].x(),bioCells[cell].y());
        QVector3D pigment=bioCellColor[cell];
        bioBurstColor[group]=pigment;
        for(int i=0;i<36;i++){
            int slot=group*36+i;
            float angle=2.f*pi*(i+bioRand(-.2f,.2f))/36.f;
            QVector2D direction(std::cos(angle),std::sin(angle));
            float radius=bioRand(.008f,.016f);
            QVector2D position=center+direction*bioRand(0.f,.045f);
            bioFragments[slot]=QVector4D(position.x(),position.y(),radius,1.f);
            bioFragmentVelocity[slot]=direction*bioRand(.14f,.31f);
            bioFragmentRadius[slot]=radius;
            bioFragmentDuration[slot]=bioRand(2.4f,3.8f);
        }
        int freeSparks=0;
        for(const auto &spark:bioSparks)if(spark.w()<=0.f)freeSparks++;
        int sparkCount=std::min(24,freeSparks),emitted=0;
        for(int slot=0;slot<64 && emitted<sparkCount;slot++){
            if(bioSparks[slot].w()>0.f)continue;
            float angle=2.f*pi*(emitted+bioRand(-.3f,.3f))/float(sparkCount);
            QVector2D direction(std::cos(angle),std::sin(angle));
            QVector2D position=center+direction*bioRand(0.f,.025f);
            bioSparks[slot]=QVector4D(position.x(),position.y(),bioRand(.0012f,.0028f),1.f);
            bioSparkVelocity[slot]=direction*bioRand(.20f,.55f);
            bioSparkDuration[slot]=bioRand(.65f,1.45f);
            emitted++;
        }
        for(auto &g:bioGlows)if(g.w()<=0){g=QVector4D(center.x(),center.y(),4.f,1.f);break;}
        for(int i=0;i<48;i++)if(bioFlyOwner[i]==cell){
            bioFlyOwner[i]=-1;bioFlyAge[i]=0;bioFlies[i].setW(1.f);
        }
        QVector2D edge=bioEdgePoint(true);
        bioCells[cell]=QVector4D(edge.x(),edge.y(),bioRand(.045f,.06f),.13f);
        bioCellVelocity[cell]=QVector2D(bioRand(-.07f,.07f),bioRand(-.07f,.07f));
        static const QVector3D palette[]={QVector3D(.12f,.47f,1.f),QVector3D(.55f,.15f,1.f),QVector3D(.08f,.72f,.94f),QVector3D(.75f,.18f,.85f)};
        bioCellColor[cell]=palette[cell%4];
        bioPigmentWeight[cell]=7.f;
        bioPigmentSum[cell]=palette[cell%4]*7.f;
        bioCellCooldown[cell]=2.f;
        return true;
    }
    void updateBiolume(float dt){
        dt=std::clamp(dt,0.f,.035f);
        static const QVector3D palette[]={QVector3D(.12f,.47f,1.f),QVector3D(.55f,.15f,1.f),QVector3D(.08f,.72f,.94f),QVector3D(.75f,.18f,.85f)};
        static const QVector3D flyPalette[]={QVector3D(.08f,.84f,1.f),QVector3D(.18f,.96f,.32f),
                                              QVector3D(1.f,.82f,.12f),QVector3D(1.f,.18f,.12f)};
        const float aspect=float(std::max(1,width()))/float(std::max(1,height()));
        const float bass=std::clamp(sound.energy,0.f,1.f);
        const float treble=std::clamp(sound.treble,0.f,1.f);
        if(!bioReady){
            for(int i=0;i<8;i++){
                float a=2*pi*(i+.3f)/8.f;
                bioCells[i]=QVector4D(.34f*aspect*std::cos(a),.36f*std::sin(a),bioRand(.048f,.078f),.13f);
                bioCellVelocity[i]=QVector2D(bioRand(-.08f,.08f),bioRand(-.08f,.08f));
                bioCellColor[i]=palette[i%4];
                bioPigmentWeight[i]=7.f;
                bioPigmentSum[i]=palette[i%4]*7.f;
                bioCellCooldown[i]=0.f;
                bioMergeTarget[i]=-1;
            }
            for(int i=0;i<48;i++){
                QVector2D edge=bioEdgePoint();
                bioFlies[i]=QVector4D(edge.x(),edge.y(),bioRand(0,2*pi),1.f);
                bioFlyVelocity[i]=QVector2D(bioRand(-.04f,.04f),bioRand(-.04f,.04f));
                bioFlyOwner[i]=-1;
                bioFlyAge[i]=bioRand(0,2.f);
            }
            for(auto &g:bioGlows)g=QVector4D(0,0,0,0);
            bioReady=true;
        }
        bioSeconds+=dt;
        bioPulse=std::max(std::clamp(sound.pulse,0.f,1.f),bioPulse*std::exp(-dt*6.f));
        for(auto &g:bioGlows)g.setW(std::max(0.f,g.w()-dt*.75f));
        for(int i=0;i<72;i++){
            auto &fragment=bioFragments[i];
            if(fragment.w()<=0.f)continue;
            fragment.setX(fragment.x()+bioFragmentVelocity[i].x()*dt);
            fragment.setY(fragment.y()+bioFragmentVelocity[i].y()*dt);
            bioFragmentVelocity[i]*=std::exp(-dt*.45f);
            fragment.setW(std::max(0.f,fragment.w()-dt/bioFragmentDuration[i]));
            fragment.setZ(bioFragmentRadius[i]*fragment.w());
        }
        for(int i=0;i<64;i++){
            auto &spark=bioSparks[i];
            if(spark.w()<=0.f)continue;
            spark.setX(spark.x()+bioSparkVelocity[i].x()*dt);
            spark.setY(spark.y()+bioSparkVelocity[i].y()*dt);
            bioSparkVelocity[i]*=std::exp(-dt*1.1f);
            spark.setW(std::max(0.f,spark.w()-dt/bioSparkDuration[i]));
            spark.setZ(spark.z()*std::exp(-dt*.6f));
        }
        // Move each distinct predator toward its nearest free firefly.
        for(int c=0;c<8;c++){
            auto &cell=bioCells[c];
            QVector2D pos(cell.x(),cell.y()),desired(0,0);
            bool wasInside=std::abs(pos.x())<=.5f*aspect-.035f && std::abs(pos.y())<=.46f;
            float nearest=1e6f;
            for(int i=0;i<48;i++){
                if(bioFlyOwner[i]>=0)continue;
                QVector2D delta(bioFlies[i].x()-pos.x(),bioFlies[i].y()-pos.y());
                float d=delta.lengthSquared();
                if(d<nearest){nearest=d;desired=delta;}
            }
            QVector2D v=bioCellVelocity[c];
            if(desired.lengthSquared()>.000001f)desired.normalize();
            float phase=bioSeconds*.38f+c*2.39996f;
            QVector2D towardMiddle(.16f*aspect*std::sin(phase)-pos.x(),.15f*std::cos(phase*1.13f)-pos.y());
            if(towardMiddle.lengthSquared()>.000001f)towardMiddle.normalize();
            desired=nearest<.28f*.28f ? desired*.78f+towardMiddle*.22f : towardMiddle;
            if(!wasInside)desired=towardMiddle;
            if(desired.lengthSquared()>.000001f)desired.normalize();
            desired*=.14f+.07f*bass+.045f*sound.pulse;
            // Steering is smooth, with an independently moving organic wobble.
            QVector2D wiggle(std::sin(bioSeconds*.91f+c*2.8f),std::cos(bioSeconds*.77f+c*1.9f));
            v+=(desired+wiggle*.028f-v)*std::min(1.f,dt*1.7f);
            pos+=v*dt;
            if(wasInside){
                if(pos.x() < -.5f*aspect+.035f || pos.x() > .5f*aspect-.035f)v.setX(-v.x());
                if(pos.y() < -.46f || pos.y() > .46f)v.setY(-v.y());
                pos.setX(std::clamp(pos.x(),-.5f*aspect+.035f,.5f*aspect-.035f));
                pos.setY(std::clamp(pos.y(),-.46f,.46f));
            }
            bioCellVelocity[c]=v;
            cell.setX(pos.x());cell.setY(pos.y());
            cell.setZ(std::min(.18f,cell.z()+dt*(.0011f+.0014f*bass)));
            cell.setW(std::max(.06f,cell.w()-dt*.025f));
            bioCellCooldown[c]=std::max(0.f,bioCellCooldown[c]-dt);
        }
        // Pull the smaller membrane into the larger one while its area transfers.
        for(int small=0;small<8;small++){
            int big=bioMergeTarget[small];
            if(big<0)continue;
            auto &prey=bioCells[small],&host=bioCells[big];
            bioMergeProgress[small]=std::min(1.f,bioMergeProgress[small]+dt/.65f);
            float x=bioMergeProgress[small],ease=x*x*(3.f-2.f*x);
            float hostArea=bioMergeHostRadius[small]*bioMergeHostRadius[small];
            float preyArea=bioMergeRadius[small]*bioMergeRadius[small];
            float pull=std::min(1.f,dt*(3.f+6.f*x));
            QVector2D offset(prey.x()-host.x(),prey.y()-host.y());
            QVector2D hostPos(host.x(),host.y()),preyPos(prey.x(),prey.y());
            hostPos+=offset*(pull*preyArea/(hostArea+preyArea));
            preyPos-=offset*(pull*hostArea/(hostArea+preyArea));
            host.setX(hostPos.x());host.setY(hostPos.y());
            prey.setX(preyPos.x());prey.setY(preyPos.y());
            prey.setZ(std::max(.002f,bioMergeRadius[small]*(1.f-ease)));
            host.setZ(std::min(.17f,std::sqrt(bioMergeHostRadius[small]*bioMergeHostRadius[small]
                +.65f*bioMergeRadius[small]*bioMergeRadius[small]*ease)));
            bioCellColor[big]=(bioPigmentSum[big]+bioPigmentSum[small]*ease)/
                (bioPigmentWeight[big]+bioPigmentWeight[small]*ease);
            if(x<1.f)continue;
            bioPigmentSum[big]+=bioPigmentSum[small];
            bioPigmentWeight[big]+=bioPigmentWeight[small];
            host.setW(std::min(1.5f,host.w()+prey.w()*.7f));
            for(auto &g:bioGlows)if(g.w()<=0){g=QVector4D(host.x(),host.y(),4.f,1.f);break;}
            QVector2D edge=bioEdgePoint(true);prey=QVector4D(edge.x(),edge.y(),bioRand(.045f,.065f),.14f);
            bioPigmentWeight[small]=7.f;
            bioPigmentSum[small]=palette[small%4]*7.f;
            bioCellColor[small]=palette[small%4];
            bioCellVelocity[small]=QVector2D(bioRand(-.07f,.07f),bioRand(-.07f,.07f));
            bioCellCooldown[big]=2.5f;bioCellCooldown[small]=3.f;
            for(int f=0;f<48;f++)if(bioFlyOwner[f]==small)bioFlyOwner[f]=big;
            bioMergeTarget[small]=-1;
        }
        // Flee instead of approaching the organisms. Predators have higher top speed.
        for(int i=0;i<48;i++){
            auto &f=bioFlies[i];
            if(bioFlyOwner[i]>=0){
                int c=bioFlyOwner[i];
                bioFlyAge[i]+=dt;
                float angle=f.z()+dt*(4.f+treble*4.f);
                float radius=std::max(.004f,bioCells[c].z()*(.65f-bioFlyAge[i]*.55f));
                f=QVector4D(bioCells[c].x()+std::cos(angle)*radius,bioCells[c].y()+std::sin(angle)*radius,angle,-1.f);
                if(bioFlyAge[i]>1.25f){
                    bioFlyOwner[i]=-1;bioFlyAge[i]=0;
                    QVector2D edge=bioEdgePoint();f=QVector4D(edge.x(),edge.y(),bioRand(0,2*pi),1.f);
                    bioFlyVelocity[i]=QVector2D(bioRand(-.07f,.07f),bioRand(-.07f,.07f));
                }
                continue;
            }
            QVector2D pos(f.x(),f.y());
            QVector2D avoid(0,0);float dist=1e6f;int nearest=0;
            for(int c=0;c<8;c++){
                QVector2D d(pos.x()-bioCells[c].x(),pos.y()-bioCells[c].y());
                float r=d.length();
                if(r<dist){dist=r;nearest=c;}
                if(r<.30f && r>.0001f)avoid+=d/(r*r+.009f);
            }
            QVector2D motion(std::cos(f.z()),std::sin(f.z()));
            QVector2D desired=motion*.04f+avoid*.005f;
            if(desired.lengthSquared()>.00001f)desired.normalize();
            desired*=.075f+.035f*treble;
            bioFlyVelocity[i]+=(desired-bioFlyVelocity[i])*std::min(1.f,dt*3.f);
            pos+=bioFlyVelocity[i]*dt;
            float angle=f.z()+dt*(.7f*std::sin(bioSeconds*2.f+i*1.71f));
            f=QVector4D(pos.x(),pos.y(),angle,1.f);
            if(dist<bioCells[nearest].z()*.75f){
                bioFlyOwner[i]=nearest;bioFlyAge[i]=0;
                bioCells[nearest].setZ(std::min(.18f,bioCells[nearest].z()+.0025f));
                bioCells[nearest].setW(std::min(1.5f,bioCells[nearest].w()+.16f));
                bioPigmentSum[nearest]+=flyPalette[i%4];
                bioPigmentWeight[nearest]+=1.f;
                bioCellColor[nearest]=bioPigmentSum[nearest]/bioPigmentWeight[nearest];
                for(auto &g:bioGlows)if(g.w()<=0){g=QVector4D(pos.x(),pos.y(),float(i%4),1.f);break;}
            }else if(std::abs(pos.x())>.5f*aspect+.17f || std::abs(pos.y())>.67f){
                QVector2D edge=bioEdgePoint(); f.setX(edge.x());f.setY(edge.y());
            }
        }
        // Larger predators assimilate smaller ones and inherit their pigment/energy.
        for(int a=0;a<8;a++)for(int b=a+1;b<8;b++){
            auto &A=bioCells[a],&B=bioCells[b];
            float dx=A.x()-B.x(),dy=A.y()-B.y();
            float distance=std::sqrt(dx*dx+dy*dy);
            if(distance>(A.z()+B.z())*.82f || bioCellCooldown[a]>0 || bioCellCooldown[b]>0 ||
                bioMergeTarget[a]>=0 || bioMergeTarget[b]>=0)continue;
            bool occupied=false;
            for(int target:bioMergeTarget)if(target==a || target==b){occupied=true;break;}
            if(occupied)continue;
            int big=A.z()>=B.z()?a:b,small=big==a?b:a;
            bioMergeTarget[small]=big;
            bioMergeProgress[small]=0.f;
            bioMergeRadius[small]=bioCells[small].z();
            bioMergeHostRadius[small]=bioCells[big].z();
        }
        for(int c=0;c<8;c++){
            if(bioCells[c].z()<.13f || bioMergeTarget[c]>=0 ||
                std::abs(bioCells[c].x())>=.5f*aspect-.18f || std::abs(bioCells[c].y())>=.32f)continue;
            bool merging=false;
            for(int target:bioMergeTarget)if(target==c){merging=true;break;}
            if(!merging)burstBiolume(c);
        }
    }
    QColor fireworksColor(){
        static const std::array<QColor,10> palette = {QColor("#ff5c8a"),QColor("#ffb347"),QColor("#8efc54"),QColor("#58d3ff"),QColor("#c48bff"),QColor("#fff275"),QColor("#ff7a40"),QColor("#ffffff"),QColor("#7df9ff"),QColor("#ffd1f9")};
        return palette[size_t(rng()%palette.size())];
    }
    void resetFireworks(){
        for(auto &r:fwRockets)r.active=false;
        for(auto &s:fwSparks)s.active=false;
        fireworksReady=true;fireSpawnTimer=1.0f;fireSeconds=0;
        fireParticleCursor=0;fireLaunchCount=0;fireCrackles.clear();comets.clear();
    }
    FireSpark *freeSpark(){
        // Circular allocator avoids O(pool size) searches for each of ~1500 particles.
        constexpr size_t count=18000;
        for(size_t attempt=0;attempt<count;attempt++){
            size_t index=(fireParticleCursor+attempt)%count;
            if(!fwSparks[index].active){fireParticleCursor=(index+1)%count;return &fwSparks[index];}
        }
        return nullptr;
    }
    void emitTrail(const QVector2D &pos,const QColor &color){
        auto *s=freeSpark();if(!s)return;
        s->active=true;s->pos=pos;
        s->vel=QVector2D(bioRand(-13.f,13.f),bioRand(8.f,38.f));s->color=color;
        s->life=s->maxLife=bioRand(.48f,1.35f);s->size=bioRand(.7f,2.3f);
        s->drag=.96f;s->gravity=35.f;s->shape=0;s->angle=0;s->rotation=0;
    }
    void emitCrackle(const QVector2D &point,const QColor &color,int count){
        for(int j=0;j<count;j++){
            auto *s=freeSpark();if(!s)break;
            float angle=bioRand(0.f,2.f*float(pi));float velocity=bioRand(35.f,160.f);
            s->active=true;s->pos=point;
            s->vel=QVector2D(std::cos(angle)*velocity,std::sin(angle)*velocity);
            s->color=color;
            s->life=s->maxLife=(j%3==0 ? bioRand(2.1f,3.5f) : bioRand(.85f,1.6f));
            s->size=(j%4==0)?1.f:bioRand(.8f,1.7f);
            s->drag=.95f;s->gravity=35.f;s->shape=j%3==0?3:2;
            s->angle=bioRand(0.f,6.28f);s->rotation=bioRand(-5.5f,5.5f);
        }
    }
    void explodeRocket(const FireRocket &rocket, int preset=0){
        // ~10x v0.2: dense, mostly 1px pinpoints and only occasional larger glows.
        const bool crackles=preset ? preset==7 || preset==9 : bioRand(0,1)<.25f;
        const int pattern=preset ? (preset==8?5:(preset>=2 && preset<=5?preset:0))
                                 : (bioRand(0,1)<.26f?2+int(rng()%4):0);
        const float density=preset==8?0.f:pattern?.55f:preset==7?.38f:1.f;
        const int baseCount=(rocket.finale?2300:1300)+int(rng()%(rocket.finale?500:400));
        const float scale=std::clamp(std::min(width()/1280.f,height()/720.f),.80f,2.3f);
        const float velocity=340.f*scale*(1.f+.28f*sound.energy);
        for(int i=0;i<int(baseCount*density);i++){
            auto *s=freeSpark();if(!s)break;
            // Volumetric burst: random angles and a strongly center-weighted
            // radial velocity distribution. No hard minimum-power shell.
            float theta=bioRand(0.f,2.f*float(pi));
            float u=bioRand(0.f,1.f);
            float radius=0.f;
            // 16% near-stationary core, 38% inner cloud, 31% middle,
            // and 15% far-reaching outer sparks. Within every layer the
            // radius remains random, not an evenly spaced circular perimeter.
            float selector=bioRand(0.f,1.f);
            if(selector<.035f) radius=.001f+std::pow(u,1.3f)*.11f;
            else if(selector<.31f) radius=.08f+std::sqrt(u)*.48f;
            else if(selector<.83f) radius=.36f+std::sqrt(u)*.56f;
            else radius=.80f+u*.48f;
            // Different silhouettes without losing the dense inner volume.
            if(rocket.style==1)radius*=bioRand(.8f,1.08f); // willow
            if(rocket.style==2)radius*=.90f+.10f*std::abs(std::sin(theta*5.f)); // starburst
            if(rocket.style==3)theta+=.13f*std::sin(i*.12f); // twisting flower
            s->active=true;s->pos=rocket.pos;
            s->vel=QVector2D(std::cos(theta)*velocity*radius,std::sin(theta)*velocity*radius);
            s->color=rocket.color;
            s->life=s->maxLife=bioRand(rocket.finale?2.1f:1.45f,rocket.finale?3.8f:3.0f);
            s->drag=(rocket.style==1)? .991f : .988f;
            s->gravity=(rocket.style==1)?65.f:42.f;
            // Some shells carry persistent irregular white twinkles; no triangles.
            const bool twinkle=!rocket.finale && rocket.style==3 && (i%27==0);
            s->shape=twinkle?3:((i%4==0)?2:0);
            if(twinkle){s->color=QColor("#ffffff");s->life=s->maxLife=bioRand(3.5f,5.3f);}
            s->size=(s->shape==2)?1.f:(rocket.finale?bioRand(2.0f,5.8f):((i%7==0)?bioRand(2.3f,4.5f):bioRand(.65f,1.4f)));
            s->angle=bioRand(0.f,2.f*float(pi));
            s->rotation=(rng()%2?-1.f:1.f)*bioRand(1.5f,5.5f);
        }
        if(pattern) emitPattern(rocket,scale,velocity*(310.f/340.f),pattern);
        if(!crackles)return;
        // Visible secondary crackling AFTER the main explosion has expanded.
        for(int j=0;j<8;j++){
            float angle=bioRand(0.f,2.f*float(pi));
            float offset=bioRand(25.f,90.f)*scale;
            CrackleBurst c;
            c.pos=rocket.pos+QVector2D(std::cos(angle)*offset,std::sin(angle)*offset);
            c.vel=QVector2D(std::cos(angle)*bioRand(75,150),std::sin(angle)*bioRand(75,150));
            static const QColor colors[]={Qt::white,QColor("#ff382e"),QColor("#ffd114"),QColor("#2e7aff"),QColor("#33ff52")};
            c.color=colors[rng()%5];c.delay=bioRand(.30f,1.15f);
            fireCrackles.push_back(c);
        }
    }
    void emitPattern(const FireRocket &rocket,float scale,float speed,int pattern){
        const int count=pattern==5?6+int(rng()%7):66+int(rng()%33);
        const float phase=bioRand(0,2*pi), tilt=bioRand(.12f,.42f)*(rng()%2?1:-1);
        const float flatten=pattern==4?bioRand(.72f,.94f):bioRand(.25f,.47f);
        const float inner=bioRand(.52f,.69f);
        const int petals=4+int(rng()%4);
        for(int i=0;i<count;++i){
            const float a=phase+2*pi*i/count;
            QVector2D direction(std::cos(a),std::sin(a));
            if(pattern==5){
                comets.push_back({rocket.pos,direction*speed*bioRand(.84f,1.15f),rocket.color,fireSeconds,scale});
                continue;
            }
            auto *spark=freeSpark();
            if(!spark)break;
            float x=direction.x(),y=direction.y()*flatten;
            float radius=pattern==4?.72f+.28f*std::cos(petals*a):(pattern==3 && i%2==0?inner:1.f);
            spark->pos=rocket.pos;
            spark->vel=QVector2D(x*std::cos(tilt)-y*std::sin(tilt),x*std::sin(tilt)+y*std::cos(tilt))
                       *speed*radius*bioRand(.985f,1.015f);
            spark->active=true;spark->color=rocket.color;spark->shape=1;
            spark->size=scale*bioRand(5,8);spark->life=spark->maxLife=bioRand(1.9f,2.6f);
            spark->angle=0;spark->rotation=0;spark->drag=.994f;spark->gravity=28*scale;
        }
    }
    void spawnRocket(){
        if(width()<2 || height()<2)return;
        for(auto &r:fwRockets)if(!r.active){
            r.active=true;
            r.pos=QVector2D(bioRand(width()*.14f,width()*.86f),height()+bioRand(8.f,35.f));
            r.start=r.pos;
            // Each launch has its own apex. Choose a target HEIGHT above the
            // bottom of the viewport and derive its initial velocity from v^2=2*g*h.
            // The rocket visibly slows to zero close to the target altitude.
            r.finale=(++fireLaunchCount%9==0); // occasional grand finale
            const bool high=(rng()%100)<42;
            // Finale shells rise almost to the top before decelerating at the apex.
            const float targetY=height()*(r.finale?bioRand(.06f,.19f):bioRand(high?.11f:.35f,high?.33f:.57f));
            const float rise=std::max(120.f,r.pos.y()-targetY);
            r.gravity=bioRand(155.f,250.f);
            const float vy=-std::sqrt(2.f*r.gravity*rise);
            r.vel=QVector2D(bioRand(-38.f,38.f),vy);
            // A few shells hang motionless briefly before the burst.
            r.hoverAtApex=(rng()%100)<28;
            r.hover=r.hoverAtApex?bioRand(.12f,.48f):0.f;
            r.waiting=false;
            r.fuse=std::abs(vy)/r.gravity+1.5f; // failsafe only
            r.color=fireworksColor();r.style=int(rng()%4);
            if(r.finale){r.hoverAtApex=true;r.hover=bioRand(.30f,.60f);}
            return;
        }
    }
    void updateFireworks(float dt){
        if(!fireworksReady)resetFireworks();
        fireSeconds+=dt;fireSpawnTimer-=dt;
        if(fireSpawnTimer<=0.f){spawnRocket();fireSpawnTimer=bioRand(2.8f,4.7f);}
        for(auto &r:fwRockets)if(r.active){
            r.fuse-=dt;
            if(r.waiting){
                r.hover-=dt;
                if(r.hover<=0.f || r.fuse<=0.f){explodeRocket(r);r.active=false;}
                continue;
            }
            const QVector2D previous=r.pos;
            r.pos+=r.vel*dt;
            r.vel+=QVector2D(0.f,r.gravity*dt);
            // A finale uses approximately one third of its former exhaust width:
            // fewer particles per frame and a much narrower lateral scatter.
            for(int t=0;t<(r.finale?5:4);t++){
                const int n=r.finale?5:4;
                QVector2D point=previous+(r.pos-previous)*float(t)/float(n);
                if(r.finale)point+=QVector2D(bioRand(-5.f,5.f),bioRand(-2.3f,2.3f));
                emitTrail(point,r.color);
            }
            // Do not detonate at a fixed time while still rising. Burst at apex.
            if(r.vel.y()>=-12.f || r.fuse<=0.f){
                if(r.hoverAtApex && r.fuse>0.f){
                    r.waiting=true;r.vel=QVector2D(0.f,0.f);
                }else{explodeRocket(r);r.active=false;}
            }
        }
        for(auto &s:fwSparks)if(s.active){
            s.life-=dt;if(s.life<=0.f){s.active=false;continue;}
            s.pos+=s.vel*dt;s.angle+=s.rotation*dt;
            s.vel*=std::pow(s.drag,dt*60.f);
            s.vel+=QVector2D(0.f,s.gravity*dt);
        }
        for(size_t i=0;i<fireCrackles.size();){
            auto &c=fireCrackles[i];c.pos+=c.vel*dt;c.vel*=std::pow(.97f,dt*60.f);
            c.delay-=dt;
            if(c.delay<=0.f){emitCrackle(c.pos,c.color,45+int(rng()%35));
                fireCrackles[i]=fireCrackles.back();fireCrackles.pop_back();
            }else ++i;
        }
    }
    struct FireGpuParticle{float x,y,size,life,r,g,b,shape,angle;};
    void paintFireworks(){
        // Simulation remains on the CPU; all spark rasterization and glow are GPU point sprites.
        if(!fireProgram){
            const auto vertex=read("fireworks.vert");
            const auto fragment=read("fireworks.frag");
            fireProgram=std::make_unique<QOpenGLShaderProgram>();
            if(!fireProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)||
               !fireProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,fragment)||
               !fireProgram->link()){
                error=QString("Fireworks GPU shader: %1").arg(fireProgram->log());
                fireProgram.reset();
                return;
            }
            glGenVertexArrays(1,&fireVao);
            glGenBuffers(1,&fireVbo);
            glBindVertexArray(fireVao);
            glBindBuffer(GL_ARRAY_BUFFER,fireVbo);
            const GLsizei stride=sizeof(FireGpuParticle);
            glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(FireGpuParticle,x)));
            glEnableVertexAttribArray(1);glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(FireGpuParticle,size)));
            glEnableVertexAttribArray(2);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(FireGpuParticle,life)));
            glEnableVertexAttribArray(3);glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(FireGpuParticle,r)));
            glEnableVertexAttribArray(4);glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(FireGpuParticle,shape)));
            glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(FireGpuParticle,angle)));
            glBindVertexArray(0);
        }
        const int w=std::max(1,qRound(width()*devicePixelRatioF()));
        const int h=std::max(1,qRound(height()*devicePixelRatioF()));
        glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());
        glViewport(0,0,w,h);
        glDisable(GL_DEPTH_TEST);
        glClearColor(.015f,.008f,.035f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        std::vector<FireGpuParticle> particles;
        particles.reserve(6000);
        const float dpr=float(devicePixelRatioF());
        for(const auto &s:fwSparks)if(s.active){
            float life=std::clamp(s.life/std::max(.0001f,s.maxLife),0.f,1.f);
            float pointSize=s.shape==2?1.5f:std::clamp(s.size*5.0f,2.f,22.f);
            if(s.shape==1)pointSize=std::clamp(s.size*6.f,24.f,84.f);
            if(s.shape==3)pointSize=std::clamp(s.size*7.5f,5.f,19.f);
            particles.push_back({s.pos.x()*dpr,s.pos.y()*dpr,pointSize*dpr,life,
                float(s.color.redF()),float(s.color.greenF()),float(s.color.blueF()),
                float(s.shape),s.angle});
        }
        for(const auto &r:fwRockets)if(r.active)
            particles.push_back({r.pos.x()*dpr,r.pos.y()*dpr,28.f*dpr,1.f,
                float(r.color.redF()),float(r.color.greenF()),float(r.color.blueF()),0.f,0.f});
        constexpr float travel=1.85f, persistence=1.62f;
        std::erase_if(comets,[&](const Comet &c){return fireSeconds-c.start>=travel+persistence;});
        for(const auto &c:comets){
            const float age=fireSeconds-c.start;
            for(int i=0;i<150;++i){
                const float progress=i/149.f, born=progress*travel, elapsed=age-born;
                if(elapsed<0)break;
                if(elapsed>=persistence)continue;
                auto pos=c.origin+c.velocity*((1-std::exp(-.85f*born))/.85f);
                pos.setY(pos.y()+49*c.scale*born*born);
                float size=20+20*progress;
                float light=(.28f+.38f*progress)*std::clamp((persistence-elapsed)/.90f,0.f,1.f)
                            *std::clamp((travel+.10f-born)/.58f,0.f,1.f);
                if(age<travel && elapsed<travel/149.f){
                    const float fade=std::clamp((travel+.03f-age)/.57f,0.f,1.f);
                    size=60+10*fade;light=.95f*fade;
                }
                particles.push_back({pos.x()*dpr,pos.y()*dpr,size*c.scale*dpr,light,
                    float(c.color.redF()),float(c.color.greenF()),float(c.color.blueF()),1.f,0.f});
            }
        }
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);
        fireProgram->bind();fireProgram->setUniformValue("u_resolution",QVector2D(float(w),float(h)));fireProgram->setUniformValue("u_time",fireSeconds);
        glBindVertexArray(fireVao);glBindBuffer(GL_ARRAY_BUFFER,fireVbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(particles.size()*sizeof(FireGpuParticle)),particles.data(),GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS,0,GLsizei(particles.size()));
        glBindVertexArray(0);fireProgram->release();glDisable(GL_BLEND);
    }
    void paintGL() override {
        auto now=clock.elapsed();float dt=std::clamp(float(now-last)*.001f,0.f,.1f);last=now;
        energy+=(sound.energy-energy)*(1-std::exp(-dt/.22f));
        float target=.06f+4.8f*energy+1.2f*sound.mid+.6f*sound.treble+2*sound.pulse;
        speed+=(target-speed)*(1-std::exp(-dt/.25f));travel+=speed*dt;
        phase+=dt*(.025f+1.8f*energy+.5f*sound.mid+.35f*sound.treble);
        if(effect==3){matrix(dt);return;}
        if(effect==7)updateBiolume(dt);
        if(effect==8){updateFireworks(dt);paintFireworks();return;}
        if(effect==9){auto w=std::max(1,qRound(width()*devicePixelRatioF())),h=std::max(1,qRound(height()*devicePixelRatioF()));circler.paint(dt,w,h);return;}
        if(effect==10 || effect==11){
            auto w=std::max(1,qRound(width()*devicePixelRatioF())),h=std::max(1,qRound(height()*devicePixelRatioF()));
            if(effect==10)fog.paint(dt,w,h,defaultFramebufferObject(),sound.energy,sound.mid,sound.treble);
            else {
                crawler.paint(dt,w,h,defaultFramebufferObject(),sound.energy,sound.mid,sound.treble);
                if(crawler.hudVisible()){
                    QPainter p(this);p.fillRect(QRect(12,12,350,132),QColor(5,5,20,180));
                    p.setPen(QColor("#a6fcff"));QFont font("Monospace",10);font.setStyleHint(QFont::TypeWriter);p.setFont(font);
                    const QStringList lines={
                        QString("CRAWLER  |  %1 balls  |  trail %2").arg(crawler.ballCount()).arg(crawler.trailAlpha()),
                        "H HUD  SPACE pause  C clear  R reset",
                        "UP/DOWN balls  LEFT/RIGHT trail",
                        "B plasma  L bloom  S scanlines  V vignette",
                        "F11 fullscreen  ESC close"};
                    for(int i=0;i<lines.size();++i)p.drawText(24,35+i*22,lines[i]);
                }
            }
            return;
        }
        if(effect>=4 && effect<=6){if(!laser)laser=std::make_unique<LaserScene>(laserVariant(effect));laser->paint(this,dt,sound);return;}
        if(!program)setup();
        if(!program){QPainter p(this);p.fillRect(rect(),QColor("#21132d"));p.setPen(Qt::white);p.drawText(rect(),Qt::AlignCenter,error);return;}
        auto w=std::max(1,qRound(width()*devicePixelRatioF())),h=std::max(1,qRound(height()*devicePixelRatioF()));
        glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glBindVertexArray(vao);
        if(effect==2){
            ensureFbo(w,h);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,w,h);
            cube->bind();cube->setUniformValue("resolution",QVector2D(w,h));cube->setUniformValue("angle",phase*.65f);glDrawArrays(GL_TRIANGLES,0,3);cube->release();
            glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());glViewport(0,0,w,h);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
            program->bind();program->setUniformValue("source",0);program->setUniformValue("resolution",QVector2D(w,h));program->setUniformValue("time",phase);
            program->setUniformValue("strength",.07f+.46f*energy+.26f*sound.mid+.22f*sound.pulse);
            program->setUniformValue("dispersion",.08f+.46f*sound.treble+.24f*sound.pulse);program->setUniformValue("turbulence",.48f+1.2f*sound.treble);
        }else{
            glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());glViewport(0,0,w,h);program->bind();
            if(effect==7){
                program->setUniformValue("u_resolution",QVector2D(w,h));
                program->setUniformValue("u_time",now*.001f);
                program->setUniformValue("u_bass",sound.energy);
                program->setUniformValue("u_treble",sound.treble);
                program->setUniformValue("u_pulse",bioPulse);
                program->setUniformValueArray("u_cells",bioCells.data(),8);
                program->setUniformValueArray("u_flies",bioFlies.data(),48);
                program->setUniformValueArray("u_cellColors",bioCellColor.data(),8);
                program->setUniformValueArray("u_glows",bioGlows.data(),16);
                program->setUniformValueArray("u_fragments",bioFragments.data(),72);
                program->setUniformValueArray("u_burstColors",bioBurstColor.data(),2);
                program->setUniformValueArray("u_sparks",bioSparks.data(),64);
            }
            else if(effect==0){float desired=std::clamp(sound.pulse,0.f,1.f);float tau=desired>kickEnvelope?.020f:.180f;kickEnvelope+=(desired-kickEnvelope)*(1-std::exp(-dt/tau));program->setUniformValue("u_kick",kickEnvelope);program->setUniformValue("u_resolution",QVector2D(w,h));program->setUniformValue("u_time",now*.001f);program->setUniformValue("u_density",1.4f);program->setUniformValue("u_speed",1.f);program->setUniformValue("u_pulse",sound.pulse);program->setUniformValue("u_flash",QVector3D(.7f,.2f,.8f));program->setUniformValue("u_mid",sound.mid);program->setUniformValue("u_treble",sound.treble);}
            else {program->setUniformValue("uResolution",QVector2D(w,h));program->setUniformValue("uTime",phase);program->setUniformValue("uCamera",QVector3D(.18f*std::sin(phase*.23f),.15f*std::cos(phase*.19f),travel));program->setUniformValue("uForward",QVector3D(0,0,1));program->setUniformValue("uRight",QVector3D(std::cos(phase*.08f),std::sin(phase*.08f),0));program->setUniformValue("uUp",QVector3D(-std::sin(phase*.08f),std::cos(phase*.08f),0));program->setUniformValue("uPaletteFrom",1);program->setUniformValue("uPaletteTo",1);program->setUniformValue("uPaletteMix",0.f);program->setUniformValue("uFlash",std::min(.65f,sound.pulse*.65f));}
        }
        glDrawArrays(GL_TRIANGLES,0,3);program->release();
        frames++;fpsSeconds+=dt;if(fpsSeconds>=1){setWindowTitle(QString("μSIK · %1 · %2 FPS · F11 / Esc").arg(names().value(effect)).arg(std::round(frames/fpsSeconds)));frames=0;fpsSeconds=0;}
    }
private:
    std::unique_ptr<QOpenGLShaderProgram> fireProgram;GLuint fireVao=0,fireVbo=0;
    std::array<FireRocket,12> fwRockets{};
    std::array<FireSpark,18000> fwSparks{};
    std::vector<CrackleBurst> fireCrackles;size_t fireParticleCursor=0;
    std::vector<Comet> comets;
    bool fireworksDebug=false;
    int fireLaunchCount=0;bool fireworksReady=false;float fireSpawnTimer=0,fireSeconds=0;
    std::array<QVector4D,8> bioCells{};
    std::array<int,8> bioMergeTarget{};
    std::array<float,8> bioMergeProgress{},bioMergeRadius{},bioMergeHostRadius{};
    std::array<QVector2D,8> bioCellVelocity{};
    std::array<QVector3D,8> bioCellColor{};
    std::array<QVector3D,8> bioPigmentSum{};
    std::array<float,8> bioPigmentWeight{};
    std::array<float,8> bioCellCooldown{};
    std::array<QVector2D,48> bioFlyVelocity{};
    std::array<int,48> bioFlyOwner{};
    std::array<float,48> bioFlyAge{};
    std::array<QVector4D,48> bioFlies{};
    std::array<QVector4D,16> bioGlows{};
    std::array<QVector4D,72> bioFragments{};
    std::array<QVector3D,2> bioBurstColor{};
    std::array<QVector2D,72> bioFragmentVelocity{};
    std::array<float,72> bioFragmentRadius{},bioFragmentDuration{};
    std::array<QVector4D,64> bioSparks{};
    std::array<QVector2D,64> bioSparkVelocity{};
    std::array<float,64> bioSparkDuration{};
    float bioPulse=0.f;
    bool bioReady=false;float bioSeconds=0;
    QTimer timer;QElapsedTimer clock; qint64 last=0;
    bool borderless=false;QRect savedGeometry;Qt::WindowFlags savedFlags;
#ifdef Q_OS_WIN
    LONG_PTR nativeStyle=0, nativeExStyle=0;
    WINDOWPLACEMENT nativePlacement{};
#endif
    SoundState sound;int effect=0,color=1;float energy=0,speed=0,travel=0,phase=0,kickEnvelope=0;int frames=0;float fpsSeconds=0;bool partyMode=false;int paletteBeats=16;
    std::unique_ptr<QOpenGLShaderProgram> program,cube;GLuint texture=0,fbo=0,vao=0;int fw=0,fh=0;QString error;
    CirclerScene circler;FogScene fog;CrawlerScene crawler;std::unique_ptr<LaserScene> laser;std::mt19937 rng;std::vector<Stream> streams;std::vector<std::vector<QPixmap>> glyphCache;double pixelRatio=0;
};

class ScopeWidget : public QWidget {
public:
    explicit ScopeWidget(QWidget *parent=nullptr):QWidget(parent) {
        setMinimumHeight(115);
        setToolTip("Click: Oscilloscope → Spectrum → Combo");
        setCursor(Qt::PointingHandCursor);
        samples=QVector<float>(512,0.f);
        bands=QVector<float>(2048,0.f);
    }
    void setBands(int count){bandCount=std::clamp(count,8,2048);update();}
    void setScopePoints(int count){scopePoints=std::clamp(count,64,4096);update();}
    void setMode(int mode){displayMode=((mode%3)+3)%3;update();}
    int mode() const{return displayMode;}
    void setSignal(const QVector<float> &wave,const QVector<float> &fftBands){
        if(!wave.isEmpty())samples=wave;
        if(!fftBands.isEmpty())bands=fftBands;
        update();
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton){setMode((displayMode+1)%3);event->accept();return;}
        QWidget::mousePressEvent(event);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(),QColor(32,21,46,100));
        if(displayMode==2){
            int split=width()/2;
            drawWave(p,QRect(0,0,split,height()));
            drawSpectrum(p,QRect(split,0,width()-split,height()));
            p.setPen(QPen(QColor("#805475"),1));p.drawLine(split,0,split,height());
        }else if(displayMode==1){drawSpectrum(p,rect());}
        else {drawWave(p,rect());}
        p.setPen(QColor("#e4c7d6"));
        p.drawText(10,16,displayMode==0?"OSCILLOSCOPE · click for spectrum":displayMode==1?"SPECTRUM · click for combo":"COMBO · click for oscilloscope");
    }
private:
    void drawWave(QPainter &p,const QRect &area){
        if(samples.size()<2||area.width()<2)return;
        p.save();p.setClipRect(area);
        const int count=std::max(2,std::min(scopePoints,area.width()));
        QPainterPath path;
        for(int j=0;j<count;++j){
            const float pos=float(j)*(samples.size()-1)/(count-1);
            int i=std::clamp(int(pos),0,int(samples.size())-1);
            float frac=pos-i;
            float value=samples[i]*(1.f-frac)+samples[std::min(i+1,int(samples.size())-1)]*frac;
            qreal x=area.left()+double(j)*(area.width()-1)/(count-1);
            qreal y=area.center().y()-value*area.height()*.43;
            if(j==0)path.moveTo(x,y);else path.lineTo(x,y);
        }
        p.setPen(QPen(QColor(255,195,163,55),7));p.drawPath(path);
        p.setPen(QPen(QColor("#ffc3a3"),1.6));p.drawPath(path);
        p.restore();
    }
    void drawSpectrum(QPainter &p,const QRect &area){
        if(bands.isEmpty()||area.width()<1)return;
        p.save();p.setClipRect(area);
        const int count=std::max(1,std::min({bandCount,area.width(),int(bands.size())}));
        const qreal step=qreal(area.width())/count;
        p.setPen(Qt::NoPen);
        for(int i=0;i<count;++i){
            const int bandSize = static_cast<int>(bands.size());
            const int begin = i * bandSize / count;
            const int end = std::min(bandSize, std::max(begin + 1, (i + 1) * bandSize / count));
            float level=0;
            for(int k=begin;k<end && k<bands.size();++k)level=std::max(level,bands[k]);
            const qreal bar=std::clamp(level,0.f,1.f)*std::max(0,area.height()-30);
            p.fillRect(QRectF(area.left()+i*step,area.bottom()-3-bar,std::max(qreal(1),step-.1),bar),QColor("#ffc3a3"));
        }
        p.restore();
    }
    QVector<float> samples,bands;
    int displayMode=0,bandCount=48,scopePoints=512;
};

class PlayerWindow:public QMainWindow {
public:
    PlayerWindow(){
        setWindowTitle("μSIK");resize(1130,820);setMinimumSize(930,670);setStyleSheet(R"CSS(
QMainWindow{background:#24132c;}QWidget{color:#fff4ed;font-family: "Noto Sans";font-size:13px;}
QLabel#brand{font-size:42px;font-weight:800;}QLabel#eyebrow{color:#e8c1d6;font-size:11px;}
QLabel#track{font-size:22px;font-weight:600;}QFrame#panel{background:#28162f;border:1px solid #69425b;border-radius:18px;}
QPushButton{background:#422442;border:1px solid #7d5277;border-radius:9px;padding:10px 12px;}
QPushButton:hover{background:#693b66;border-color:#f7ae89;}QPushButton:pressed{background:#7d436f;}
QFrame#nowPanel,QFrame#playlistPanel{background:rgba(23,13,35,190);border:1px solid rgba(255,255,255,28);border-radius:18px;}
QFrame#playlistPanel QPushButton,QPushButton#visualize{background:rgba(255,255,255,17);border-color:rgba(255,255,255,35);}
QFrame#playlistPanel QPushButton:hover,QPushButton#visualize:hover{background:rgba(255,255,255,38);border-color:#f7ae89;}
QFrame#playlistPanel QPushButton:pressed,QPushButton#visualize:pressed{background:rgba(125,67,111,185);}
QListWidget#playlist{background:transparent;border:none;outline:none;}
QListWidget#playlist::item{padding:12px 9px;border-bottom:1px solid rgba(255,255,255,12);border-radius:5px;}
QListWidget#playlist::item:hover{background:rgba(255,255,255,18);}
QListWidget#playlist::item:selected{background:rgba(196,131,224,70);}
QComboBox{background:#422442;padding:6px;}
QSlider::groove:horizontal{height:5px;background:#69455b;}QSlider::handle:horizontal{background:#ffe1cc;width:12px;margin:-5px 0;border-radius:6px;}
)CSS");
        auto *central=new QWidget;central->setObjectName("canvas");setCentralWidget(central);auto *v=new QVBoxLayout(central);v->setContentsMargins(30,22,30,24);v->setSpacing(16);
        auto *header=new QHBoxLayout;auto *brand=new QVBoxLayout;auto *title=new QLabel("μSIK");title->setObjectName("brand");brand->addWidget(title);auto *tagline=new QLabel("SOUND INTO COLOUR");tagline->setObjectName("eyebrow");brand->addWidget(tagline);header->addLayout(brand);header->addStretch();
        auto *party=new QPushButton("✦ Party Mode");party->setStyleSheet("color:#ffffff;");party->setCheckable(true);party->setObjectName("primary");header->addWidget(party);auto *settingsButton=new QPushButton("Settings");header->addWidget(settingsButton);v->addLayout(header);
        auto *now=new QFrame;now->setObjectName("nowPanel");auto *nowLayout=new QVBoxLayout(now);nowLayout->addWidget(new QLabel("NOW PLAYING"));trackLabel=new QLabel("Your next favourite sound");trackLabel->setObjectName("track");nowLayout->addWidget(trackLabel);
        scope=new ScopeWidget;nowLayout->addWidget(scope);
        auto *seekRow=new QHBoxLayout;elapsedLabel=new QLabel("0:00");durationLabel=new QLabel("0:00");seek=new SeekSlider(Qt::Horizontal);seek->setRange(0,0);
        seekRow->addWidget(elapsedLabel);seekRow->addWidget(seek);seekRow->addWidget(durationLabel);nowLayout->addLayout(seekRow);v->addWidget(now);
        auto *playlistPanel=new QFrame;playlistPanel->setObjectName("playlistPanel");
        auto *playlistLayout=new QVBoxLayout(playlistPanel);
        playlistLayout->setContentsMargins(18,12,18,14);
        v->addWidget(playlistPanel,1);
        list=new QListWidget;list->setObjectName("playlist");list->setSelectionMode(QAbstractItemView::ExtendedSelection);list->setDragDropMode(QAbstractItemView::InternalMove);list->setContextMenuPolicy(Qt::CustomContextMenu);playlistLayout->addWidget(list,1);list->viewport()->setAutoFillBackground(false);
        QObject::connect(list,&QWidget::customContextMenuRequested,this,[this](const QPoint &pos){auto *item=list->itemAt(pos);if(!item)return;QMenu menu(this);auto *metadata=menu.addAction("Track information");auto *selected=menu.exec(list->viewport()->mapToGlobal(pos));if(selected==metadata)showTrackInformation(item->data(Qt::UserRole).toString());});
        auto *row=new QHBoxLayout;auto *add=new QPushButton("+  Add files");auto *addFolders=new QPushButton("Add folders");auto *load=new QPushButton("Load playlist");auto *save=new QPushButton("Save playlist");auto *remove=new QPushButton("Remove selected");
        for(auto *b:{add,addFolders,remove,load,save})row->addWidget(b);playlistLayout->addLayout(row);
        auto *searchRow=new QHBoxLayout;
        auto *prompt=new QLabel(QString::fromUtf8(">"));
        prompt->setStyleSheet("color:white;font-size:24px;font-weight:900;background:transparent;");
        search=new QLineEdit;
        search->setObjectName("fzfSearch");search->setPlaceholderText("Fuzzy-search playlist (Esc clears)");
        search->setClearButtonEnabled(true);
        search->setStyleSheet("QLineEdit{color:white;background:rgba(10,9,20,220);border:1px solid #79508e;border-radius:8px;padding:9px 12px;font-size:15px;}QLineEdit:focus{border:1px solid #ee88eb;}");
        searchRow->addWidget(prompt);searchRow->addWidget(search,1);playlistLayout->addLayout(searchRow);
        QObject::connect(search,&QLineEdit::textChanged,this,[this](const QString &text){
            searchTerms=text.toCaseFolded().simplified().split(' ',Qt::SkipEmptyParts);
            for(int i=0;i<list->count();++i)filterTrack(list->item(i));
        });
        auto *clearSearch=new QShortcut(QKeySequence(Qt::Key_Escape),search);
        clearSearch->setContext(Qt::WidgetShortcut);
        QObject::connect(clearSearch,&QShortcut::activated,search,&QLineEdit::clear);
        QObject::connect(search,&QLineEdit::returnPressed,this,[this]{
            for(int i=0;i<list->count();++i)if(!list->item(i)->isHidden()){playRow(i);return;}
        });
        // Compact transport buttons in the header, leaving vertical room for playlist.
        auto makeTransportIcon=[this](int kind){
            QPixmap pix(34,34); pix.fill(Qt::transparent); QPainter painter(&pix); painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen); painter.setBrush(Qt::white);
            if(kind==0){
                painter.drawRect(QRectF(6,7,3,20));
                QPolygonF a; a << QPointF(24,7) << QPointF(11,17) << QPointF(24,27); painter.drawPolygon(a);
            }else if(kind==1){
                QPolygonF a; a << QPointF(10,7) << QPointF(26,17) << QPointF(10,27); painter.drawPolygon(a);
            }else if(kind==2){
                painter.drawRect(QRectF(9,9,16,16));
            }else if(kind==3){
                painter.drawRect(QRectF(25,7,3,20));
                QPolygonF a; a << QPointF(10,7) << QPointF(23,17) << QPointF(10,27); painter.drawPolygon(a);
            }else if(kind==4){
                painter.drawRect(QRectF(10,7,5,20));
                painter.drawRect(QRectF(19,7,5,20));
            }
            painter.end();
            return QIcon(pix);
        };
        auto *previous=new QPushButton;previous->setToolTip("Previous"); previous->setIcon(makeTransportIcon(0));
        auto *play=new QPushButton;play->setToolTip("Play"); play->setAccessibleName("Play"); play->setIcon(makeTransportIcon(1));
        auto *stop=new QPushButton;stop->setToolTip("Stop"); stop->setIcon(makeTransportIcon(2));
        auto *next=new QPushButton;next->setToolTip("Next"); next->setIcon(makeTransportIcon(3));
        auto *visButton=new QPushButton(QString::fromUtf8("✦ Visualization"));visButton->setObjectName("visualize");
        int transportIndex=2;
        for(auto *button:{previous,play,stop,next}){
            button->setFixedWidth(42);
            button->setIconSize(QSize(26,26));
            button->setText("");
            button->setStyleSheet("QPushButton{background-color:rgba(32,17,46,155);border:1px solid rgba(255,255,255,45);border-radius:8px;}QPushButton:hover{background-color:#75426b;}");
            header->insertWidget(transportIndex++,button);
        }
        header->insertWidget(transportIndex,visButton);
        auto *effectRow=new QHBoxLayout;
        effects=new QComboBox;effects->addItems(Visual::names());effects->hide();
        auto *mute=new QPushButton;mute->setStyleSheet("color:#ffffff;font-size:19px;");
        auto updateMuteIcon=[mute](bool muted){QPixmap pix(36,36);pix.fill(Qt::transparent);QPainter painter(&pix);painter.setRenderHint(QPainter::Antialiasing);QPen white(Qt::white,3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);painter.setPen(white);painter.drawArc(QRectF(7,5,22,25),0,180*16);painter.drawRoundedRect(QRectF(5,18,6,12),2,2);painter.drawRoundedRect(QRectF(25,18,6,12),2,2);if(muted){QPen strike(QColor("#ff354c"),3);painter.setPen(strike);painter.drawLine(5,31,31,5);}painter.end();mute->setIcon(QIcon(pix));mute->setIconSize(QSize(28,28));};
        updateMuteIcon(false);mute->setToolTip("Mute / Unmute");mute->setCheckable(true);
        auto *volume=new QSlider(Qt::Horizontal);volume->setRange(0,100);volume->setValue(70);volume->setMaximumWidth(180);
        effectRow->addStretch();effectRow->addWidget(new QLabel("Volume"));effectRow->addWidget(mute);effectRow->addWidget(volume);v->addLayout(effectRow);
        status=new QLabel("Add your music to begin · Right-click a track for information");v->addWidget(status);
        player=new QMediaPlayer(this);output=new QAudioOutput(this);output->setVolume(.7);player->setAudioOutput(output);
        route=std::make_unique<PlaybackOutput>(player,output,[this](const QAudioBuffer &b){analyze(b);},[this](QString message){status->setText(message);});
        QObject::connect(player,&QMediaPlayer::durationChanged,this,[this](qint64 ms){seek->setRange(0,int(ms));durationLabel->setText(QString("%1:%2").arg(ms/60000).arg((ms/1000)%60,2,10,QChar('0')));});
        QObject::connect(player,&QMediaPlayer::positionChanged,this,[this](qint64 ms){if(!seek->isSliderDown())seek->setValue(int(ms));elapsedLabel->setText(QString("%1:%2").arg(ms/60000).arg((ms/1000)%60,2,10,QChar('0')));});
        QObject::connect(seek,&QSlider::sliderReleased,this,[this]{route->flush();player->setPosition(seek->value());});
        seek->trackClicked=[this](int position){route->flush();player->setPosition(position);};
        QObject::connect(volume,&QSlider::valueChanged,this,[this](int n){output->setVolume(n/100.f);});
        QObject::connect(mute,&QPushButton::toggled,this,[this,updateMuteIcon](bool on){output->setMuted(on);updateMuteIcon(on);});
        QObject::connect(add,&QPushButton::clicked,this,[this]{
            addFiles(QFileDialog::getOpenFileNames(this,"Add audio",QString(),"Audio (*.mp3 *.flac *.wav *.ogg *.m4a *.opus *.aac);;All (*.*)"));
        });
        QObject::connect(addFolders,&QPushButton::clicked,this,[this]{
            auto folder=QFileDialog::getExistingDirectory(this,"Add music folder (including subfolders)");
            if(folder.isEmpty())return;
            QDirIterator files(folder,{"*.mp3","*.flac","*.wav","*.ogg","*.m4a","*.opus","*.aac"},QDir::Files,QDirIterator::Subdirectories);
            QStringList paths;
            while(files.hasNext())paths.append(files.next());
            const int before=list->count();
            addFiles(paths);
            status->setText(QString("Added %1 audio files from folder").arg(list->count()-before));
        });
        QObject::connect(remove,&QPushButton::clicked,this,[this]{qDeleteAll(list->selectedItems());refreshNumbers();});
        QObject::connect(save,&QPushButton::clicked,this,[this]{auto p=QFileDialog::getSaveFileName(this,"Save playlist","MusiK.m3u8","Playlist (*.m3u8)");if(!p.isEmpty())saveList(p);});
        QObject::connect(load,&QPushButton::clicked,this,[this]{auto p=QFileDialog::getOpenFileName(this,"Load playlist",QString(),"Playlist (*.m3u8 *.m3u)");if(!p.isEmpty())loadList(p);});
        QObject::connect(player, &QMediaPlayer::playbackStateChanged, play,
                         [play, playIcon = play->icon(), pauseIcon = makeTransportIcon(4)](QMediaPlayer::PlaybackState state) {
            const bool playing = state == QMediaPlayer::PlayingState;
            play->setIcon(playing ? pauseIcon : playIcon);
            const QString action = playing ? QStringLiteral("Pause") : QStringLiteral("Play");
            play->setToolTip(action);
            play->setAccessibleName(action);
        });
        QObject::connect(play,&QPushButton::clicked,this,[this]{if(player->playbackState()==QMediaPlayer::PlayingState)player->pause();else {if(player->source().isEmpty())playRow(list->currentRow()<0?0:list->currentRow());else player->play();}});
        QObject::connect(stop,&QPushButton::clicked,player,&QMediaPlayer::stop);
        QObject::connect(previous,&QPushButton::clicked,this,[this]{playRow(nextTrackIndex(-1));});
        QObject::connect(next,&QPushButton::clicked,this,[this]{playRow(nextTrackIndex());});
        QObject::connect(party,&QPushButton::toggled,this,[this,party,title](bool on){
            partyEnabled=on;visual.setPartyMode(on);status->setText(on?"Party Mode enabled":"Party Mode disabled");
            if(on){visual.setEffect(effects->currentIndex());visual.show();visual.raise();partyTimer.start(12000);rainbowTimer.start(45);}
            else {partyTimer.stop();rainbowTimer.stop();title->setStyleSheet("");}
        });
        QObject::connect(&partyTimer,&QTimer::timeout,this,[this]{if(!partyEnabled)return;int next=(effects->currentIndex()+1)%effects->count();effects->setCurrentIndex(next);visual.setEffect(next);visual.show();});
        QObject::connect(&rainbowTimer,&QTimer::timeout,this,[this,title]{rainbowPhase=(rainbowPhase+3)%360;title->setStyleSheet(QString("color: hsl(%1, 100%, 65%);").arg(rainbowPhase));});
        QObject::connect(settingsButton,&QPushButton::clicked,this,[this]{
            QDialog dialog(this);dialog.setWindowTitle("μSIK Settings");dialog.resize(520,510);
            auto *layout=new QVBoxLayout(&dialog);
            auto *tabs=new QTabWidget;
            layout->addWidget(tabs);
            auto *general=new QWidget;
            auto *audio=new QWidget;
            tabs->addTab(general,"Player");
            tabs->addTab(audio,"Audio");
            auto *form=new QFormLayout(general);
            OutputControls audioControls(new QFormLayout(audio),route->choice());
            auto *choice=new QComboBox;choice->addItems(Visual::names());choice->setCurrentIndex(effects->currentIndex());form->addRow("Visualization",choice);
            QSettings glSettings(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");
            const QString savedBackend=glSettings.value("gl_backend","auto").toString();
            auto *backendChoice=new QComboBox;
            for(const auto &entry:availableGlBackends())backendChoice->addItem(entry.second,entry.first);
            if(backendChoice->findData(savedBackend)<0)backendChoice->addItem(savedBackend+" (unavailable here)",savedBackend);
            backendChoice->setCurrentIndex(backendChoice->findData(savedBackend));
            form->addRow("OpenGL backend",backendChoice);
            auto *backendHint=new QLabel("Current Qt platform: "+QGuiApplication::platformName()+
                ". Changing the OpenGL backend takes effect after restarting MusiK.");
            backendHint->setWordWrap(true);
            form->addRow("",backendHint);
            auto *beats=new QComboBox;beats->addItems({"2 bars (8 beats)","4 bars (16 beats)","8 bars (32 beats)"});beats->setCurrentIndex(paletteSelection);form->addRow("Laser palette cycle",beats);
            auto *sorting=new QComboBox;sorting->addItems({"Manual / insertion","Title A-Z","Title Z-A","Path A-Z"});sorting->setCurrentIndex(sortSelection);form->addRow("Sort playlist",sorting);
            auto *ordering=new QComboBox;ordering->addItems({"Normal","Reversed","Shuffle (tracks)","Shuffle (albums)","Random"});ordering->setCurrentIndex(orderSelection);form->addRow("Playback order",ordering);
            auto *scopeMode=new QComboBox;scopeMode->addItems({"Oscilloscope","Spectrum analyzer","Combo (half + half)"});scopeMode->setCurrentIndex(scope->mode());form->addRow("Audio display",scopeMode);
            auto *scopeDensity=new QSpinBox;scopeDensity->setRange(64,4096);scopeDensity->setValue(scopeSetting);form->addRow("Oscilloscope points",scopeDensity);
            auto *spectrumDensity=new QSpinBox;spectrumDensity->setRange(8,2048);spectrumDensity->setValue(spectrumSetting);form->addRow("Spectrum bars (1 px possible)",spectrumDensity);
            auto *gradientChoice=new QComboBox;gradientChoice->addItems({"μSIK purple / orange","Black → White","Bright yellow → Deep green","Electric cyan → Deep blue","Coral → Midnight violet","Lime → Deep teal","Electric pink → Midnight blue","Golden orange → Deep burgundy","Custom colors"});gradientChoice->setCurrentIndex(gradientSelection);form->addRow("Background gradient",gradientChoice);
            auto *reverse=new QCheckBox("Reverse gradient direction");reverse->setChecked(reverseGradient);form->addRow("",reverse);
            auto *colors=new QPushButton("Choose custom gradient stops…");form->addRow(colors);
            QObject::connect(colors,&QPushButton::clicked,&dialog,[this,&dialog]{QColor a=QColorDialog::getColor(customStart,&dialog,"Gradient start");if(a.isValid())customStart=a;QColor b=QColorDialog::getColor(customEnd,&dialog,"Gradient end");if(b.isValid())customEnd=b;});
            auto *apply=new QPushButton("Apply");layout->addWidget(apply);
            QObject::connect(apply,&QPushButton::clicked,&dialog,&QDialog::accept);
            if(dialog.exec()!=QDialog::Accepted)return;
            const QString selectedBackend=backendChoice->currentData().toString();
            glSettings.setValue("gl_backend",selectedBackend);
            glSettings.sync();
            if(selectedBackend!=savedBackend)status->setText("OpenGL backend saved. Restart MusiK to use it.");
            route->configure(audioControls.value());route->choice().save();
            effects->setCurrentIndex(choice->currentIndex());paletteSelection=beats->currentIndex();visual.setPaletteBeats(8<<paletteSelection);
            const bool sortChanged=sortSelection!=sorting->currentIndex();
            sortSelection=sorting->currentIndex();orderSelection=ordering->currentIndex();scopeSetting=scopeDensity->value();spectrumSetting=spectrumDensity->value();gradientSelection=gradientChoice->currentIndex();reverseGradient=reverse->isChecked();
            scope->setScopePoints(scopeSetting);scope->setBands(spectrumSetting);scope->setMode(scopeMode->currentIndex());applyGradient();if(sortChanged)sortTracks();
        });
        QObject::connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *i){playRow(list->row(i));});
        QObject::connect(player,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus s){if(s==QMediaPlayer::EndOfMedia)playRow(nextTrackIndex());});
        QObject::connect(effects,&QComboBox::currentIndexChanged,this,[this](int index){if(visual.isVisible())visual.setEffect(index);});
        QObject::connect(visButton,&QPushButton::clicked,this,[this]{visual.setEffect(effects->currentIndex());visual.show();visual.raise();});
        
        QSettings settings(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");
        int storedEffect=settings.value("effect",0).toInt();
        if(settings.value("effectListVersion",0).toInt()<2){
            // V1 had Laser Nexus before Orbit Streams. Preserve old selection.
            if(storedEffect==5)storedEffect=4;
            else if(storedEffect==6)storedEffect=5;
            else if(storedEffect==7)storedEffect=6;
        }
        effects->setCurrentIndex(std::clamp(storedEffect,0,effects->count()-1));sortSelection=settings.value("sort",0).toInt();orderSelection=settings.value("order",0).toInt();gradientSelection=settings.value("gradient",0).toInt();reverseGradient=settings.value("reverseGradient",false).toBool();scopeSetting=settings.value("scope",512).toInt();spectrumSetting=settings.value("spectrum",48).toInt();scope->setScopePoints(scopeSetting);scope->setBands(spectrumSetting);scope->setMode(settings.value("scopeMode",0).toInt());applyGradient();
        auto autosave=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/autosave.m3u8";if(QFile::exists(autosave))loadList(autosave);
        QObject::connect(qApp,&QApplication::aboutToQuit,this,[this]{QString directory=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(directory);saveList(directory+"/autosave.m3u8");QSettings s(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");s.setValue("effect",effects->currentIndex());s.setValue("effectListVersion",2);s.setValue("sort",sortSelection);s.setValue("order",orderSelection);s.setValue("gradient",gradientSelection);s.setValue("reverseGradient",reverseGradient);s.setValue("scopeMode",scope->mode());s.setValue("scope",scopeSetting);s.setValue("spectrum",spectrumSetting);});
    }
    void applyGradient(){
        QColor a("#381754"),b("#cf713e");
        if(gradientSelection==1){a=Qt::black;b=Qt::white;}
        if(gradientSelection==2){a=QColor(255,255,0);b=QColor(0,65,22);}
        if(gradientSelection==3){a=QColor(0,255,255);b=QColor(10,12,85);}
        if(gradientSelection==4){a=QColor(255,110,85);b=QColor(35,8,80);}
        if(gradientSelection==5){a=QColor(190,255,20);b=QColor(3,40,48);}
        if(gradientSelection==6){a=QColor(255,40,180);b=QColor(6,18,70);}
        if(gradientSelection==7){a=QColor(255,160,30);b=QColor(70,8,35);}
        if(gradientSelection==8){a=customStart;b=customEnd;}
        if(reverseGradient)std::swap(a,b);
        QString stops=QString("stop:0 %1,").arg(a.name());
        if(gradientSelection==0 && !reverseGradient)stops+="stop:0.46 #632b60,";
        stops+="stop:1 "+b.name();
        centralWidget()->setStyleSheet(QString("QWidget#canvas{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,%1);}").arg(stops));
    }
    void sortTracks(){
        if(!sortSelection)return;
        QString playing=player->source().toLocalFile();
        QList<QPair<QString,QString>> tracks;
        for(int i=0;i<list->count();++i){auto *x=list->item(i);tracks.append({x->data(Qt::UserRole).toString(),x->data(Qt::UserRole+1).toString()});}
        std::sort(tracks.begin(),tracks.end(),[this](const auto &a,const auto &b){
            if(sortSelection==3)return a.first.compare(b.first,Qt::CaseInsensitive)<0;
            auto cmp=a.second.compare(b.second,Qt::CaseInsensitive);return sortSelection==2?cmp>0:cmp<0;
        });
        QStringList paths;
        paths.reserve(tracks.size());
        for(const auto &track:tracks)paths.append(track.first);
        list->setUpdatesEnabled(false);
        list->clear();
        addFiles(paths);
        for(int i=0;i<list->count();++i){
            if(list->item(i)->data(Qt::UserRole).toString()==playing){list->setCurrentRow(i);break;}
        }
        list->setUpdatesEnabled(true);
    }
    int nextTrackIndex(int direction=1){
        QStringList paths;
        for(int i=0;i<list->count();++i)paths.append(list->item(i)->data(Qt::UserRole).toString());
        const int count=paths.size();
        if(!count)return -1;
        const auto current=player->source().toLocalFile();
        int row=paths.indexOf(current);
        if(paths!=shufflePaths || orderSelection!=shuffleMode){
            shuffleMode=orderSelection;shufflePaths=paths;shuffleQueue.clear();
            history.removeIf([&](const QString &path){return !paths.contains(path);});
            historyIndex=history.lastIndexOf(current);
            if(historyIndex<0){history.clear();if(row>=0){history.append(current);historyIndex=0;}}
        }
        if(orderSelection==2 || orderSelection==4){
            if(direction<0)return historyIndex>0?paths.indexOf(history[--historyIndex]):-1;
            if(historyIndex+1<history.size())return paths.indexOf(history[++historyIndex]);
            if(shuffleQueue.isEmpty() || orderSelection==4){
                shuffleQueue.clear();
                for(int i=0;i<count;++i)if(i!=row || count==1)shuffleQueue.append(i);
                std::shuffle(shuffleQueue.begin(),shuffleQueue.end(),*QRandomGenerator::global());
            }
            row=shuffleQueue.takeLast();
            history.append(paths[row]);historyIndex=history.size()-1;
            return row;
        }
        if(orderSelection==3){
            if(shuffleQueue.isEmpty()){
                QMap<QString,QVector<int>> albums;
                for(int i=0;i<count;++i)albums[QFileInfo(paths[i]).absolutePath()].append(i);
                auto blocks=albums.values();
                std::shuffle(blocks.begin(),blocks.end(),*QRandomGenerator::global());
                for(const auto &block:blocks)shuffleQueue+=block;
            }
            int pos=shuffleQueue.indexOf(row);
            return shuffleQueue[(pos+direction+count)%count];
        }
        return (row+(orderSelection==1?-direction:direction)+count)%count;
    }
    void closeEvent(QCloseEvent *event) override {visual.close();QMainWindow::closeEvent(event);}
    void filterTrack(QListWidgetItem *item){
        const auto path=item->data(Qt::UserRole).toString().toCaseFolded();
        const bool matches=std::all_of(searchTerms.cbegin(),searchTerms.cend(),
            [&](const QString &term){return path.contains(term);});
        item->setHidden(!matches);
    }
    // Used for individual files (e.g. dropping a file on the command line).
    void addFile(const QString &path){addFiles(QStringList{path});}

    // Bulk insertion: one duplicate-index pass, one view refresh and one renumber.
    // Previously addFile() scanned all existing tracks AND renumbered the entire
    // list after each insert. Loading N tracks was O(N^2) (or worse with painting).
    void addFiles(const QStringList &paths){
        if(paths.isEmpty())return;
        QSet<QString> known;
        known.reserve(list->count()+paths.size());
        for(int i=0;i<list->count();++i)
            known.insert(list->item(i)->data(Qt::UserRole).toString());

        const bool updatesWereEnabled=list->updatesEnabled();
        list->setUpdatesEnabled(false);
        int added=0;
        for(const QString &path:paths){
            QFileInfo file(path);
            if(!file.isFile())continue;
            const QString absolute=file.absoluteFilePath();
            if(known.contains(absolute))continue;
            known.insert(absolute);
            auto *item=new QListWidgetItem(file.completeBaseName(),list);
            item->setData(Qt::UserRole,absolute);
            item->setData(Qt::UserRole+1,file.completeBaseName());
            item->setToolTip(absolute);
            if(!searchTerms.isEmpty())filterTrack(item);
            ++added;
        }
        if(added)refreshNumbers();
        list->setUpdatesEnabled(updatesWereEnabled);
    }

    void refreshNumbers(){
        // Resolve the playing file once, not separately for every list item.
        const QString playing=(player && player->source().isLocalFile())
            ? QFileInfo(player->source().toLocalFile()).absoluteFilePath() : QString();
        for(int n=0;n<list->count();++n){
            auto *item=list->item(n);
            const QString title=item->data(Qt::UserRole+1).toString();
            const bool isPlaying=!playing.isEmpty() && playing==item->data(Qt::UserRole).toString();
            item->setText(QString("%1    %2  %3").arg(n+1,3).arg(isPlaying?"▶":" ").arg(title));
        }
    }

    void loadList(const QString &path){
        QElapsedTimer timer;
        timer.start();
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly|QIODevice::Text))return;
        const QDir dir=QFileInfo(path).dir();
        QStringList paths;
        while(!file.atEnd()){
            const QString entry=QString::fromUtf8(file.readLine()).trimmed();
            if(entry.isEmpty()||entry.startsWith('#')||entry.contains("://"))continue;
            paths.append(dir.absoluteFilePath(entry));
        }
        const qint64 readMs=timer.elapsed();
        const int previousCount=list->count();
        addFiles(paths);
        qInfo() << "[MusiK] Playlist read:" << readMs << "ms, inserted:"
                << timer.elapsed()-readMs << "ms, entries:" << list->count()-previousCount;
    }
    void saveList(const QString &path){QFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Text))return;f.write("#EXTM3U\n");QDir dir=QFileInfo(path).dir();for(int i=0;i<list->count();i++)f.write((dir.relativeFilePath(list->item(i)->data(Qt::UserRole).toString())+"\n").toUtf8());}
    void playRow(int i){if(i<0||i>=list->count())return;
        const auto path=list->item(i)->data(Qt::UserRole).toString();
        if((orderSelection==2 || orderSelection==4) && history.value(historyIndex)!=path){
            history.resize(historyIndex+1);history.append(path);++historyIndex;
            shuffleQueue.removeAll(i);
        }
        list->setCurrentRow(i);player->setSource(QUrl::fromLocalFile(list->item(i)->data(Qt::UserRole).toString()));player->play();trackLabel->setText(list->item(i)->data(Qt::UserRole+1).toString());refreshNumbers();}
    void showTrackInformation(const QString &path){
        auto *dialog=new TrackInformation(path,this,centralWidget()->styleSheet());
        dialog->open();
    }
    void analyze(const QAudioBuffer &b){
        if(!b.isValid()||b.frameCount()==0)return;
        const auto format=b.format();int count=b.sampleCount(),channels=format.channelCount();if(!count||!channels)return;
        double sum=0,mid=0,high=0;int samples=0;
        // Lightweight sample-domain envelope for the benchmark; NOT equivalent to Python's FFT detector.
        auto add=[&](double x){sum+=x*x;if(samples)high+=std::abs(x-lastSample);lastSample=x;samples++;};
        switch(format.sampleFormat()){
            case QAudioFormat::Int16:{auto *p=b.constData<qint16>();for(int i=0;i<count;i+=channels)add(p[i]/32768.);break;}
            case QAudioFormat::Int32:{auto *p=b.constData<qint32>();for(int i=0;i<count;i+=channels)add(p[i]/2147483648.);break;}
            case QAudioFormat::Float:{auto *p=b.constData<float>();for(int i=0;i<count;i+=channels)add(p[i]);break;}
            case QAudioFormat::UInt8:{auto *p=b.constData<quint8>();for(int i=0;i<count;i+=channels)add((p[i]-128)/128.);break;}
            default:return;
        }
        // Actual decoded PCM samples (no synthetic waveform). Downsample for display.
        waveform.clear(); waveform.reserve(512);
        auto sampleAt=[&](int frame)->float {
            int offset=std::clamp(frame*channels,0,count-channels);
            switch(format.sampleFormat()){
                case QAudioFormat::Int16:return b.constData<qint16>()[offset]/32768.f;
                case QAudioFormat::Int32:return b.constData<qint32>()[offset]/2147483648.f;
                case QAudioFormat::Float:return b.constData<float>()[offset];
                case QAudioFormat::UInt8:return (b.constData<quint8>()[offset]-128)/128.f;
                default:return 0;
            }
        };
        const int frames=count/channels;
        for(int i=0;i<512;++i)waveform.append(sampleAt(i*std::max(1,frames-1)/511));
        // Iterative Cooley-Tukey FFT: one transform per PCM buffer, not 48 DFTs.
        constexpr int n=4096;
        std::array<std::complex<double>,n> fft{};
        for(int i=0;i<n;++i){double x=i<frames?sampleAt(i):0.;fft[i]={x*(.5-.5*std::cos(2*pi*i/(n-1.))),0.};}
        for(int i=1,j=0;i<n;i++){int bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(fft[i],fft[j]);}
        for(int len=2;len<=n;len<<=1){const double a=-2*pi/len;std::complex<double> root(std::cos(a),std::sin(a));for(int i=0;i<n;i+=len){std::complex<double> w(1,0);for(int j=0;j<len/2;j++){auto u=fft[i+j],v=fft[i+j+len/2]*w;fft[i+j]=u+v;fft[i+j+len/2]=u-v;w*=root;}}}
        // One value per FFT frequency bin; allow settings to select up to 2048 distinct bars.
        spectrumBands.resize(n/2);
        for(int bin=0;bin<n/2;++bin){
            const double amplitude=std::abs(fft[bin])*4.0/n;
            const double db=20.0*std::log10(std::max(1e-9,amplitude));
            spectrumBands[bin]=std::clamp(float((db+72.0)/72.0),0.f,1.f);
        }
        float raw=std::sqrt(sum/std::max(1,samples));float level=std::clamp(float(std::log1p(raw*18)/std::log1p(.4*18)),0.f,1.f);
        float previous=state.energy;state.energy=.82f*state.energy+.18f*level;
        state.treble=std::clamp(float(high/std::max(1,samples)*6),0.f,1.f);
        state.mid=state.energy;
        state.pulse=state.energy>previous+.055f?1.f:state.pulse*.85f;
        if(state.pulse>.9f){++beats;visual.beat();if(beats%16==0 && effects->currentIndex()>=4 && effects->currentIndex()<=6)visual.changeLaserPhrase();}visual.setSound(state);visual.setWaveform(waveform);scope->setSignal(waveform,spectrumBands);
    }
private:
    QLineEdit *search;
    QStringList searchTerms;
    QListWidget *list;SeekSlider *seek;QLabel *elapsedLabel,*durationLabel;ScopeWidget *scope;QLabel *trackLabel;QComboBox *effects;QLabel *status;QMediaPlayer *player;QAudioOutput *output;std::unique_ptr<PlaybackOutput> route;Visual visual;SoundState state;double lastSample=0;int beats=0;QVector<float> waveform,spectrumBands;QTimer partyTimer,rainbowTimer;bool partyEnabled=false,reverseGradient=false;int rainbowPhase=0,sortSelection=0,orderSelection=0,paletteSelection=1,gradientSelection=0,scopeSetting=512,spectrumSetting=48;QColor customStart=QColor(255,0,255),customEnd=QColor(255,150,0);QVector<int> shuffleQueue;
    QStringList shufflePaths,history;
    int shuffleMode=-1,historyIndex=-1;
};
int main(int argc, char **argv) {
    QElapsedTimer startup;
    startup.start();

    QSettings glSettings(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");
    applyGlBackend(glSettings.value("gl_backend","auto").toString());

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("MusiKCpp");
    app.setApplicationDisplayName("μSIK");
    app.setOrganizationName("MusiK");

    QIcon icon(QCoreApplication::applicationDirPath() + "/MusiK-logo.png");
    app.setWindowIcon(icon);

    qInfo() << "[MusiK] Qt initialized:"
            << startup.elapsed() << "ms";

    PlayerWindow window;

    qInfo() << "[MusiK] Window constructed:"
            << startup.elapsed() << "ms";

    window.setWindowIcon(icon);

    QStringList commandLineFiles;
    for (int i = 1; i < argc; i++)
        commandLineFiles.append(QString::fromLocal8Bit(argv[i]));
    window.addFiles(commandLineFiles);

    qInfo() << "[MusiK] Files added:"
            << startup.elapsed() << "ms";

    window.show();

    qInfo() << "[MusiK] Window shown:"
            << startup.elapsed() << "ms";

    return app.exec();
}
