#include "raylib.h"
#include "raymath.h"
#include "flecs.h"
#include <stdio.h>

// Define components (unchanged)
typedef struct {
    Vector2 local_pos;   // Local position relative to parent
    Vector2 local_scale; // Local scale relative to parent
    float rotation;      // Local rotation in degrees
    Matrix local_matrix; // Local transformation matrix
    Matrix world_matrix; // Computed world transformation matrix
    bool isDirty;       // Flag to indicate if transform needs updating
} transform_2d_t;
ECS_COMPONENT_DECLARE(transform_2d_t);

typedef struct {
    Color color;         // Color for rendering
    float radius;        // Radius of the circle
} circle_t;
ECS_COMPONENT_DECLARE(circle_t);

// Helper function to update a single transform
void UpdateTransform2D(ecs_world_t *world, ecs_entity_t entity, transform_2d_t *transform) {
    // Check if parent is dirty
    ecs_entity_t parent = ecs_get_parent(world, entity);
    bool parentIsDirty = false;
    if (parent && ecs_is_valid(world, parent)) {
        const transform_2d_t *parent_transform = ecs_get(world, parent, transform_2d_t);
        if (parent_transform && parent_transform->isDirty) {
            parentIsDirty = true;
        }
    }

    // Skip update if neither this transform nor its parent is dirty
    if (!transform->isDirty && !parentIsDirty) {
        return;
    }

    // Compute local transformation matrix (scale -> rotation -> translation)
    Matrix scale = MatrixScale(transform->local_scale.x, transform->local_scale.y, 1);
    Matrix rotation = MatrixRotate((Vector3){0, 0, 1}, DEG2RAD * transform->rotation);
    Matrix translation = MatrixTranslate(transform->local_pos.x, transform->local_pos.y, 0);
    transform->local_matrix = MatrixMultiply(MatrixMultiply(scale, rotation), translation);

    // Compute world matrix
    if (!parent || !ecs_is_valid(world, parent)) {
        // Root entity: world matrix = local matrix
        transform->world_matrix = transform->local_matrix;
    } else {
        // Child entity: world matrix = local matrix * parent world matrix
        const transform_2d_t *parent_transform = ecs_get(world, parent, transform_2d_t);
        if (!parent_transform) {
            transform->world_matrix = transform->local_matrix;
            return;
        }

        // Validate parent world matrix (prevent extreme values)
        float px = parent_transform->world_matrix.m12;
        float py = parent_transform->world_matrix.m13;
        if (fabs(px) > 1e6 || fabs(py) > 1e6) {
            transform->world_matrix = transform->local_matrix;
            return;
        }

        // Compute world matrix (local_matrix * parent.world_matrix for 2D hierarchy)
        transform->world_matrix = MatrixMultiply(transform->local_matrix, parent_transform->world_matrix);
    }

    // Mark children as dirty to ensure they update
    ecs_iter_t it = ecs_children(world, entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            transform_2d_t *child_transform = ecs_get_mut(world, it.entities[i], transform_2d_t);
            if (child_transform) {
                child_transform->isDirty = true;
                ecs_modified(world, it.entities[i], transform_2d_t);
            }
        }
    }

    // Reset isDirty after updating
    transform->isDirty = false;
}

// Function to update a single entity and its descendants
void UpdateChildTransform2DOnly(ecs_world_t *world, ecs_entity_t entity) {
    transform_2d_t *transform = ecs_get_mut(world, entity, transform_2d_t);
    if (!transform) return;

    // Update the entity's transform
    UpdateTransform2D(world, entity, transform);
    ecs_modified(world, entity, transform_2d_t);

    // Recursively update descendants
    ecs_iter_t it = ecs_children(world, entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            UpdateChildTransform2DOnly(world, it.entities[i]);
        }
    }
}

// System to process all entities with transform_2d_t
void ChildTransformSystem(ecs_iter_t *it) {
    transform_2d_t *transforms = ecs_field(it, transform_2d_t, 0);

    // Update root entity's transform (dynamic updates for parent)
    for (int i = 0; i < it->count; i++) {
        ecs_entity_t current_transform = it->entities[i];
        if (!ecs_is_valid(it->world, current_transform)) {
            continue;
        }

        transform_2d_t *transform = &transforms[i];
        if (!ecs_get_parent(it->world, current_transform)) {
            // Root entity (parent) gets dynamic updates
            transform->rotation = 60.0f * GetTime(); // Rotate at 60 degrees per second
            // transform->local_pos.x = 400 + 100 * sinf((float)GetTime()); // Oscillate horizontally
            transform->local_pos.x = 400;
            transform->local_pos.y = 300; // Fixed Y position
            transform->isDirty = true; // Mark as dirty to trigger update
        }
    }

    // Update all entities hierarchically
    for (int i = 0; i < it->count; i++) {
        ecs_entity_t current_transform = it->entities[i];
        if (!ecs_is_valid(it->world, current_transform)) {
            continue;
        }
        UpdateChildTransform2DOnly(it->world, current_transform);
    }
}


// Render systems (unchanged)
void RenderBeginSystem(ecs_iter_t *it) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
}

void RenderObjectsSystem(ecs_iter_t *it) {
    transform_2d_t *t = ecs_field(it, transform_2d_t, 0);
    circle_t *r = ecs_field(it, circle_t, 1);

    for (int i = 0; i < it->count; i++) {
        // Extract world position from world_matrix
        Vector2 world_pos = { t[i].world_matrix.m12, t[i].world_matrix.m13 };
        // Extract rotation from world_matrix (in radians, convert to degrees)
        // Use -m4 to correct the rotation direction
        float world_rotation = atan2f(-t[i].world_matrix.m4, t[i].world_matrix.m0) * RAD2DEG;
        // Extract scale from world_matrix
        float scale_x = sqrtf(t[i].world_matrix.m0 * t[i].world_matrix.m0 + t[i].world_matrix.m1 * t[i].world_matrix.m1);
        
        // Draw circle with scaled radius
        DrawCircleV(world_pos, r[i].radius * scale_x, r[i].color);
        // Draw rotation indicator
        Vector2 end = {
            world_pos.x + (r[i].radius * scale_x) * cosf(DEG2RAD * world_rotation),
            world_pos.y + (r[i].radius * scale_x) * sinf(DEG2RAD * world_rotation)
        };
        DrawLineV(world_pos, end, BLACK);

        // Debug output to verify positions and rotations
        printf("Entity %llu: world_pos = (%.2f, %.2f), world_rotation = %.2f\n", 
               it->entities[i], world_pos.x, world_pos.y, world_rotation);
    }
}


void RenderEndSystem(ecs_iter_t *it) {
    DrawFPS(10, 10);
    EndDrawing();
}

int main() {
    InitWindow(800, 600, "Flecs + raylib Matrix Transform Test");
    SetTargetFPS(60);

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_DEFINE(world, transform_2d_t);
    ECS_COMPONENT_DEFINE(world, circle_t);

    ecs_entity_t BeginRenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t RenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t EndRenderPhase = ecs_new_w_id(world, EcsPhase);

    ecs_add_pair(world, BeginRenderPhase, EcsDependsOn, EcsPreUpdate);
    ecs_add_pair(world, RenderPhase, EcsDependsOn, BeginRenderPhase);
    ecs_add_pair(world, EndRenderPhase, EcsDependsOn, RenderPhase);

    // Render systems (unchanged)
    ecs_system(world, {
        .entity = ecs_entity(world, { 
            .name = "RenderBeginSystem",
            .add = ecs_ids(ecs_dependson(BeginRenderPhase))
        }),
        .callback = RenderBeginSystem
    });

    ecs_system(world, {
        .entity = ecs_entity(world, { 
            .name = "RenderEndSystem",
            .add = ecs_ids(ecs_dependson(EndRenderPhase))
        }),
        .callback = RenderEndSystem
    });

    ecs_system(world, {
        .entity = ecs_entity(world, { 
            .name = "RenderObjectsSystem", 
            .add = ecs_ids(ecs_dependson(RenderPhase)) 
        }),
        .query.terms = {
            { .id = ecs_id(transform_2d_t) },
            { .id = ecs_id(circle_t) }
        },
        .callback = RenderObjectsSystem
    });

    // Transform system
    ecs_system(world, {
        .entity = ecs_entity(world, { 
            .name = "ChildTransformSystem", 
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)) 
        }),
        .query.terms = {
            { .id = ecs_id(transform_2d_t) }
        },
        .callback = ChildTransformSystem
    });

    // Create entities
    ecs_entity_t parent = ecs_new(world);
    ecs_set(world, parent, transform_2d_t, {
        .local_pos = {400, 300}, 
        .local_scale = {1, 1},
        .rotation = 0,
        .local_matrix = MatrixIdentity(),
        .world_matrix = MatrixIdentity(),
        .isDirty = true // Initialize as dirty to trigger initial update
    });
    ecs_set(world, parent, circle_t, {
        .color = RED, 
        .radius = 20 
    });

    ecs_entity_t child = ecs_new(world);
    ecs_add_pair(world, child, EcsChildOf, parent);
    ecs_set(world, child, transform_2d_t, {
        .local_pos = {50, 0},
        .local_scale = {1, 1},
        .rotation = 0,
        .local_matrix = MatrixIdentity(),
        .world_matrix = MatrixIdentity(),
        .isDirty = true // Initialize as dirty
    });
    ecs_set(world, child, circle_t, {
        .color = BLUE,
        .radius = 20 
    });

    ecs_entity_t grandchild = ecs_new(world);
    ecs_add_pair(world, grandchild, EcsChildOf, child);
    ecs_set(world, grandchild, transform_2d_t, {
        .local_pos = {25, 50},
        .local_scale = {1, 1},
        .rotation = 0,
        .local_matrix = MatrixIdentity(),
        .world_matrix = MatrixIdentity(),
        .isDirty = true // Initialize as dirty
    });
    ecs_set(world, grandchild, circle_t, {
        .color = GREEN,
        .radius = 5
    });


    // Initial positions (for debugging, unchanged)
    const transform_2d_t *parent_t = ecs_get(world, parent, transform_2d_t);
    const transform_2d_t *child_t = ecs_get(world, child, transform_2d_t);
    const transform_2d_t *grandchild_t = ecs_get(world, grandchild, transform_2d_t);
    printf("Initial Parent (ID %llu): local_pos = (%f, %f), rotation = %f\n", 
           parent, parent_t->local_pos.x, parent_t->local_pos.y, parent_t->rotation);
    printf("Initial Child (ID %llu): local_pos = (%f, %f), rotation = %f\n", 
           child, child_t->local_pos.x, child_t->local_pos.y, child_t->rotation);
    printf("Initial Grandchild (ID %llu): local_pos = (%f, %f), rotation = %f\n", 
           grandchild, grandchild_t->local_pos.x, grandchild_t->local_pos.y, grandchild_t->rotation);

    while (!WindowShouldClose()) {
        ecs_progress(world, GetFrameTime());
    }

    ecs_fini(world);
    CloseWindow();
    return 0;
}