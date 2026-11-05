#pragma once

#include <QColor>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QVector2D>
#include <array>
#include <memory>
#include <random>
#include <vector>

// Native port of musik/circler_reactive.py. Coordinates are physical pixels.
class CirclerScene : protected QOpenGLFunctions_3_3_Core {
public:
    CirclerScene();
    void reset();
    void beat();
    void paint(float dt, int width, int height);
    void release(); // Call with the visualizer's OpenGL context current.
    QString error() const { return error_; }

private:
    struct Tetrahedron {
        QColor color;
        float size=1, rotation=0, radius=0, angle=0, kick=0;
        QVector2D position(int width, int height) const;
        void update(float dt);
        bool visible(int width, int height) const;
    };
    using Palette = std::array<QColor,2>;
    Tetrahedron spawn();
    QColor backgroundColor() const;
    bool initialize();
    static std::unique_ptr<QOpenGLShaderProgram> shader(const char *vertex, const char *fragment);

    std::vector<Tetrahedron> tetrahedra_;
    std::mt19937 random_;
    Palette palette_;
    int totalSpawns_=0, totalBeats_=0, paletteIndex_=-1;
    float elapsed_=0, spawnTimer_=0, flash_=0, lastBeat_=-1000;
    bool ready_=false, failed_=false;
    QString error_;
    std::unique_ptr<QOpenGLShaderProgram> tetraProgram_, backgroundProgram_, flashProgram_;
    GLuint tetraVao_=0, tetraVbo_=0, backgroundVao_=0, flashVao_=0, quadVbo_=0;
};
