#include "crawler_scene.h"
#include <QCoreApplication>
#include <QFile>
#include <QKeyEvent>
#include <QVector2D>
#include <algorithm>
#include <cmath>

std::unique_ptr<QOpenGLShaderProgram> CrawlerScene::program(const char *vertex,const char *fragment){
    const QString dir=QCoreApplication::applicationDirPath()+"/shaders/";
    QFile v(dir+vertex),f(dir+fragment);
    if(!v.open(QIODevice::ReadOnly)||!f.open(QIODevice::ReadOnly))return {};
    auto p=std::make_unique<QOpenGLShaderProgram>();
    if(!p->addShaderFromSourceCode(QOpenGLShader::Vertex,v.readAll())||
       !p->addShaderFromSourceCode(QOpenGLShader::Fragment,f.readAll())||!p->link())return {};
    return p;
}

void CrawlerScene::beat(){bass_=1;}
void CrawlerScene::reset(){bass_=0;clear_=true;}
bool CrawlerScene::key(int key){
    switch(key){
    case Qt::Key_H:hudVisible_=!hudVisible_;return true;
    case Qt::Key_Space:paused_=!paused_;return true;
    case Qt::Key_C:clear_=true;return true;
    case Qt::Key_R:ballCount_=94;trailAlpha_=14;clear_=true;return true;
    case Qt::Key_Up:ballCount_=std::min(200,ballCount_+1);return true;
    case Qt::Key_Down:ballCount_=std::max(2,ballCount_-1);return true;
    case Qt::Key_Left:trailAlpha_=std::max(1,trailAlpha_-1);return true;
    case Qt::Key_Right:trailAlpha_=std::min(64,trailAlpha_+1);return true;
    case Qt::Key_B:plasma_=!plasma_;return true;
    case Qt::Key_L:bloom_=!bloom_;return true;
    case Qt::Key_S:scanlines_=!scanlines_;return true;
    case Qt::Key_V:vignette_=!vignette_;return true;
    default:return false;
    }
}
bool CrawlerScene::initialize(){
    if(quadVao_)return true;
    initializeOpenGLFunctions();
    fade_=program("crawler_fullscreen_vertex.vert","crawler_fade_fragment.frag");
    particle_=program("crawler_particle_vertex.vert","crawler_particle_fragment.frag");
    composite_=program("crawler_fullscreen_vertex.vert","crawler_composite_fragment.frag");
    if(!fade_||!particle_||!composite_)return false;
    glGenVertexArrays(1,&quadVao_);glGenVertexArrays(1,&particleVao_);glGenBuffers(1,&particleVbo_);
    glBindVertexArray(particleVao_);glBindBuffer(GL_ARRAY_BUFFER,particleVbo_);
    glBufferData(GL_ARRAY_BUFFER,512*6*7*sizeof(float),nullptr,GL_STREAM_DRAW);
    const char *names[]={"in_position","in_local","in_color"};
    int offsets[]={0,2,4},sizes[]={2,2,3};
    for(int i=0;i<3;++i){
        int loc=particle_->attributeLocation(names[i]);
        if(loc<0)continue;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc,sizes[i],GL_FLOAT,GL_FALSE,7*sizeof(float),reinterpret_cast<void*>(uintptr_t(offsets[i]*sizeof(float))));
    }
    glBindVertexArray(0);
    return true;
}
bool CrawlerScene::resize(int width,int height){
    int w=std::max(1,width/2),h=std::max(1,height/2);
    if(w==renderWidth_&&h==renderHeight_)return true;
    if(textures_[0])glDeleteTextures(2,textures_.data());
    if(framebuffers_[0])glDeleteFramebuffers(2,framebuffers_.data());
    textures_.fill(0);framebuffers_.fill(0);renderWidth_=w;renderHeight_=h;front_=0;
    glGenTextures(2,textures_.data());glGenFramebuffers(2,framebuffers_.data());
    for(int i=0;i<2;++i){
        glBindTexture(GL_TEXTURE_2D,textures_[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures_[i],0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return false;
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    }
    return true;
}
void CrawlerScene::addQuad(std::vector<float> &out,float x,float y,float radius,const std::array<float,3> &color){
    if(x+radius<0||x-radius>renderWidth_||y+radius<0||y-radius>renderHeight_)return;
    float l=(x-radius)*2/renderWidth_-1,r=(x+radius)*2/renderWidth_-1;
    float b=1-(y+radius)*2/renderHeight_,t=1-(y-radius)*2/renderHeight_;
    for(auto v:{std::array<float,4>{l,b,-1,-1},{r,b,1,-1},{l,t,-1,1},{l,t,-1,1},{r,b,1,-1},{r,t,1,1}}){
        out.insert(out.end(),v.begin(),v.end());out.insert(out.end(),color.begin(),color.end());
    }
}
static std::array<float,3> hsv(float hue,float saturation,float value){
    std::array<float,3> rgb{};
    for(int i=0;i<3;++i){
        float x=std::fmod(hue*6+(i==0?0:i==1?4:2),6.f);
        rgb[i]=value*((1-saturation)+saturation*std::clamp(std::abs(x-3)-1.f,0.f,1.f));
    }
    return rgb;
}
void CrawlerScene::paint(float dt,int width,int height,GLuint target,float energy,float mid,float treble){
    if(failed_)return;
    if(!initialize()||!resize(width,height)){failed_=true;return;}
    bass_*=std::exp(-dt*5.5f);
    if(!paused_)elapsed_+=dt;
    if(clear_){
        for(GLuint fbo:framebuffers_){glBindFramebuffer(GL_FRAMEBUFFER,fbo);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);}
        clear_=false;
    }
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    if(!paused_){
        int back=1-front_;
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[back]);glViewport(0,0,renderWidth_,renderHeight_);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures_[front_]);
        fade_->bind();fade_->setUniformValue("previous_frame",0);
        fade_->setUniformValue("decay",std::pow(1.f-trailAlpha_/255.f,std::clamp(dt,0.f,.1f)*60.f));
        glBindVertexArray(quadVao_);glDrawArrays(GL_TRIANGLES,0,3);fade_->release();
        std::vector<float> vertices;vertices.reserve(512*6*7);
        float t=elapsed_,zoom=.99f+.07f*std::sin(t*.20f)+bass_*.16f;
        float xs=zoom*(1+.032f*std::sin(t*.16f)),ys=zoom*(1+.037f*std::cos(t*.19f));
        float cx=renderWidth_*(.5f+.009f*std::sin(t*.21f)),cy=renderHeight_*(.5f+.009f*std::cos(t*.18f));
        float scale=std::min(renderWidth_/640.f,renderHeight_/480.f);
        float phase=t*(1.34f+.13f*mid)+.75f*std::sin(t*.27f);
        float radius=scale*(2.3f+1.2f*std::sin(t*.40f)+1.9f*energy),step=180.f/ballCount_;
        auto point=[&](int i){float angle=step*i+phase,dist=float(M_PI)*i;
            return std::array<float,2>{cx+(std::sin(angle)*dist+step)*scale*xs,cy+(std::cos(angle)*dist+step)*scale*ys};};
        for(int i=1;i<ballCount_;++i){
            auto p=point(i);
            float hue=std::fmod(t*.047f+i/float(ballCount_)*.86f+mid*.12f,1.f);
            float size=std::max(1.f,radius*(1+.18f*std::sin(t*.75f+i*.17f)))*3.7f;
            addQuad(vertices,p[0],p[1],size,hsv(hue,.86f,.58f+.25f*bass_));
        }
        if(treble>.12f)for(int j=0;j<std::min(25,int(treble*26)+3);++j){
            int i=1+(j*31)%(ballCount_-1);auto p=point(i);
            addQuad(vertices,p[0]+std::sin(t*9+j*7)*scale*5,p[1]+std::cos(t*11+j*3)*scale*5,
                    scale*(3+3*treble),{.65f,.85f,1.f});
        }
        glBindVertexArray(particleVao_);glBindBuffer(GL_ARRAY_BUFFER,particleVbo_);
        glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data());
        glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);
        particle_->bind();glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/7));particle_->release();
        glDisable(GL_BLEND);front_=back;
    }
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,width,height);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures_[front_]);
    composite_->bind();composite_->setUniformValue("trail",0);
    composite_->setUniformValue("render_resolution",QVector2D(width,height));
    composite_->setUniformValue("trail_resolution",QVector2D(renderWidth_,renderHeight_));
    composite_->setUniformValue("elapsed",elapsed_);composite_->setUniformValue("energy",energy);
    composite_->setUniformValue("mid",mid);composite_->setUniformValue("treble",treble);
    composite_->setUniformValue("bass",bass_);composite_->setUniformValue("plasma_on",int(plasma_));
    composite_->setUniformValue("bloom_on",int(bloom_));composite_->setUniformValue("scanlines_on",int(scanlines_));
    composite_->setUniformValue("vignette_on",int(vignette_));
    glBindVertexArray(quadVao_);glDrawArrays(GL_TRIANGLES,0,3);composite_->release();
}
void CrawlerScene::release(){
    if(quadVao_){glDeleteVertexArrays(1,&quadVao_);glDeleteVertexArrays(1,&particleVao_);glDeleteBuffers(1,&particleVbo_);}
    if(textures_[0])glDeleteTextures(2,textures_.data());
    if(framebuffers_[0])glDeleteFramebuffers(2,framebuffers_.data());
    quadVao_=particleVao_=particleVbo_=0;textures_.fill(0);framebuffers_.fill(0);
    renderWidth_=renderHeight_=0;fade_.reset();particle_.reset();composite_.reset();failed_=false;
}
