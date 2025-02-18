//=============================================================================================
// Mintaprogram: Zˆld h·romszˆg. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!!
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Gyongyosi Akos
// Neptun : B04G6C
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//============================================================================================
#include "framework.h"
#include <vector>
#include <cmath>
#include <cstdio>

const char* const vertexSource = R"(
    #version 330
    precision highp float;

    layout(location = 0) in vec3 vp;    // 3D coords in CPU => GPU
    uniform mat4 MVP;

    void main() {
        // We consider vp.z=0 for 2D geometry. 
        // Multiply by MVP for camera transformations
        gl_Position = vec4(vp, 1) * MVP;
    }
)";

const char* const fragmentSource = R"(
    #version 330
    precision highp float;

    uniform vec3 color;
    out vec4 outColor;

    void main() {
        outColor = vec4(color, 1);
    }
)";

GPUProgram gpuProgram;

class Camera {
    vec2 o;
    float width, height;

public:
    Camera() : o(0.0f, 0.0f), width(2.0f), height(2.0f) {}

    mat4 view() {
        return TranslateMatrix(-o);
    }

    mat4 projection() {
        return ScaleMatrix(vec2(2.0f / width, 2.0f / height));
    }

    mat4 viewinv() {
        return TranslateMatrix(o);
    }

    mat4 projinv() {
        return ScaleMatrix(vec2(width / 2.0f, height / 2.0f));
    }

    void setWindow(float w, float h) {
        width = w;
        height = h;
    }

    void zoom(float s) {
        width  *= s;
        height *= s;
    }

    void pan(float val) {
        o.x += val * (1.0f/15.0f);
    }
};

Camera camera;

class Object {
    bool created = false;
    unsigned int vao=0, vbo=0;
    std::vector<vec3> cpuData;

public:
    Object() {}

    void createGL() {
        if (created) return;
        created = true;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    std::vector<vec3>& data() { return cpuData; }
    
    const std::vector<vec3>& data() const { return cpuData; }

    void updateGPU() {
        if (!created) return;
        if (cpuData.empty()) return;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, cpuData.size()*sizeof(vec3), &cpuData[0], GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw(int primitive, vec3 color) {
        if (!created) return;
        if (cpuData.empty()) return;
        glBindVertexArray(vao);
        mat4 MVP = camera.projection() * camera.view();
        gpuProgram.setUniform(MVP, "MVP");
        gpuProgram.setUniform(color, "color");
        glDrawArrays(primitive, 0, (GLsizei)cpuData.size());
        glBindVertexArray(0);
    }
};

class PointCollection {
    Object obj;
public:
    PointCollection() {}

    void createGL() { obj.createGL(); }

    void addPoint(vec3 p) {
        obj.data().push_back(p);
        printf("Point %.1f, %.1f added\n", p.x, p.y);
    }

    vec3* pick(vec3 mouse, float threshold) {
        auto &pts = obj.data();
        for (auto &pt : pts) {
            float dx = pt.x - mouse.x;
            float dy = pt.y - mouse.y;
            if ((dx*dx + dy*dy) < threshold*threshold) {
                return &pt;
            }
        }
        return nullptr;
    }

    void draw() {
        obj.updateGPU();
        glPointSize(10.0f);
        obj.draw(GL_POINTS, vec3(1,0,0));
    }
};

class Line {
    Object obj;
    float a, b, c;

public:
    Line(const vec3& p1, const vec3& p2) {
        obj.createGL();
        a = p2.y - p1.y;
        b = p1.x - p2.x;
        c = (p2.x * p1.y) - (p2.y * p1.x);
        obj.data().push_back(p1);
        obj.data().push_back(p2);

        printf("Line added\n");
        printf("Implicit: %.1f x + %.1f y + %.1f = 0\n", a, b, c);
        printf("Parametric: r(t) = (%.1f, %.1f) + (%.1f, %.1f)t\n",
               p1.x, p1.y, p2.x - p1.x, p2.y - p1.y);
    }

    vec3 getP1() const { return obj.data()[0]; }
    vec3 getP2() const { return obj.data()[1]; }

    bool isClose(const vec3& point, float threshold = 0.01f) const {
        return distanceToPoint(point) < threshold;
    }

    float distanceToPoint(const vec3& point) const {
        float numerator = fabs(a * point.x + b * point.y + c);
        float denominator = sqrt(a * a + b * b);
        return numerator / denominator;
    }

    void draw() {
        obj.updateGPU();
        glLineWidth(3.0f);

        vec3 dir = getP2() - getP1();
        float length = sqrt(dir.x * dir.x + dir.y * dir.y);
        vec3 normDir = vec3(dir.x / length, dir.y / length, 0);

        vec3 extendedP1 = getP1() - normDir * 2.0f;
        vec3 extendedP2 = getP2() + normDir * 2.0f;

        obj.data()[0] = extendedP1;
        obj.data()[1] = extendedP2;

        obj.updateGPU();
        obj.draw(GL_LINES, vec3(0, 1, 1));
    }

    void translate(const vec3& point) {
        vec3 shift = point - getP1();
        obj.data()[0] = obj.data()[0] + shift;
        obj.data()[1] = obj.data()[1] + shift;

        vec3 p1 = getP1();
        vec3 p2 = getP2();
        a = p2.y - p1.y;
        b = p1.x - p2.x;
        c = (p2.x * p1.y) - (p2.y * p1.x);

        printf("Line moved to new position\n");
    }

    vec3 intersect(const Line& other) const {
        float x1 = getP1().x, y1 = getP1().y;
        float x2 = getP2().x, y2 = getP2().y;
        float x3 = other.getP1().x, y3 = other.getP1().y;
        float x4 = other.getP2().x, y4 = other.getP2().y;

        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        if (fabs(denom) < 1e-8) return vec3(NAN, NAN, 0);

        float xi = ((x3 - x4) * (x1 * y2 - y1 * x2) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denom;
        float yi = ((y3 - y4) * (x1 * y2 - y1 * x2) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denom;

        printf("Point %.1f, %.1f added\n", xi, yi);
        return vec3(xi, yi, 0);
    }
};

class LineCollection {
    std::vector<Line*> lines;
public:
    LineCollection() {}

    ~LineCollection() {
        for (auto* ln : lines) delete ln;
    }

    void addLine(vec3 p1, vec3 p2) {
        auto* ln = new Line(p1,p2);
        lines.push_back(ln);
    }

    Line* pick(vec3 mouse, float threshold) {
        for (auto* ln : lines) {
            if(ln->isClose(mouse, threshold)) return ln;
        }
        return nullptr;
    }

    void draw() {
        for (auto* ln : lines) {
            ln->draw();
        }
    }
};

PointCollection* pc = nullptr;
LineCollection*  lc = nullptr;

bool pMode=false, lMode=false, mMode=false, iMode=false;

static vec3* firstPoint   = nullptr;
static Line* movingLine   = nullptr;
static Line* firstLine    = nullptr;


void onInitialization() {
    glViewport(0, 0, windowWidth, windowHeight);
    gpuProgram.create(vertexSource, fragmentSource, "outColor");
    
    pc = new PointCollection();
    lc = new LineCollection();
    pc->createGL();
    pMode=true;
}

void onDisplay() {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    lc->draw();
    pc->draw();

    glutSwapBuffers();
}

void onKeyboard(unsigned char key, int pX, int pY) {
    switch(key) {
    
    case 'Z': camera.zoom(1.1f); break;
    case 'z': camera.zoom(1.0f/1.1f); break;
    case 'P': camera.pan(1.0f); break;
    case 'p':
        //camera.pan(-1.0f);
        pMode=true; lMode=false; mMode=false; iMode=false;
        firstPoint=nullptr; firstLine=nullptr; movingLine=nullptr;
        break;
    case 'l':
        pMode=false; lMode=true; mMode=false; iMode=false;
        firstPoint=nullptr; firstLine=nullptr; movingLine=nullptr;
        break;
    case 'm':
        pMode=false; lMode=false; mMode=true; iMode=false;
        movingLine=nullptr;
        break;
    case 'i':
        pMode=false; lMode=false; mMode=false; iMode=true;
        firstLine=nullptr;
        break;
    default: break;
    }
    glutPostRedisplay();
}

void onKeyboardUp(unsigned char key, int pX, int pY) {
    // not used
}

void onMouse(int button, int state, int pX, int pY) {
    if(button==GLUT_LEFT_BUTTON && state==GLUT_DOWN){
        // pixel coords to camera coords, to world
        float ndcX = 2.0f * (float)pX / (float)windowWidth - 1.0f;
        float ndcY = 1.0f - 2.0f * (float)pY / (float)windowHeight;
        vec4 wpos = vec4(ndcX, ndcY, 0,1)*camera.projinv()*camera.viewinv();
        vec3 click = vec3(wpos.x, wpos.y, 0);

        if(pMode){
            pc->addPoint(click);
        }
        else if(lMode){
            vec3* picked = pc->pick(click, 0.03f);
            if(picked){
                if(!firstPoint){
                    firstPoint = picked;
                } else {
                    if(picked!=firstPoint){
                        lc->addLine(*firstPoint, *picked);
                    }
                    firstPoint=nullptr;
                }
            }
        }
        else if(mMode){
            movingLine = lc->pick(click, 0.03f);
        }
        else if(iMode){
            Line* cand = lc->pick(click, 0.03f);
            if(cand){
                if(!firstLine){
                    firstLine=cand;
                } else {
                    if(cand!=firstLine){
                        vec3 ip = firstLine->intersect(*cand);
                        if(!std::isnan(ip.x) && !std::isnan(ip.y)){
                            pc->addPoint(ip);
                        }
                    }
                    firstLine=nullptr;
                }
            }
        }
        glutPostRedisplay();
    }

    if(button==GLUT_LEFT_BUTTON && state==GLUT_UP){
        movingLine=nullptr;
    }
}

void onMouseMotion(int pX, int pY) {
    if(mMode && movingLine){
        float ndcX=2.0f*pX/windowWidth -1.0f;
        float ndcY=1.0f-2.0f*pY/windowHeight;
        vec4 wpos = vec4(ndcX, ndcY,0,1)*camera.projinv()*camera.viewinv();
        vec3 click = vec3(wpos.x, wpos.y, 0);

        movingLine->translate(click);
        glutPostRedisplay();
    }
}

void onIdle() {
    // no animation
}
