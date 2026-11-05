#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QVector>
#include <array>
#include <memory>

class FogScene : protected QOpenGLFunctions_3_3_Core {
public:
    void setWaveform(const QVector<float> &samples);
    void beat();
    void reset();
    void paint(float dt,int width,int height,GLuint target,float energy,float mid,float treble);
    void release();
private:
    bool initialize();
    bool resize(int width,int height);
    void drawQuad(QOpenGLShaderProgram &program);
    void drawMesh(QOpenGLShaderProgram &program);
    void bindTexture(int unit,GLuint texture);
    std::unique_ptr<QOpenGLShaderProgram> program(const char *vertex,const char *fragment);
    std::unique_ptr<QOpenGLShaderProgram> fog_,text_,glow_,blur_,composite_;
    GLuint quadVao_=0,meshVao_=0,meshVbo_=0,audioTexture_=0,depth_=0;
    std::array<GLuint,4> textures_{},framebuffers_{};
    std::array<float,512> waveform_{},smoothed_{};
    int meshCount_=0,renderWidth_=0,renderHeight_=0;
    float elapsed_=0,pulse_=0;
    bool failed_=false;
};
