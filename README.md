# raylib_transform_hierarchy

A simple project to learn how transform hierarchies work in C using Raylib and Flecs.

# License

MIT License - Free to use, modify, and share!

# What You Need

- Libraries:
    - Raylib 5.5 (for graphics and math)
    - Flecs 4.0.5 (for entity-component-system management)
- Tools:
    - CMake (to build the project)
    - Visual Studio 2022 (or any C compiler, but VS2022 is recommended)

What Is This Project?

This is a beginner-friendly test to understand transform hierarchies in C. Imagine a tree where objects (like a parent and its children) are connected. Each object has a position, rotation, and scale, and when the parent moves or rotates, the children follow along. This project shows how to code that in C with Raylib!

With help from Grok 3 (an AI assistant), you can experiment by searching "raylib c transform hierarchy" and using Raylib/Raymath cheatsheets.

What Works?

- [x] 2D Transform Hierarchy with Raylib
- [x] 3D Transform Hierarchy with Raylib
- [x] 2D Transform Hierarchy with Raylib + Flecs
- [x] 3D Transform Hierarchy with Raylib + Flecs
    
# CMakeLists.txt:
 Work in progress. As there main.c file as main entry point. Make sure there one main entry point or use the examples loop to test those files.


# How to Build and Run

1. Build: Run build.bat in your terminal (Windows).
2. Run: Run run.bat to see it in action!
    

Simple Code Example

Here’s a basic example of a transform hierarchy in C:

c
```c
#include "raylib.h"
#include "raymath.h"

// A "node" is like an object with position, rotation, and scale
typedef struct TransformNode {
    Vector3 position;         // Where it is (x, y, z)
    Quaternion rotation;      // How it’s rotated
    Vector3 scale;            // How big it is (x, y, z)
    Matrix localMatrix;       // Its own transform
    Matrix worldMatrix;       // Its transform including the parent’s
    struct TransformNode* parent;  // Link to parent (NULL if no parent)
    Color color;              // Color for drawing
    Color wire_color;         // Outline color
} TransformNode;

// Create a new node
TransformNode CreateTransformNode(Vector3 pos, Quaternion rot, Vector3 scl, 
                                 TransformNode* parent, Color color, Color wire_color) {
    TransformNode node = {
        .position = pos,
        .rotation = rot,
        .scale = scl,
        .localMatrix = MatrixIdentity(),  // Starts with no transform
        .worldMatrix = MatrixIdentity(),
        .parent = parent,
        .color = color,
        .wire_color = wire_color
    };
    return node;
}

// Update the node’s position based on its parent
void UpdateTransform(TransformNode* node) {
    Matrix translation = MatrixTranslate(node->position.x, node->position.y, node->position.z);
    Matrix rotation = QuaternionToMatrix(node->rotation);
    Matrix scaling = MatrixScale(node->scale.x, node->scale.y, node->scale.z);

    // Combine scale, rotation, and position into one transform
    node->localMatrix = MatrixMultiply(scaling, MatrixMultiply(rotation, translation));
    
    // If it has a parent, include the parent’s transform
    if (node->parent != NULL) {
        node->worldMatrix = MatrixMultiply(node->localMatrix, node->parent->worldMatrix);
    } else {
        node->worldMatrix = node->localMatrix;
    }
}

// Example: A parent and a child
TransformNode parent = CreateTransformNode(
    (Vector3){ 0.0f, 0.0f, 0.0f },  // Position at origin
    QuaternionIdentity(),            // No rotation
    (Vector3){ 1.0f, 1.0f, 1.0f },  // Normal size
    NULL,                            // No parent (root node)
    RED,                             // Red color
    BLACK                            // Black outline
);

TransformNode child = CreateTransformNode(
    (Vector3){ 2.0f, 0.0f, 0.0f },           // 2 units right of parent
    QuaternionFromAxisAngle((Vector3){ 0, 1, 0 }, DEG2RAD * 45),  // Rotate 45°
    (Vector3){ 0.5f, 0.5f, 0.5f },          // Half size
    &parent,                                 // Linked to parent
    BLUE,                                    // Blue color
    WHITE                                    // White outline
);

// Update both nodes
UpdateTransform(&parent);
UpdateTransform(&child);
```

This creates a "parent" object at (0,0,0) and a "child" object 2 units to the right, rotated 45 degrees, and half the size. The child moves with the parent!

Visual Diagram

Here’s how the hierarchy looks:

```text
[Parent] ----> [Child]
   |               |
Position: (0,0,0)  Position: (2,0,0)
Rotation: 0°       Rotation: 45°
Scale: 1x          Scale: 0.5x
```

(Imagine a red square with a smaller blue square attached to its right side, tilted 45°!)  
(Note: To generate an actual image, let me know if you’d like one!)

Learn More

- Transforms Explained: [Gabor Makes Games - Transforms](https://gabormakesgames.com/blog_transforms.html)
- Project Code: [GitHub - raylib_transform_hierarchy](https://github.com/Lightnet/raylib_transform_hierarchy)
- Raylib: Graphics and math tools ([Raylib Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html))
- Raymath: Math helper functions (part of Raylib)
- Flecs: Entity-component-system library for organizing game objects

Credits
- Inspired by Gabor’s transform tutorial.