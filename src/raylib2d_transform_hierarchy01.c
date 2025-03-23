#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

typedef struct {
    Vector2 local_pos;   // Local position relative to parent
    Vector2 world_pos;   // Computed world position
    float rotation;      // Rotation in degrees
} FTransform;

typedef struct {
    Color color;         // Color for rendering
    float radius;        // Radius of the circle
} Renderable;

void UpdateTransforms(FTransform *parent, FTransform *child) {
    // Update parent's world position (root entity)
    parent->world_pos = parent->local_pos;

    // Update child's world position based on parent's transform
    float parent_rot_rad = DEG2RAD * parent->rotation;
    Vector2 rotated_local = {
        child->local_pos.x * cosf(parent_rot_rad) - child->local_pos.y * sinf(parent_rot_rad),
        child->local_pos.x * sinf(parent_rot_rad) + child->local_pos.y * cosf(parent_rot_rad)
    };
    child->world_pos = Vector2Add(parent->world_pos, rotated_local);
    child->rotation = parent->rotation; // Child inherits parent's rotation
}

void RenderEntities(FTransform *t, Renderable *r, int count) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    for (int i = 0; i < count; i++) {
        DrawCircleV(t[i].world_pos, r[i].radius, r[i].color);
        Vector2 end = {
            t[i].world_pos.x + r[i].radius * cosf(DEG2RAD * t[i].rotation),
            t[i].world_pos.y + r[i].radius * sinf(DEG2RAD * t[i].rotation)
        };
        DrawLineV(t[i].world_pos, end, BLACK);
        // Debug: Print positions
        printf("Entity %d: world_pos = (%f, %f), rotation = %f\n", i, t[i].world_pos.x, t[i].world_pos.y, t[i].rotation);
    }

    DrawFPS(10, 10);
    EndDrawing();
}

int main() {
    // Initialize raylib window
    InitWindow(800, 600, "Raylib Orbit Test");
    SetTargetFPS(60);

    // Create entities
    FTransform transforms[2];
    Renderable renderables[2];

    // Parent entity
    transforms[0] = (FTransform){ .local_pos = {400, 300}, .world_pos = {400, 300}, .rotation = 0 };
    renderables[0] = (Renderable){ .color = RED, .radius = 20 };

    // Child entity
    transforms[1] = (FTransform){ .local_pos = {50, 0}, .world_pos = {0, 0}, .rotation = 0 };
    renderables[1] = (Renderable){ .color = BLUE, .radius = 10 };

    // Main loop
    while (!WindowShouldClose()) {
        // Animate parent
        transforms[0].rotation += 1.0f; // Rotate 1 degree per frame
        transforms[0].local_pos.x = 400 + 100 * sinf((float)GetTime()); // Oscillate x

        // Update transforms
        UpdateTransforms(&transforms[0], &transforms[1]);

        // Render
        RenderEntities(transforms, renderables, 2);
    }

    // Cleanup
    CloseWindow();
    return 0;
}