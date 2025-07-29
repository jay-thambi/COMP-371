#include <iostream>
#include <vector>
#include <cmath>
#include <string>

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

// Camera variables
vec3 cameraPosition = vec3(0.0f, 5.0f, 20.0f);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
bool firstMouse = true;
float lastX = 400;
float lastY = 300;

// Time variables
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Orbit speed
float orbitSpeedMultiplier = 1.0f;

// Texture IDs
GLuint sunTexture, mercuryTexture, venusTexture, earthTexture, marsTexture, jupiterTexture;
GLuint saturnTexture, uranusTexture, neptuneTexture;
GLuint moonTexture, earthMoonTexture, marsMoonTexture, jupiterMoonTexture;

const char* getVertexShaderSource()
{
    return
                "#version 330 core\n"
                "layout (location = 0) in vec3 aPos;"
                "layout (location = 1) in vec3 aColor;"
                "layout (location = 2) in vec2 aTexCoord;"
                ""
                "uniform mat4 worldMatrix;"
                "uniform mat4 viewMatrix = mat4(1.0);"
                "uniform mat4 projectionMatrix = mat4(1.0);"
                ""
                "out vec3 vertexColor;"
                "out vec2 TexCoord;"
                "void main()"
                "{"
                "   vertexColor = aColor;"
                "   TexCoord = aTexCoord;"
                "   mat4 modelViewProjection = projectionMatrix * viewMatrix * worldMatrix;"
                "   gl_Position = modelViewProjection * vec4(aPos.x, aPos.y, aPos.z, 1.0);"
                "}";
}

const char* getFragmentShaderSource()
{
    return
                "#version 330 core\n"
                "in vec3 vertexColor;"
                "in vec2 TexCoord;"
                "out vec4 FragColor;"
                ""
                "uniform sampler2D texture1;"
                "uniform bool useTexture;"
                ""
                "void main()"
                "{"
                "   if (useTexture) {"
                "       vec4 texColor = texture(texture1, TexCoord);"
                "       FragColor = mix(texColor, vec4(vertexColor, 1.0), 0.3);"
                "   } else {"
                "       FragColor = vec4(vertexColor.r, vertexColor.g, vertexColor.b, 1.0f);"
                "   }"
                "}";
}

int compileAndLinkShaders()
{
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexShaderSource = getVertexShaderSource();
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragmentShaderSource = getFragmentShaderSource();
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}

GLuint loadTexture(const char* filename) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int width, height, nrChannels;
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
    } else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
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
    int segments = 20;
    int rings = 10;
    
    for (int i = 0; i < rings; ++i) {
        float theta1 = ((float)i / rings) * M_PI;
        float theta2 = ((float)(i + 1) / rings) * M_PI;
        
        for (int j = 0; j < segments; ++j) {
            float phi1 = ((float)j / segments) * 2.0f * M_PI;
            float phi2 = ((float)(j + 1) / segments) * 2.0f * M_PI;
            
            // Calculate vertices
            vec3 v1 = vec3(
                radius * sin(theta1) * cos(phi1),
                radius * cos(theta1),
                radius * sin(theta1) * sin(phi1)
            );
            vec3 v2 = vec3(
                radius * sin(theta1) * cos(phi2),
                radius * cos(theta1),
                radius * sin(theta1) * sin(phi2)
            );
            vec3 v3 = vec3(
                radius * sin(theta2) * cos(phi1),
                radius * cos(theta2),
                radius * sin(theta2) * sin(phi1)
            );
            vec3 v4 = vec3(
                radius * sin(theta2) * cos(phi2),
                radius * cos(theta2),
                radius * sin(theta2) * sin(phi2)
            );
            
            // Calculate texture coordinates
            vec2 t1 = vec2((float)j / segments, (float)i / rings);
            vec2 t2 = vec2((float)(j + 1) / segments, (float)i / rings);
            vec2 t3 = vec2((float)j / segments, (float)(i + 1) / rings);
            vec2 t4 = vec2((float)(j + 1) / segments, (float)(i + 1) / rings);
            
            // First triangle
            vertices.push_back(v1.x); vertices.push_back(v1.y); vertices.push_back(v1.z);
            vertices.push_back(color.x); vertices.push_back(color.y); vertices.push_back(color.z);
            vertices.push_back(t1.x); vertices.push_back(t1.y);
            
            vertices.push_back(v2.x); vertices.push_back(v2.y); vertices.push_back(v2.z);
            vertices.push_back(color.x); vertices.push_back(color.y); vertices.push_back(color.z);
            vertices.push_back(t2.x); vertices.push_back(t2.y);
            
            vertices.push_back(v3.x); vertices.push_back(v3.y); vertices.push_back(v3.z);
            vertices.push_back(color.x); vertices.push_back(color.y); vertices.push_back(color.z);
            vertices.push_back(t3.x); vertices.push_back(t3.y);
            
            // Second triangle
            vertices.push_back(v2.x); vertices.push_back(v2.y); vertices.push_back(v2.z);
            vertices.push_back(color.x); vertices.push_back(color.y); vertices.push_back(color.z);
            vertices.push_back(t2.x); vertices.push_back(t2.y);
            
            vertices.push_back(v4.x); vertices.push_back(v4.y); vertices.push_back(v4.z);
            vertices.push_back(color.x); vertices.push_back(color.y); vertices.push_back(color.z);
            vertices.push_back(t4.x); vertices.push_back(t4.y);
            
            vertices.push_back(v3.x); vertices.push_back(v3.y); vertices.push_back(v3.z);
            vertices.push_back(color.x); vertices.push_back(color.y); vertices.push_back(color.z);
            vertices.push_back(t3.x); vertices.push_back(t3.y);
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
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    return VAO;
}

vector<vec3> createOrbitPath(float radius)
{
    vector<vec3> vertices;
    int segments = 100;
    vec3 color = vec3(0.3f, 0.3f, 0.3f); 
    
    for (int i = 0; i <= segments; ++i) {
        float angle = ((float)i / segments) * 2.0f * M_PI;
        vec3 position = vec3(
            radius * cos(angle),
            0.0f,  
            radius * sin(angle)
        );
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
        vertexCount = vertices.size() / 8; 
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
    
    void draw(int shaderProgram) {
        setWorldMatrix(shaderProgram, getWorldMatrix());
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
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
    
    Planet(vec3 c, float r, float orbitRad, float orbitSpd, float rotSpd, GLuint texID) 
        : color(c), radius(r), orbitRadius(orbitRad), orbitSpeed(orbitSpd), rotationSpeed(rotSpd), 
          currentOrbitAngle(0.0f), currentRotationAngle(0.0f), textureID(texID) {
        
        vector<float> vertices = createTexturedSphere(radius, color);
        VAO = createTexturedSphereVBO(vertices);
        vertexCount = vertices.size() / 8;
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
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        
        for (auto& moon : moons) {
            mat4 moonMatrix = planetMatrix * moon.getWorldMatrix();
            setWorldMatrix(shaderProgram, moonMatrix);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, moon.textureID);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
            
            glBindVertexArray(moon.VAO);
            glDrawArrays(GL_TRIANGLES, 0, moon.vertexCount);
        }
    }
};

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

    float sensitivity = 0.1f;
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
        speedMultiplier = 3.0f; // 3x faster when shift is held
    }

    // slow down orbit speed
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || 
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        orbitSpeedMultiplier = 0.1f; // 10%
    }
    else {
        orbitSpeedMultiplier = 1.0f; 
    }
    
    float cameraSpeed = 5.0f * deltaTime * speedMultiplier;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPosition += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPosition -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPosition -= normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPosition += normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
}

int main(int argc, char*argv[])
{
    glfwInit();
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "Solar System", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Initialize GLEW
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to create GLEW" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glEnable(GL_DEPTH_TEST);
    
    int shaderProgram = compileAndLinkShaders();
    
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
    earthMoonTexture = loadTexture("textures/moon.jpg");
    marsMoonTexture = loadTexture("textures/moon.jpg");
    jupiterMoonTexture = loadTexture("textures/moon.jpg");
    
    vector<float> sunVertices = createTexturedSphere(3.0f, vec3(1.0f, 1.0f, 0.0f)); // Yellow sun
    GLuint sunVAO = createTexturedSphereVBO(sunVertices);
    int sunVertexCount = sunVertices.size() / 8;
    
    vector<Planet> planets;
    planets.push_back(Planet(vec3(0.7f, 0.4f, 0.2f), 0.038f, 6.0f, 2.0f, 3.0f, mercuryTexture));   // Mercury
    planets.push_back(Planet(vec3(1.0f, 0.8f, 0.4f), 0.095f, 9.5f, 1.6f, 2.4f, venusTexture));     // Venus
    planets.push_back(Planet(vec3(0.2f, 0.6f, 1.0f), 0.1f, 13.0f, 1.2f, 2.0f, earthTexture));      // Earth
    planets.push_back(Planet(vec3(0.8f, 0.3f, 0.1f), 0.053f, 17.0f, 1.0f, 1.8f, marsTexture));     // Mars
    planets.push_back(Planet(vec3(0.9f, 0.7f, 0.5f), 1.1f, 25.0f, 0.6f, 1.2f, jupiterTexture));    // Jupiter
    planets.push_back(Planet(vec3(0.8f, 0.6f, 0.4f), 0.9f, 35.0f, 0.4f, 1.0f, saturnTexture));     // Saturn
    planets.push_back(Planet(vec3(0.6f, 0.8f, 1.0f), 0.4f, 45.0f, 0.3f, 0.8f, uranusTexture));     // Uranus
    planets.push_back(Planet(vec3(0.2f, 0.4f, 0.8f), 0.4f, 55.0f, 0.2f, 0.7f, neptuneTexture));    // Neptune

    Moon earthMoon(vec3(0.8f, 0.8f, 0.8f), 0.025f, 0.3f, 4.0f, 2.5f, earthMoonTexture);
    planets[2].addMoon(earthMoon); 
    
    Moon marsMoon1(vec3(0.6f, 0.6f, 0.6f), 0.015f, 0.25f, 5.0f, 3.0f, marsMoonTexture);
    Moon marsMoon2(vec3(0.5f, 0.5f, 0.5f), 0.012f, 0.4f, 3.5f, 2.8f, marsMoonTexture);
    planets[3].addMoon(marsMoon1);
    planets[3].addMoon(marsMoon2);
    
    Moon jupiterMoon1(vec3(0.9f, 0.9f, 0.9f), 0.08f, 0.8f, 2.0f, 1.5f, jupiterMoonTexture);
    Moon jupiterMoon2(vec3(0.8f, 0.8f, 0.8f), 0.06f, 1.2f, 1.5f, 1.2f, jupiterMoonTexture);
    Moon jupiterMoon3(vec3(0.7f, 0.7f, 0.7f), 0.05f, 1.6f, 1.2f, 1.0f, jupiterMoonTexture);
    Moon jupiterMoon4(vec3(0.6f, 0.6f, 0.6f), 0.04f, 2.0f, 0.8f, 0.8f, jupiterMoonTexture);
    planets[4].addMoon(jupiterMoon1);
    planets[4].addMoon(jupiterMoon2);
    planets[4].addMoon(jupiterMoon3);
    planets[4].addMoon(jupiterMoon4);
    
    vector<GLuint> orbitVAOs;
    vector<int> orbitVertexCounts;
    
    for (const auto& planet : planets) {
        vector<vec3> orbitVertices = createOrbitPath(planet.orbitRadius);
        GLuint orbitVAO = createOrbitVBO(orbitVertices);
        orbitVAOs.push_back(orbitVAO);
        orbitVertexCounts.push_back(orbitVertices.size() / 2);
    }
    
    mat4 projectionMatrix = perspective(radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    setProjectionMatrix(shaderProgram, projectionMatrix);
    
    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window);
        
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
        mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
        setViewMatrix(shaderProgram, viewMatrix);
        
        setWorldMatrix(shaderProgram, mat4(1.0f));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
        glBindVertexArray(sunVAO);
        glDrawArrays(GL_TRIANGLES, 0, sunVertexCount);
        
        glLineWidth(1.0f); 
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0); 
        for (int i = 0; i < orbitVAOs.size(); ++i) {
            setWorldMatrix(shaderProgram, mat4(1.0f));
            glBindVertexArray(orbitVAOs[i]);
            glDrawArrays(GL_LINE_LOOP, 0, orbitVertexCounts[i]);
        }
        glLineWidth(1.0f); 
        
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
