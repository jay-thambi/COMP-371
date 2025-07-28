//
// COMP 371 Solar System Project
// Basic Solar System with hierarchical animation
//

#include <iostream>
#include <vector>
#include <cmath>

#define GLEW_STATIC 1   // This allows linking with Static Library on Windows, without DLL
#include <GL/glew.h>    // Include GLEW - OpenGL Extension Wrangler

#include <GLFW/glfw3.h> // GLFW provides a cross-platform interface for creating a graphical context,
                        // initializing OpenGL and binding inputs

#include <glm/glm.hpp>  // GLM is an optimized math library with syntax to similar to OpenGL Shading Language
#include <glm/gtc/matrix_transform.hpp> // include this to create transformation matrices
#include <glm/common.hpp>

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

const char* getVertexShaderSource()
{
    return
                "#version 330 core\n"
                "layout (location = 0) in vec3 aPos;"
                "layout (location = 1) in vec3 aColor;"
                ""
                "uniform mat4 worldMatrix;"
                "uniform mat4 viewMatrix = mat4(1.0);"
                "uniform mat4 projectionMatrix = mat4(1.0);"
                ""
                "out vec3 vertexColor;"
                "void main()"
                "{"
                "   vertexColor = aColor;"
                "   mat4 modelViewProjection = projectionMatrix * viewMatrix * worldMatrix;"
                "   gl_Position = modelViewProjection * vec4(aPos.x, aPos.y, aPos.z, 1.0);"
                "}";
}

const char* getFragmentShaderSource()
{
    return
                "#version 330 core\n"
                "in vec3 vertexColor;"
                "out vec4 FragColor;"
                "void main()"
                "{"
                "   FragColor = vec4(vertexColor.r, vertexColor.g, vertexColor.b, 1.0f);"
                "}";
}

int compileAndLinkShaders()
{
    // vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexShaderSource = getVertexShaderSource();
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    // fragment shader
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragmentShaderSource = getFragmentShaderSource();
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    // link shaders
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}

// Simple sphere using triangulated approach
vector<vec3> createSimpleSphere(float radius, vec3 color)
{
    vector<vec3> vertices;
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
            
            // First triangle
            vertices.push_back(v1); vertices.push_back(color);
            vertices.push_back(v2); vertices.push_back(color);
            vertices.push_back(v3); vertices.push_back(color);
            
            // Second triangle
            vertices.push_back(v2); vertices.push_back(color);
            vertices.push_back(v4); vertices.push_back(color);
            vertices.push_back(v3); vertices.push_back(color);
        }
    }
    
    return vertices;
}

int createSphereVBO(vector<vec3>& sphereVertices)
{
    // Create Vertex Buffer Object and Vertex Array Object
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    // Bind the Vertex Array Object first
    glBindVertexArray(VAO);
    
    // Bind and set vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(vec3), sphereVertices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    return VAO;
}

// Create orbital path (circle) vertices
vector<vec3> createOrbitPath(float radius)
{
    vector<vec3> vertices;
    int segments = 100; // Number of line segments for smooth circle
    vec3 color = vec3(0.3f, 0.3f, 0.3f); // Much dimmer gray color for subtle orbit lines
    
    for (int i = 0; i <= segments; ++i) {
        float angle = ((float)i / segments) * 2.0f * M_PI;
        vec3 position = vec3(
            radius * cos(angle),
            0.0f,  // Keep orbit in XZ plane
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
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
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

// Planet class for hierarchical animation
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
    
    Planet(vec3 c, float r, float orbitRad, float orbitSpd, float rotSpd) 
        : color(c), radius(r), orbitRadius(orbitRad), orbitSpeed(orbitSpd), rotationSpeed(rotSpd), 
          currentOrbitAngle(0.0f), currentRotationAngle(0.0f) {
        
        vector<vec3> vertices = createSimpleSphere(radius, color);
        VAO = createSphereVBO(vertices);
        vertexCount = vertices.size() / 2; // Each vertex has position and color
    }
    
    void update(float deltaTime) {
        currentOrbitAngle += orbitSpeed * deltaTime;
        currentRotationAngle += rotationSpeed * deltaTime;
    }
    
    mat4 getWorldMatrix() {
        // Hierarchical transformation: orbit then rotate
        mat4 orbitMatrix = rotate(mat4(1.0f), currentOrbitAngle, vec3(0.0f, 1.0f, 0.0f));
        mat4 translateMatrix = translate(mat4(1.0f), vec3(orbitRadius, 0.0f, 0.0f));
        mat4 rotationMatrix = rotate(mat4(1.0f), currentRotationAngle, vec3(0.0f, 1.0f, 0.0f));
        
        return orbitMatrix * translateMatrix * rotationMatrix;
    }
    
    void draw(int shaderProgram) {
        setWorldMatrix(shaderProgram, getWorldMatrix());
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }
};

// Mouse callback
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
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

// Process input
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Check if shift is pressed for faster movement
    float speedMultiplier = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        speedMultiplier = 3.0f; // 3x faster when shift is held
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
    // Initialize GLFW and OpenGL version
    glfwInit();
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create Window and rendering context using GLFW
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
    
    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    
    // Compile and link shaders
    int shaderProgram = compileAndLinkShaders();
    
    // Create the Sun (center star) - Much larger than planets
    vector<vec3> sunVertices = createSimpleSphere(3.0f, vec3(1.0f, 1.0f, 0.0f)); // Yellow sun
    GLuint sunVAO = createSphereVBO(sunVertices);
    int sunVertexCount = sunVertices.size() / 2;
    
    // Create planets with realistic relative scales (simplified but proportional)
    // Real scale ratios: Sun=109, Jupiter=11, Earth=1, Venus=0.95, Mars=0.53, Mercury=0.38
    // Scaled down for visibility: Sun=3.0, Jupiter=1.1, Earth=0.1, Venus=0.095, Mars=0.053, Mercury=0.038
    // Orbital distances spaced out more for better visibility
    vector<Planet> planets;
    planets.push_back(Planet(vec3(0.7f, 0.4f, 0.2f), 0.038f, 6.0f, 2.0f, 3.0f));   // Mercury (smallest, fastest orbit)
    planets.push_back(Planet(vec3(1.0f, 0.8f, 0.4f), 0.095f, 9.5f, 1.6f, 2.4f));   // Venus (similar to Earth size)
    planets.push_back(Planet(vec3(0.2f, 0.6f, 1.0f), 0.1f, 13.0f, 1.2f, 2.0f));    // Earth (reference size)
    planets.push_back(Planet(vec3(0.8f, 0.3f, 0.1f), 0.053f, 17.0f, 1.0f, 1.8f));  // Mars (about half Earth's size)
    planets.push_back(Planet(vec3(0.9f, 0.7f, 0.5f), 1.1f, 25.0f, 0.6f, 1.2f));    // Jupiter (largest planet, much farther)
    
    // Create orbital path rings for each planet
    vector<GLuint> orbitVAOs;
    vector<int> orbitVertexCounts;
    
    for (const auto& planet : planets) {
        vector<vec3> orbitVertices = createOrbitPath(planet.orbitRadius);
        GLuint orbitVAO = createOrbitVBO(orbitVertices);
        orbitVAOs.push_back(orbitVAO);
        orbitVertexCounts.push_back(orbitVertices.size() / 2);
    }
    
    // Set up view and projection matrices
    mat4 projectionMatrix = perspective(radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    setProjectionMatrix(shaderProgram, projectionMatrix);
    
    // Rendering Loop
    while(!glfwWindowShouldClose(window))
    {
        // Calculate deltatime
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        // Process input
        processInput(window);
        
        // Background Fill Color
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
        // Set view matrix
        mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
        setViewMatrix(shaderProgram, viewMatrix);
        
        // Draw the Sun at center
        setWorldMatrix(shaderProgram, mat4(1.0f));
        glBindVertexArray(sunVAO);
        glDrawArrays(GL_TRIANGLES, 0, sunVertexCount);
        
        // Draw orbital paths (subtle gray circles)
        glLineWidth(1.0f); // Set thin line width for orbital paths
        for (int i = 0; i < orbitVAOs.size(); ++i) {
            setWorldMatrix(shaderProgram, mat4(1.0f)); // Identity matrix for orbits centered at origin
            glBindVertexArray(orbitVAOs[i]);
            glDrawArrays(GL_LINE_LOOP, 0, orbitVertexCounts[i]);
        }
        glLineWidth(1.0f); // Reset line width
        
        // Update and draw planets
        for (auto& planet : planets) {
            planet.update(deltaTime);
            planet.draw(shaderProgram);
        }
        
        // End frame
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Shutdown GLFW
    glfwTerminate();
    
    return 0;
}
