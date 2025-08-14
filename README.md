# Solar System Project - COMP 371

## Team Members

- Yassine Hajou (40284609)
- Sanjay Thambithurai (40184405)
- Harun Slahaldin Omar (40250981)

## Project Overview & Features

A 3D solar system simulation built with OpenGL 3.3 featuring realistic planetary motion and interactive controls.

**Core Features:**

- 8 planets orbiting the sun with individual textures and realistic relative speeds
- Hierarchical animation system (Sun → Planet → Moon) with multiple moons per planet
- Phong lighting model with ambient, diffuse, and specular components
- Eclipse system with shadow calculations for Earth-Moon interactions
- Interactive camera with free movement and planet-following modes

**Visual Effects:**

- 50,000 procedurally generated stars with realistic colors and brightness
- Animated shooting stars with particle trail effects
- Saturn's textured ring system
- Orbiting spacecraft with OBJ model loading
- Sun glow effects and lighting toggle

**Straight-Forward Controls:**

- Mouse and keyboard camera controls (WASD + mouse look)
- Planet selection and follow mode (F key + 1-8 number keys)
- Lighting toggle (L/K keys) and animation controls (P for pause, +/- for speed)
- Fullscreen mode (F11) and speed boost (Shift)

## Complete Controls Guide

### Camera Movement (Free Mode Only)

- **Mouse**: Look around (camera rotation) - disabled in follow mode
- **W**: Move forward
- **S**: Move backward
- **A**: Move left (strafe)
- **D**: Move right (strafe)
- **Q**: Move up
- **E**: Move down
- **Shift + Movement**: 5x speed boost for camera movement
- **ESC**: Exit application

### Camera Modes

- **F**: Enable follow mode (orbiting camera around selected planet)
- **G**: Disable follow mode (return to free camera)
- **1-8**: Select planet to follow (1=Mercury, 2=Venus, 3=Earth, etc.)

### Lighting Controls

- **L**: Turn ON sun lighting (Phong model lighting)
- **K**: Turn OFF sun lighting (see planets without lighting)

### Animation Controls

- **P**: Pause all planetary orbits and rotations
- **O**: Resume orbital motion
- **- (Minus)**: Decrease orbit speed multiplier
- **= (Plus/Equal)**: Increase orbit speed multiplier

### Display Controls

- **F11**: Toggle fullscreen mode

## Build Instructions

1. Make sure you have the required libraries installed (OpenGL, GLFW, GLEW, GLM)
2. Open the COMP-371 folder in VS Code
3. Open `solarsystem.cpp`
4. Run the C++ program
5. Run the generated executable
