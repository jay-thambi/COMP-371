#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <random>

#define GLEW_STATIC 1   
#include <GL/glew.h>    

#include <GLFW/glfw3.h> 

#include <glm/glm.hpp>  
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/common.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>

using namespace glm;
using namespace std;

const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;
const int STAR_COUNT = 50000;
const float STAR_FIELD_RADIUS = 4000.0f;
vec3 cameraPosition = vec3(0.0f, 10.0f, 40.0f);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
bool firstMouse = true;
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;
const int SHOOTING_STAR_COUNT = 20;  
const float SHOOTING_STAR_SPEED = 50.0f;  
const float SHOOTING_STAR_LENGTH = 5.0f;  

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float orbitSpeedMultiplier = 1.0f;

GLuint sunTexture, mercuryTexture, venusTexture, earthTexture, marsTexture, jupiterTexture;
GLuint saturnTexture, uranusTexture, neptuneTexture;
GLuint moonTexture;
GLuint ringTexture;

GLuint starfieldVAO;
int starfieldVertexCount;

GLuint shootingStarVAO, shootingStarVBO;

struct ShootingStar {
    vec3 position;
    vec3 direction;
    vec3 color;
    float speed;
    float lifetime;
    float maxLifetime;
};
vector<ShootingStar> shootingStars;

const char* getVertexShaderSource()
{
    return
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "layout (location = 2) in vec2 aTexCoord;\n"
        "layout (location = 3) in vec3 aNormal;\n"
        "\n"
        "uniform mat4 worldMatrix;\n"
        "uniform mat4 viewMatrix = mat4(1.0);\n"
        "uniform mat4 projectionMatrix = mat4(1.0);\n"
        "\n"
        "out vec3 vertexColor;\n"
        "out vec2 TexCoord;\n"
        "out vec3 FragPos;\n"
        "out vec3 Normal;\n"
        "\n"
        "void main()\n"
        "{\n"
        "   vertexColor = aColor;\n"
        "   TexCoord = aTexCoord;\n"
        "   FragPos = vec3(worldMatrix * vec4(aPos, 1.0));\n"
        "   Normal = mat3(transpose(inverse(worldMatrix))) * aNormal;\n"
        "   mat4 modelViewProjection = projectionMatrix * viewMatrix * worldMatrix;\n"
        "   gl_Position = modelViewProjection * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "}";
}

const char* getFragmentShaderSource()
{
    return
        "#version 330 core\n"
        "in vec3 vertexColor;\n"
        "in vec2 TexCoord;\n"
        "in vec3 FragPos;\n"
        "in vec3 Normal;\n"
        "out vec4 FragColor;\n"
        "\n"
        "uniform sampler2D texture1;\n"
        "uniform bool useTexture;\n"
        "uniform bool useLighting;\n"
        "uniform bool isSun;\n"
        "uniform vec3 lightPos;\n"
        "uniform vec3 lightColor;\n"
        "uniform vec3 viewPos;\n"
        "\n"
        "void main()\n"
        "{\n"
        "   vec3 color;\n"
        "   if (useTexture) {\n"
        "       vec4 texColor = texture(texture1, TexCoord);\n"
        "       color = mix(texColor.rgb, vertexColor, 0.3);\n"
        "   } else {\n"
        "       color = vertexColor;\n"
        "   }\n"
        "\n"
        "   if (isSun) {\n"
        "       float glow = 1.5 + 0.3 * sin(gl_FragCoord.x * 0.01) * cos(gl_FragCoord.y * 0.01);\n"
        "       color = color * glow;\n"
        "       color = mix(color, vec3(1.0, 0.9, 0.6), 0.3);\n"
        "       FragColor = vec4(color, 1.0);\n"
        "       return;\n"
        "   }\n"
        "\n"
        "   if (useLighting) {\n"
        "       vec3 ambient = 0.15 * lightColor;\n"
        "       vec3 norm = normalize(Normal);\n"
        "       vec3 lightDir = normalize(lightPos - FragPos);\n"
        "       float diff = max(dot(norm, lightDir), 0.0);\n"
        "       vec3 diffuse = diff * lightColor;\n"
        "       vec3 viewDir = normalize(viewPos - FragPos);\n"
        "       vec3 reflectDir = reflect(-lightDir, norm);\n"
        "       float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "       vec3 specular = spec * lightColor * 0.3;\n"
        "       vec3 lighting = ambient + diffuse + specular;\n"
        "       color = color * lighting;\n"
        "   }\n"
        "\n"
        "   FragColor = vec4(color, 1.0);\n"
        "}";
}

const char* getStarfieldVertexShaderSource()
{
    return
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "layout (location = 2) in float aBrightness;\n"
        "\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 projectionMatrix;\n"
        "\n"
        "out vec3 starColor;\n"
        "out float starBrightness;\n"
        "\n"
        "void main()\n"
        "{\n"
        "   starColor = aColor;\n"
        "   starBrightness = aBrightness;\n"
        "   gl_Position = projectionMatrix * viewMatrix * vec4(aPos, 1.0);\n"
        "   gl_PointSize = 1.5 + 3.0 * aBrightness;\n"  
        "}";
}

const char* getStarfieldFragmentShaderSource()
{
    return
        "#version 330 core\n"
        "in vec3 starColor;\n"
        "in float starBrightness;\n"
        "out vec4 FragColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "   // Create circular star points with soft edges\n"
        "   vec2 coord = gl_PointCoord - vec2(0.5);\n"
        "   float dist = length(coord);\n"
        "   if(dist > 0.5) discard;\n"
        "   float alpha = 1.0 - smoothstep(0.0, 0.5, dist);\n"
        "   // Make stars more visible with enhanced brightness\n"
        "   vec3 finalColor = starColor * starBrightness * 1.2;\n"
        "   FragColor = vec4(finalColor, alpha * starBrightness);\n"
        "}";
}

const char* getShootingStarVertexShader() {
    return
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 projectionMatrix;\n"
        "void main() {\n"
        "   gl_Position = projectionMatrix * viewMatrix * vec4(aPos, 1.0);\n"
        "}";
}

const char* getShootingStarFragmentShader() {
    return
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 starColor;\n"
        "uniform float alpha;\n"
        "void main() {\n"
        "   FragColor = vec4(starColor, alpha);\n"
        "}";
}

int compileShader(GLenum type, const char* source)
{
    int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    return shader;
}

int compileAndLinkShaders(const char* vertexSource, const char* fragmentSource)
{
    int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}

void createStarfield(int& shaderProgram)
{
    shaderProgram = compileAndLinkShaders(getStarfieldVertexShaderSource(), getStarfieldFragmentShaderSource());
    
    vector<float> starVertices;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    std::uniform_real_distribution<> brightness(0.2, 1.0);  
    std::uniform_real_distribution<> colorVariation(0.7, 1.0); 
    std::uniform_real_distribution<> starType(0.0, 1.0);  
    
    for (int i = 0; i < STAR_COUNT; ++i)
    {
        vec3 direction = normalize(vec3(dis(gen), dis(gen), dis(gen)));
        vec3 position = direction * STAR_FIELD_RADIUS;
        
        float starTypeVal = starType(gen);
        float r, g, b;
        
        if (starTypeVal < 0.7f) {
            r = colorVariation(gen);
            g = colorVariation(gen);
            b = colorVariation(gen) * 1.1f;
        } else if (starTypeVal < 0.85f) {
            r = 0.9f + colorVariation(gen) * 0.1f;
            g = 0.7f + colorVariation(gen) * 0.3f;
            b = 0.4f + colorVariation(gen) * 0.2f;
        } else if (starTypeVal < 0.95f) {
            r = 0.8f + colorVariation(gen) * 0.2f;
            g = 0.3f + colorVariation(gen) * 0.3f;
            b = 0.2f + colorVariation(gen) * 0.2f;
        } else {
            r = 0.4f + colorVariation(gen) * 0.2f;
            g = 0.6f + colorVariation(gen) * 0.3f;
            b = 0.9f + colorVariation(gen) * 0.1f;
        }
        
        float bright = brightness(gen);
        
        starVertices.push_back(position.x);
        starVertices.push_back(position.y);
        starVertices.push_back(position.z);
        
        starVertices.push_back(r);
        starVertices.push_back(g);
        starVertices.push_back(b);
        
        starVertices.push_back(bright);
    }
    
    GLuint VBO;
    glGenVertexArrays(1, &starfieldVAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(starfieldVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, starVertices.size() * sizeof(float), starVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    starfieldVertexCount = STAR_COUNT;
}

void initShootingStars() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> posDist(-100.0f, 100.0f);
    std::uniform_real_distribution<> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<> lifeDist(2.0f, 5.0f);
    std::uniform_real_distribution<> colorDist(0.7f, 1.0f);

    for (int i = 0; i < SHOOTING_STAR_COUNT; ++i) {
        ShootingStar star;
        star.position = vec3(posDist(gen), posDist(gen), posDist(gen));
        star.direction = normalize(vec3(dirDist(gen), dirDist(gen), dirDist(gen)));
        star.color = vec3(colorDist(gen), colorDist(gen)*0.6f, 0.3f);  // Orange/white
        star.speed = SHOOTING_STAR_SPEED * (0.8f + 0.4f * dirDist(gen));
        star.maxLifetime = lifeDist(gen);
        star.lifetime = 0.0f;
        shootingStars.push_back(star);
    }

    glGenVertexArrays(1, &shootingStarVAO);
    glGenBuffers(1, &shootingStarVBO);
    glBindVertexArray(shootingStarVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shootingStarVBO);

    glBufferData(GL_ARRAY_BUFFER, SHOOTING_STAR_COUNT * 2 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

GLuint loadTexture(const char* filename) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int width, height, nrChannels;
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        std::cout << "Loaded texture: " << filename << " (" << width << "x" << height << ")" << std::endl;
    } else {
        std::cout << "Using fallback texture for: " << filename << std::endl;
        unsigned char defaultTexture[] = {
            255, 255, 255, 128, 128, 128, 255, 255, 255, 128, 128, 128,
            128, 128, 128, 255, 255, 255, 128, 128, 128, 255, 255, 255,
            255, 255, 255, 128, 128, 128, 255, 255, 255, 128, 128, 128,
            128, 128, 128, 255, 255, 255, 128, 128, 128, 255, 255, 255
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, defaultTexture);
    }
    
    return textureID;
}

vector<float> createTexturedSphere(float radius, vec3 color)
{
    vector<float> vertices;
    const int segments = 30;
    const int rings = 20;
    
    for (int i = 0; i < rings; ++i) {
        float theta1 = ((float)i / rings) * M_PI;
        float theta2 = ((float)(i + 1) / rings) * M_PI;
        
        for (int j = 0; j < segments; ++j) {
            float phi1 = ((float)j / segments) * 2.0f * M_PI;
            float phi2 = ((float)(j + 1) / segments) * 2.0f * M_PI;
            
            vec3 v1(radius * sin(theta1) * cos(phi1),
                    radius * cos(theta1),
                    radius * sin(theta1) * sin(phi1));
            vec3 v2(radius * sin(theta1) * cos(phi2),
                    radius * cos(theta1),
                    radius * sin(theta1) * sin(phi2));
            vec3 v3(radius * sin(theta2) * cos(phi1),
                    radius * cos(theta2),
                    radius * sin(theta2) * sin(phi1));
            vec3 v4(radius * sin(theta2) * cos(phi2),
                    radius * cos(theta2),
                    radius * sin(theta2) * sin(phi2));
            
            vec3 n1 = normalize(v1);
            vec3 n2 = normalize(v2);
            vec3 n3 = normalize(v3);
            vec3 n4 = normalize(v4);
            
            vec2 t1((float)j / segments, (float)i / rings);
            vec2 t2((float)(j + 1) / segments, (float)i / rings);
            vec2 t3((float)j / segments, (float)(i + 1) / rings);
            vec2 t4((float)(j + 1) / segments, (float)(i + 1) / rings);
            
            // First triangle
            vertices.insert(vertices.end(), {v1.x, v1.y, v1.z, color.x, color.y, color.z, t1.x, t1.y, n1.x, n1.y, n1.z});
            vertices.insert(vertices.end(), {v2.x, v2.y, v2.z, color.x, color.y, color.z, t2.x, t2.y, n2.x, n2.y, n2.z});
            vertices.insert(vertices.end(), {v3.x, v3.y, v3.z, color.x, color.y, color.z, t3.x, t3.y, n3.x, n3.y, n3.z});
            
            // Second triangle
            vertices.insert(vertices.end(), {v2.x, v2.y, v2.z, color.x, color.y, color.z, t2.x, t2.y, n2.x, n2.y, n2.z});
            vertices.insert(vertices.end(), {v4.x, v4.y, v4.z, color.x, color.y, color.z, t4.x, t4.y, n4.x, n4.y, n4.z});
            vertices.insert(vertices.end(), {v3.x, v3.y, v3.z, color.x, color.y, color.z, t3.x, t3.y, n3.x, n3.y, n3.z});
        }
    }
    
    return vertices;
}

int createTexturedSphereVBO(vector<float>& sphereVertices)
{
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    return VAO;
}

vector<vec3> createOrbitPath(float radius)
{
    vector<vec3> vertices;
    const int segments = 100;
    vec3 color(0.3f, 0.3f, 0.3f);
    
    for (int i = 0; i <= segments; ++i) {
        float angle = ((float)i / segments) * 2.0f * M_PI;
        vec3 position(radius * cos(angle), 0.0f, radius * sin(angle));
        vertices.push_back(position);
        vertices.push_back(color);
    }
    
    return vertices;
}

int createOrbitVBO(vector<vec3>& orbitVertices)
{
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, orbitVertices.size() * sizeof(vec3), orbitVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    return VAO;
}

vector<float> createRing(float innerRadius, float outerRadius, vec3 color) {
    vector<float> vertices;
    const int segments = 60;
    
    for (int i = 0; i < segments; ++i) {
        float angle1 = ((float)i / segments) * 2.0f * M_PI;
        float angle2 = ((float)(i + 1) / segments) * 2.0f * M_PI;
        
        vec3 v1(outerRadius * cos(angle1), 0.0f, outerRadius * sin(angle1));
        vec3 v2(innerRadius * cos(angle1), 0.0f, innerRadius * sin(angle1));
        vec3 v3(outerRadius * cos(angle2), 0.0f, outerRadius * sin(angle2));
        vec3 v4(innerRadius * cos(angle2), 0.0f, innerRadius * sin(angle2));
        
        vec3 normal(0.0f, 1.0f, 0.0f);
        
        vec2 t1((float)i / segments, 1.0f);
        vec2 t2((float)i / segments, 0.0f);
        vec2 t3((float)(i + 1) / segments, 1.0f);
        vec2 t4((float)(i + 1) / segments, 0.0f);
        
        // First triangle
        vertices.insert(vertices.end(), {v1.x, v1.y, v1.z, color.x, color.y, color.z, t1.x, t1.y, normal.x, normal.y, normal.z});
        vertices.insert(vertices.end(), {v2.x, v2.y, v2.z, color.x, color.y, color.z, t2.x, t2.y, normal.x, normal.y, normal.z});
        vertices.insert(vertices.end(), {v3.x, v3.y, v3.z, color.x, color.y, color.z, t3.x, t3.y, normal.x, normal.y, normal.z});
        
        // Second triangle
        vertices.insert(vertices.end(), {v2.x, v2.y, v2.z, color.x, color.y, color.z, t2.x, t2.y, normal.x, normal.y, normal.z});
        vertices.insert(vertices.end(), {v4.x, v4.y, v4.z, color.x, color.y, color.z, t4.x, t4.y, normal.x, normal.y, normal.z});
        vertices.insert(vertices.end(), {v3.x, v3.y, v3.z, color.x, color.y, color.z, t3.x, t3.y, normal.x, normal.y, normal.z});
    }
    
    return vertices;
}

void setProjectionMatrix(int shaderProgram, mat4 projectionMatrix)
{
    glUseProgram(shaderProgram);
    GLuint projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix");
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &projectionMatrix[0][0]);
}

void setViewMatrix(int shaderProgram, mat4 viewMatrix)
{
    glUseProgram(shaderProgram);
    GLuint viewMatrixLocation = glGetUniformLocation(shaderProgram, "viewMatrix");
    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
}

void setWorldMatrix(int shaderProgram, mat4 worldMatrix)
{
    glUseProgram(shaderProgram);
    GLuint worldMatrixLocation = glGetUniformLocation(shaderProgram, "worldMatrix");
    glUniformMatrix4fv(worldMatrixLocation, 1, GL_FALSE, &worldMatrix[0][0]);
}

void renderShootingStars(int shaderProgram, mat4 viewMatrix, mat4 projectionMatrix) {
    glUseProgram(shaderProgram);
    setViewMatrix(shaderProgram, viewMatrix);
    setProjectionMatrix(shaderProgram, projectionMatrix);

    glBegin(GL_LINES);
    for (const auto& star : shootingStars) {
        if (star.lifetime >= star.maxLifetime) continue;
        
        float alpha = 1.0f - (star.lifetime / star.maxLifetime);
        vec3 endPos = star.position - star.direction * SHOOTING_STAR_LENGTH;
        
        glColor4f(star.color.r, star.color.g, star.color.b, alpha);
        glVertex3f(star.position.x, star.position.y, star.position.z);
        glVertex3f(endPos.x, endPos.y, endPos.z);
    }
    glEnd();
}

class Moon {
public:
    vec3 color;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    float rotationSpeed;
    float currentOrbitAngle;
    float currentRotationAngle;
    GLuint VAO;
    int vertexCount;
    GLuint textureID;
    
    Moon(vec3 c, float r, float orbitRad, float orbitSpd, float rotSpd, GLuint texID) 
        : color(c), radius(r), orbitRadius(orbitRad), orbitSpeed(orbitSpd), rotationSpeed(rotSpd), 
          currentOrbitAngle(0.0f), currentRotationAngle(0.0f), textureID(texID) {
        
        vector<float> vertices = createTexturedSphere(radius, color);
        VAO = createTexturedSphereVBO(vertices);
        vertexCount = vertices.size() / 11;
    }
    
    void update(float deltaTime) {
        currentOrbitAngle += orbitSpeed * deltaTime * orbitSpeedMultiplier;
        currentRotationAngle += rotationSpeed * deltaTime * orbitSpeedMultiplier;
    }
    
    mat4 getWorldMatrix() {
        mat4 orbitMatrix = rotate(mat4(1.0f), currentOrbitAngle, vec3(0.0f, 1.0f, 0.0f));
        mat4 translateMatrix = translate(mat4(1.0f), vec3(orbitRadius, 0.0f, 0.0f));
        mat4 rotationMatrix = rotate(mat4(1.0f), currentRotationAngle, vec3(0.0f, 1.0f, 0.0f));
        
        return orbitMatrix * translateMatrix * rotationMatrix;
    }
};

class Planet {
public:
    vec3 color;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    float rotationSpeed;
    float currentOrbitAngle;
    float currentRotationAngle;
    GLuint VAO;
    int vertexCount;
    GLuint textureID;
    vector<Moon> moons;
    GLuint ringVAO;
    int ringVertexCount;
    GLuint ringTextureID;
    bool hasRings;
    
    Planet(vec3 c, float r, float orbitRad, float orbitSpd, float rotSpd, GLuint texID, 
           bool rings = false, GLuint ringTexID = 0, float ringInner = 0.0f, float ringOuter = 0.0f) 
        : color(c), radius(r), orbitRadius(orbitRad), orbitSpeed(orbitSpd), rotationSpeed(rotSpd), 
          currentOrbitAngle(0.0f), currentRotationAngle(0.0f), textureID(texID),
          hasRings(rings), ringTextureID(ringTexID) {
        
        vector<float> vertices = createTexturedSphere(radius, color);
        VAO = createTexturedSphereVBO(vertices);
        vertexCount = vertices.size() / 11;
        
        if (hasRings) {
            vector<float> ringVertices = createRing(ringInner, ringOuter, vec3(1.0f));
            ringVAO = createTexturedSphereVBO(ringVertices);
            ringVertexCount = ringVertices.size() / 11;
        }
    }
    
    void addMoon(const Moon& moon) {
        moons.push_back(moon);
    }
    
    void update(float deltaTime) {
        currentOrbitAngle += orbitSpeed * deltaTime * orbitSpeedMultiplier;
        currentRotationAngle += rotationSpeed * deltaTime * orbitSpeedMultiplier;
        
        for (auto& moon : moons) {
            moon.update(deltaTime);
        }
    }
    
    mat4 getWorldMatrix() {
        mat4 orbitMatrix = rotate(mat4(1.0f), currentOrbitAngle, vec3(0.0f, 1.0f, 0.0f));
        mat4 translateMatrix = translate(mat4(1.0f), vec3(orbitRadius, 0.0f, 0.0f));
        mat4 rotationMatrix = rotate(mat4(1.0f), currentRotationAngle, vec3(0.0f, 1.0f, 0.0f));
        
        return orbitMatrix * translateMatrix * rotationMatrix;
    }
    
    void draw(int shaderProgram) {
        mat4 planetMatrix = getWorldMatrix();
        setWorldMatrix(shaderProgram, planetMatrix);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "useLighting"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "isSun"), 0);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        
                if (hasRings) {
            setWorldMatrix(shaderProgram, planetMatrix);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ringTextureID);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
            glUniform1i(glGetUniformLocation(shaderProgram, "useLighting"), 1);
            glBindVertexArray(ringVAO);
            glDrawArrays(GL_TRIANGLES, 0, ringVertexCount);
        }

        for (auto& moon : moons) {
            mat4 moonMatrix = planetMatrix * moon.getWorldMatrix();
            setWorldMatrix(shaderProgram, moonMatrix);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, moon.textureID);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
            glUniform1i(glGetUniformLocation(shaderProgram, "useLighting"), 1);
            glUniform1i(glGetUniformLocation(shaderProgram, "isSun"), 0);
            
            glBindVertexArray(moon.VAO);
            glDrawArrays(GL_TRIANGLES, 0, moon.vertexCount);
        }
    }
};

void updateShootingStars(float deltaTime) {
    static float timeSinceLastSpawn = 0.0f;
    timeSinceLastSpawn += deltaTime;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> posDist(-100.0f, 100.0f);
    std::uniform_real_distribution<> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<> lifeDist(2.0f, 5.0f);
    std::uniform_real_distribution<> colorDist(0.7f, 1.0f);

    vector<float> vertices;
    
    shootingStars.erase(
        std::remove_if(shootingStars.begin(), shootingStars.end(),
            [](const ShootingStar& star) { return star.lifetime >= star.maxLifetime; }),
        shootingStars.end());

    for (auto& star : shootingStars) {
        star.position += star.direction * star.speed * deltaTime;
        star.lifetime += deltaTime;
        
        vec3 endPos = star.position - star.direction * SHOOTING_STAR_LENGTH;
        vertices.insert(vertices.end(), {star.position.x, star.position.y, star.position.z});
        vertices.insert(vertices.end(), {endPos.x, endPos.y, endPos.z});
    }

    if (shootingStars.size() < SHOOTING_STAR_COUNT && 
        timeSinceLastSpawn > (1.0f/SHOOTING_STAR_COUNT)) {
        
        ShootingStar newStar;
        newStar.position = vec3(posDist(gen), posDist(gen), posDist(gen));
        newStar.direction = normalize(vec3(dirDist(gen), dirDist(gen), dirDist(gen)));
        newStar.color = vec3(colorDist(gen), colorDist(gen)*0.6f, 0.3f);
        newStar.speed = SHOOTING_STAR_SPEED * (0.8f + 0.4f * dirDist(gen));
        newStar.maxLifetime = lifeDist(gen);
        newStar.lifetime = 0;
        shootingStars.push_back(newStar);
        timeSinceLastSpawn = 0.0f;
    }

    if (!vertices.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, shootingStarVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    vec3 front;
    front.x = cos(radians(yaw)) * cos(radians(pitch));
    front.y = sin(radians(pitch));
    front.z = sin(radians(yaw)) * cos(radians(pitch));
    cameraFront = normalize(front);
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float speedMultiplier = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        speedMultiplier = 5.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || 
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        orbitSpeedMultiplier = 0.1f;
    }
    else {
        orbitSpeedMultiplier = 1.0f;
    }
    
    float cameraSpeed = 15.0f * deltaTime * speedMultiplier;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPosition += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPosition -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPosition -= normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPosition += normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPosition += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPosition -= cameraSpeed * cameraUp;
}

int main(int argc, char*argv[])
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Enhanced Solar System", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    int shaderProgram = compileAndLinkShaders(getVertexShaderSource(), getFragmentShaderSource());
    int starfieldShaderProgram;
    createStarfield(starfieldShaderProgram);
    initShootingStars();
    int shootingStarShader = compileAndLinkShaders(getShootingStarVertexShader(), getShootingStarFragmentShader());
    
    sunTexture = loadTexture("textures/sun.jpg");
    mercuryTexture = loadTexture("textures/mercury.jpg");
    venusTexture = loadTexture("textures/venus.jpg");
    earthTexture = loadTexture("textures/earth.jpg");
    marsTexture = loadTexture("textures/mars.jpg");
    jupiterTexture = loadTexture("textures/jupiter.jpg");
    saturnTexture = loadTexture("textures/saturn.jpg");
    uranusTexture = loadTexture("textures/uranus.jpg");
    neptuneTexture = loadTexture("textures/neptune.jpg");
    moonTexture = loadTexture("textures/moon.jpg");
    ringTexture = loadTexture("textures/rings.jpg");
    
    vector<float> sunVertices = createTexturedSphere(3.0f, vec3(1.0f, 0.95f, 0.7f));
    GLuint sunVAO = createTexturedSphereVBO(sunVertices);
    int sunVertexCount = sunVertices.size() / 11;
    
    vector<Planet> planets;
    planets.push_back(Planet(vec3(0.7f, 0.4f, 0.2f), 0.38f, 12.0f, 0.5f, 3.0f, mercuryTexture));    // Mercury
    planets.push_back(Planet(vec3(1.0f, 0.8f, 0.4f), 0.95f, 19.0f, 0.4f, 2.4f, venusTexture));     // Venus
    planets.push_back(Planet(vec3(0.2f, 0.6f, 1.0f), 1.0f, 26.0f, 0.3f, 2.0f, earthTexture));      // Earth
    planets.push_back(Planet(vec3(0.8f, 0.3f, 0.1f), 0.53f, 34.0f, 0.25f, 1.8f, marsTexture));     // Mars
    planets.push_back(Planet(vec3(0.9f, 0.7f, 0.5f), 2.5f, 50.0f, 0.15f, 1.2f, jupiterTexture));   // Jupiter
    planets.push_back(Planet(vec3(0.8f, 0.6f, 0.4f), 2.0f, 70.0f, 0.1f, 1.0f, saturnTexture, 
                       true, ringTexture, 2.5f, 4.0f));   
    planets.push_back(Planet(vec3(0.6f, 0.8f, 1.0f), 1.2f, 90.0f, 0.075f, 0.8f, uranusTexture));   // Uranus
    planets.push_back(Planet(vec3(0.2f, 0.4f, 0.8f), 1.2f, 110.0f, 0.05f, 0.7f, neptuneTexture));  // Neptune
    
    Moon earthMoon(vec3(0.8f, 0.8f, 0.8f), 0.27f, 2.0f, 1.0f, 2.5f, moonTexture);
    planets[2].addMoon(earthMoon);
    
    Moon marsMoon1(vec3(0.6f, 0.6f, 0.6f), 0.15f, 1.0f, 1.25f, 3.0f, moonTexture);
    Moon marsMoon2(vec3(0.5f, 0.5f, 0.5f), 0.12f, 1.5f, 0.875f, 2.8f, moonTexture);
    planets[3].addMoon(marsMoon1);
    planets[3].addMoon(marsMoon2);
    
    Moon io(vec3(1.0f, 0.9f, 0.7f), 0.29f, 3.5f, 0.5f, 1.5f, moonTexture);
    Moon europa(vec3(0.8f, 0.8f, 0.9f), 0.25f, 4.5f, 0.375f, 1.2f, moonTexture);
    Moon ganymede(vec3(0.7f, 0.7f, 0.7f), 0.42f, 5.5f, 0.3f, 1.0f, moonTexture);
    Moon callisto(vec3(0.6f, 0.6f, 0.6f), 0.38f, 7.0f, 0.2f, 0.8f, moonTexture);
    planets[4].addMoon(io);
    planets[4].addMoon(europa);
    planets[4].addMoon(ganymede);
    planets[4].addMoon(callisto);
    
    vector<GLuint> orbitVAOs;
    vector<int> orbitVertexCounts;
    
    for (const auto& planet : planets) {
        vector<vec3> orbitVertices = createOrbitPath(planet.orbitRadius);
        GLuint orbitVAO = createOrbitVBO(orbitVertices);
        orbitVAOs.push_back(orbitVAO);
        orbitVertexCounts.push_back(orbitVertices.size() / 2);
    }
    
    mat4 projectionMatrix = perspective(radians(45.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 5000.0f);
    setProjectionMatrix(shaderProgram, projectionMatrix);
    setProjectionMatrix(starfieldShaderProgram, projectionMatrix);
    
    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window);

        updateShootingStars(deltaTime);
        
        glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
        
        glUseProgram(starfieldShaderProgram);
        setViewMatrix(starfieldShaderProgram, viewMatrix);
        glBindVertexArray(starfieldVAO);
        glDrawArrays(GL_POINTS, 0, starfieldVertexCount);

        glUseProgram(shootingStarShader);
        setViewMatrix(shootingStarShader, viewMatrix);
        setProjectionMatrix(shootingStarShader, projectionMatrix);
    
        glBindVertexArray(shootingStarVAO);
        for (const auto& star : shootingStars) {
            if (star.lifetime < star.maxLifetime) {
                float alpha = 1.0f - (star.lifetime / star.maxLifetime);
                glUniform3fv(glGetUniformLocation(shootingStarShader, "starColor"), 1, &star.color[0]);
                glUniform1f(glGetUniformLocation(shootingStarShader, "alpha"), alpha);
                glDrawArrays(GL_LINES, 0, SHOOTING_STAR_COUNT * 2);  // Draw one line segment
            }
        }
        
        glUseProgram(shaderProgram);
        setViewMatrix(shaderProgram, viewMatrix);
        
        vec3 lightPos(0.0f, 0.0f, 0.0f);
        vec3 lightColor(1.0f, 1.0f, 0.9f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, &lightPos[0]);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, &lightColor[0]);
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, &cameraPosition[0]);
        
        setWorldMatrix(shaderProgram, mat4(1.0f));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "useLighting"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "isSun"), 1);
        glBindVertexArray(sunVAO);
        glDrawArrays(GL_TRIANGLES, 0, sunVertexCount);
        
        glLineWidth(1.0f);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "useLighting"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "isSun"), 0);
        for (size_t i = 0; i < orbitVAOs.size(); ++i) {
            setWorldMatrix(shaderProgram, mat4(1.0f));
            glBindVertexArray(orbitVAOs[i]);
            glDrawArrays(GL_LINE_LOOP, 0, orbitVertexCounts[i]);
        }
        
        for (auto& planet : planets) {
            planet.update(deltaTime);
            planet.draw(shaderProgram);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
    
    return 0;
}