#pragma once
#include <QImage>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QString>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

struct AttractorGpuFrame {
    int kind=0,count=250000,steps=0,palette=0;
    std::array<float,6> parameters{};
    float dt=0,bass=0,treble=0,angle=0,tilt=.4f,phase=0;
    uint64_t generation=0,clearGeneration=0,burstGeneration=0;
};

// The OpenGL 3.3 passes and GLSL are identical to musik/attractor_pack_renderer.py.
// No CPU particle arrays, transfers, glFinish or readbacks in draw().
class AttractorPackRenderer : protected QOpenGLFunctions_3_3_Core {
public:
    bool draw(GLuint target,int width,int height,const AttractorGpuFrame &frame);
    QImage screenshot(GLuint target,int width,int height);
    void release(); // owner must make the owning context current
    QString error() const { return error_; }
private:
    struct Target { GLuint texture=0,fbo=0;int width=0,height=0; };
    enum Pass { Update,Points,Fade,Reduce,Peak,Color,Composite,PassCount };
    bool initialize(int count);
    bool resize(int width,int height);
    bool create(Target &target,int width,int height,bool rgba,bool floating);
    void destroy(Target &target);
    void clear(const Target &target,float value=0);
    QOpenGLShaderProgram &use(Pass pass,const Target &target);
    void texture(QOpenGLShaderProgram &program,const char *name,int unit,const Target &target);
    void triangle();
    std::array<std::unique_ptr<QOpenGLShaderProgram>,PassCount> programs_;
    std::array<Target,2> particles_{},densities_{},peaks_{};
    std::vector<Target> reductions_;
    Target colors_;
    GLuint vao_=0;
    int front_=0,densityFront_=0,peakFront_=0,serial_=0,count_=0;
    uint64_t generation_=UINT64_MAX,clearGeneration_=UINT64_MAX,burstGeneration_=0;
    QString error_;
};
