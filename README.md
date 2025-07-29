# Solar System Project - COMP 371

## Team Members

- Yassine Hajou
- Sanjay Thambithurai (40184405)
- Harun Slahaldin Omar

## Description

This is a complete 3D solar system simulation featuring:

- A central star (Sun) with texture
- Planets of the solar system with orbital motion and textures
- **Hierarchical Animation**: Moons orbiting planets (2-level deep hierarchy)
- Interactive camera controls with relative movement
- Realistic relative sizes and orbital speeds
- Multiple textured surfaces with different textures
- Advanced OpenGL features with shader-based rendering

## Camera Controls

- **Mouse**: Look around (camera rotation)
- **W**: Move forward
- **S**: Move backward
- **A**: Move left
- **D**: Move right
- **Shift**: Speed Up Movement 300%
- **CTRL**: Slow orbit to 10%
- **ESC**: Exit application

## Build Instructions

1. Make sure you have the required libraries installed (OpenGL, GLFW, GLEW, GLM)
2. Open the COMP-371 folder in VS Code
3. Open `solarsystem.cpp`
4. Run the C++ program
5. Run the generated executable

(g++ -std=c++11 -I/opt/homebrew/include -L/opt/homebrew/lib -framework OpenGL -lglfw -lGLEW solarsystem.cpp -o solarsystem) command for Yassine's Mac.

## Features Implemented

**Complete Assignment Requirements:**
- Real-time 3D interactive application using OpenGL
- Interactive camera with mouse + keyboard controls (flying/walking style)
- Multiple textured surfaces with different textures
- Hierarchical animation (sun→planet→moon, 2 levels deep)
- Advanced OpenGL features with shader-based rendering

## Solar System Details

**Sun**: Central star with yellow texture
**Planets**: Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune with unique textures
**Moons**: 
- Earth: 1 moon
- Mars: 2 moons (Phobos, Deimos)
- Jupiter: 4 Galilean moons

## Technical Features

- Texture mapping with stb_image.h
- Hierarchical transformations for orbital mechanics
- Fragment and vertex shaders with texture support
- Realistic relative planet sizes and orbital speeds
- Smooth camera controls with speed boost (Shift key)
