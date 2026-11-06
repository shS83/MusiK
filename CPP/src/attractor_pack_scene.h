#pragma once
#include "attractor_pack_renderer.h"
#include <QImage>
#include <QPointF>
#include <QString>
#include <array>
#include <random>
#include <vector>

class QPainter;
class QRect;

// Small control state plus GPU renderer. CPU render() is a regression reference only.
class AttractorPackScene {
public:
    explicit AttractorPackScene(int count=250000);
    void reset();
    void beat(float strength=1.f);
    void burst();
    void select(int index);
    void advanceClock(double dt);
    bool key(int key);
    bool hudVisible() const { return hudVisible_; }
    bool autoSwitch() const { return autoSwitch_; }
    bool audioEnabled() const { return audioEnabled_; }
    int current() const { return current_; }
    double switchElapsed() const { return switchElapsed_; }
    QString name() const;
    void setPointer(QPointF position) { pointer_=position; }
    QImage render(double dt, int width, int height, float energy, float mid, float treble);
    bool paint(double dt,int width,int height,GLuint target,float energy,float mid,float treble);
    void paintHud(QPainter &painter);
    void release();
    QString gpuError() const { return gpuError_; }
private:
    AttractorGpuFrame prepareFrame(double elapsed,float energy,float mid,float treble);
    std::unique_ptr<AttractorPackRenderer> renderer_;
    QString gpuError_;
    int count_;
    uint64_t generation_=0,clearGeneration_=0,burstGeneration_=0;
    float random(float low, float high);
    void seed(size_t i, float spread=1);
    void parameters();
    void step(float drive);
    std::mt19937 rng_{7};
    std::vector<float> x_,y_,z_,hist_;
    std::array<std::array<float,4>,2> bases_{{{-1.4f,1.6f,1.f,.7f},{1.4f,-2.3f,2.4f,-2.1f}}};
    std::array<float,6> parameters_{};
    std::array<float,3> levels_{};
    QImage image_;
    QPointF pointer_;
    float time_=0,phase_=0,bass_=0,spin_=0,spinVelocity_=0,pending_=1.f/60,peak_=1e-3f;
    double switchElapsed_=0;
    int current_=0,palette_=0;
    bool paused_=false,hudVisible_=false,autoSwitch_=true,audioEnabled_=true,wantShot_=false;
};
