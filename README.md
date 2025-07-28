# Solar System Project - COMP 371

## Team Members

- Yassine Hajou
- Jay
- Omar

## Description

This is a basic 3D solar system simulation featuring:

- A central star (Sun)
- Planets of the solar system with an orbtital motion
- Interactive camera controls
- Relative Sizes

## Camera Controls

- **Mouse**: Look around (camera rotation)
- **W**: Move forward
- **S**: Move backward
- **A**: Move left
- **D**: Move right
- **Shift**: Speed Up 300%
- **ESC**: Exit application

## Build Instructions

1. Make sure you have the required libraries installed (OpenGL, GLFW, GLEW, GLM)
2. Open the COMP-371 folder in VS Code
3. Open `solarsystem.cpp`
4. Run the C++ program
5. Run the generated executable

(g++ -std=c++11 -I/opt/homebrew/include -L/opt/homebrew/lib -framework OpenGL -lglfw -lGLEW solarsystem.cpp -o solarsystem) command for Yassine's Mac.

## Future Extensions

- Add textures to planets
- Implement lighting system
- Add moons to some planets
- Add asteroid belt
- Add more realistic planet scales and orbital speeds
- Add planet names and information display
