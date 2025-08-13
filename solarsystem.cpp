#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <string>
#include <random>
#include <algorithm>

#define GLEW_STATIC 1
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace glm;
using namespace std;

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;
vec3 cameraPosition = vec3(0.0f, 10.0f, 40.0f);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);
vec3 cameraUp    = vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f, pitch = 0.0f;
bool firstMouse = true;
float lastX = WINDOW_WIDTH / 2.0f, lastY = WINDOW_HEIGHT / 2.0f;

float deltaTime = 0.0f, lastFrame = 0.0f;

const int STAR_COUNT = 50000;
const float STAR_FIELD_RADIUS = 4000.0f;
GLuint starfieldVAO; int starfieldVertexCount;

const int SHOOTING_STAR_COUNT = 20;
const float SHOOTING_STAR_SPEED = 50.0f;
const float SHOOTING_STAR_LENGTH = 5.0f;
GLuint shootingStarVAO=0, shootingStarVBO=0;
int shootingStarLineVertexCount = 0;

struct ShootingStar {
    vec3 position, direction, color;
    float speed, lifetime, maxLifetime;
};
vector<ShootingStar> shootingStars;

GLuint sunTexture, mercuryTexture, venusTexture, earthTexture, marsTexture, jupiterTexture;
GLuint saturnTexture, uranusTexture, neptuneTexture, moonTexture, ringTexture;

float orbitSpeedMultiplier = 1.0f;
bool pausedOrbits = false;

vector<float> createTexturedSphere(float radius, vec3 color);
int createTexturedSphereVBO(vector<float>& v);
vector<vec3> createOrbitPath(float radius);
int createOrbitVBO(vector<vec3>& orbitVertices);
vector<float> createRing(float innerR, float outerR, vec3 color);

int compileShader(GLenum type, const char* src){
    int sh = glCreateShader(type);
    glShaderSource(sh,1,&src,nullptr);
    glCompileShader(sh);
    int ok; char log[1024]; glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if(!ok){ glGetShaderInfoLog(sh,1024,nullptr,log); cerr<<"Shader compile error:\n"<<log<<endl; }
    return sh;
}
int linkProgram(const char* vs, const char* fs){
    int v=compileShader(GL_VERTEX_SHADER,vs);
    int f=compileShader(GL_FRAGMENT_SHADER,fs);
    int p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    int ok; char log[1024]; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){ glGetProgramInfoLog(p,1024,nullptr,log); cerr<<"Program link error:\n"<<log<<endl; }
    glDeleteShader(v); glDeleteShader(f); return p;
}

const char* VS_MAIN = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec3 aNormal;

uniform mat4 worldMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 vColor;
out vec2 vUV;
out vec3 vWorldPos;
out vec3 vNormal;

void main(){
    vColor = aColor;
    vUV = aTexCoord;
    vec4 wp = worldMatrix * vec4(aPos,1.0);
    vWorldPos = wp.xyz;
    vNormal = mat3(transpose(inverse(worldMatrix))) * aNormal;
    gl_Position = projectionMatrix * viewMatrix * wp;
}
)GLSL";

const char* FS_MAIN = R"GLSL(
#version 330 core
in vec3 vColor;
in vec2 vUV;
in vec3 vWorldPos;
in vec3 vNormal;
out vec4 FragColor;

uniform sampler2D texture1;
uniform bool useTexture;
uniform bool isSun;
uniform bool useLighting;

uniform vec3 viewPos;

// Phong material
uniform vec3 Ka; // ambient
uniform vec3 Kd; // diffuse
uniform vec3 Ks; // spec
uniform float shininess;

// Directional light (dynamic)
uniform vec3 lightDir;   // direction of light rays (pointing FROM light)
uniform vec3 lightColor;

// Shadow mapping
uniform bool useShadows;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

float shadowFactor(vec3 worldPos, vec3 normal){
    if(!useShadows) return 0.0;

    vec4 lsp = lightSpaceMatrix * vec4(worldPos,1.0);
    vec3 proj = lsp.xyz / lsp.w;
    proj = proj * 0.5 + 0.5;

    if(proj.z > 1.0) return 0.0;

    // bias against acne (angle-dependent)
    float bias = max(0.0008 * (1.0 - max(dot(normal, -normalize(lightDir)), 0.0)), 0.0004);

    // PCF 3x3
    float texel = 1.0 / float(textureSize(shadowMap,0).x);
    float shadow = 0.0;
    for(int x=-1; x<=1; ++x)
    for(int y=-1; y<=1; ++y){
        float closest = texture(shadowMap, proj.xy + vec2(x,y)*texel).r;
        shadow += (proj.z - bias) > closest ? 1.0 : 0.0;
    }
    shadow /= 9.0;
    return clamp(shadow,0.0,1.0);
}

void main(){
    // emissive Sun
    if(isSun){
        vec3 col = (useTexture ? texture(texture1, vUV).rgb : vColor);
        float glow = 1.5 + 0.3 * sin(gl_FragCoord.x*0.01) * cos(gl_FragCoord.y*0.01);
        col = mix(col*glow, vec3(1.0,0.9,0.6), 0.3);
        FragColor = vec4(col,1.0);
        return;
    }

    vec3 base = useTexture ? texture(texture1, vUV).rgb : vColor;

    if(!useLighting){
        FragColor = vec4(base,1.0);
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 V = normalize(viewPos - vWorldPos);
    vec3 L = normalize(-lightDir); // convert direction to vector towards light
    float ndotl = max(dot(N,L),0.0);

    vec3 ambient  = Ka * lightColor;
    vec3 diffuse  = Kd * lightColor * ndotl;
    vec3 R = reflect(-L,N);
    float specPow = pow(max(dot(V,R),0.0), shininess);
    vec3 specular = Ks * lightColor * specPow;

    float sh = shadowFactor(vWorldPos, N);
    vec3 lighting = ambient + (1.0 - sh)*(diffuse + specular);

    FragColor = vec4(base * lighting, 1.0);
}
)GLSL";

const char* VS_STAR = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in float aBrightness;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
out vec3 starColor;
out float starBrightness;
void main(){
    starColor = aColor;
    starBrightness = aBrightness;
    gl_Position = projectionMatrix * viewMatrix * vec4(aPos,1.0);
    gl_PointSize = 1.5 + 3.0*aBrightness;
}
)GLSL";
const char* FS_STAR = R"GLSL(
#version 330 core
in vec3 starColor;
in float starBrightness;
out vec4 FragColor;
void main(){
    vec2 c = gl_PointCoord - vec2(0.5);
    float d = length(c);
    if(d>0.5) discard;
    float a = 1.0 - smoothstep(0.0,0.5,d);
    vec3 finalColor = starColor * starBrightness * 1.2;
    FragColor = vec4(finalColor, a*starBrightness);
}
)GLSL";

const char* VS_SHOOT = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
void main(){
    gl_Position = projectionMatrix * viewMatrix * vec4(aPos,1.0);
}
)GLSL";
const char* FS_SHOOT = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 starColor;
uniform float alpha;
void main(){ FragColor = vec4(starColor, alpha); }
)GLSL";

const char* VS_DEPTH = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 worldMatrix;
uniform mat4 lightSpaceMatrix;
void main(){
    gl_Position = lightSpaceMatrix * worldMatrix * vec4(aPos,1.0);
}
)GLSL";
const char* FS_DEPTH = R"GLSL(
#version 330 core
void main(){}
)GLSL";

void setProjectionMatrix(int program, mat4 P){
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program,"projectionMatrix"),1,GL_FALSE,&P[0][0]);
}
void setViewMatrix(int program, mat4 V){
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program,"viewMatrix"),1,GL_FALSE,&V[0][0]);
}
void setWorldMatrix(int program, mat4 M){
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program,"worldMatrix"),1,GL_FALSE,&M[0][0]);
}

struct VertexPTN{ vec3 p; vec3 c; vec2 uv; vec3 n; };
struct Mesh {
    vector<VertexPTN> vertices;
    vector<unsigned>  indices;
    GLuint VAO=0,VBO=0,EBO=0;
    void upload(){
        if(VAO==0){ glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO); glGenBuffers(1,&EBO);}
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER,VBO);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(VertexPTN),vertices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned),indices.data(),GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VertexPTN),(void*)offsetof(VertexPTN,p)); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VertexPTN),(void*)offsetof(VertexPTN,c)); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(VertexPTN),(void*)offsetof(VertexPTN,uv)); glEnableVertexAttribArray(2);
        glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(VertexPTN),(void*)offsetof(VertexPTN,n)); glEnableVertexAttribArray(3);
        glBindVertexArray(0);
    }
    void drawElements(){ glBindVertexArray(VAO); glDrawElements(GL_TRIANGLES,(GLsizei)indices.size(),GL_UNSIGNED_INT,0); }
};

bool loadOBJ(const string& path, Mesh& out, vec3 defaultColor=vec3(0.8f)){
    ifstream f(path);
    if(!f.good()){ cout<<"OBJ not found: "<<path<<" (will use fallback)\n"; return false; }
    vector<vec3> P; vector<vec2> T; vector<vec3> N;
    vector<unsigned> ip,it,in;
    string line;
    while(getline(f,line)){
        if(line.size()<2) continue;
        istringstream ss(line);
        string tag; ss>>tag;
        if(tag=="v"){ vec3 p; ss>>p.x>>p.y>>p.z; P.push_back(p); }
        else if(tag=="vt"){ vec2 t; ss>>t.x>>t.y; T.push_back(t); }
        else if(tag=="vn"){ vec3 n; ss>>n.x>>n.y>>n.z; N.push_back(n); }
        else if(tag=="f"){
            vector<string> toks; string tok; while(ss>>tok) toks.push_back(tok);
            auto parseV = [&](const string& s, int& vi,int& ti,int& ni){
                vi=ti=ni=0;
                sscanf(s.c_str(), "%d/%d/%d", &vi, &ti, &ni);
                if(vi<0) vi = (int)P.size()+1+vi;
                if(ti<0) ti = (int)T.size()+1+ti;
                if(ni<0) ni = (int)N.size()+1+ni;
            };
            for(size_t k=1;k+1<toks.size();++k){
                int i0, t0, n0, i1,t1,n1, i2,t2,n2;
                parseV(toks[0], i0,t0,n0);
                parseV(toks[k], i1,t1,n1);
                parseV(toks[k+1], i2,t2,n2);
                ip.insert(ip.end(),{(unsigned)i0,(unsigned)i1,(unsigned)i2});
                it.insert(it.end(),{(unsigned)t0,(unsigned)t1,(unsigned)t2});
                in.insert(in.end(),{(unsigned)n0,(unsigned)n1,(unsigned)n2});
            }
        }
    }
    out.vertices.clear(); out.indices.clear();
    out.vertices.reserve(ip.size());
    for(size_t i=0;i<ip.size();++i){
        VertexPTN v{};
        v.p = P[ip[i]-1];
        v.c = defaultColor;
        v.uv = (it[i]>0 && it[i]<=T.size()) ? T[it[i]-1] : vec2(0.0f);
        v.n  = (in[i]>0 && in[i]<=N.size()) ? N[in[i]-1] : vec3(0,1,0);
        out.vertices.push_back(v);
        out.indices.push_back((unsigned)i);
    }
    out.upload();
    cout<<"Loaded OBJ: "<<path<<" verts="<<out.vertices.size()<<" tris="<<out.indices.size()/3<<"\n";
    return true;
}

Mesh makeFallbackShip(){
    Mesh m;
    vector<VertexPTN> V = {
        {{ 0, 0.3f,  1.2f},{0.9,0.9,1.0},{0,0},{0,1,0}},
        {{-0.6f,0, -1.0f},{0.7,0.7,0.9},{0,0},{-0.2,0.8,-0.5}},
        {{ 0.6f,0, -1.0f},{0.7,0.7,0.9},{0,0},{ 0.2,0.8,-0.5}},
        {{ 0, 0.7f,-0.4f},{0.8,0.8,1.0},{0,0},{0,1,0}}, 
        {{ 0, -0.2f,-0.4f},{0.6,0.6,0.9},{0,0},{0,-1,0}}, 
        {{-1.0f,0,-0.2f},{0.6,0.6,1.0},{0,0},{-1,0,0}}, 
        {{ 1.0f,0,-0.2f},{0.6,0.6,1.0},{0,0},{ 1,0,0}}, 
    };
    vector<unsigned> I = {
        0,1,3, 0,3,2, 
        0,2,4, 0,4,1,  
        1,5,3, 3,5,2,  
        1,4,5, 2,6,4,  
    };
    m.vertices = V; m.indices = I; m.upload();
    cout<<"Using fallback ship mesh.\n";
    return m;
}

GLuint loadTexture(const char* filename) {
    GLuint tex; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    int w,h,n; unsigned char* data = stbi_load(filename,&w,&h,&n,0);
    if(data){
        GLenum fmt = (n==4)?GL_RGBA:GL_RGB;
        glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        cout<<"Loaded texture: "<<filename<<" ("<<w<<"x"<<h<<")\n";
    }else{
        cout<<"Using fallback texture for: "<<filename<<"\n";
        unsigned char checker[] = {
            255,255,255,  80,80,80,  255,255,255,  80,80,80,
            80,80,80,    255,255,255,80,80,80,    255,255,255,
            255,255,255,  80,80,80,  255,255,255,  80,80,80,
            80,80,80,    255,255,255,80,80,80,    255,255,255
        };
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,4,4,0,GL_RGB,GL_UNSIGNED_BYTE,checker);
    }
    return tex;
}

vector<float> createTexturedSphere(float radius, vec3 color){
    vector<float> v; const int seg=30, ring=20;
    for(int i=0;i<ring;++i){
        float t1=float(i)/ring*M_PI, t2=float(i+1)/ring*M_PI;
        for(int j=0;j<seg;++j){
            float p1=float(j)/seg*2.f*M_PI, p2=float(j+1)/seg*2.f*M_PI;
            vec3 a(radius*sin(t1)*cos(p1), radius*cos(t1), radius*sin(t1)*sin(p1));
            vec3 b(radius*sin(t1)*cos(p2), radius*cos(t1), radius*sin(t1)*sin(p2));
            vec3 c(radius*sin(t2)*cos(p1), radius*cos(t2), radius*sin(t2)*sin(p1));
            vec3 d(radius*sin(t2)*cos(p2), radius*cos(t2), radius*sin(t2)*sin(p2));
            vec3 nA=normalize(a), nB=normalize(b), nC=normalize(c), nD=normalize(d);
            vec2 tA(float(j)/seg,float(i)/ring), tB(float(j+1)/seg,float(i)/ring);
            vec2 tC(float(j)/seg,float(i+1)/ring), tD(float(j+1)/seg,float(i+1)/ring);
            auto push=[&](vec3 P, vec2 UV, vec3 N){
                v.insert(v.end(), {P.x,P.y,P.z, color.x,color.y,color.z, UV.x,UV.y, N.x,N.y,N.z});
            };
            push(a,tA,nA); push(b,tB,nB); push(c,tC,nC);
            push(b,tB,nB); push(d,tD,nD); push(c,tC,nC);
        }
    }
    return v;
}
int createTexturedSphereVBO(vector<float>& s){
    GLuint VBO,VAO; glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,s.size()*sizeof(float),s.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,11*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,11*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,11*sizeof(float),(void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,11*sizeof(float),(void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
    return VAO;
}
vector<vec3> createOrbitPath(float r){
    vector<vec3> v; const int seg=100; vec3 col(0.3f);
    for(int i=0;i<=seg;++i){ float a=float(i)/seg*2.f*M_PI; vec3 p(r*cos(a),0,r*sin(a)); v.push_back(p); v.push_back(col); }
    return v;
}
int createOrbitVBO(vector<vec3>& v){
    GLuint VBO,VAO; glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(vec3),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    return VAO;
}
vector<float> createRing(float innerR,float outerR, vec3 color){
    vector<float> v; const int seg=60; vec3 n(0,1,0);
    for(int i=0;i<seg;++i){
        float a1=float(i)/seg*2.f*M_PI, a2=float(i+1)/seg*2.f*M_PI;
        vec3 v1(outerR*cos(a1),0,outerR*sin(a1)), v2(innerR*cos(a1),0,innerR*sin(a1));
        vec3 v3(outerR*cos(a2),0,outerR*sin(a2)), v4(innerR*cos(a2),0,innerR*sin(a2));
        vec2 t1(float(i)/seg,1), t2(float(i)/seg,0), t3(float(i+1)/seg,1), t4(float(i+1)/seg,0);
        auto push=[&](vec3 P, vec2 UV){ v.insert(v.end(),{P.x,P.y,P.z, color.x,color.y,color.z, UV.x,UV.y, n.x,n.y,n.z}); };
        push(v1,t1); push(v2,t2); push(v3,t3); push(v2,t2); push(v4,t4); push(v3,t3);
    }
    return v;
}

void createStarfield(int& program){
    program = linkProgram(VS_STAR, FS_STAR);
    vector<float> V; V.reserve(STAR_COUNT*7);
    random_device rd; mt19937 g(rd());
    uniform_real_distribution<> dis(-1.0,1.0), bright(0.2,1.0), colVar(0.7,1.0), type(0.0,1.0);
    for(int i=0;i<STAR_COUNT;++i){
        vec3 dir = normalize(vec3(dis(g),dis(g),dis(g)));
        vec3 pos = dir * (float)STAR_FIELD_RADIUS;
        float t = type(g); float r,gc,b;
        if(t<0.7){ r=colVar(g); gc=colVar(g); b=colVar(g)*1.1f; }
        else if(t<0.85){ r=0.9f+colVar(g)*0.1f; gc=0.7f+colVar(g)*0.3f; b=0.4f+colVar(g)*0.2f; }
        else if(t<0.95){ r=0.8f+colVar(g)*0.2f; gc=0.3f+colVar(g)*0.3f; b=0.2f+colVar(g)*0.2f; }
        else{ r=0.4f+colVar(g)*0.2f; gc=0.6f+colVar(g)*0.3f; b=0.9f+colVar(g)*0.1f; }
        float br=bright(g);
        V.insert(V.end(),{pos.x,pos.y,pos.z, r,gc,b, br});
    }
    GLuint VBO; glGenVertexArrays(1,&starfieldVAO); glGenBuffers(1,&VBO);
    glBindVertexArray(starfieldVAO); glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,V.size()*sizeof(float),V.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    starfieldVertexCount = STAR_COUNT;
}
void initShootingStars(){
    random_device rd; mt19937 gen(rd());
    uniform_real_distribution<> pos(-100.0,100.0), dir(-1.0,1.0), life(2.0,5.0), col(0.7,1.0);
    for(int i=0;i<SHOOTING_STAR_COUNT;++i){
        ShootingStar s;
        s.position=vec3(pos(gen),pos(gen),pos(gen));
        s.direction=normalize(vec3(dir(gen),dir(gen),dir(gen)));
        s.color=vec3(col(gen),col(gen)*0.6f,0.3f);
        s.speed=SHOOTING_STAR_SPEED*(0.8f+0.4f*dir(gen));
        s.maxLifetime=life(gen); s.lifetime=0.0f;
        shootingStars.push_back(s);
    }
    glGenVertexArrays(1,&shootingStarVAO); glGenBuffers(1,&shootingStarVBO);
    glBindVertexArray(shootingStarVAO); glBindBuffer(GL_ARRAY_BUFFER,shootingStarVBO);
    glBufferData(GL_ARRAY_BUFFER, SHOOTING_STAR_COUNT*2*3*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
}
void updateShootingStars(float dt){
    static float sinceSpawn=0; sinceSpawn+=dt;
    random_device rd; mt19937 gen(rd());
    uniform_real_distribution<> pos(-100.0,100.0), dir(-1.0,1.0), life(2.0,5.0), col(0.7,1.0);

    vector<float> V; V.reserve(SHOOTING_STAR_COUNT*2*3);
    shootingStars.erase(remove_if(shootingStars.begin(),shootingStars.end(),
        [](const ShootingStar& s){ return s.lifetime>=s.maxLifetime; }), shootingStars.end());

    for(auto& s: shootingStars){
        s.position += s.direction * s.speed * dt;
        s.lifetime += dt;
        vec3 tail = s.position - s.direction * SHOOTING_STAR_LENGTH;
        V.insert(V.end(),{s.position.x,s.position.y,s.position.z});
        V.insert(V.end(),{tail.x,tail.y,tail.z});
    }
    if((int)shootingStars.size()<SHOOTING_STAR_COUNT && sinceSpawn>(1.0/SHOOTING_STAR_COUNT)){
        ShootingStar n;
        n.position=vec3(pos(gen),pos(gen),pos(gen));
        n.direction=normalize(vec3(dir(gen),dir(gen),dir(gen)));
        n.color=vec3(col(gen),col(gen)*0.6f,0.3f);
        n.speed=SHOOTING_STAR_SPEED*(0.8f+0.4f*dir(gen));
        n.maxLifetime=life(gen); n.lifetime=0;
        shootingStars.push_back(n); sinceSpawn=0.0f;
    }
    shootingStarLineVertexCount = (int)V.size()/3;
    if(!V.empty()){ glBindBuffer(GL_ARRAY_BUFFER,shootingStarVBO);
        glBufferSubData(GL_ARRAY_BUFFER,0,V.size()*sizeof(float),V.data()); }
}

class Moon {
public:
    vec3 color; float radius, orbitRadius, orbitSpeed, rotationSpeed;
    float currentOrbitAngle=0.0f, currentRotationAngle=0.0f;
    GLuint VAO=0; int vertexCount=0; GLuint textureID=0;
    Moon(vec3 c,float r,float oRad,float oSpd,float rotSpd,GLuint tex):color(c),radius(r),orbitRadius(oRad),
        orbitSpeed(oSpd),rotationSpeed(rotSpd),textureID(tex){
        auto v = createTexturedSphere(radius, color);
        VAO = createTexturedSphereVBO(v); vertexCount = (int)v.size()/11;
    }
    void update(float dt){
        if(!pausedOrbits){
            currentOrbitAngle   += orbitSpeed   * dt * orbitSpeedMultiplier;
            currentRotationAngle+= rotationSpeed* dt * orbitSpeedMultiplier;
        }
    }
    mat4 getWorldMatrix(){
        mat4 orbit = rotate(mat4(1), currentOrbitAngle, vec3(0,1,0));
        mat4 trans = translate(mat4(1), vec3(orbitRadius,0,0));
        mat4 rot   = rotate(mat4(1), currentRotationAngle, vec3(0,1,0));
        return orbit*trans*rot;
    }
};

class Planet {
public:
    vec3 color; float radius, orbitRadius, orbitSpeed, rotationSpeed;
    float currentOrbitAngle=0.0f, currentRotationAngle=0.0f;
    GLuint VAO=0; int vertexCount=0; GLuint textureID=0;
    vector<Moon> moons;
    GLuint ringVAO=0; int ringVertexCount=0; GLuint ringTextureID=0; bool hasRings=false;

    Planet(vec3 c,float r,float oRad,float oSpd,float rotSpd,GLuint tex,
           bool rings=false, GLuint ringTex=0, float ringInner=0, float ringOuter=0)
        :color(c),radius(r),orbitRadius(oRad),orbitSpeed(oSpd),rotationSpeed(rotSpd),
         textureID(tex),hasRings(rings),ringTextureID(ringTex){
        auto v = createTexturedSphere(radius,color);
        VAO = createTexturedSphereVBO(v); vertexCount=(int)v.size()/11;
        if(hasRings){
            auto rv = createRing(ringInner, ringOuter, vec3(1));
            ringVAO = createTexturedSphereVBO(rv); ringVertexCount=(int)rv.size()/11;
        }
    }
    void addMoon(const Moon& m){ moons.push_back(m); }
    void update(float dt){
        if(!pausedOrbits){
            currentOrbitAngle += orbitSpeed   * dt * orbitSpeedMultiplier;
            currentRotationAngle += rotationSpeed* dt * orbitSpeedMultiplier;
        }
        for(auto& m: moons) m.update(dt);
    }
    mat4 getWorldMatrix(){
        mat4 orbit=rotate(mat4(1), currentOrbitAngle, vec3(0,1,0));
        mat4 trans=translate(mat4(1), vec3(orbitRadius,0,0));
        mat4 rot=rotate(mat4(1), currentRotationAngle, vec3(0,1,0));
        return orbit*trans*rot;
    }
};

struct ShadowMap {
    GLuint FBO=0, depthTex=0; int size=2048;
    void init(int res=2048){
        size=res; glGenFramebuffers(1,&FBO);
        glGenTextures(1,&depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,size,size,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
        float border[4]={1,1,1,1}; glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
        glBindFramebuffer(GL_FRAMEBUFFER,FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthTex,0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
} gShadow;

int selectedTarget = 2;
bool followMode = false;
bool spotlightOn = false;
bool isFullscreen = false;
int currentWindowWidth = WINDOW_WIDTH;
int currentWindowHeight = WINDOW_HEIGHT;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    currentWindowWidth = width;
    currentWindowHeight = height;
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos){
    if(followMode) return; 
    if(firstMouse){ lastX=xpos; lastY=ypos; firstMouse=false; }
    float xoff = (float)(xpos-lastX), yoff = (float)(lastY - ypos);
    lastX=xpos; lastY=ypos;
    float sens=0.1f; xoff*=sens; yoff*=sens;
    yaw += xoff; pitch += yoff;
    pitch = clamp(pitch, -89.0f, 89.0f);
    vec3 f; f.x = cos(radians(yaw))*cos(radians(pitch));
    f.y = sin(radians(pitch));
    f.z = sin(radians(yaw))*cos(radians(pitch));
    cameraFront = normalize(f);
}
void processInput(GLFWwindow *w){
    if(glfwGetKey(w,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(w,true);

    if(glfwGetKey(w,GLFW_KEY_P)==GLFW_PRESS) pausedOrbits = true;
    if(glfwGetKey(w,GLFW_KEY_O)==GLFW_PRESS) pausedOrbits = false; // hidden resume if needed
    if(glfwGetKey(w,GLFW_KEY_MINUS)==GLFW_PRESS) orbitSpeedMultiplier = std::max(0.01f, orbitSpeedMultiplier-0.2f*deltaTime*10.f);
    if(glfwGetKey(w,GLFW_KEY_EQUAL)==GLFW_PRESS) orbitSpeedMultiplier = std::min(5.0f, orbitSpeedMultiplier+0.2f*deltaTime*10.f);

    if(glfwGetKey(w,GLFW_KEY_L)==GLFW_PRESS) spotlightOn = true;
    if(glfwGetKey(w,GLFW_KEY_K)==GLFW_PRESS) spotlightOn = false;

    if(glfwGetKey(w,GLFW_KEY_F)==GLFW_PRESS) followMode = true;
    if(glfwGetKey(w,GLFW_KEY_G)==GLFW_PRESS) followMode = false;
    
    static bool f11Pressed = false;
    if(glfwGetKey(w,GLFW_KEY_F11)==GLFW_PRESS && !f11Pressed) {
        isFullscreen = !isFullscreen;
        if(isFullscreen) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(w, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(w, nullptr, 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
        }
        f11Pressed = true;
    }
    if(glfwGetKey(w,GLFW_KEY_F11)==GLFW_RELEASE) {
        f11Pressed = false;
    }

    for(int i=0;i<8;++i){
        if(glfwGetKey(w, GLFW_KEY_1 + i)==GLFW_PRESS) { selectedTarget = i; followMode = true; }
    }

    if(!followMode){
        float speedMult = (glfwGetKey(w,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS || glfwGetKey(w,GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS)? 5.0f:1.0f;
        float camSpeed = 15.0f * deltaTime * speedMult;
        if(glfwGetKey(w,GLFW_KEY_W)==GLFW_PRESS) cameraPosition += camSpeed * cameraFront;
        if(glfwGetKey(w,GLFW_KEY_S)==GLFW_PRESS) cameraPosition -= camSpeed * cameraFront;
        if(glfwGetKey(w,GLFW_KEY_A)==GLFW_PRESS) cameraPosition -= normalize(cross(cameraFront,cameraUp)) * camSpeed;
        if(glfwGetKey(w,GLFW_KEY_D)==GLFW_PRESS) cameraPosition += normalize(cross(cameraFront,cameraUp)) * camSpeed;
        if(glfwGetKey(w,GLFW_KEY_Q)==GLFW_PRESS) cameraPosition += camSpeed * cameraUp;
        if(glfwGetKey(w,GLFW_KEY_E)==GLFW_PRESS) cameraPosition -= camSpeed * cameraUp;
    }
}

int main(){
    if(!glfwInit()){ cerr<<"GLFW init fail\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(WINDOW_WIDTH,WINDOW_HEIGHT,"Enhanced Solar System (A2)",nullptr,nullptr);
    if(!win){ cerr<<"Window fail\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if(glewInit()!=GLEW_OK){ cerr<<"GLEW init fail\n"; glfwTerminate(); return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int progMain = linkProgram(VS_MAIN, FS_MAIN);
    int progStar = linkProgram(VS_STAR, FS_STAR);
    int progShoot= linkProgram(VS_SHOOT, FS_SHOOT);
    int progDepth= linkProgram(VS_DEPTH, FS_DEPTH);

    createStarfield(progStar);
    initShootingStars();

    sunTexture     = loadTexture("textures/sun.jpg");
    mercuryTexture = loadTexture("textures/mercury.jpg");
    venusTexture   = loadTexture("textures/venus.jpg");
    earthTexture   = loadTexture("textures/earth.jpg");
    marsTexture    = loadTexture("textures/mars.jpg");
    jupiterTexture = loadTexture("textures/jupiter.jpg");
    saturnTexture  = loadTexture("textures/saturn.jpg");
    uranusTexture  = loadTexture("textures/uranus.jpg");
    neptuneTexture = loadTexture("textures/neptune.jpg");
    moonTexture    = loadTexture("textures/moon.jpg");
    ringTexture    = loadTexture("textures/rings.jpg");

    vector<float> sunV = createTexturedSphere(3.0f, vec3(1.0f,0.95f,0.7f));
    GLuint sunVAO = createTexturedSphereVBO(sunV);
    int sunVerts = (int)sunV.size()/11;

    vector<Planet> planets;
    planets.emplace_back(vec3(0.7,0.4,0.2), 0.38f, 12.0f, 0.5f, 3.0f, mercuryTexture);
    planets.emplace_back(vec3(1.0,0.8,0.4), 0.95f, 19.0f, 0.4f, 2.4f, venusTexture);
    planets.emplace_back(vec3(0.2,0.6,1.0), 1.00f, 26.0f, 0.3f, 2.0f, earthTexture);
    planets.emplace_back(vec3(0.8,0.3,0.1), 0.53f, 34.0f, 0.25f,1.8f, marsTexture);
    planets.emplace_back(vec3(0.9,0.7,0.5), 2.50f, 50.0f, 0.15f,1.2f, jupiterTexture);
    planets.emplace_back(vec3(0.8,0.6,0.4), 2.00f, 70.0f, 0.10f,1.0f, saturnTexture, true, ringTexture, 2.5f, 4.0f);
    planets.emplace_back(vec3(0.6,0.8,1.0), 1.20f, 90.0f, 0.075f,0.8f, uranusTexture);
    planets.emplace_back(vec3(0.2,0.4,0.8), 1.20f,110.0f, 0.050f,0.7f, neptuneTexture);

    planets[2].addMoon(Moon(vec3(0.8),0.27f,2.0f,1.0f,2.5f,moonTexture)); // Earth moon
    planets[3].addMoon(Moon(vec3(0.6),0.15f,1.0f,1.25f,3.0f,moonTexture));
    planets[3].addMoon(Moon(vec3(0.5),0.12f,1.5f,0.875f,2.8f,moonTexture));
    planets[4].addMoon(Moon(vec3(1.0,0.9,0.7),0.29f,3.5f,0.5f,1.5f,moonTexture));
    planets[4].addMoon(Moon(vec3(0.8,0.8,0.9),0.25f,4.5f,0.375f,1.2f,moonTexture));
    planets[4].addMoon(Moon(vec3(0.7),0.42f,5.5f,0.3f,1.0f,moonTexture));
    planets[4].addMoon(Moon(vec3(0.6),0.38f,7.0f,0.2f,0.8f,moonTexture));

    vector<GLuint> orbitVAOs; vector<int> orbitCounts;
    for(auto& p: planets){
        auto ov = createOrbitPath(p.orbitRadius);
        orbitVAOs.push_back(createOrbitVBO(ov));
        orbitCounts.push_back((int)ov.size()/2);
    }

    gShadow.init(2048);

    mat4 P = perspective(radians(45.0f),(float)currentWindowWidth/(float)currentWindowHeight,0.1f,5000.0f);
    setProjectionMatrix(progMain,P);
    setProjectionMatrix(progStar,P);
    setProjectionMatrix(progShoot,P);

    Mesh ship;
    if(!loadOBJ("models/spacecraft.obj", ship, vec3(0.85f,0.9f,1.0f)))
        ship = makeFallbackShip();
    vec3 shipPos = vec3(0.0f, 3.0f, 30.0f);
    float shipYaw = 0.0f;

    while(!glfwWindowShouldClose(win)){
        float t = (float)glfwGetTime();
        deltaTime = t - lastFrame; lastFrame = t;

        processInput(win);
        updateShootingStars(deltaTime);
        
        static int lastWidth = currentWindowWidth;
        static int lastHeight = currentWindowHeight;
        if(lastWidth != currentWindowWidth || lastHeight != currentWindowHeight) {
            P = perspective(radians(45.0f),(float)currentWindowWidth/(float)currentWindowHeight,0.1f,5000.0f);
            setProjectionMatrix(progMain,P);
            setProjectionMatrix(progStar,P);
            setProjectionMatrix(progShoot,P);
            lastWidth = currentWindowWidth;
            lastHeight = currentWindowHeight;
        }

        mat4 V;
        if(followMode){
            vec3 targetPos = vec3(0);
            if(selectedTarget>=0 && selectedTarget<(int)planets.size()){
                mat4 M = planets[selectedTarget].getWorldMatrix();
                targetPos = vec3(M[3]); 
            }
            float camDist = std::max(15.0f, planets[std::max(0,std::min(selectedTarget,(int)planets.size()-1))].radius*18.0f+20.0f);
            vec3 orbitOff = vec3(cos(t*0.2f)*camDist*1.2f, camDist*0.35f, sin(t*0.2f)*camDist*1.2f);
            cameraPosition = mix(cameraPosition, targetPos + orbitOff, 0.08f);
            cameraFront = normalize(targetPos - cameraPosition);
            V = lookAt(cameraPosition, cameraPosition + cameraFront, vec3(0,1,0));
        }else{
            V = lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
        }

        vec3 lightDir = normalize(vec3(sin(t*0.3f), 0.75f, cos(t*0.3f)));
        vec3 lightColor = vec3(1.0f,1.0f,0.9f);

        mat4 Lview = lookAt(-lightDir*160.0f, vec3(0), vec3(0,1,0));
        mat4 Lproj = ortho(-150.f,150.f,-150.f,150.f, 1.f, 400.f);
        mat4 Lspace = Lproj * Lview;

        glViewport(0,0,gShadow.size,gShadow.size);
        glBindFramebuffer(GL_FRAMEBUFFER,gShadow.FBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT); 

        glUseProgram(progDepth);
        glUniformMatrix4fv(glGetUniformLocation(progDepth,"lightSpaceMatrix"),1,GL_FALSE,&Lspace[0][0]);

        auto drawDepthSphereVAO = [&](GLuint vao, int count, const mat4& M){
            glUniformMatrix4fv(glGetUniformLocation(progDepth,"worldMatrix"),1,GL_FALSE,&M[0][0]);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES,0,count);
        };
        auto drawDepthMesh = [&](Mesh& m, const mat4& M){
            glUniformMatrix4fv(glGetUniformLocation(progDepth,"worldMatrix"),1,GL_FALSE,&M[0][0]);
            m.drawElements();
        };

        for(auto& p: planets){
            mat4 Mp = p.getWorldMatrix();
            drawDepthSphereVAO(p.VAO, p.vertexCount, Mp);
            if(p.hasRings){
                drawDepthSphereVAO(p.ringVAO, p.ringVertexCount, Mp);
            }
            for(auto& m: p.moons){
                mat4 Mm = Mp * m.getWorldMatrix();
                drawDepthSphereVAO(m.VAO, m.vertexCount, Mm);
            }
        }
        {
            mat4 M = translate(mat4(1), shipPos) * rotate(mat4(1), shipYaw, vec3(0,1,0)) * scale(mat4(1), vec3(2.0f));
            drawDepthMesh(ship, M);
        }

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER,0);

        glViewport(0,0,currentWindowWidth,currentWindowHeight);
        glClearColor(0.0f,0.0f,0.05f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(progStar);
        setViewMatrix(progStar, V);
        setProjectionMatrix(progStar, P);
        glBindVertexArray(starfieldVAO);
        glDrawArrays(GL_POINTS,0,starfieldVertexCount);

        glUseProgram(progShoot);
        setViewMatrix(progShoot, V);
        setProjectionMatrix(progShoot, P);
        glBindVertexArray(shootingStarVAO);
        glUniform3f(glGetUniformLocation(progShoot,"starColor"), 1.0f, 0.85f, 0.5f);
        glUniform1f(glGetUniformLocation(progShoot,"alpha"), 0.85f);
        glDrawArrays(GL_LINES, 0, shootingStarLineVertexCount);


        glUseProgram(progMain);
        setViewMatrix(progMain, V);
        setProjectionMatrix(progMain, P);

        glUniform3fv(glGetUniformLocation(progMain,"viewPos"),1,&cameraPosition[0]);
        glUniform3fv(glGetUniformLocation(progMain,"lightDir"),1,&lightDir[0]);
        glUniform3fv(glGetUniformLocation(progMain,"lightColor"),1,&lightColor[0]);
        glUniform1i(glGetUniformLocation(progMain,"useShadows"), 1);
        glUniformMatrix4fv(glGetUniformLocation(progMain,"lightSpaceMatrix"),1,GL_FALSE,&Lspace[0][0]);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, gShadow.depthTex);
        glUniform1i(glGetUniformLocation(progMain,"shadowMap"),5);

        setWorldMatrix(progMain, mat4(1));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sunTexture);
        glUniform1i(glGetUniformLocation(progMain,"texture1"), 0);
        glUniform1i(glGetUniformLocation(progMain,"useTexture"), 1);
        glUniform1i(glGetUniformLocation(progMain,"isSun"), 1);
        glUniform1i(glGetUniformLocation(progMain,"useLighting"), 0);
        glUniform3f(glGetUniformLocation(progMain,"Ka"),0.0,0.0,0.0);
        glUniform3f(glGetUniformLocation(progMain,"Kd"),1.0,1.0,1.0);
        glUniform3f(glGetUniformLocation(progMain,"Ks"),0.0,0.0,0.0);
        glUniform1f(glGetUniformLocation(progMain,"shininess"),16.0f);
        glBindVertexArray(sunVAO);
        glDrawArrays(GL_TRIANGLES,0,sunVerts);

        glLineWidth(1.0f);
        glUniform1i(glGetUniformLocation(progMain,"useTexture"), 0);
        glUniform1i(glGetUniformLocation(progMain,"useLighting"),0);
        glUniform1i(glGetUniformLocation(progMain,"isSun"),0);
        for(size_t i=0;i<orbitVAOs.size();++i){
            setWorldMatrix(progMain, mat4(1));
            glBindVertexArray(orbitVAOs[i]);
            glDrawArrays(GL_LINE_LOOP,0,orbitCounts[i]);
        }

        glUniform1i(glGetUniformLocation(progMain,"useLighting"),1);
        glUniform3f(glGetUniformLocation(progMain,"Ka"),0.05f,0.05f,0.05f);
        glUniform3f(glGetUniformLocation(progMain,"Kd"),0.9f,0.9f,0.9f);
        glUniform3f(glGetUniformLocation(progMain,"Ks"),0.2f,0.2f,0.2f);
        glUniform1f(glGetUniformLocation(progMain,"shininess"),32.0f);

        for(auto& p: planets){
            p.update(deltaTime);

            mat4 Mp = p.getWorldMatrix();
            setWorldMatrix(progMain, Mp);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p.textureID);
            glUniform1i(glGetUniformLocation(progMain,"texture1"),0);
            glUniform1i(glGetUniformLocation(progMain,"useTexture"),1);
            glUniform1i(glGetUniformLocation(progMain,"isSun"),0);

            glBindVertexArray(p.VAO);
            glDrawArrays(GL_TRIANGLES,0,p.vertexCount);

            if(p.hasRings){
                setWorldMatrix(progMain, Mp);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, p.ringTextureID);
                glUniform1i(glGetUniformLocation(progMain,"texture1"),0);
                glUniform1i(glGetUniformLocation(progMain,"useTexture"),1);
                glBindVertexArray(p.ringVAO);
                glDrawArrays(GL_TRIANGLES,0,p.ringVertexCount);
            }

            for(auto& m: p.moons){
                mat4 Mm = Mp * m.getWorldMatrix();
                setWorldMatrix(progMain, Mm);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m.textureID);
                glUniform1i(glGetUniformLocation(progMain,"texture1"),0);
                glUniform1i(glGetUniformLocation(progMain,"useTexture"),1);
                glUniform1i(glGetUniformLocation(progMain,"isSun"),0);
                glBindVertexArray(m.VAO);
                glDrawArrays(GL_TRIANGLES,0,m.vertexCount);
            }
        }

        {
            mat4 M = translate(mat4(1), shipPos) * rotate(mat4(1), shipYaw, vec3(0,1,0)) * scale(mat4(1), vec3(2.0f));
            setWorldMatrix(progMain, M);
            glUniform1i(glGetUniformLocation(progMain,"useTexture"),0);
            glUniform3f(glGetUniformLocation(progMain,"Ka"),0.08f,0.08f,0.10f);
            glUniform3f(glGetUniformLocation(progMain,"Kd"),0.95f,0.95f,1.0f);
            glUniform3f(glGetUniformLocation(progMain,"Ks"),0.6f,0.6f,0.8f);
            glUniform1f(glGetUniformLocation(progMain,"shininess"),64.0f);
            ship.drawElements();

            shipYaw += 0.2f*deltaTime;
            shipPos += vec3(0.0f, 0.0f, -5.0f*deltaTime);
            if(shipPos.z < -120.0f) shipPos.z = 120.0f;
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}