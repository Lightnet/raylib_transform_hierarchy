#include "raylib.h"
#include "raymath.h"
#include "flecs.h"
#include <stdio.h>
#include "raygui.h"

// Define components (unchanged)
typedef struct {
    Vector2 local_pos;    // Local position relative to parent
    Vector2 world_pos;    // Computed world position
    Vector2 local_scale;  // Local scale relative to parent
    float local_rotation; // Local rotation in degrees
    float world_rotation; // Computed world rotation in degrees
    bool isDirty;        // Flag to indicate if transform needs updating
} transform_2d_t;
ECS_COMPONENT_DECLARE(transform_2d_t);

typedef struct {
    Color color;         // Color for rendering
    float radius;        // Radius of the circle
} circle_t;
ECS_COMPONENT_DECLARE(circle_t);

typedef struct {
    ecs_entity_t id;
} transform_2d_select_t;
ECS_COMPONENT_DECLARE(transform_2d_select_t);

float NormalizeAngle(float degrees) {
    degrees = fmodf(degrees, 360.0f);
    if (degrees > 180.0f) {
        degrees -= 360.0f;
    } else if (degrees < -180.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

// Helper function to rotate a Vector2 around the origin by an angle (in degrees)
Vector2 RotateVector2(Vector2 v, float degrees) {
    float radians = DEG2RAD * degrees;
    float cos_r = cosf(radians);
    float sin_r = sinf(radians);
    return (Vector2){
        v.x * cos_r - v.y * sin_r,
        v.x * sin_r + v.y * cos_r
    };
}

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

    // Compute world transform
    if (!parent || !ecs_is_valid(world, parent)) {
        // Root entity: world = local
        transform->world_pos = transform->local_pos;
        transform->world_rotation = NormalizeAngle(transform->local_rotation); // Normalize
    } else {
        // Child entity: compute world position and rotation
        const transform_2d_t *parent_transform = ecs_get(world, parent, transform_2d_t);
        if (!parent_transform) {
            transform->world_pos = transform->local_pos;
            transform->world_rotation = NormalizeAngle(transform->local_rotation); // Normalize
            return;
        }

        // Validate parent world position
        if (fabs(parent_transform->world_pos.x) > 1e6 || fabs(parent_transform->world_pos.y) > 1e6) {
            transform->world_pos = transform->local_pos;
            transform->world_rotation = NormalizeAngle(transform->local_rotation); // Normalize
            return;
        }

        // Rotate local_pos around origin by parent's world_rotation, then translate by parent's world_pos
        transform->world_pos = RotateVector2(transform->local_pos, parent_transform->world_rotation);
        transform->world_pos = Vector2Add(transform->world_pos, parent_transform->world_pos);
        // Combine rotations and normalize
        transform->world_rotation = NormalizeAngle(transform->local_rotation + parent_transform->world_rotation);
    }

    // Mark children as dirty
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

    // Reset isDirty
    transform->isDirty = false;
}


// Function to update a single entity and its descendants
void UpdateChildTransform2DOnly(ecs_world_t *world, ecs_entity_t entity) {
    transform_2d_t *transform = ecs_get_mut(world, entity, transform_2d_t);
    if (!transform) return;

    UpdateTransform2D(world, entity, transform);
    ecs_modified(world, entity, transform_2d_t);

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

    // Update root entity's transform
    for (int i = 0; i < it->count; i++) {
        ecs_entity_t current_transform = it->entities[i];
        if (!ecs_is_valid(it->world, current_transform)) {
            continue;
        }

        transform_2d_t *transform = &transforms[i];
        if (!ecs_get_parent(it->world, current_transform)) {
            // transform->local_rotation = 60.0f * GetTime(); // Rotate at 60 degrees per second
            transform->local_rotation = NormalizeAngle(60.0f * GetTime()); // Rotate at 60 degrees per second, normalized
            transform->local_pos.x = 400 + 100 * sinf((float)GetTime()); // Oscillate horizontally
            transform->local_pos.y = 300;
            transform->isDirty = true;
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

// render 2d circle
void RenderObjectsSystem(ecs_iter_t *it) {
    transform_2d_t *t = ecs_field(it, transform_2d_t, 0);
    circle_t *r = ecs_field(it, circle_t, 1);

    for (int i = 0; i < it->count; i++) {
        // Use world position and rotation directly
        Vector2 world_pos = t[i].world_pos;
        float world_rotation = t[i].world_rotation;
        float scale_x = t[i].local_scale.x; // Use local scale (world scale could be computed if needed)

        // Draw circle with scaled radius
        DrawCircleV(world_pos, r[i].radius * scale_x, r[i].color);
        // Draw rotation indicator
        Vector2 end = {
            world_pos.x + (r[i].radius * scale_x) * cosf(DEG2RAD * world_rotation),
            world_pos.y + (r[i].radius * scale_x) * sinf(DEG2RAD * world_rotation)
        };
        DrawLineV(world_pos, end, BLACK);

        // Debug output
        printf("Entity %llu: world_pos = (%.2f, %.2f), world_rotation = %.2f\n", 
               it->entities[i], world_pos.x, world_pos.y, world_rotation);
    }
}

// end render 2d
void RenderEndSystem(ecs_iter_t *it) {
    DrawFPS(10, 10);
    EndDrawing();
}

// 
void render_2d_gui_list_system(ecs_iter_t *it) {
    // Singleton
    transform_2d_select_t *transform_2d_select = ecs_field(it, transform_2d_select_t, 0);  // Field index 0

    // Create a query for all entities with transform_2d_t
    ecs_query_t *query = ecs_query(it->world, {
        .terms = {{ ecs_id(transform_2d_t) }}
    });

    int entity_count = 0;
    ecs_iter_t transform_it = ecs_query_iter(it->world, query);
    while (ecs_query_next(&transform_it)) {
        entity_count += transform_it.count;
    }

    // Allocate buffers for entity names and IDs
    ecs_entity_t *entity_ids = (ecs_entity_t *)RL_MALLOC(entity_count * sizeof(ecs_entity_t));
    char **entity_names = (char **)RL_MALLOC(entity_count * sizeof(char *));
    int index = 0;

    // Populate entity names and IDs
    transform_it = ecs_query_iter(it->world, query);
    while (ecs_query_next(&transform_it)) {
        for (int j = 0; j < transform_it.count; j++) {
            const char *name = ecs_get_name(it->world, transform_it.entities[j]);
            entity_names[index] = (char *)RL_MALLOC(256 * sizeof(char));
            snprintf(entity_names[index], 256, "%s", name ? name : "(unnamed)");
            entity_ids[index] = transform_it.entities[j];
            index++;
        }
    }

    // Create a single string for GuiListView
    char *name_list = (char *)RL_MALLOC(entity_count * 256 * sizeof(char));
    name_list[0] = '\0';
    for (int j = 0; j < entity_count; j++) {
        if (j > 0) strcat(name_list, ";");
        strcat(name_list, entity_names[j]);
    }

    // Draw the list view on the right side
    Rectangle list_rect = {520, 10, 240, 200};
    static int scroll_index = 0;
    static int selected_index = -1;

    GuiGroupBox(list_rect, "Entity List");
    GuiListView(list_rect, name_list, &scroll_index, &selected_index);

    // Draw transform controls if an entity is selected
    if (selected_index >= 0 && selected_index < entity_count && ecs_is_valid(it->world, entity_ids[selected_index])) {
        transform_2d_select->id = entity_ids[selected_index];
        transform_2d_t *transform = ecs_get_mut(it->world, transform_2d_select->id, transform_2d_t);
        bool modified = false;

        if (transform) {
            Rectangle control_rect = {520, 220, 240, 240};
            GuiGroupBox(control_rect, "Transform Controls");

            // Position controls
            GuiLabel((Rectangle){530, 230, 100, 20}, "Position");
            float new_pos_x = transform->local_pos.x;
            float new_pos_y = transform->local_pos.y;
            GuiSlider((Rectangle){530, 250, 200, 20}, "X", TextFormat("%.2f", new_pos_x), &new_pos_x, -800.0f, 800.0f);
            GuiSlider((Rectangle){530, 270, 200, 20}, "Y", TextFormat("%.2f", new_pos_y), &new_pos_y, -600.0f, 600.0f);
            if (new_pos_x != transform->local_pos.x || new_pos_y != transform->local_pos.y) {
                transform->local_pos.x = new_pos_x;
                transform->local_pos.y = new_pos_y;
                modified = true;
            }

            // Rotation controls
            GuiLabel((Rectangle){530, 290, 100, 20}, "Rotation");
            float new_rotation = transform->local_rotation;
            GuiSlider((Rectangle){530, 310, 200, 20}, "Angle", TextFormat("%.2f", new_rotation), &new_rotation, -180.0f, 180.0f);
            if (new_rotation != transform->local_rotation) {
                // transform->local_rotation = new_rotation;
                transform->local_rotation = NormalizeAngle(new_rotation); // Normalize
                modified = true;
            }

            // Scale controls
            GuiLabel((Rectangle){530, 330, 100, 20}, "Scale");
            float new_scale_x = transform->local_scale.x;
            float new_scale_y = transform->local_scale.y;
            GuiSlider((Rectangle){530, 350, 200, 20}, "X", TextFormat("%.2f", new_scale_x), &new_scale_x, 0.1f, 5.0f);
            GuiSlider((Rectangle){530, 370, 200, 20}, "Y", TextFormat("%.2f", new_scale_y), &new_scale_y, 0.1f, 5.0f);
            if (new_scale_x != transform->local_scale.x || new_scale_y != transform->local_scale.y) {
                transform->local_scale.x = new_scale_x;
                transform->local_scale.y = new_scale_y;
                modified = true;
            }

            // Mark transform and descendants as dirty if modified
            if (modified) {
                transform->isDirty = true;
            }
        }
    }

    // Clean up
    for (int j = 0; j < entity_count; j++) {
        RL_FREE(entity_names[j]);
    }
    RL_FREE(entity_names);
    RL_FREE(entity_ids);
    RL_FREE(name_list);
    ecs_query_fini(query);
}




// main
int main() {
    InitWindow(800, 600, "Flecs + raylib Matrix Transform Test");
    SetTargetFPS(60);

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_DEFINE(world, transform_2d_t);
    ECS_COMPONENT_DEFINE(world, circle_t);
    ECS_COMPONENT_DEFINE(world, transform_2d_select_t);

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

    ecs_system(world, {
        .entity = ecs_entity(world, { .name = "render_2d_gui_list_system", .add = ecs_ids(ecs_dependson(RenderPhase)) }),
        .query.terms = {
            { .id = ecs_id(transform_2d_select_t), .src.id = ecs_id(transform_2d_select_t) } // Singleton
        },
        .callback = render_2d_gui_list_system
    });

    // Create entities
    ecs_entity_t parent = ecs_new(world);
    ecs_set_name(world,parent,"node_parent");
    ecs_set(world, parent, transform_2d_t, {
        .local_pos = {400, 300}, 
        .world_pos = {400, 300},
        .local_scale = {1, 1},
        .local_rotation = 0,
        .world_rotation = 0,
        .isDirty = true
    });
    ecs_set(world, parent, circle_t, {
        .color = RED, 
        .radius = 20 
    });

    ecs_singleton_set(world, transform_2d_select_t, {
        .id = parent
    });

    ecs_entity_t child = ecs_new(world);
    ecs_set_name(world, child, "node_child");
    ecs_add_pair(world, child, EcsChildOf, parent);
    ecs_set(world, child, transform_2d_t, {
        .local_pos = {50, 0},
        .world_pos = {0, 0}, // Will be computed
        .local_scale = {1, 1},
        .local_rotation = 0,
        .world_rotation = 0,
        .isDirty = true
    });
    ecs_set(world, child, circle_t, {
        .color = BLUE,
        .radius = 20 
    });

    ecs_entity_t grandchild = ecs_new(world);
    ecs_set_name(world, grandchild, "node_grandchild");
    ecs_add_pair(world, grandchild, EcsChildOf, child);
    ecs_set(world, grandchild, transform_2d_t, {
        .local_pos = {25, 50},
        .world_pos = {0, 0}, // Will be computed
        .local_scale = {1, 1},
        .local_rotation = 0,
        .world_rotation = 0,
        .isDirty = true
    });
    ecs_set(world, grandchild, circle_t, {
        .color = GREEN,
        .radius = 5
    });


    // Initial positions (for debugging)
    const transform_2d_t *parent_t = ecs_get(world, parent, transform_2d_t);
    const transform_2d_t *child_t = ecs_get(world, child, transform_2d_t);
    const transform_2d_t *grandchild_t = ecs_get(world, grandchild, transform_2d_t);
    printf("Initial Parent (ID %llu): local_pos = (%.2f, %.2f), rotation = %.2f\n", 
           parent, parent_t->local_pos.x, parent_t->local_pos.y, parent_t->local_rotation);
    printf("Initial Child (ID %llu): local_pos = (%.2f, %.2f), rotation = %.2f\n", 
           child, child_t->local_pos.x, child_t->local_pos.y, child_t->local_rotation);
    printf("Initial Grandchild (ID %llu): local_pos = (%.2f, %.2f), rotation = %.2f\n", 
           grandchild, grandchild_t->local_pos.x, grandchild_t->local_pos.y, grandchild_t->local_rotation);

    while (!WindowShouldClose()) {
        ecs_progress(world, GetFrameTime());
    }

    ecs_fini(world);
    CloseWindow();
    return 0;
}