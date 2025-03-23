#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>  // For realloc

typedef struct TransformNode {
    Vector3 position; Quaternion rotation; Vector3 scale;
    Matrix localMatrix; Matrix worldMatrix;
    struct TransformNode* parent;
    struct TransformNode** children; int childCount;
    Model* model;
} TransformNode;

TransformNode CreateTransformNode(Vector3 pos, Quaternion rot, Vector3 scl, TransformNode* parent) {
    TransformNode node = { pos, rot, scl, MatrixIdentity(), MatrixIdentity(), parent, NULL, 0, NULL };
    return node;
}

void AddChild(TransformNode* parent, TransformNode* child) {
    child->parent = parent;
    parent->childCount++;
    parent->children = realloc(parent->children, parent->childCount * sizeof(TransformNode*));
    parent->children[parent->childCount - 1] = child;
}

void UpdateTransform(TransformNode* node) {
    node->localMatrix = MatrixMultiply(MatrixScale(node->scale.x, node->scale.y, node->scale.z),
                                       MatrixMultiply(QuaternionToMatrix(node->rotation),
                                                      MatrixTranslate(node->position.x, node->position.y, node->position.z)));
    node->worldMatrix = (node->parent != NULL) ? MatrixMultiply(node->localMatrix, node->parent->worldMatrix) : node->localMatrix;
    for (int i = 0; i < node->childCount; i++) {
        UpdateTransform(node->children[i]);
    }
}

void RenderNode(TransformNode* node) {
    if (node->model != NULL) {
        node->model->transform = node->worldMatrix;
        DrawModel(*node->model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }
    for (int i = 0; i < node->childCount; i++) RenderNode(node->children[i]);
}

int main(void) {
    InitWindow(800, 600, "Fixed Transform Hierarchy");
    SetTargetFPS(60);

    Camera3D camera = { .position = {10, 10, 10}, .target = {0, 0, 0}, .up = {0, 1, 0}, .fovy = 45, .projection = CAMERA_PERSPECTIVE };
    Model cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    TransformNode root = CreateTransformNode((Vector3){0, 0, 0}, QuaternionIdentity(), (Vector3){1, 1, 1}, NULL);
    TransformNode child1 = CreateTransformNode((Vector3){2, 0, 0}, QuaternionIdentity(), (Vector3){0.5, 0.5, 0.5}, &root);
    TransformNode child2 = CreateTransformNode((Vector3){0, 2, 0}, QuaternionIdentity(), (Vector3){0.5, 0.5, 0.5}, &root);
    root.model = child1.model = child2.model = &cube;
    AddChild(&root, &child1);
    AddChild(&root, &child2);

    TransformNode* activeNode = &root;
    bool isMovementMode = true;
    float moveSpeed = 2.0f, rotateSpeed = 90.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_TAB)) isMovementMode = !isMovementMode;
        if (IsKeyPressed(KEY_SPACE)) activeNode = (activeNode == &root) ? &child1 : &root;

        if (isMovementMode) {
            if (IsKeyDown(KEY_W)) activeNode->position.z -= moveSpeed * dt;
            if (IsKeyDown(KEY_S)) activeNode->position.z += moveSpeed * dt;
            if (IsKeyDown(KEY_A)) activeNode->position.x -= moveSpeed * dt;
            if (IsKeyDown(KEY_D)) activeNode->position.x += moveSpeed * dt;
        } else {
            if (IsKeyDown(KEY_Q)) activeNode->rotation = QuaternionMultiply(activeNode->rotation, QuaternionFromAxisAngle((Vector3){0, 1, 0}, DEG2RAD * rotateSpeed * dt));
            if (IsKeyDown(KEY_E)) activeNode->rotation = QuaternionMultiply(activeNode->rotation, QuaternionFromAxisAngle((Vector3){0, 1, 0}, -DEG2RAD * rotateSpeed * dt));
            if (IsKeyDown(KEY_W)) activeNode->rotation = QuaternionMultiply(activeNode->rotation, QuaternionFromAxisAngle((Vector3){1, 0, 0}, DEG2RAD * rotateSpeed * dt));
            if (IsKeyDown(KEY_S)) activeNode->rotation = QuaternionMultiply(activeNode->rotation, QuaternionFromAxisAngle((Vector3){1, 0, 0}, -DEG2RAD * rotateSpeed * dt));
            if (IsKeyDown(KEY_A)) activeNode->rotation = QuaternionMultiply(activeNode->rotation, QuaternionFromAxisAngle((Vector3){0, 0, 1}, DEG2RAD * rotateSpeed * dt));
            if (IsKeyDown(KEY_D)) activeNode->rotation = QuaternionMultiply(activeNode->rotation, QuaternionFromAxisAngle((Vector3){0, 0, 1}, -DEG2RAD * rotateSpeed * dt));
        }

        if (IsKeyPressed(KEY_R)) {
            activeNode->position = (Vector3){0, 0, 0};
            activeNode->rotation = QuaternionIdentity();
            activeNode->scale = (Vector3){1, 1, 1};
        }

        UpdateTransform(&root);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        RenderNode(&root);
        DrawGrid(10, 1.0f);
        EndMode3D();
        DrawText(isMovementMode ? "Mode: Movement" : "Mode: Rotation", 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("Active: %s", (activeNode == &root) ? "Root" : "Child1"), 10, 30, 20, DARKGRAY);
        DrawText("Tab: Toggle Mode | Space: Switch Node | R: Reset", 10, 50, 20, DARKGRAY);
        DrawFPS(10, 70);
        EndDrawing();
    }

    UnloadModel(cube);
    CloseWindow();
    return 0;
}