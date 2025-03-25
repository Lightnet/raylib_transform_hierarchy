# raylib_transform_hierarchy

# License: MIT

# Libs:
 * Raylib 5.5
 * Flecs 4.0.5

# Required:
 * CMake
 * VS2022 (need a compiler)

# Information:
  This is very simple build test for learning how transform hierarchy works.

  In simple words it use transform node tree style to loop to update position and rotate of the children.

  With the help of the AI model. Grok 3. As long you type raylib c transform hierarchy and feed the raylib and raymath cheatsheets. It should able to make tree node logic as transform hierarchy.
  
# Test:
 * raylib c transform hierarchy 3d (worked)
 * raylib c transform hierarchy 2d (worked)
 * raylib flecs c transform hierarchy 2d (worked)
 * raylib flecs c transform hierarchy 3d (working partly)
 
# Build:
  build.bat

# Run:
  run.bat

# c Transform Hierarchy:
```c
#include "raylib.h"
#include "raymath.h"

typedef struct TransformNode {
    Vector3 position;         // Local position
    Quaternion rotation;      // Local rotation
    Vector3 scale;            // Local scale
    Matrix localMatrix;       // Local transform matrix
    Matrix worldMatrix;       // World transform matrix
    struct TransformNode* parent;  // Pointer to parent node (NULL if root)
    Color color;              // Solid color
    Color wire_color;         // Wireframe color
} TransformNode;

TransformNode CreateTransformNode(Vector3 pos, Quaternion rot, Vector3 scl, TransformNode* parent, Color color, Color wire_color) {
    TransformNode node = {
        .position = pos,
        .rotation = rot,
        .scale = scl,
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity(),
        .parent = parent,
        .color = color,
        .wire_color = wire_color
    };
    return node;
}

void UpdateTransform(TransformNode* node) {
    Matrix translation = MatrixTranslate(node->position.x, node->position.y, node->position.z);
    Matrix rotation = QuaternionToMatrix(node->rotation);
    Matrix scaling = MatrixScale(node->scale.x, node->scale.y, node->scale.z);

    node->localMatrix = MatrixMultiply(scaling, MatrixMultiply(rotation, translation));
    if (node->parent != NULL) {
        node->worldMatrix = MatrixMultiply(node->localMatrix, node->parent->worldMatrix);
    } else {
        node->worldMatrix = node->localMatrix;
    }
}

TransformNode parent = CreateTransformNode(
    (Vector3){ 0.0f, 0.0f, 0.0f },
    QuaternionIdentity(),
    (Vector3){ 1.0f, 1.0f, 1.0f },
    NULL,
    RED,
    BLACK
);

TransformNode child = CreateTransformNode(
    (Vector3){ 2.0f, 0.0f, 0.0f },
    QuaternionFromAxisAngle((Vector3){ 0, 1, 0 }, DEG2RAD * 45),
    (Vector3){ 0.5f, 0.5f, 0.5f },
    &parent,
    BLUE,
    WHITE
);

UpdateTransform(&parent);
UpdateTransform(&child);
```
  It strip down version.

# Credits:
 * https://gabormakesgames.com/blog_transforms.html