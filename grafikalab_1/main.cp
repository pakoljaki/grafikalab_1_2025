
#include "framework.h"
#include <cmath>

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
    #version 330                // Shader 3.3
    precision highp float;        // normal floats, makes no difference on desktop computers

    layout(location = 0) in vec2 vp;    // Varying input: vp = vertex position is expected in attrib array 0

    void main() {
        gl_Position = vec4(vp.x, vp.y, 0, 1);        // transform vp from modeling space to normalized device space
    }
)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
    #version 330            // Shader 3.3
    precision highp float;    // normal floats, makes no difference on desktop computers
    
    uniform vec3 color;        // uniform variable, the color of the primitive
    out vec4 outColor;        // computed color of the current pixel

    void main() {
        outColor = vec4(color, 1);    // computed color is the color of the primitive
    }
)";

class Object;
class PointCollection;
class Line;
class LineCollection;


// POINT drawing mode
bool pointDrawingMode = false;

// LINE drawing mode
bool lineDrawingMode = false;
bool lineAdded = false; // Flag to indicate whether a line has been added
vec3* linePoint1 = NULL; // Two points for line drawing
vec3* linePoint2 = NULL;

//LINE translation mode
bool lineTranslationMode = false; // Flag to indicate whether line translation mode is active
Line* selectedLine = nullptr; // Pointer to the selected line for translation

//INTERSECTION
Line* selectedLine1;
Line* selectedLine2;
bool intersectionMode = false;
bool intersectionAdded = false;


GPUProgram gpuProgram; // vertex and fragment shaders
//unsigned int vao;       // virtual world on the GPU

class Object {
    unsigned int vao, vbo; // GPU
    std::vector<vec3> vtx; // CPU
public:
    Object() {}
    void create() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,NULL);
    }
    std::vector<vec3>& Vtx() { return vtx; }
    const std::vector<vec3>& Vtx() const { return vtx; } // Add const version for read-only access

    void updateGPU() { // CPU -> GPU
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec3), &vtx[0], GL_DYNAMIC_DRAW);
        }
    void Draw(int type, vec3 color) {
        if (vtx.size() > 0) {
            glBindVertexArray(vao);
            gpuProgram.setUniform(color, "color");
            glPointSize(10);
            glDrawArrays(type, 0, vtx.size());
        }
    }
};

Object *objP = new Object();

class PointCollection {
private:
    Object points; // Reference to the Object class for sharing OpenGL resources
public:
    PointCollection(Object* obj) : points(*obj) {}
    void create(Object *obj) {
        points = *obj;
    }
    std::vector<vec3>& Vtx() {
        return points.Vtx();
    }

    // Method to add a new point
    void addPoint(vec3 p) {
        points.Vtx().push_back(p);
        objP->Vtx().push_back(p);
    }
    vec3 *pickPoint(vec3 pp, float threshold) {
        for (auto& p : points.Vtx()) if (length(pp - p) < threshold) return &p;
        return nullptr;
    }

    // Method to draw the points with the specified color
    void draw(const vec3& color) {
        // Draw the object using Object's draw method
        points.updateGPU();
        glLineWidth(3);
        points.Draw(GL_POINTS, color);
    }
};



class Line {
private:
    Object points; // Object to store the points for drawing
    float a, b, c; // Parameters of the line equation: ax + by + c = 0

public:
    // Constructor to initialize the line (and the object) with two points and print equations
    Line(const vec3& point1, const vec3& point2) {
        points.create();
        a = point2.y - point1.y;
        b = point1.x - point2.x;
        c = point1.y * (point2.x - point1.x) - point1.x * (point2.y - point1.y);
        auto outsidePoints = calculatePointsOutsideSquare(point1, point2);
        points.Vtx().push_back(outsidePoints.first);
        points.Vtx().push_back(outsidePoints.second);
        printf("Line Added\n");
        printf("Implicit equation: %.1fx + %.1fy + %.1f = 0\n", a, b, c);
        printf("Parametric: (t) = (%.1f, %.1f) + (%.1f, %.1f)t\n", point1.x, point1.y, point2.x - point1.x, point2.y - point1.y);
    }
    //get the Object (used to updateGPU)
    Object getPoints() { return this->points; }

    // Method to calculate two points on the line not inside the square
    std::pair<vec3, vec3> calculatePointsOutsideSquare(const vec3& point1, const vec3& point2) const {
        // the edge of the square
        float left = -1.0f;
        float right = 1.0f;

        if ((point2.x - point1.x) == 0) { //if the line is fully vertical
            vec3 p1(point1.x, -1, 0);
            vec3 p2(point2.x, 1, 0);
            return {p1, p2};
        }
        // calculating slope
        float slope = (point2.y - point1.y) / (point2.x - point1.x);

        // where the line intersect on x=1 and x=-1 axis
        float y_left = slope * (left - point1.x) + point1.y;
        float y_right = slope * (right - point1.x) + point1.y;

        // Create and return the intersection points
        vec3 p1(left, y_left, 0);
        vec3 p2(right, y_right, 0);

        return {p1, p2};
    }
    
    // Method to get the first point of the line
    vec3 getP1() const { return points.Vtx()[0]; }

    // Method to get the second point of the line
    vec3 getP2() const { return points.Vtx()[1]; }

    // Method to get the parameters of the line equation
    std::tuple<float, float, float> getLineEquation() const {
        return {a, b, c};
    }

    // Method to calculate the intersection point with another line
    vec3 intersection(const Line& other) const {
        float x1 = getP1().x, y1 = getP1().y;
        float x2 = getP2().x, y2 = getP2().y;
        float x3 = other.getP1().x, y3 = other.getP1().y;
        float x4 = other.getP2().x, y4 = other.getP2().y;
        
        float denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        if (denominator == 0) {
            // Lines are parallel or coincident
            return vec3(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), 0);
        }
        float xi = ((x3 - x4) * (x1 * y2 - y1 * x2) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denominator;
        float yi = ((y3 - y4) * (x1 * y2 - y1 * x2) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denominator;
        return vec3(xi, yi, 0);
    }
    
    // Method to check if a point is close to the line within a threshold distance
    bool isPointClose(const vec3& point, float threshold = 0.01f) const {
        float distance = distanceToPoint(point);
        return distance < threshold;
    }
    
    // Method to calculate the distance from a point to the line
    float distanceToPoint(const vec3& point) const {
        float numerator = std::abs(a * point.x + b * point.y + c);
        float denominator = std::sqrt(a * a + b * b);
        return numerator / denominator;
    }
    
    // Method to translate the line through a point
    void translate(const vec3& point) {
        
        vec3 translation = point - getP1();

            // Update the coordinates of the line's points
        points.Vtx()[0].x += translation.x;
        points.Vtx()[0].y += translation.y;
        points.Vtx()[1].x += translation.x;
        points.Vtx()[1].y += translation.y;

            // Calculate new points outside the square
        std::pair<vec3, vec3> newPoints = calculatePointsOutsideSquare(points.Vtx()[0], points.Vtx()[1]);

        // Update the line's points with new points outside the square
        points.Vtx()[0] = newPoints.first;
        points.Vtx()[1] = newPoints.second;

        // Update the line equation coefficients
        this->a = points.Vtx()[1].y - points.Vtx()[0].y;
        this->b = points.Vtx()[0].x - points.Vtx()[1].x;
        this->c = -(this->a * points.Vtx()[0].x + this->b * points.Vtx()[0].y);
        printf("calculating");
    }
};



class LineCollection {
private:
    std::vector<Line> lines; // Store Line objects
public:
    LineCollection() {}
    
    std::vector<Line> getLines(){ return this->lines; } //return the data

    // Method to add a new line
    void addLine(const Line& line) { lines.push_back(line); printf("Number of lines in LineCollection: %lu\n", lines.size()); } // Add the line object to the vector

    // Method to find a line close to a given point
    Line* pickLine(const vec3& point, float threshold) {
        for (auto& line : lines) {
            // Check if the point is close to the line segment
            if (line.distanceToPoint(point) < threshold) {
                return &line;
            }
        }
        return nullptr;
    }

    // Method to draw the lines with the specified color
    void draw(const vec3& color) {
        // Set the color
        gpuProgram.setUniform(color, "color");

        for (size_t i = 0; i < lines.size(); ++i) {
                lines.at(i).getPoints().updateGPU(); // Update GPU data for the line's points object NEMTOM EZ KELL E AM
                lines.at(i).getPoints().Draw(GL_LINES, color);  // Draw the line using the object's draw method
            }
    }
};

PointCollection *pcP; //pointcollection pointer
LineCollection *lcP; //linecollection pointer


void onInitialization() {
    objP->create(); //initilaize object
    pcP = new PointCollection(objP); //set pcP up
    lcP = new LineCollection(); //set up lcP

    gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

void onDisplay() {
    glClearColor(0, 0, 0, 0);     // background color
    glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer

    lcP->draw(vec3(0.0f, 1.0f, 1.0f));
    pcP->draw(vec3(1.0f, 0.0f, 0.0f));
    glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
    if (key == 'd') glutPostRedisplay();         // if d, invalidate display, i.e. redraw
    if (key == 'p') {
    // Activate point drawing model
        pointDrawingMode = true;
        lineDrawingMode = false;
        lineTranslationMode = false;
        intersectionMode = false;
    }
    if (key == 'l') {
    // Activate line drawing mode
        lineDrawingMode = true;
        pointDrawingMode = false;
        lineTranslationMode = false;
        intersectionMode = false;
    }
    if (key == 'm') {
    // Activate line translation mode
        lineDrawingMode = false; // Deactivate line drawing mode if active
        pointDrawingMode = false; // Deactivate point drawing mode if active
        lineTranslationMode = true;
        intersectionMode = false;
        }
    if (key == 'i') {
    // Activate point drawing mode
        lineDrawingMode = false; // Deactivate line drawing mode if active
        pointDrawingMode = false; // Deactivate point drawing mode if active
        lineTranslationMode = false;
        intersectionMode = true;
    }
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {    // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
    // Convert to normalized device space
    float cX = 2.0f * pX / windowWidth - 1;    // flip y axis
    float cY = 1.0f - 2.0f * pY / windowHeight;
    printf("Mouse moved to (%3.2f, %3.2f)\n", cX, cY);
    
    if (lineTranslationMode && selectedLine) {
    // Update the position of the selected line based on mouse movement
        
        //float dx = cX - selectedLine->getP1().x;
        //float dy = cY - selectedLine->getP1().y;

            // Translate the line using the calculated offset
        selectedLine->translate(vec3(cX, cY, 0.0f));
        
        glutPostRedisplay(); // Redraw the scene
    }
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
    // Convert to normalized device space
    float cX = 2.0f * pX / windowWidth - 1;    // flip y axis
    float cY = 1.0f - 2.0f * pY / windowHeight;
    
    //Point drawing mode
    if (pointDrawingMode) {
        pcP->addPoint(vec3(cX, cY, 0));
    }
    
    //Line drawing mode
    if (lineDrawingMode && !lineAdded) {
        vec3 mousePoint(cX, cY, 0.0f);
        vec3* pickedPoint = pcP->pickPoint(mousePoint, 0.01f);
        
        if (pickedPoint) {
                // Check if both points are selected
                if (!linePoint1) {
                    linePoint1 = pickedPoint;
                } else if (pickedPoint != linePoint1) { // Make sure pickedPoint is not the same as linePoint1
                    linePoint2 = pickedPoint;
                    // Create a line using the selected points
                    Line line(*linePoint1, *linePoint2);
                    // Add the line to LineCollection
                    lcP->addLine(line);
                    // Set lineAdded flag to true
                    lineAdded = true;
                }
        }
    }
    
    if (intersectionMode) {
        vec3 mousePoint(cX, cY, 0.0f);
        Line* pickedLine = lcP->pickLine(mousePoint, 0.01f);
        
        if (pickedLine) {
            if (!selectedLine1) {
                selectedLine1 = pickedLine;
            } else if (pickedLine != selectedLine1) {
                selectedLine2 = pickedLine;
                vec3 intersectionPoint = selectedLine1->intersection(*selectedLine2);
                if (!std::isnan(intersectionPoint.x)) {
                    // Add the intersection point to the point collection
                    pcP->addPoint(intersectionPoint);
                    intersectionAdded = true;
                }
            }
        }
    }
    glutPostRedisplay();
    
    //Line translation mode
    if (lineTranslationMode && button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Check if a line is selected
        vec3 mousePoint(cX, cY, 0.0f);
        selectedLine = lcP->pickLine(mousePoint, 0.01f);
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        selectedLine = nullptr; // Reset selected line when mouse button is released
    }
    

    char * buttonStat;
    switch (state) {
    case GLUT_DOWN: buttonStat = "pressed"; break;
    case GLUT_UP:   {
        if (lineAdded) {
            linePoint1 = nullptr;
            linePoint2 = nullptr;
            lineAdded = false;
        }
        if (intersectionAdded) {
            selectedLine1 = nullptr;
            selectedLine2 = nullptr;
            intersectionAdded = false;
        }
            buttonStat = "released"; break;
        }
    }
    

    switch (button) {
    case GLUT_LEFT_BUTTON:   printf("Left button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);   break;
    case GLUT_MIDDLE_BUTTON: printf("Middle button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY); break;
    case GLUT_RIGHT_BUTTON:  printf("Right button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);  break;
    }
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
    long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
}
