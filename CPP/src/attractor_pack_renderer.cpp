#include "attractor_pack_renderer.h"
#include <QCoreApplication>
#include <QFile>
#include <QVector2D>
#include <QVector4D>
#include <algorithm>
#include <cmath>

bool AttractorPackRenderer::create(Target &t,int w,int h,bool rgba,bool floating) {
    t.width=w;t.height=h;
    glGenTextures(1,&t.texture);glBindTexture(GL_TEXTURE_2D,t.texture);
    glTexImage2D(GL_TEXTURE_2D,0,floating?(rgba?GL_RGBA32F:GL_R32F):GL_RGBA8,w,h,0,
                 rgba?GL_RGBA:GL_RED,floating?GL_FLOAT:GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.texture,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){
        error_="AttractorPack requires renderable floating-point textures (OpenGL 3.3)";return false;
    }
    return true;
}

void AttractorPackRenderer::destroy(Target &t) {
    if(t.fbo)glDeleteFramebuffers(1,&t.fbo);
    if(t.texture)glDeleteTextures(1,&t.texture);
    t={};
}

void AttractorPackRenderer::clear(const Target &t,float value) {
    glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);
    glClearColor(value,value,value,1);glClear(GL_COLOR_BUFFER_BIT);
}

bool AttractorPackRenderer::initialize(int count) {
    if(vao_)return error_.isEmpty();
    if(!initializeOpenGLFunctions()){error_="AttractorPack requires OpenGL 3.3";return false;}
    error_.clear();count_=count;
    const QString directory=QCoreApplication::applicationDirPath()+"/shaders/attractor/";
    const char *names[]={"update","points","fade","reduce","peak","color","composite"};
    for(int i=0;i<PassCount;++i){
        QFile v(directory+(i==Points?"points.vert":"fullscreen.vert"));
        QFile f(directory+names[i]+".frag");
        if(!v.open(QIODevice::ReadOnly)||!f.open(QIODevice::ReadOnly)){
            error_="AttractorPack shader missing: "+directory+names[i];return false;
        }
        auto p=std::make_unique<QOpenGLShaderProgram>();
        if(!p->addShaderFromSourceCode(QOpenGLShader::Vertex,v.readAll())||
           !p->addShaderFromSourceCode(QOpenGLShader::Fragment,f.readAll())||!p->link()){
            error_="AttractorPack shader failed: "+p->log();return false;
        }
        programs_[i]=std::move(p);
    }
    glGenVertexArrays(1,&vao_);
    const int side=int(std::ceil(std::sqrt(count)));
    for(auto &t:particles_)if(!create(t,side,(count+side-1)/side,true,true))return false;
    for(auto &t:peaks_){if(!create(t,1,1,false,true))return false;clear(t,.001f);}
    return true;
}

bool AttractorPackRenderer::resize(int width,int height) {
    const double scale=std::min({1.,1100./std::max(1,width),800./std::max(1,height)});
    const int w=std::max(1,qRound(width*scale)),h=std::max(1,qRound(height*scale));
    if(colors_.width==w&&colors_.height==h)return true;
    for(auto &t:densities_)destroy(t);
    for(auto &t:reductions_)destroy(t);
    reductions_.clear();destroy(colors_);
    for(auto &t:densities_){if(!create(t,w,h,false,true))return false;clear(t);}
    int rw=w,rh=h;
    while(rw>1||rh>1){
        rw=(rw+1)/2;rh=(rh+1)/2;reductions_.push_back({});
        if(!create(reductions_.back(),rw,rh,false,true))return false;
    }
    if(!create(colors_,w,h,true,false))return false;
    for(auto &t:peaks_)clear(t,.001f);
    return true;
}

QOpenGLShaderProgram &AttractorPackRenderer::use(Pass pass,const Target &target) {
    glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,target.width,target.height);
    auto &p=*programs_[pass];p.bind();glBindVertexArray(vao_);return p;
}

void AttractorPackRenderer::texture(QOpenGLShaderProgram &p,const char *name,int unit,const Target &t) {
    glActiveTexture(GL_TEXTURE0+unit);glBindSampler(unit,0);
    glBindTexture(GL_TEXTURE_2D,t.texture);p.setUniformValue(name,unit);
}
void AttractorPackRenderer::triangle(){glDrawArrays(GL_TRIANGLES,0,3);}

bool AttractorPackRenderer::draw(GLuint target,int width,int height,const AttractorGpuFrame &f) {
    if(!initialize(f.count))return false;
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    if(!resize(width,height))return false;
    const bool initializePoints=generation_!=f.generation,burst=burstGeneration_!=f.burstGeneration;
    if(clearGeneration_!=f.clearGeneration)for(auto &t:densities_)clear(t);
    if(initializePoints||clearGeneration_!=f.clearGeneration)for(auto &t:peaks_)clear(t,.001f);
    for(int tick=0;tick<std::max(f.steps,int(initializePoints||burst));++tick){
        int back=1-front_;serial_=int((uint32_t(serial_)+1u)&0x7fffffffu);
        auto &p=use(Update,particles_[back]);texture(p,"particles",0,particles_[front_]);
        p.setUniformValue("kind",f.kind);
        p.setUniformValue("parameters",QVector4D(f.parameters[0],f.parameters[1],f.parameters[2],f.parameters[3]));
        p.setUniformValue("extra",QVector2D(f.parameters[4],f.parameters[5]));
        p.setUniformValue("drive",1+1.5f*f.bass);p.setUniformValue("rebirth",.005f+.02f*f.treble);
        p.setUniformValue("initialize_points",int(initializePoints&&tick==0));p.setUniformValue("burst",int(burst&&tick==0));
        p.setUniformValue("integrate",int(tick<f.steps));p.setUniformValue("count",count_);p.setUniformValue("serial",serial_);
        triangle();front_=back;
        back=1-densityFront_;
        auto &fade=use(Fade,densities_[back]);texture(fade,"source",0,densities_[densityFront_]);
        fade.setUniformValue("decay",(tick<f.steps?.86f:1.f)*(initializePoints&&tick==0?.35f:1.f));triangle();
        auto &point=use(Points,densities_[back]);texture(point,"particles",0,particles_[front_]);
        point.setUniformValue("kind",f.kind);point.setUniformValue("resolution",QVector2D(colors_.width,colors_.height));
        point.setUniformValue("angle",f.angle);point.setUniformValue("tilt",f.tilt);
        constexpr float scales[]={5.4f,4.4f,62.f,3.8f};
        point.setUniformValue("zoom",std::min(colors_.width,colors_.height)/scales[f.kind]*(1+.1f*f.bass));
        glEnable(GL_PROGRAM_POINT_SIZE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE);
        glDrawArrays(GL_POINTS,0,count_);glDisable(GL_BLEND);glDisable(GL_PROGRAM_POINT_SIZE);densityFront_=back;
    }
    Target source=densities_[densityFront_];
    for(auto &t:reductions_){auto &p=use(Reduce,t);texture(p,"source",0,source);triangle();source=t;}
    const int back=1-peakFront_;const float gain=.9f*(1+.9f*f.bass);
    auto &peak=use(Peak,peaks_[back]);texture(peak,"maximum",0,source);texture(peak,"previous_peak",1,peaks_[peakFront_]);
    peak.setUniformValue("gain",gain);peak.setUniformValue("decay",std::exp(-f.dt*1.2f));triangle();peakFront_=back;
    auto &color=use(Color,colors_);texture(color,"density",0,densities_[densityFront_]);texture(color,"peak",1,peaks_[back]);
    color.setUniformValue("gain",gain);color.setUniformValue("phase",f.phase);color.setUniformValue("palette",f.palette);triangle();
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,colors_.texture);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    Target output{0,target,width,height};
    auto &composite=use(Composite,output);texture(composite,"color_image",0,colors_);triangle();
    composite.release();glBindVertexArray(0);glActiveTexture(GL_TEXTURE0);
    generation_=f.generation;clearGeneration_=f.clearGeneration;burstGeneration_=f.burstGeneration;
    return true;
}

QImage AttractorPackRenderer::screenshot(GLuint target,int width,int height) {
    QImage image(width,height,QImage::Format_RGBA8888);
    glBindFramebuffer(GL_FRAMEBUFFER,target);glPixelStorei(GL_PACK_ALIGNMENT,4);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return image.flipped(Qt::Vertical);
#else
    return image.mirrored(false,true);
#endif
}

void AttractorPackRenderer::release() {
    // initializeOpenGLFunctions is needed even after an early shader load failure.
    if(!QOpenGLContext::currentContext())return;
    if(!initializeOpenGLFunctions())return;
    for(auto &t:particles_)destroy(t);
    for(auto &t:densities_)destroy(t);
    for(auto &t:peaks_)destroy(t);
    for(auto &t:reductions_)destroy(t);
    reductions_.clear();destroy(colors_);
    if(vao_)glDeleteVertexArrays(1,&vao_);
    vao_=0;for(auto &p:programs_)p.reset();
    front_=densityFront_=peakFront_=serial_=0;
    generation_=clearGeneration_=UINT64_MAX;burstGeneration_=0;error_.clear();
}
