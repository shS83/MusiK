#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <array>
#include <memory>
#include <vector>

class CrawlerScene : protected QOpenGLFunctions_3_3_Core {
public:
    void beat();
    void reset();
    bool key(int key);
    bool hudVisible() const {return hudVisible_;}
    int ballCount() const {return ballCount_;}
    int trailAlpha() const {return trailAlpha_;}
    void paint(float dt,int width,int height,GLuint target,float energy,float mid,float treble);
    void release();
private:
    bool initialize();
    bool resize(int width,int height);
    void addQuad(std::vector<float> &vertices,float x,float y,float radius,const std::array<float,3> &color);
    std::unique_ptr<QOpenGLShaderProgram> program(const char *vertex,const char *fragment);
    std::unique_ptr<QOpenGLShaderProgram> fade_,particle_,composite_;
    GLuint quadVao_=0,particleVao_=0,particleVbo_=0;
    std::array<GLuint,2> textures_{},framebuffers_{};
    int renderWidth_=0,renderHeight_=0,front_=0,ballCount_=94,trailAlpha_=14;
    float elapsed_=0,bass_=0;
    bool paused_=false,clear_=false,plasma_=true,bloom_=true,scanlines_=true,vignette_=true,failed_=false,hudVisible_=false;
};
