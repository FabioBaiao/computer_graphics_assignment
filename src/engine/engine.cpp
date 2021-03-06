#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <ctype.h>
#include <math.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <IL/il.h>
#else
#include <GL/glew.h>
#include <GL/glut.h>
#include <IL/il.h>
#endif

#include "geoTransform.h"
#include "group.h"
#include "light.h"
#include "rotation.h"
#include "scale.h"
#include "tinyxml2.h"
#include "translation.h"

#define ANG2RAD M_PI/180
#define ESC 27

using namespace std;
using namespace tinyxml2;

/********************** SCENE MODELS **********************/ 
typedef vector<float> Model;
vector<group> groups;
map<string, Model> models, normals, texCoords;
// maps model_name to buffer id and number of vertices
map<string, pair<GLuint,int>> model_to_buffer;
map<string, GLuint> normals_to_buffer;
map<string, GLuint> texCoords_to_buffer;
/******************** END SCENE MODELS ********************/ 

/********************** KEY BINDINGS **********************/ 
// maps keybindings to increase and decrease angle actions in rotations
map<char, rotation*> increaseBindings, decreaseBindings;
// maps keybindings to increase and decrease coordinates actions in translations 
map<char, translationCoords*> increaseX, increaseY, increaseZ, decreaseX, decreaseY, decreaseZ;
vector<char> keysInUse{ESC, 'W', 'S', 'A', 'D', 'M', 'L', 'C'};
/******************** END KEY BINDINGS ********************/ 

// VBO's
int n_models;
GLuint *buffers, *normalsBuffers, *textureBuffers;

/********************** CAMERA CONTROL **********************/ 
float r = 10.0f;
float alpha;
float beta;

float pitch = 0.0f, yaw = 0.0f;

float Px = 0.0f, Py = 0.0f, Pz = 0.0f;
float lookX= Px + cos(pitch) * sin(yaw);
float lookY= Py + sin(pitch);
float lookZ= Pz + cos(pitch) * cos(yaw);
/******************** END CAMERA CONTROL ********************/ 

// Lights
std::vector<light> lights;

// Polygon Mode
GLenum modes[] = {GL_FILL, GL_LINE, GL_POINT};
GLenum mode;

// directory of the read file
string directory;

string directoryOfFile(const string& fname) {
    size_t pos = fname.find_last_of("\\/");

    return (string::npos == pos)? "" : fname.substr(0, pos+1);
}

float randFloat() {
    return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}

void changeSize(int w, int h) {
    if(h == 0)
        h = 1;

    float ratio = w * 1.0 / h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, w, h);
    gluPerspective(45.0f,ratio, 1.0f,1000.0f);
    glMatrixMode(GL_MODELVIEW);
}

void drawModel(string model, std::vector<color> v, GLuint texture) {
    auto buffer_id_size = model_to_buffer[model];
    auto normalsBuffer_id_size = normals_to_buffer[model];
    auto texCoords_id_size = texCoords_to_buffer[model];
    
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id_size.first);
    glVertexPointer(3,GL_FLOAT, 0, 0);
    
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer_id_size);
    glNormalPointer(GL_FLOAT, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, texCoords_id_size);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);

    for(auto c : v){
        glMaterialfv(GL_FRONT, c.component, c.colors);
    }

    glBindTexture(GL_TEXTURE_2D, texture);

    glDrawArrays(GL_TRIANGLES, 0, buffer_id_size.second/3);

    //RESET

    glBindTexture(GL_TEXTURE_2D, 0);

    float amb[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    float dif[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    float spec[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float emi[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_EMISSION, emi);
}

void drawRandom(randomModel rnd) {
    for (auto spec : rnd.specs) {
        int n = spec.n;
        float maxR = spec.maxR;
        float minR = spec.minR;
        float maxS = spec.maxS;
        float minS = spec.minS;
        int i = 0;
        for (string model : rnd.models) {
            std::vector<color> v = rnd.modelsColor[i];
            GLuint texture = rnd.modelsTextures[i];
            for (int j = 0; j < n; j++) {
                float r = rand();
                float alfa = rand();
                float s = rand();
                // polar coordinates
                r = r/RAND_MAX * (maxR - minR) + minR;
                alfa = alfa/RAND_MAX * 2*M_PI;
                // random scale
                s = s/RAND_MAX * (maxS - minS) + minS;

                glPushMatrix();
                glTranslatef(r * sin(alfa), 0, r * cos(alfa));
                glScalef(s, s, s);
                drawModel(model, v, texture);
                glPopMatrix();
            }
            i++;
        }
    }
}

void drawGroup(group g) {
    glPushMatrix();

    for(geoTransform* t : g.transforms) {
        t->apply();
    }
    int i = 0;
    for(string model : g.models) {
        std::vector<color> v = g.modelsColor[i];
        GLuint texture = g.modelsTextures[i];

        drawModel(model, v, texture);
        i++;
    }
    for (auto rnd : g.randoms) {
        drawRandom(rnd);
    }
    for(auto gr : g.childGroups) {
        drawGroup(gr);
    }
    glPopMatrix();
}

void renderScene(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT, modes[mode]);
    glLoadIdentity();
    gluLookAt(Px, Py, Pz,
              lookX,lookY,lookZ,
              0.0f,1.0f,0.0f);

    for(auto l : lights){
        l.apply();
    }

    srand(0);
    for(auto g : groups) {
        drawGroup(g);
    }
    glutSwapBuffers();
}

void lookAt(float alpha, float beta) {
    lookX = Px + cos(beta) * sin(alpha);
    lookY = Py + sin(beta);
    lookZ = Pz + cos(beta) * cos(alpha);
}

float magnitude(float vx, float vy, float vz) {
    return sqrtf((vx*vx) + (vy*vy) + (vz*vz));
}

void normalize(float* vx, float* vy, float* vz) {
    float mag = magnitude(*vx, *vy, *vz);
    *vx = (*vx) / mag;
    *vy = (*vy) / mag;
    *vz = (*vz) / mag;
}

void processKeys(unsigned char c, int xx, int yy) {
    float k = 0.5f;
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

    // forward vector
    float dX = lookX - Px;
    float dY = lookY - Py;
    float dZ = lookZ - Pz;

    normalize(&dX, &dY, &dZ);
    float rX, rY, rZ;
    char cc = toupper(c);
    switch(cc) {
    case ESC:
        exit(0);
    case 'W':
        Px += k*dX;
        Py += k*dY;
        Pz += k*dZ;

        lookX += k*dX;
        lookY += k*dY;
        lookZ += k*dZ;
        break;
    case 'S':
        Px -= k*dX;
        Py -= k*dY;
        Pz -= k*dZ;

        lookX -= k*dX;
        lookY -= k*dY;
        lookZ -= k*dZ;
        break;
    case 'A':
        // cross product: up x forwardV 
        rX = (upY * dZ) - (upZ * dY);
        rY = (upZ * dX) - (upX * dZ);
        rZ = (upX * dY) - (upY * dX);

        Px += k*rX;
        Py += k*rY;
        Pz += k*rZ;

        lookX += k*rX;
        lookY += k*rY;
        lookZ += k*rZ;
        break;
    case 'D':
        // cross product: forwardV x up
        rX = (dY * upZ) - (dZ * upY);
        rY = (dZ * upX) - (dX * upZ);
        rZ = (dX * upY) - (dY * upX);

        Px += k*rX;
        Py += k*rY;
        Pz += k*rZ;

        lookX += k*rX;
        lookY += k*rY;
        lookZ += k*rZ;
        break;
    case 'M': 
        // More radius
        r += 0.2f;
        break;
    case 'L': 
        // Less radius
        r -= 0.2f;
        if(r < 0.2f)
            r = 0.2f;
        break;
    case 'C':
        mode = (mode + 1) % 3;
        break;
    default:
        auto itRotation = decreaseBindings.find(cc);
        if(itRotation != decreaseBindings.end()) {
            itRotation->second->decreaseAngle();
        } else if((itRotation = increaseBindings.find(cc)) != increaseBindings.end()) {
            itRotation->second->increaseAngle();
            break;
        }
        auto itTranslate = increaseX.find(cc);
        if(itTranslate != increaseX.end()) {
            itTranslate->second->increaseX();
        } else if((itTranslate = increaseY.find(cc)) != increaseY.end()) {
            itTranslate->second->increaseY();
        } else if((itTranslate = increaseZ.find(cc)) != increaseZ.end()) {
            itTranslate->second->increaseZ();
        } else if((itTranslate = decreaseX.find(cc)) != decreaseX.end()) {
            itTranslate->second->decreaseX();
        } else if((itTranslate = decreaseY.find(cc)) != decreaseY.end()) {
            itTranslate->second->decreaseY();
        } else if((itTranslate = decreaseZ.find(cc)) != decreaseZ.end()) {
            itTranslate->second->decreaseZ();
        }
        break;
    }
    glutPostRedisplay();
}

void processSpecialKeys(int key, int xx, int yy) {
    switch(key) {
    case GLUT_KEY_UP:
        pitch += 0.01f;
        if(pitch > 1.5f)
            pitch = 1.5f;
        break;
    case GLUT_KEY_LEFT:
        yaw += 0.01f;
        break;
    case GLUT_KEY_DOWN:
        pitch -= 0.01f;
        if(pitch < -1.5f)
            pitch = -1.5;
        break;
    case GLUT_KEY_RIGHT:
        yaw -= 0.01f;
        break;
    }
    lookAt(yaw, pitch);
    glutPostRedisplay();
}

void parsePoint(float **points, int i, XMLElement* elem) {
    float x, y, z;
    elem->QueryFloatAttribute("X", &x);
    elem->QueryFloatAttribute("Y", &y);
    elem->QueryFloatAttribute("Z", &z);

    points[i] = (float *) malloc(sizeof(float) * 3);

    points[i][0] = x;
    points[i][1] = y;
    points[i][2] = z;
}

void bindTranslateIfAvailable(XMLElement* elem, const char* attributeName, map<char, translationCoords*>& binds, translationCoords* trans){
    const char * attribute = elem->Attribute(attributeName);
    if(attribute && strlen(attribute) == 1) {
        char c = toupper(attribute[0]);
        binds[c] = trans;
    }
}

void parseTranslate(group& g, XMLElement* elem) {
    float x, y, z, time;
    x = y = z = time = 0.0f;

    XMLError hasTime = elem->QueryFloatAttribute("time", &time);
    
    if(hasTime != XML_NO_ATTRIBUTE) {
        // animated translation
        XMLElement *child = elem->FirstChildElement();
        float **points = NULL;
        int n = 0;
        for(; child; child = child->NextSiblingElement()) {
            n++;
            points = (float **) realloc(points, n*sizeof(float *));
            parsePoint(points, n-1, child);
        }
        g.transforms.push_back(new translationTime(time, points, n));
    } else {
        // static translation
    	elem->QueryFloatAttribute("X", &x);
    	elem->QueryFloatAttribute("Y", &y);
    	elem->QueryFloatAttribute("Z", &z);

        translationCoords* trans = new translationCoords(x,y,z);
    	g.transforms.push_back(trans);

        bindTranslateIfAvailable(elem, "incX", increaseX, trans);
        bindTranslateIfAvailable(elem, "incY", increaseY, trans);
        bindTranslateIfAvailable(elem, "incZ", increaseZ, trans);
        bindTranslateIfAvailable(elem, "decX", decreaseX, trans);
        bindTranslateIfAvailable(elem, "decY", decreaseY, trans);
        bindTranslateIfAvailable(elem, "decZ", decreaseZ, trans);
    }
}

void parseRotate(group& g, XMLElement* elem) {
    float angle, time, axisX, axisY, axisZ;
    angle = axisX = axisY = axisZ = time = 0.0f;

    elem->QueryFloatAttribute("time", &time);
    elem->QueryFloatAttribute("angle", &angle);
    elem->QueryFloatAttribute("axisX", &axisX);
    elem->QueryFloatAttribute("axisY", &axisY);
    elem->QueryFloatAttribute("axisZ", &axisZ);


    rotation* rot = new rotation(angle, axisX, axisY, axisZ, time);
    g.transforms.push_back(rot);

    const char * decreaseBind = elem->Attribute("decreaseBind");
    const char * increaseBind = elem->Attribute("increaseBind");

    // TO-DO: Create pparser and check existing bindings like above
    if(decreaseBind && strlen(decreaseBind) == 1) {
        // if there is a keybinding to lower the angle
        char c = toupper(decreaseBind[0]);
        decreaseBindings[c] = rot;
    } 

    if(increaseBind && strlen(increaseBind) == 1) {
        // if there is a keybinding to lower the angle
        char c = toupper(increaseBind[0]);
        increaseBindings[c] = rot;
    } 

}

void parseScale(group& g, XMLElement* elem) {
    float x, y, z;
    x = y = z = 1.0f;
    elem->QueryFloatAttribute("X", &x);
    elem->QueryFloatAttribute("Y", &y);
    elem->QueryFloatAttribute("Z", &z);
    g.transforms.push_back(new scale(x,y,z));
}

/*color& parseColor(XMLElement* model) {
    float rr, gg, bb;
    int r_r, r_g, r_b;
    rr = gg = bb = 0.0;
    r_r = model->QueryFloatAttribute("R", &rr);
    r_g = model->QueryFloatAttribute("G", &gg);
    r_b = model->QueryFloatAttribute("B", &bb);
    if(r_r != XML_SUCCESS && r_g != XML_SUCCESS && r_b != XML_SUCCESS) {
        // the color defaults to white if not specified
        rr = gg = bb = 1;
    }
    color* c = new color(rr,gg,bb);
    return *c;
}*/

std::vector<color> parseColor(XMLElement* model){
    float rr, gg, bb;
    int isDiff, isAmb, isSpec, isEmi;
    std::vector<color> v;
    rr = gg = bb = 0.0f;
    isDiff = model->QueryFloatAttribute("diffR", &rr);
    if (isDiff != XML_NO_ATTRIBUTE){
        model->QueryFloatAttribute("diffG", &gg);
        model->QueryFloatAttribute("diffB", &bb);
        color *c = new color(rr, gg, bb, GL_DIFFUSE);
        v.push_back(*c);
    }
    isAmb = model->QueryFloatAttribute("ambR", &rr);
    if(isAmb != XML_NO_ATTRIBUTE){
        model->QueryFloatAttribute("ambG", &gg);
        model->QueryFloatAttribute("ambB", &bb);
        color *c = new color(rr, gg, bb, GL_AMBIENT);
        v.push_back(*c);
    }
    isSpec = model->QueryFloatAttribute("specR", &rr);
    if(isSpec != XML_NO_ATTRIBUTE){
        model->QueryFloatAttribute("specG", &gg);
        model->QueryFloatAttribute("specB", &bb);
        color *c = new color(rr, gg, bb, GL_SPECULAR);
        v.push_back(*c);
    }
    isEmi = model->QueryFloatAttribute("emiR", &rr);
    if(isEmi != XML_NO_ATTRIBUTE){
        model->QueryFloatAttribute("emiG", &gg);
        model->QueryFloatAttribute("emiB", &bb);
        color *c = new color(rr, gg, bb, GL_EMISSION);
        v.push_back(*c);
    }
    if (isDiff != XML_NO_ATTRIBUTE && isAmb != XML_NO_ATTRIBUTE 
        && isSpec != XML_NO_ATTRIBUTE && isEmi != XML_NO_ATTRIBUTE){ 
        rr = gg = bb = 1.0f;
        // qual é a componente por defeito?
    }
    return v;
}

void readFile(ifstream& file, string fName){
    int n_vertex, n_values;
    Model model_read;
    Model normals_read;
    Model texCoords_read;

    file >> n_vertex;
    n_values = 3*n_vertex;


    for(int i = 0; i < n_values; i++) {
        float tmp;
        file >> tmp;
        model_read.push_back(tmp);
    }

    for(int i = 0; i < n_values; i++){
        float tmp;
        file >> tmp;
        normals_read.push_back(tmp);
    }

    n_values = n_vertex * 2;

    for (int i = 0; i < n_values; i++){
        float tmp;
        file >> tmp;
        texCoords_read.push_back(tmp);
    }

    file.close();
    models[fName] = model_read;
    normals[fName] = normals_read;
    texCoords[fName] = texCoords_read;

}

int loadTexture(const char *s) {

    unsigned int t,tw,th;
    unsigned char *texData;
    unsigned int texID;

    ilInit();
    ilEnable(IL_ORIGIN_SET);
    ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
    ilGenImages(1,&t);
    ilBindImage(t);
    ilLoadImage((ILstring) (directory + s).c_str());
    tw = ilGetInteger(IL_IMAGE_WIDTH);
    th = ilGetInteger(IL_IMAGE_HEIGHT);
    ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
    texData = ilGetData();

    glEnable(GL_TEXTURE_2D);

    glGenTextures(1,&texID);
    
    glBindTexture(GL_TEXTURE_2D,texID);
    glTexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_WRAP_S,      GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_WRAP_T,      GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_MAG_FILTER,      GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texID;

}

void parseModel(group& g, XMLElement * model) {
    const char* filename= model->Attribute("file");
    if(filename != NULL) {
        GLuint texture = 0;
        string fName = string(filename);
        ifstream file(directory + filename);
        if(!file) {
            cerr << "The file \"" << filename << "\" was not found.\n";
        }
        std::vector<color> v = parseColor(model);
        const char *tex = model->Attribute("texture");
        if (tex != NULL){
            texture = loadTexture(tex);
        }
        g.models.push_back(fName);
        g.modelsColor.push_back(v);
        g.modelsTextures.push_back(texture);
        if(models.find(fName) == models.end()) {
            // if the model file was not already read
            readFile(file, fName);
        }
    }
}

void parseModel(randomModel& r, XMLElement * model) {
    const char* filename= model->Attribute("file");
    if(filename != NULL) {
        GLuint texture = 0;
        string fName = string(filename);
        ifstream file(directory + filename);
        if(!file) {
            cerr << "The file \"" << filename << "\" was not found.\n";
        }
        std::vector<color> v = parseColor(model);
        const char *tex = model->Attribute("texture");
        if (tex != NULL){
            texture = loadTexture(tex);
        }
        r.models.push_back(fName);
        r.modelsColor.push_back(v);
        r.modelsTextures.push_back(texture);
        if(models.find(fName) == models.end()) {
            // if the model file was not already read
            readFile(file, fName);
        }
    }
}

void parseSpecs(randomModel& r, XMLElement * model) {
    int n = 0;
    float minR=0.0, maxR=0.0, minS=1.0, maxS=1.0;
    minR = maxR = minS = maxS = 0;
    model->QueryIntAttribute("N", &n);
    model->QueryFloatAttribute("minRadius", &minR);
    model->QueryFloatAttribute("maxRadius", &maxR);
    model->QueryFloatAttribute("minScale", &minS);
    model->QueryFloatAttribute("maxScale", &maxS);

    randSpecs* rs = new randSpecs(n, minR, maxR, minS, maxS);
    r.specs.push_back(*rs);
}

void parseRandom(group& g, XMLElement * elem) {
    randomModel r;
    XMLElement *child = elem->FirstChildElement();
    for(; child; child = child->NextSiblingElement()) {
        string type = string(child->Name());
        if (type == "model") {
            parseModel(r, child);
        }
        else if(type == "specs") {
            parseSpecs(r, child);
        }
    }
    g.randoms.push_back(r);
}

void parseModels(group& g, XMLElement * elem) {
    XMLElement *child = elem->FirstChildElement();
    for(; child; child = child->NextSiblingElement()) {
        string type = string(child->Name());
        if (type == "model") {
            parseModel(g, child);
        }
        else if(type == "random") {
            parseRandom(g, child);
        }
    }
}

group parseGroup(XMLElement *gr) {
    group g;
    XMLElement *child = gr->FirstChildElement();
    for( ; child; child = child->NextSiblingElement()) {
        string type = string(child->Name());
        if(type == "translate") {
            parseTranslate(g, child);
        } else if(type == "rotate") {
            parseRotate(g, child);
        } else if(type == "scale") {
            parseScale(g, child);
        } else if(type == "group") {
            group g_child = parseGroup(child);
            g.childGroups.push_back(g_child);
        } else if(type == "models") {
            parseModels(g, child);
        }
    }
    return g;
}

void parseInitialPosition(XMLElement* scene) {
    // if a coordinate is not specified, it will default to 0
    scene->QueryFloatAttribute("camX", &Px);
    scene->QueryFloatAttribute("camY", &Py);
    scene->QueryFloatAttribute("camZ", &Pz);
    scene->QueryFloatAttribute("yaw", &yaw);
    scene->QueryFloatAttribute("pitch", &pitch);
    yaw *= ANG2RAD;
    pitch *= ANG2RAD;
    lookX = Px + cos(pitch) * sin(yaw);
    lookY = Py + sin(pitch);
    lookZ = Pz + cos(pitch) * cos(yaw);
}

void parseLights(XMLElement* lgts){
    XMLElement *lgt = lgts->FirstChildElement();
    for (int i = 0; lgt; lgt = lgt->NextSiblingElement()){
        
        const char *type = lgt->Attribute("type");
        float pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float amb[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float diff[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float spec[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float dir[3] = {0.0f, 0.0f, -1.0f};
        float exp = 0.0f;
        float cut = 180.0f;
        lgt->QueryFloatAttribute("posX", &pos[0]);
        lgt->QueryFloatAttribute("posY", &pos[1]);
        lgt->QueryFloatAttribute("posZ", &pos[2]);
        if(strcmp(type, "POINT") == 0){
            pos[3] = 1.0f;
        }
        else if(strcmp(type, "DIRECTIONAL") == 0){
            pos[3] = 0.0f;
        }
        else if(strcmp(type, "SPOTLIGHT") == 0){
            pos[3] = 1.0f;
        }
        lgt->QueryFloatAttribute("ambR", &amb[0]);
        lgt->QueryFloatAttribute("ambG", &amb[0]);
        lgt->QueryFloatAttribute("ambB", &amb[0]);

        lgt->QueryFloatAttribute("diffR", &diff[0]);
        lgt->QueryFloatAttribute("diffG", &diff[1]);
        lgt->QueryFloatAttribute("diffB", &diff[2]);

        lgt->QueryFloatAttribute("specR", &spec[0]);
        lgt->QueryFloatAttribute("specG", &spec[1]);
        lgt->QueryFloatAttribute("specB", &spec[2]);

        lgt->QueryFloatAttribute("dirX", &dir[0]);
        lgt->QueryFloatAttribute("dirY", &dir[1]);
        lgt->QueryFloatAttribute("dirZ", &dir[2]);

        lgt->QueryFloatAttribute("exp", &exp);

        lgt->QueryFloatAttribute("cut", &cut);

        GLenum individualLight = GL_LIGHT0 + i;

        glEnable(individualLight);
        light *l = new light(individualLight, pos, amb, diff, spec, dir, exp, cut);
        i++;
        lights.push_back(*l);
    }
}

void initGL(){
    // init GLUT and the window
    glutInitDisplayMode(GLUT_DEPTH|GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowPosition(100,100);
    glutInitWindowSize(glutGet(GLUT_SCREEN_WIDTH), glutGet(GLUT_SCREEN_HEIGHT));
    glutCreateWindow("Pratical Assignment");

    // Callback registry
    glutDisplayFunc(renderScene);
    glutIdleFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(processKeys);
    glutSpecialFunc(processSpecialKeys);

    #ifndef __APPLE__
    glewInit();
    #endif

    //  OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

//We assume that the .xml and .3d files passed are correct.
int main(int argc, char **argv) {
    glutInit(&argc, argv);
    initGL();

    if(argc != 2) {
        cerr << "Usage: " << argv[0] << " config_file\n";
        return 1;
    }
    XMLDocument doc;
    XMLError loadOkay = doc.LoadFile(argv[1]);

    if(loadOkay != XML_SUCCESS) {
        perror("LoadFile");
        cerr << "Error loading file '" << argv[1] << "'.\n";
        return 1;
    }

    directory = directoryOfFile(argv[1]);
    XMLElement* scene = doc.FirstChildElement("scene");
    if(scene == NULL) {
        cerr << "Error: The first XML element must be \"scene\"" << endl;
        return 1;
    }

    parseInitialPosition(scene);

    XMLElement* lgts = scene->FirstChildElement("lights");
    if (lgts){
        parseLights(lgts);
    }

    XMLElement* gr = scene->FirstChildElement("group");

    // Read all the groups
    for(; gr; gr = gr->NextSiblingElement()) {
        group g = parseGroup(gr);
        groups.push_back(g);
    } 

    n_models = models.size();
    buffers = (GLuint *) malloc(sizeof(GLuint) * n_models);
    normalsBuffers = (GLuint *) malloc(sizeof(GLuint) * n_models);
    textureBuffers = (GLuint *) malloc(sizeof(GLuint) * n_models);
    glGenBuffers(n_models, buffers);
    glGenBuffers(n_models, normalsBuffers);
    glGenBuffers(n_models, textureBuffers);

    int i = 0;
    for(auto it = models.begin(); it != models.end(); it++) {
        auto model = it->second;

        // maps the name of the model to the id of the buffer and number of vertices
        pair<GLuint, int> id_number_elements(buffers[i], model.size());
        model_to_buffer[it->first] = id_number_elements;

        // fills the buffers 
        glBindBuffer(GL_ARRAY_BUFFER, buffers[i++]);
        glBufferData(GL_ARRAY_BUFFER, model.size() * sizeof(float), model.data(), GL_STATIC_DRAW);
    }

    i = 0;
    for(auto it = normals.begin(); it != normals.end(); it++) {
        auto normal = it->second;

        // maps the name of the model to the id of the buffer and number of vertices
        //pair<GLuint, int> id_number_elements(normalsBuffers[i], normal.size());
        normals_to_buffer[it->first] = normalsBuffers[i];

        // fills the buffers 
        glBindBuffer(GL_ARRAY_BUFFER, normalsBuffers[i++]);
        glBufferData(GL_ARRAY_BUFFER, normal.size() * sizeof(float), normal.data(), GL_STATIC_DRAW);
    }

    i = 0;
    for(auto it = texCoords.begin(); it != texCoords.end(); it++){
        auto texCoord = it->second;

        texCoords_to_buffer[it->first] = textureBuffers[i];

        glBindBuffer(GL_ARRAY_BUFFER, textureBuffers[i++]);
        glBufferData(GL_ARRAY_BUFFER, texCoord.size() * sizeof(float), texCoord.data(), GL_STATIC_DRAW);
    }

    glutMainLoop();

    return 1;
}
