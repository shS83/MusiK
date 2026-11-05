#include "fog_scene.h"
#include <QCoreApplication>
#include <QFile>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstring>

std::unique_ptr<QOpenGLShaderProgram> FogScene::program(const char *vertex,const char *fragment){
    const QString dir=QCoreApplication::applicationDirPath()+"/shaders/";
    QFile v(dir+vertex),f(dir+fragment);
    if(!v.open(QIODevice::ReadOnly)||!f.open(QIODevice::ReadOnly))return {};
    auto p=std::make_unique<QOpenGLShaderProgram>();
    if(!p->addShaderFromSourceCode(QOpenGLShader::Vertex,v.readAll())||
       !p->addShaderFromSourceCode(QOpenGLShader::Fragment,f.readAll())||!p->link())return {};
    return p;
}

void FogScene::setWaveform(const QVector<float> &samples){
    if(samples.isEmpty())return;
    for(int i=0;i<512;++i){
        int index=i*(samples.size()-1)/511;
        waveform_[i]=std::clamp(std::isfinite(samples[index])?samples[index]:0.f,-1.f,1.f);
    }
}
void FogScene::beat(){pulse_=1.f;}
void FogScene::reset(){pulse_=0;waveform_.fill(0);smoothed_.fill(0);}

bool FogScene::initialize(){
    if(quadVao_)return true;
    initializeOpenGLFunctions();
    fog_=program("fog_fullscreen_vertex.vert","fog_fog_fragment.frag");
    text_=program("fog_text_vertex.vert","fog_text_fragment.frag");
    glow_=program("fog_text_vertex.vert","fog_glow_fragment.frag");
    blur_=program("fog_fullscreen_vertex.vert","fog_blur_fragment.frag");
    composite_=program("fog_fullscreen_vertex.vert","fog_composite_fragment.frag");
    QFile mesh(QCoreApplication::applicationDirPath()+"/shaders/fog_mesh.bin");
    if(!fog_||!text_||!glow_||!blur_||!composite_||!mesh.open(QIODevice::ReadOnly))return false;
    QByteArray bytes=mesh.readAll();
    if(bytes.size()%int(10*sizeof(float))!=0||bytes.isEmpty())return false;
    meshCount_=bytes.size()/int(10*sizeof(float));
    glGenVertexArrays(1,&quadVao_);
    glGenVertexArrays(1,&meshVao_);
    glGenBuffers(1,&meshVbo_);
    glBindVertexArray(meshVao_);
    glBindBuffer(GL_ARRAY_BUFFER,meshVbo_);
    glBufferData(GL_ARRAY_BUFFER,bytes.size(),bytes.constData(),GL_STATIC_DRAW);
    for(int i=0;i<4;++i){
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,i<2?3:2,GL_FLOAT,GL_FALSE,10*sizeof(float),
                              reinterpret_cast<void*>(uintptr_t((i==0?0:i==1?3:i==2?6:8)*sizeof(float))));
    }
    glBindVertexArray(0);
    glGenTextures(1,&audioTexture_);
    glBindTexture(GL_TEXTURE_2D,audioTexture_);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,512,1,0,GL_RED,GL_FLOAT,smoothed_.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    return true;
}

bool FogScene::resize(int width,int height){
    float scale=std::min({1.f,1920.f/width,1080.f/height});
    int w=std::max(1,int(std::lround(width*scale))),h=std::max(1,int(std::lround(height*scale)));
    if(w==renderWidth_&&h==renderHeight_)return true;
    if(textures_[0])glDeleteTextures(4,textures_.data());
    if(framebuffers_[0])glDeleteFramebuffers(4,framebuffers_.data());
    if(depth_)glDeleteRenderbuffers(1,&depth_);
    textures_.fill(0);framebuffers_.fill(0);depth_=0;
    renderWidth_=w;renderHeight_=h;
    glGenTextures(4,textures_.data());glGenFramebuffers(4,framebuffers_.data());
    for(int i=0;i<4;++i){
        glBindTexture(GL_TEXTURE_2D,textures_[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_HALF_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures_[i],0);
    }
    glGenRenderbuffers(1,&depth_);
    glBindRenderbuffer(GL_RENDERBUFFER,depth_);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_);
    bool complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return complete;
}

void FogScene::drawQuad(QOpenGLShaderProgram &p){p.bind();glBindVertexArray(quadVao_);glDrawArrays(GL_TRIANGLES,0,3);p.release();}
void FogScene::drawMesh(QOpenGLShaderProgram &p){p.bind();glBindVertexArray(meshVao_);glDrawArrays(GL_TRIANGLES,0,meshCount_);p.release();}
void FogScene::bindTexture(int unit,GLuint texture){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,texture);}

void FogScene::paint(float dt,int width,int height,GLuint target,float energy,float mid,float treble){
    if(failed_)return;
    if(!initialize()||!resize(width,height)){failed_=true;return;}
    elapsed_+=dt;pulse_*=std::exp(-dt*6.f);
    float alpha=1-std::exp(-dt*18.f);
    for(int i=0;i<512;++i)smoothed_[i]+=(waveform_[i]-smoothed_[i])*alpha;
    bindTexture(2,audioTexture_);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,512,1,GL_RED,GL_FLOAT,smoothed_.data());
    QMatrix4x4 model,view,proj;
    model.translate(0,std::sin(elapsed_*.8f)*.035f,0);
    model.rotate(elapsed_*.55f*180.f/float(M_PI),0,1,0);
    model.rotate(elapsed_*.28f*180.f/float(M_PI),1,0,0);
    view.lookAt({0,0,3.3f},{0,0,0},{0,1,0});
    proj.perspective(48.f,float(renderWidth_)/renderHeight_,.1f,100.f);
    for(auto *p:{text_.get(),glow_.get()}){
        p->bind();
        p->setUniformValue("u_model",model);p->setUniformValue("u_view",view);p->setUniformValue("u_proj",proj);
        p->setUniformValue("u_audio",2);p->setUniformValue("u_time",elapsed_);
        p->setUniformValue("u_wave_strength",.075f+.060f*mid+.055f*treble+.12f*pulse_);
        p->setUniformValue("u_beat",pulse_);
        p->setUniformValue("u_camera_pos",QVector3D(0,0,3.3f));
        p->release();
    }
    text_->bind();text_->setUniformValue("u_light_pos",QVector3D(std::cos(elapsed_*.7f)*1.6f,1.f,2.2f));text_->release();
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glViewport(0,0,renderWidth_,renderHeight_);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[0]);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    fog_->bind();fog_->setUniformValue("u_time",elapsed_);fog_->setUniformValue("u_density",1.05f+.18f*energy);
    fog_->setUniformValue("u_resolution",QVector2D(renderWidth_,renderHeight_));fog_->release();
    drawQuad(*fog_);
    glEnable(GL_DEPTH_TEST);drawMesh(*text_);glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[1]);glClear(GL_COLOR_BUFFER_BIT);drawMesh(*glow_);
    blur_->bind();blur_->setUniformValue("u_texture",0);blur_->release();
    GLuint source=textures_[1];
    for(int pass=0;pass<5;++pass){
        bindTexture(0,source);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[2]);glClear(GL_COLOR_BUFFER_BIT);
        blur_->bind();blur_->setUniformValue("u_direction",QVector2D(1.f/renderWidth_,0));blur_->release();drawQuad(*blur_);
        bindTexture(0,textures_[2]);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers_[3]);glClear(GL_COLOR_BUFFER_BIT);
        blur_->bind();blur_->setUniformValue("u_direction",QVector2D(0,1.f/renderHeight_));blur_->release();drawQuad(*blur_);
        source=textures_[3];
    }
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,width,height);
    bindTexture(0,textures_[0]);bindTexture(1,textures_[3]);
    composite_->bind();composite_->setUniformValue("u_scene",0);composite_->setUniformValue("u_bloom",1);
    composite_->setUniformValue("u_bloom_strength",1.55f+.2f*pulse_);composite_->release();drawQuad(*composite_);
}

void FogScene::release(){
    if(quadVao_){glDeleteVertexArrays(1,&quadVao_);glDeleteVertexArrays(1,&meshVao_);glDeleteBuffers(1,&meshVbo_);glDeleteTextures(1,&audioTexture_);}
    if(textures_[0])glDeleteTextures(4,textures_.data());
    if(framebuffers_[0])glDeleteFramebuffers(4,framebuffers_.data());
    if(depth_)glDeleteRenderbuffers(1,&depth_);
    quadVao_=meshVao_=meshVbo_=audioTexture_=depth_=0;textures_.fill(0);framebuffers_.fill(0);
    renderWidth_=renderHeight_=0;fog_.reset();text_.reset();glow_.reset();blur_.reset();composite_.reset();failed_=false;
}
