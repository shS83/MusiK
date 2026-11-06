#include "attractor_pack_scene.h"
#include <QDateTime>
#include <QPainter>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr float tick=1.f/60;
constexpr double switchSeconds=15.;
constexpr float tau=2*std::numbers::pi_v<float>;
constexpr float limits[4]={10,6,150,8};
constexpr float scales[4]={5.4f,4.4f,62,3.8f};
constexpr float palettes[4][2][3]={
    {{1,1,1},{0,.33f,.67f}}, {{1,1,.5f},{.80f,.90f,.30f}},
    {{1,.7f,.4f},{0,.15f,.20f}}, {{2,1,0},{.50f,.20f,.25f}}
};
}

AttractorPackScene::AttractorPackScene(int count):count_(std::max(1,count)) {
    // Particle arrays are allocated only by the CPU regression reference render().
    parameters();
}

QString AttractorPackScene::name() const {
    static const QStringList names={"Clifford","De Jong","Lorenz","Aizawa"};
    return names[current_];
}

float AttractorPackScene::random(float low,float high) {
    return std::uniform_real_distribution<float>(low,high)(rng_);
}

void AttractorPackScene::seed(size_t i,float spread) {
    if(current_==2){x_[i]=random(-15,15)*spread;y_[i]=random(-20,20)*spread;z_[i]=random(5,40);}
    else if(current_==3){x_[i]=random(-.8f,.8f)*spread;y_[i]=random(-.8f,.8f)*spread;z_[i]=random(-.2f,1.2f);}
    else{x_[i]=random(-1,1)*spread;y_[i]=random(-1,1)*spread;z_[i]=0;}
}

void AttractorPackScene::reset() {
    ++clearGeneration_;
    levels_.fill(0);bass_=spinVelocity_=0;peak_=1e-3f;
    std::fill(hist_.begin(),hist_.end(),0);
}

void AttractorPackScene::select(int index) {
    ++generation_;
    current_=(index%4+4)%4;
    for(size_t i=0;i<x_.size();++i)seed(i);
    for(auto &v:hist_)v*=.35f;
    peak_=1e-3f;pending_=tick;switchElapsed_=0;
    parameters();
}

void AttractorPackScene::advanceClock(double dt) {
    if(!autoSwitch_)return;
    const double elapsed=switchElapsed_+std::max(0.,dt);
    const double switches=std::floor(elapsed/switchSeconds);
    if(switches>=1)select(current_+int(std::fmod(switches,4.)));
    switchElapsed_=std::fmod(elapsed,switchSeconds);
}

void AttractorPackScene::beat(float strength) {
    if(!audioEnabled_)return;
    strength=std::clamp(strength,0.f,1.f);
    bass_=std::max(bass_,strength);
    phase_+=.06f*strength;
    spinVelocity_=std::min(3.f,spinVelocity_+.6f*strength);
}

void AttractorPackScene::burst() {
    ++burstGeneration_;
    for(size_t i=0;i<x_.size()/3;++i)seed(i,1.8f);
}

bool AttractorPackScene::key(int key) {
    if(key>=Qt::Key_1&&key<=Qt::Key_4){select(key-Qt::Key_1);return true;}
    switch(key){
    case Qt::Key_H: hudVisible_=!hudVisible_;break;
    case Qt::Key_T: autoSwitch_=!autoSwitch_;switchElapsed_=0;break;
    case Qt::Key_N: select(current_+1);break;
    case Qt::Key_A:
        audioEnabled_=!audioEnabled_;
        if(!audioEnabled_){levels_.fill(0);bass_=spinVelocity_=0;}
        break;
    case Qt::Key_Space: paused_=!paused_;break;
    case Qt::Key_C: palette_=(palette_+1)%4;break;
    case Qt::Key_S: wantShot_=true;break;
    case Qt::Key_R:
        if(current_==0)bases_[0]={random(-2,-1.2f),random(1.2f,2),random(.5f,1.6f),random(.5f,1.6f)};
        else if(current_==1)for(auto &v:bases_[1])v=(random(0,1)<.5f?-1:1)*random(1.2f,2.8f);
        else spin_=random(0,tau);
        break;
    default:return false;
    }
    return true;
}

void AttractorPackScene::parameters() {
    const float t=time_,mid=levels_[1],mx=float(pointer_.x()),my=float(pointer_.y());
    if(current_==0){
        const auto &b=bases_[0];
        parameters_={b[0]+.30f*std::sin(t*.31f)+.12f*mid,
                     b[1]+.25f*std::sin(t*.23f+1.3f)-.10f*mid,
                     b[2]+.35f*std::sin(t*.17f+.4f)+mx*.8f,
                     b[3]+.30f*std::sin(t*.29f+2.1f)+my*.8f,0,0};
    }else if(current_==1){
        const auto &b=bases_[1];
        parameters_={b[0]+.25f*std::sin(t*.27f)+.15f*mid,
                     b[1]+.25f*std::sin(t*.21f+1),
                     b[2]+.20f*std::sin(t*.33f+2)+mx*.6f,
                     b[3]+.20f*std::sin(t*.19f+.5f)+my*.6f,0,0};
    }else if(current_==2)parameters_={10,28+6*mid+mx*6,8.f/3+.3f*std::sin(t*.2f),0,0,0};
    else parameters_={.95f+.04f*std::sin(t*.23f),.70f+mx*.15f,.60f,
                      3.5f+.3f*std::sin(t*.17f)+.5f*mid,.25f,.10f};
}

void AttractorPackScene::step(float drive) {
    const auto &p=parameters_;
    const float dt=(current_==2?.006f:.011f)*drive,limit=limits[current_];
    for(size_t i=0;i<x_.size();++i){
        float x=x_[i],y=y_[i],z=z_[i];
        if(current_==0){
            const float nx=std::sin(p[0]*y)+p[2]*std::cos(p[0]*x);
            y=std::sin(p[1]*x)+p[3]*std::cos(p[1]*y);x=nx;
        }else if(current_==1){
            const float nx=std::sin(p[0]*y)-std::cos(p[1]*x);
            y=std::sin(p[2]*x)-std::cos(p[3]*y);x=nx;
        }else for(int j=0;j<2;++j){
            float dx,dy,dz;
            if(current_==2){dx=p[0]*(y-x);dy=x*(p[1]-z)-y;dz=x*y-p[2]*z;}
            else{
                dx=(z-p[1])*x-p[3]*y;dy=p[3]*x+(z-p[1])*y;
                dz=p[2]+p[0]*z-z*z*z/3-(x*x+y*y)*(1+p[4]*z)+p[5]*z*x*x*x;
            }
            x+=dx*dt;y+=dy*dt;z+=dz*dt;
        }
        // NaN also fails these comparisons. Never project an escaping trajectory.
        if(!(std::abs(x)<limit&&std::abs(y)<limit&&std::abs(z)<limit))seed(i);
        else{x_[i]=x;y_[i]=y;z_[i]=z;}
    }
}

QImage AttractorPackScene::render(double elapsed,int width,int height,float energy,float mid,float treble) {
    if(x_.empty()){
        x_.resize(count_);y_.resize(count_);z_.resize(count_);
        for(size_t i=0;i<x_.size();++i)seed(i);
    }
    const float scale=std::min({1.f,1100.f/std::max(1,width),800.f/std::max(1,height)});
    const int w=std::max(1,qRound(width*scale)),h=std::max(1,qRound(height*scale));
    if(image_.size()!=QSize(w,h)){
        image_=QImage(w,h,QImage::Format_RGB32);
        hist_.assign(size_t(w)*h,0);peak_=1e-3f;
    }
    const auto frame=prepareFrame(elapsed,energy,mid,treble);
    const float dt=frame.dt,bass=frame.bass;treble=frame.treble;
    const bool is3d=current_>=2;
    const float angle=spin_+(is3d?float(pointer_.x())*3:0),tilt=.4f+float(pointer_.y())*.8f;
    const float ca=std::cos(angle),sa=std::sin(angle),ct=std::cos(tilt),st=std::sin(tilt);
    const float zoom=std::min(w,h)/scales[current_]*(1+.10f*bass);
    for(int tickIndex=0;tickIndex<frame.steps;++tickIndex){
        step(1+1.5f*bass);
        std::uniform_int_distribution<size_t> index(0,x_.size()-1);
        const auto count=std::max(size_t(1),size_t(x_.size()*(.005f+treble*.02f)));
        for(size_t i=0;i<count;++i)seed(index(rng_));
        for(auto &v:hist_)v*=.86f;
        for(size_t i=0;i<x_.size();++i){
            const float horizontal=x_[i]*ca-y_[i]*sa;
            float vertical=x_[i]*sa+y_[i]*ca,weight=1;
            if(is3d){
                const float z=z_[i]-(current_==2?25.f:.45f),depth=vertical*ct+z*st;
                vertical=-(z*ct-vertical*st);
                weight=std::clamp(.8f+depth*(current_==2?.02f:.25f),.25f,1.6f);
            }
            const int px=int(horizontal*zoom+w*.5f),py=int(vertical*zoom+h*.5f);
            if(px>=0&&px<w&&py>=0&&py<h)hist_[size_t(py)*w+px]+=weight;
        }
    }
    const float gain=.9f*(1+.9f*bass);
    peak_=std::max({peak_*std::exp(-dt*1.2f),std::log1p(*std::max_element(hist_.begin(),hist_.end())*gain),1e-3f});
    std::array<QRgb,256> palette;
    for(int i=0;i<256;++i){
        const float t=i/255.f,fade=std::pow(t,.75f);
        int rgb[3];
        for(int j=0;j<3;++j){
            const float color=(.5f+.5f*std::cos(tau*(t*palettes[palette_][0][j]+palettes[palette_][1][j]+phase_)))*fade;
            rgb[j]=int(std::clamp(color,0.f,1.f)*255);
        }
        palette[i]=qRgb(rgb[0],rgb[1],rgb[2]);
    }
    for(int y=0;y<h;++y){
        auto *line=reinterpret_cast<QRgb *>(image_.scanLine(y));
        for(int x=0;x<w;++x){
            const float v=std::log1p(hist_[size_t(y)*w+x]*gain)/peak_;
            line[x]=palette[int(std::clamp(v,0.f,1.f)*255)];
        }
    }
    return image_;
}

AttractorGpuFrame AttractorPackScene::prepareFrame(double elapsed,float energy,float mid,float treble) {
    advanceClock(elapsed);
    const float dt=float(std::clamp(elapsed,0.,.05));
    const std::array<float,3> target=audioEnabled_?std::array<float,3>{energy,mid,treble}:std::array<float,3>{};
    for(int i=0;i<3;++i)levels_[i]+=(std::clamp(target[i],0.f,1.f)-levels_[i])*(1-std::exp(-dt/.12f));
    bass_*=std::exp(-dt*5.5f);
    const float bass=std::max(levels_[0],bass_);
    treble=levels_[2];spinVelocity_*=std::exp(-dt*2.5f);
    const bool is3d=current_>=2;
    if(!paused_){time_+=dt;spin_+=(.12f*(is3d?.6f:1.f)+spinVelocity_)*dt;}
    phase_+=dt*(.03f+.25f*treble);
    parameters();
    pending_=std::min(.05f,pending_+dt);
    const int steps=int(pending_/tick);pending_-=steps*tick;
    AttractorGpuFrame frame;
    frame.kind=current_;frame.count=count_;frame.steps=steps;frame.palette=palette_;
    frame.parameters=parameters_;frame.dt=dt;frame.bass=bass;frame.treble=treble;
    frame.angle=spin_+(is3d?float(pointer_.x())*3:0);frame.tilt=.4f+float(pointer_.y())*.8f;frame.phase=phase_;
    frame.generation=generation_;frame.clearGeneration=clearGeneration_;frame.burstGeneration=burstGeneration_;
    return frame;
}

bool AttractorPackScene::paint(double dt,int width,int height,GLuint target,float energy,float mid,float treble) {
    if(!renderer_)renderer_=std::make_unique<AttractorPackRenderer>();
    const auto frame=prepareFrame(dt,energy,mid,treble);
    if(!renderer_->draw(target,width,height,frame)){gpuError_=renderer_->error();return false;}
    if(wantShot_){
        wantShot_=false;
        const QString path="attractorpack_"+QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")+".png";
        if(!renderer_->screenshot(target,width,height).save(path))qWarning("AttractorPack screenshot save failed: %s",qPrintable(path));
    }
    return true;
}

void AttractorPackScene::release() {
    if(renderer_){renderer_->release();renderer_.reset();}
    gpuError_.clear();
}

void AttractorPackScene::paintHud(QPainter &p) {
    if(hudVisible_){
        p.fillRect(QRect(12,12,610,122),QColor(0,0,0,190));p.setPen(QColor("#c8f4ff"));
        QStringList values;
        for(int i=0;i<(current_==2?3:(current_==3?6:4));++i)values<<QString::number(parameters_[i],'f',2);
        const QStringList lines={
            QString("ATTRACTORPACK · %1 [%2/4] · Auto %3 · Audio %4").arg(name()).arg(current_+1)
                .arg(autoSwitch_?QString::number(switchSeconds-switchElapsed_,'f',1)+"s":"off").arg(audioEnabled_?"on":"off"),
            "N next · T auto switch · 1–4 select · A audio · H HUD",
            "SPACE drift on/off · R new shape/angle · C palette · S screenshot",
            "Mouse: shape · Click: burst · F / F11 fullscreen · ESC close",
            "Parameters: "+values.join("  ")};
        for(int i=0;i<lines.size();++i)p.drawText(24,34+i*22,lines[i]);
    }
}
