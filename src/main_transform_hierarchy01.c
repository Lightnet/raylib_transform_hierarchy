#include "raylib.h"
#include "raymath.h"

// Transform node structure
typedef struct TransformNode {
    Vector3 position;         // Local position
    Quaternion rotation;      // Local rotation
    Vector3 scale;            // Local scale
    Matrix localMatrix;       // Local transform matrix
    Matrix worldMatrix;       // World transform matrix
    struct TransformNode* parent;  // Pointer to parent node (NULL if root)
} TransformNode;

// Initialize a transform node
TransformNode CreateTransformNode(Vector3 pos, Quaternion rot, Vector3 scl, TransformNode* parent) {
    TransformNode node = {
        .position = pos,
        .rotation = rot,
        .scale = scl,
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity(),
        .parent = parent
    };
    return node;
}

// Update transform matrices
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

int main(void) {
    // Initialize window
    InitWindow(800, 600, "Transform Hierarchy with Toggle");
    SetTargetFPS(60);

    // Camera setup
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load a cube model
    Model cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    // Initial parent and child transforms
    TransformNode parent = CreateTransformNode(
        (Vector3){ 0.0f, 0.0f, 0.0f },
        QuaternionIdentity(),
        (Vector3){ 1.0f, 1.0f, 1.0f },
        NULL
    );

    TransformNode child = CreateTransformNode(
        (Vector3){ 2.0f, 0.0f, 0.0f },
        QuaternionFromAxisAngle((Vector3){ 0, 1, 0 }, DEG2RAD * 45),
        (Vector3){ 0.5f, 0.5f, 0.5f },
        &parent
    );

    float moveSpeed = 2.0f;   // Units per second
    float rotateSpeed = 90.0f; // Degrees per second
    bool isMovementMode = true; // Start in movement mode (true = move, false = rotate)

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Toggle between movement and rotation with Tab
        if (IsKeyPressed(KEY_TAB)) {
            isMovementMode = !isMovementMode;
        }

        if (isMovementMode) {
            // Movement controls (WASD)
            if (IsKeyDown(KEY_W)) parent.position.z -= moveSpeed * dt;  // Forward
            if (IsKeyDown(KEY_S)) parent.position.z += moveSpeed * dt;  // Backward
            if (IsKeyDown(KEY_A)) parent.position.x -= moveSpeed * dt;  // Left
            if (IsKeyDown(KEY_D)) parent.position.x += moveSpeed * dt;  // Right
        } else {
            // Rotation controls (QWE/ASD)
            if (IsKeyDown(KEY_Q)) {  // Rotate Y (yaw)
                Quaternion rot = QuaternionFromAxisAngle((Vector3){ 0, 1, 0 }, DEG2RAD * rotateSpeed * dt);
                parent.rotation = QuaternionMultiply(parent.rotation, rot);
            }
            if (IsKeyDown(KEY_E)) {  // Rotate Y (opposite)
                Quaternion rot = QuaternionFromAxisAngle((Vector3){ 0, 1, 0 }, -DEG2RAD * rotateSpeed * dt);
                parent.rotation = QuaternionMultiply(parent.rotation, rot);
            }
            if (IsKeyDown(KEY_W)) {  // Rotate X (pitch)
                Quaternion rot = QuaternionFromAxisAngle((Vector3){ 1, 0, 0 }, DEG2RAD * rotateSpeed * dt);
                parent.rotation = QuaternionMultiply(parent.rotation, rot);
            }
            if (IsKeyDown(KEY_S)) {  // Rotate X (opposite)
                Quaternion rot = QuaternionFromAxisAngle((Vector3){ 1, 0, 0 }, -DEG2RAD * rotateSpeed * dt);
                parent.rotation = QuaternionMultiply(parent.rotation, rot);
            }
            if (IsKeyDown(KEY_A)) {  // Rotate Z (roll)
                Quaternion rot = QuaternionFromAxisAngle((Vector3){ 0, 0, 1 }, DEG2RAD * rotateSpeed * dt);
                parent.rotation = QuaternionMultiply(parent.rotation, rot);
            }
            if (IsKeyDown(KEY_D)) {  // Rotate Z (opposite)
                Quaternion rot = QuaternionFromAxisAngle((Vector3){ 0, 0, 1 }, -DEG2RAD * rotateSpeed * dt);
                parent.rotation = QuaternionMultiply(parent.rotation, rot);
            }
        }

        // Reset transform (R) works in both modes
        if (IsKeyPressed(KEY_R)) {
            parent.position = (Vector3){ 0.0f, 0.0f, 0.0f };
            parent.rotation = QuaternionIdentity();
            parent.scale = (Vector3){ 1.0f, 1.0f, 1.0f };
        }

        // Update transforms
        UpdateTransform(&parent);
        UpdateTransform(&child);

        // Render
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
        // Draw parent (red cube)
        cube.transform = parent.worldMatrix;
        DrawModel(cube, (Vector3){ 0, 0, 0 }, 1.0f, RED);

        // Draw child (blue cube)
        cube.transform = child.worldMatrix;
        DrawModel(cube, (Vector3){ 0, 0, 0 }, 1.0f, BLUE);

        // Draw a grid for reference
        DrawGrid(10, 1.0f);
        EndMode3D();

        // Display mode and instructions
        const char* modeText = isMovementMode ? "Mode: Movement (WASD)" : "Mode: Rotation (QWE/ASD)";
        DrawText(modeText, 10, 10, 20, DARKGRAY);
        DrawText("Tab: Toggle Mode | R: Reset", 10, 30, 20, DARKGRAY);
        DrawFPS(10, 60);
        EndDrawing();
    }

    // Cleanup
    UnloadModel(cube);
    CloseWindow();
    return 0;
}