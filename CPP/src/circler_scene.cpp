#include "circler_scene.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
constexpr float pi=3.14159265358979323846f;
constexpr float tau=2*pi;
constexpr float spawnInterval=.3f;
constexpr std::array<std::array<int,3>,3> backgroundColors{{{{20,20,40}},{{40,40,20}},{{40,20,20}}}};
const std::array<std::array<std::array<int,3>,2>,6> neonPalettes{{
    {{{0,240,255},{160,255,20}}},
    {{{180,65,255},{255,230,45}}},
    {{{255,60,100},{10,255,205}}},
    {{{255,175,30},{85,120,255}}},
    {{{45,255,110},{255,35,220}}},
    {{{255,235,75},{70,155,255}}}
}};
QColor mix(const QColor &a, const QColor &b, float t) {
    return QColor(std::lround(a.red()+(b.red()-a.red())*t),
                  std::lround(a.green()+(b.green()-a.green())*t),
                  std::lround(a.blue()+(b.blue()-a.blue())*t));
}
constexpr char tetraVertex[] = R"GLSL(#version 330
in vec3 in_position;
in vec3 in_normal;
uniform vec2 viewport_size;
uniform vec2 center;
uniform float size;
uniform vec3 rotation;
out vec3 normal;
void main() {
    vec3 c = cos(rotation);
    vec3 s = sin(rotation);
    mat3 rx = mat3(1, 0, 0, 0, c.x, s.x, 0, -s.x, c.x);
    mat3 ry = mat3(c.y, 0, -s.y, 0, 1, 0, s.y, 0, c.y);
    mat3 rz = mat3(c.z, s.z, 0, -s.z, c.z, 0, 0, 0, 1);
    mat3 model_rotation = rz * ry * rx;
    vec3 position = model_rotation * in_position * size;
    normal = model_rotation * in_normal;
    vec2 pixel = center + vec2(position.x, -position.y);
    gl_Position = vec4(2.0 * pixel.x / viewport_size.x - 1.0,
                       1.0 - 2.0 * pixel.y / viewport_size.y,
                       -position.z / 1000.0, 1.0);
}
)GLSL";
constexpr char tetraFragment[] = R"GLSL(#version 330
in vec3 normal;
uniform vec3 color;
out vec4 frag_color;
void main() {
    vec3 n = normalize(normal);
    vec3 light = normalize(vec3(-0.5, 0.8, 1.0));
    float diffuse = max(dot(n, light), 0.0);
    vec3 halfway = normalize(light + vec3(0.0, 0.0, 1.0));
    float specular = pow(max(dot(n, halfway), 0.0), 32.0);
    frag_color = vec4(color * (0.25 + 0.75 * diffuse) + 0.2 * specular, 1.0);
}
)GLSL";
constexpr char quadVertex[] = R"GLSL(#version 330
in vec2 in_position;
void main() { gl_Position = vec4(in_position, 0.0, 1.0); }
)GLSL";
constexpr char backgroundFragment[] = R"GLSL(#version 330
uniform vec2 viewport_size;
uniform vec3 edge_color;
out vec4 frag_color;
void main() {
    vec2 half_size = max(viewport_size * 0.5, vec2(1.0));
    vec2 delta = gl_FragCoord.xy - half_size;
    float distance_to_center = length(delta);
    vec2 direction = delta / max(distance_to_center, 0.00001);
    float distance_to_edge = min(half_size.x / max(abs(direction.x), 0.00001),
                                 half_size.y / max(abs(direction.y), 0.00001));
    float radius = clamp(distance_to_center / distance_to_edge, 0.0, 1.0);
    float brightness = pow(smoothstep(0.07, 1.0, radius), 1.45);
    frag_color = vec4(edge_color * brightness, 1.0);
}
)GLSL";
constexpr char flashFragment[] = R"GLSL(#version 330
uniform float flash_alpha;
out vec4 frag_color;
void main() { frag_color = vec4(1.0, 1.0, 1.0, flash_alpha); }
)GLSL";
}

CirclerScene::CirclerScene() : random_(std::random_device{}()), palette_{QColor(255,110,0),QColor(255,20,185)} { reset(); }

QVector2D CirclerScene::Tetrahedron::position(int w, int h) const {
    float r=radius+kick;
    return {w*.5f+std::cos(angle)*r,h*.5f+std::sin(angle)*r};
}
void CirclerScene::Tetrahedron::update(float dt) {
    size=std::min(50.f,size+6*dt);
    rotation=std::fmod(rotation+pi/3*dt,20*tau);
    angle+=2*dt;
    radius+=20*dt;
    kick*=std::exp(-7.5f*dt);
}
bool CirclerScene::Tetrahedron::visible(int w, int h) const {
    auto p=position(w,h);
    return p.x()>-size && p.x()<w+size && p.y()>-size && p.y()<h+size;
}
CirclerScene::Tetrahedron CirclerScene::spawn() {
    float t=(std::sin(totalSpawns_*1.517f)+1)*.5f;
    ++totalSpawns_;
    Tetrahedron tetra;
    tetra.color=mix(palette_[0],palette_[1],t);
    return tetra;
}
void CirclerScene::reset() {
    elapsed_=spawnTimer_=flash_=0;
    lastBeat_=-1000;
    totalSpawns_=totalBeats_=0;
    paletteIndex_=-1;
    palette_={QColor(255,110,0),QColor(255,20,185)};
    tetrahedra_.clear();
    tetrahedra_.push_back(spawn());
}
void CirclerScene::beat() {
    if(elapsed_-lastBeat_<.2f)return;
    lastBeat_=elapsed_;
    ++totalBeats_;
    flash_=1;
    for(auto &t:tetrahedra_)
        t.kick=std::min(144.f,t.kick+1.8f*(16.f+t.size*.38f));
    if(totalBeats_%16==0){
        std::uniform_int_distribution<int> pick(0,int(neonPalettes.size())-1);
        int candidate;
        do { candidate=pick(random_); } while(candidate==paletteIndex_);
        paletteIndex_=candidate;
        for(int i=0;i<2;++i){
            const auto &rgb=neonPalettes[candidate][i];
            palette_[i]=QColor(rgb[0],rgb[1],rgb[2]);
        }
    }
}
QColor CirclerScene::backgroundColor() const {
    int segment=int(elapsed_/12.f);
    float t=std::fmod(elapsed_,12.f)/12.f;
    t=t*t*(3-2*t);
    const auto &a=backgroundColors[segment%3],&b=backgroundColors[(segment+1)%3];
    return mix(QColor(a[0],a[1],a[2]),QColor(b[0],b[1],b[2]),t);
}
std::unique_ptr<QOpenGLShaderProgram> CirclerScene::shader(const char *vertex,const char *fragment) {
    auto p=std::make_unique<QOpenGLShaderProgram>();
    if(!p->addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) ||
       !p->addShaderFromSourceCode(QOpenGLShader::Fragment,fragment) || !p->link()) return {};
    return p;
}
bool CirclerScene::initialize() {
    initializeOpenGLFunctions();
    tetraProgram_=shader(tetraVertex,tetraFragment);
    backgroundProgram_=shader(quadVertex,backgroundFragment);
    flashProgram_=shader(quadVertex,flashFragment);
    if(!tetraProgram_ || !backgroundProgram_ || !flashProgram_){
        error_="Circler OpenGL shader compilation failed";
        release();
        failed_=true;
        return false;
    }
    const std::array<QVector3D,4> vertices{{QVector3D(1,1,1).normalized(),QVector3D(1,-1,-1).normalized(),
                                             QVector3D(-1,1,-1).normalized(),QVector3D(-1,-1,1).normalized()}};
    constexpr int faces[4][3]={{0,1,2},{0,3,1},{0,2,3},{1,3,2}};
    std::array<float,12*6> mesh{};
    int at=0;
    for(const auto &face:faces){
        QVector3D a=vertices[face[0]],b=vertices[face[1]],c=vertices[face[2]];
        QVector3D normal=QVector3D::crossProduct(b-a,c-a).normalized();
        if(QVector3D::dotProduct(normal,a)<0){std::swap(b,c);normal=-normal;}
        for(const auto &v:{a,b,c}){
            mesh[at++]=v.x();mesh[at++]=v.y();mesh[at++]=v.z();
            mesh[at++]=normal.x();mesh[at++]=normal.y();mesh[at++]=normal.z();
        }
    }
    glGenVertexArrays(1,&tetraVao_);glBindVertexArray(tetraVao_);
    glGenBuffers(1,&tetraVbo_);glBindBuffer(GL_ARRAY_BUFFER,tetraVbo_);
    glBufferData(GL_ARRAY_BUFFER,sizeof(mesh),mesh.data(),GL_STATIC_DRAW);
    auto position=GLuint(tetraProgram_->attributeLocation("in_position"));
    auto normal=GLuint(tetraProgram_->attributeLocation("in_normal"));
    glEnableVertexAttribArray(position);glVertexAttribPointer(position,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);
    glEnableVertexAttribArray(normal);glVertexAttribPointer(normal,3,GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
    constexpr std::array<float,6> quad{{-1,-1,3,-1,-1,3}};
    glGenBuffers(1,&quadVbo_);glBindBuffer(GL_ARRAY_BUFFER,quadVbo_);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad.data(),GL_STATIC_DRAW);
    auto makeQuadVao=[this](QOpenGLShaderProgram &program,GLuint &vao){
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,quadVbo_);
        auto loc=GLuint(program.attributeLocation("in_position"));
        glEnableVertexAttribArray(loc);glVertexAttribPointer(loc,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
    };
    makeQuadVao(*backgroundProgram_,backgroundVao_);
    makeQuadVao(*flashProgram_,flashVao_);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    ready_=true;
    error_.clear();
    return true;
}
void CirclerScene::paint(float dt,int w,int h) {
    dt=std::clamp(dt,0.f,.1f);
    elapsed_+=dt;
    flash_*=std::exp(-9*dt);
    for(auto &t:tetrahedra_)t.update(dt);
    std::erase_if(tetrahedra_,[=](const Tetrahedron &t){return !t.visible(w,h);});
    spawnTimer_+=dt;
    while(spawnTimer_>=spawnInterval){spawnTimer_-=spawnInterval;tetrahedra_.push_back(spawn());}
    if(w<=0 || h<=0)return;
    if(failed_ || (!ready_ && !initialize()))return;
    glViewport(0,0,w,h);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    backgroundProgram_->bind();
    QColor edge=backgroundColor();
    backgroundProgram_->setUniformValue("viewport_size",QVector2D(w,h));
    backgroundProgram_->setUniformValue("edge_color",QVector3D(edge.redF(),edge.greenF(),edge.blueF()));
    glBindVertexArray(backgroundVao_);glDrawArrays(GL_TRIANGLES,0,3);backgroundProgram_->release();
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDepthFunc(GL_LESS);glCullFace(GL_BACK);
    tetraProgram_->bind();tetraProgram_->setUniformValue("viewport_size",QVector2D(w,h));
    glBindVertexArray(tetraVao_);
    for(const auto &t:tetrahedra_){
        float angle=t.rotation;
        tetraProgram_->setUniformValue("center",t.position(w,h));
        tetraProgram_->setUniformValue("size",t.size);
        tetraProgram_->setUniformValue("rotation",QVector3D(.55f*angle+.4f,.7f*angle+.3f,angle));
        tetraProgram_->setUniformValue("color",QVector3D(t.color.redF(),t.color.greenF(),t.color.blueF()));
        glDrawArrays(GL_TRIANGLES,0,12);
    }
    tetraProgram_->release();
    if(flash_>.002f){
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        flashProgram_->bind();flashProgram_->setUniformValue("flash_alpha",std::min(.14f,flash_*.14f));
        glBindVertexArray(flashVao_);glDrawArrays(GL_TRIANGLES,0,3);flashProgram_->release();
        glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
}
void CirclerScene::release() {
    if(tetraVao_)glDeleteVertexArrays(1,&tetraVao_);
    if(backgroundVao_)glDeleteVertexArrays(1,&backgroundVao_);
    if(flashVao_)glDeleteVertexArrays(1,&flashVao_);
    if(tetraVbo_)glDeleteBuffers(1,&tetraVbo_);
    if(quadVbo_)glDeleteBuffers(1,&quadVbo_);
    tetraVao_=backgroundVao_=flashVao_=tetraVbo_=quadVbo_=0;
    tetraProgram_.reset();backgroundProgram_.reset();flashProgram_.reset();
    ready_=false;
    failed_=false;
}
