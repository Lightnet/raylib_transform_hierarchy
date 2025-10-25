#include "raylib.h"    // raylib 5.5
#include "raymath.h"   // raymath from raylib 5.5
#include "flecs.h"     // Flecs 4.1.1
#include <stdio.h>

// Define components
typedef struct {
    Vector2 local_pos;   // Local position relative to parent
    Vector2 world_pos;   // Computed world position
    float rotation;      // Local rotation in degrees
} transform_2d_t;
ECS_COMPONENT_DECLARE(transform_2d_t);

typedef struct {
    Color color;         // Color for rendering
    float radius;        // Radius of the circle
} Renderable;
ECS_COMPONENT_DECLARE(Renderable);

// Child transform system (updates children based on parents)
void ChildTransformSystem(ecs_iter_t *it) {
    transform_2d_t *child_t = ecs_field(it, transform_2d_t, 0); // Child's FTransform

    //printf("ChildTransformSystem: count = %d\n", it->count);
    for (int i = 0; i < it->count; i++) {
        ecs_entity_t child = it->entities[i];
        if (!ecs_is_valid(it->world, child)) {
            // printf("Invalid child entity: %llu\n", child);
            continue;
        }

        ecs_entity_t parent = ecs_get_target(it->world, child, EcsChildOf, 0);
        if (!ecs_is_valid(it->world, parent)) {
            // printf("Invalid parent for child %llu: %llu\n", child, parent);
            continue;
        }

        transform_2d_t *t_mut = ecs_get_mut(it->world, child, transform_2d_t);
        const transform_2d_t *parent_t = ecs_get(it->world, parent, transform_2d_t);
        if (!parent_t) {
            // printf("Parent %llu has no FTransform for child %llu\n", parent, child);
            continue;
        }

        // printf("Child %llu: Before - local_pos = (%f, %f), world_pos = (%f, %f), rotation = %f, parent = %llu\n", 
        //        child, child_t[i].local_pos.x, child_t[i].local_pos.y, child_t[i].world_pos.x, child_t[i].world_pos.y, child_t[i].rotation, parent);
        
        float parent_rot_rad = DEG2RAD * parent_t->rotation;
        Vector2 rotated_local = {
            child_t[i].local_pos.x * cosf(parent_rot_rad) - child_t[i].local_pos.y * sinf(parent_rot_rad),
            child_t[i].local_pos.x * sinf(parent_rot_rad) + child_t[i].local_pos.y * cosf(parent_rot_rad)
        };
        t_mut->world_pos = Vector2Add(parent_t->world_pos, rotated_local);
        t_mut->rotation = parent_t->rotation; // Inherit parent's rotation
        
        // printf("Child %llu: After - world_pos = (%f, %f), rotation = %f\n", 
        //        child, t_mut->world_pos.x, t_mut->world_pos.y, t_mut->rotation);
        
        // ecs_modified_id(it->world, child, FTransform_id);
    }
}

// Render begin system (PreUpdate)
void RenderBeginSystem(ecs_iter_t *it) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
}

// Parent render system (OnUpdate)
void RenderObjectsSystem(ecs_iter_t *it) {
    transform_2d_t *t = ecs_field(it, transform_2d_t, 0);
    Renderable *r = ecs_field(it, Renderable, 1);

    //printf("ParentRenderSystem: count = %d\n", it->count);
    for (int i = 0; i < it->count; i++) {
        // printf("Parent Entity %d (ID %llu): world_pos = (%f, %f), rotation = %f\n", 
        //        i, it->entities[i], t[i].world_pos.x, t[i].world_pos.y, t[i].rotation);
        DrawCircleV(t[i].world_pos, r[i].radius, r[i].color);
        Vector2 end = {
            t[i].world_pos.x + r[i].radius * cosf(DEG2RAD * t[i].rotation),
            t[i].world_pos.y + r[i].radius * sinf(DEG2RAD * t[i].rotation)
        };
        DrawLineV(t[i].world_pos, end, BLACK);
    }
}

// Render end system (PostUpdate)
void RenderEndSystem(ecs_iter_t *it) {
    DrawFPS(10, 10);
    EndDrawing();
}

int main() {
    InitWindow(800, 600, "Flecs + raylib FTransform Test");
    SetTargetFPS(60);

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_DEFINE(world, transform_2d_t);
    ECS_COMPONENT_DEFINE(world, Renderable);

    ecs_entity_t BeginRenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t RenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t EndRenderPhase = ecs_new_w_id(world, EcsPhase);

    ecs_add_pair(world, BeginRenderPhase, EcsDependsOn, EcsPreUpdate); // 
    ecs_add_pair(world, RenderPhase, EcsDependsOn, BeginRenderPhase); // 
    ecs_add_pair(world, EndRenderPhase, EcsDependsOn, RenderPhase); // 

    // render
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
            { .id = ecs_id(Renderable) }
        },
        .callback = RenderObjectsSystem
    });

    // Systems
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
    ecs_set(world,parent,transform_2d_t,{
        .local_pos = {400, 300}, 
        .world_pos = {400, 300}, 
        .rotation = 0
    });
    ecs_set(world,parent, Renderable,{
        .color = RED, 
        .radius = 20 
    });

    ecs_entity_t child = ecs_new(world);
    ecs_add_pair(world, child, EcsChildOf, parent);
    ecs_set(world, child, transform_2d_t,{
        .local_pos = {50, 0},
        .world_pos = {400, 300}, 
        .rotation = 0
    });
    ecs_set(world, child, Renderable,{
        .color = BLUE,
        .radius = 20 
    });

    ecs_entity_t grandchild = ecs_new(world);
    ecs_add_pair(world, grandchild, EcsChildOf, child);
    ecs_set(world, grandchild, transform_2d_t,{
        .local_pos = {25, 50}, .world_pos = {0, 0}, .rotation = 0
    });
    ecs_set(world, grandchild, Renderable,{
        .color = GREEN, .radius = 5
    });

    // Initial positions
    const transform_2d_t *parent_t = ecs_get(world, parent, transform_2d_t);
    const transform_2d_t *child_t = ecs_get(world, child, transform_2d_t);
    const transform_2d_t *grandchild_t = ecs_get(world, grandchild, transform_2d_t);
    printf("Initial Parent (ID %llu): local_pos = (%f, %f), world_pos = (%f, %f), rotation = %f\n", 
           parent, parent_t->local_pos.x, parent_t->local_pos.y, parent_t->world_pos.x, parent_t->world_pos.y, parent_t->rotation);
    printf("Initial Child (ID %llu): local_pos = (%f, %f), world_pos = (%f, %f), rotation = %f\n", 
           child, child_t->local_pos.x, child_t->local_pos.y, child_t->world_pos.x, child_t->world_pos.y, child_t->rotation);
    printf("Initial Grandchild (ID %llu): local_pos = (%f, %f), world_pos = (%f, %f), rotation = %f\n", 
           grandchild, grandchild_t->local_pos.x, grandchild_t->local_pos.y, grandchild_t->world_pos.x, grandchild_t->world_pos.y, grandchild_t->rotation);

    while (!WindowShouldClose()) {
        transform_2d_t *parent_t_mut = ecs_get_mut(world, parent, transform_2d_t);
        parent_t_mut->rotation += 1.0f;
        parent_t_mut->local_pos.x = 400 + 100 * sinf((float)GetTime());
        parent_t_mut->world_pos = parent_t_mut->local_pos; // Update world_pos directly for root

        ecs_progress(world, GetFrameTime());
    }

    ecs_fini(world);
    CloseWindow();
    return 0;
}