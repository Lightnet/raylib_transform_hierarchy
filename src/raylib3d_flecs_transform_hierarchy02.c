#include "raylib.h"
#include "raymath.h"
#include "flecs.h"

// Transform3D component
typedef struct {
    Vector3 position;         // Local position
    Quaternion rotation;      // Local rotation
    Vector3 scale;            // Local scale
    Matrix localMatrix;       // Local transform matrix
    Matrix worldMatrix;       // World transform matrix
} Transform3D;
ECS_COMPONENT_DECLARE(Transform3D);

// Pointer component for raylib Model
typedef struct {
    Model* model;             // Pointer to Model
} ModelComponent;
ECS_COMPONENT_DECLARE(ModelComponent);

// Update transform for all nodes
void UpdateTransformSystem(ecs_iter_t *it) {
    Transform3D *transforms = ecs_field(it, Transform3D, 0); // Field 0: Self Transform3D
    Transform3D *parent_transforms = ecs_field(it, Transform3D, 1); // Field 1: Parent Transform3D (optional)
    
    for (int i = 0; i < it->count; i++) {
        ecs_entity_t entity = it->entities[i];
        Transform3D *transform = &transforms[i];
        
        // Update local transform
        Matrix translation = MatrixTranslate(transform->position.x, transform->position.y, transform->position.z);
        Matrix rotation = QuaternionToMatrix(transform->rotation);
        Matrix scaling = MatrixScale(transform->scale.x, transform->scale.y, transform->scale.z);
        transform->localMatrix = MatrixMultiply(scaling, MatrixMultiply(rotation, translation));

        // Get entity name and position
        const char *entity_name = ecs_get_name(it->world, entity) ? ecs_get_name(it->world, entity) : "(unnamed)";
        Vector3 entity_pos = transform->position;
        
        // Print child position
        // printf("child %s position (%.2f, %.2f, %.2f)\n", 
        //     entity_name, entity_pos.x, entity_pos.y, entity_pos.z);
        
        // Check if parent data is available
        if (ecs_field_is_set(it, 1)) { // Parent term is set
            Transform3D *parent_transform = &parent_transforms[i];
            ecs_entity_t parent = ecs_get_parent(it->world, entity);
            const char *parent_name = ecs_get_name(it->world, parent) ? ecs_get_name(it->world, parent) : "(unnamed)";
            Vector3 parent_pos = parent_transform->position;
            transform->worldMatrix = MatrixMultiply(transform->localMatrix, parent_transform->worldMatrix);
            // printf("-parent %s: position (%.2f, %.2f, %.2f), child world pos (%.2f, %.2f, %.2f)\n", 
            //     parent_name, parent_pos.x, parent_pos.y, parent_pos.z,
            //     transform->worldMatrix.m12, transform->worldMatrix.m13, transform->worldMatrix.m14);
        } else {
            transform->worldMatrix = transform->localMatrix; // No parent, use local as world
            // printf("-parent: None\n");
        }
        // printf("\n");

        //ecs_modified_id(it->world, entity, ecs_id(Transform3D)); // Notify Flecs of update
    }
}

// Render begin system
void RenderBeginSystem(ecs_iter_t *it) {
    printf("begin render\n");
    BeginDrawing();
    ClearBackground(RAYWHITE);
}

// Render begin system
void BeginCameraSystem(ecs_iter_t *it) {
  printf("begin camera\n");
  Camera3D *camera = (Camera3D *)ecs_get_ctx(it->world);
  if (!camera) return;
  BeginMode3D(*camera);
}

// Render system
void RenderSystem(ecs_iter_t *it) {
    printf("camera render???\n");
    Transform3D *t = ecs_field(it, Transform3D, 0);
    ModelComponent *m = ecs_field(it, ModelComponent, 1);

    for (int i = 0; i < it->count; i++) {
        if (!ecs_is_valid(it->world, it->entities[i]) || !m[i].model) continue;
        m[i].model->transform = t[i].worldMatrix;
        bool isChild = ecs_has_pair(it->world, it->entities[i], EcsChildOf, EcsWildcard);
        DrawModel(*(m[i].model), (Vector3){0, 0, 0}, 1.0f, isChild ? BLUE : RED);
    }
    DrawGrid(10, 1.0f);

    Camera3D *camera = (Camera3D *)ecs_get_ctx(it->world);
    if (camera) {
        DrawText(TextFormat("Camera Pos: %.2f, %.2f, %.2f", 
            camera->position.x, camera->position.y, camera->position.z), 10, 90, 20, DARKGRAY);
        DrawText(TextFormat("Entities Rendered: %d", it->count), 10, 110, 20, DARKGRAY);
    }
}

// Render end system
void RenderEndSystem(ecs_iter_t *it) {
  printf("enddrawring\n");
    EndDrawing();
}

void EndCameraSystem(ecs_iter_t *it) {
  printf("end camera\n");
  Camera3D *camera = (Camera3D *)ecs_get_ctx(it->world);
  if (!camera) return;
  EndMode3D();
}

// Input handling system
void InputSystem(ecs_iter_t *it) {
    Transform3D *t = ecs_field(it, Transform3D, 0);
    float dt = GetFrameTime();
    static bool isMovementMode = true;
    static bool tabPressed = false;

    if (IsKeyPressed(KEY_TAB) && !tabPressed) {
        tabPressed = true;
        isMovementMode = !isMovementMode;
        printf("Toggled mode to: %s\n", isMovementMode ? "Movement" : "Rotation");
    }
    if (IsKeyReleased(KEY_TAB)) tabPressed = false;

    for (int i = 0; i < it->count; i++) {
        if (isMovementMode) {
            if (IsKeyDown(KEY_W)) t[i].position.z -= 2.0f * dt;
            if (IsKeyDown(KEY_S)) t[i].position.z += 2.0f * dt;
            if (IsKeyDown(KEY_A)) t[i].position.x -= 2.0f * dt;
            if (IsKeyDown(KEY_D)) t[i].position.x += 2.0f * dt;
        } else {
            float rotateSpeed = 90.0f;
            if (IsKeyDown(KEY_Q)) {
                Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 1, 0}, DEG2RAD * rotateSpeed * dt);
                t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
            }
            if (IsKeyDown(KEY_E)) {
                Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 1, 0}, -DEG2RAD * rotateSpeed * dt);
                t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
            }
            if (IsKeyDown(KEY_W)) {
                Quaternion rot = QuaternionFromAxisAngle((Vector3){1, 0, 0}, DEG2RAD * rotateSpeed * dt);
                t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
            }
            if (IsKeyDown(KEY_S)) {
                Quaternion rot = QuaternionFromAxisAngle((Vector3){1, 0, 0}, -DEG2RAD * rotateSpeed * dt);
                t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
            }
            if (IsKeyDown(KEY_A)) {
                Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 0, 1}, DEG2RAD * rotateSpeed * dt);
                t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
            }
            if (IsKeyDown(KEY_D)) {
                Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 0, 1}, -DEG2RAD * rotateSpeed * dt);
                t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
            }
        }
        if (IsKeyPressed(KEY_R)) {
            t[i].position = (Vector3){0.0f, 0.0f, 0.0f};
            t[i].rotation = QuaternionIdentity();
            t[i].scale = (Vector3){1.0f, 1.0f, 1.0f};
        }
        DrawText(TextFormat("Entity %s Pos: %.2f, %.2f, %.2f", 
            ecs_get_name(it->world, it->entities[i]) ? ecs_get_name(it->world, it->entities[i]) : "unnamed", 
            t[i].position.x, t[i].position.y, t[i].position.z), 10, 130 + i * 20, 20, DARKGRAY);
    }

    DrawText(isMovementMode ? "Mode: Movement (WASD)" : "Mode: Rotation (QWE/ASD)", 10, 10, 20, DARKGRAY);
    DrawText("Tab: Toggle Mode | R: Reset", 10, 30, 20, DARKGRAY);
    DrawFPS(10, 60);
}

// Dummy system
void Sys(ecs_iter_t *it) {
  (void)it;
  printf("System ran!\n");
}

int main(void) {
    InitWindow(800, 600, "Transform Hierarchy with Flecs v4.x");
    SetTargetFPS(60);

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT(world, Transform3D);
    ECS_COMPONENT(world, ModelComponent);

    // Define custom phases
    ecs_entity_t LogicUpdatePhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t BeginRenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t BeginCameraPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t UpdateCameraPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t CameraRenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t EndCameraPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t RenderScreenPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t EndRenderPhase = ecs_new_w_id(world, EcsPhase);

    // order phase not doing correct
    ecs_add_pair(world, LogicUpdatePhase, EcsDependsOn, EcsPreUpdate); // start game logics
    ecs_add_pair(world, BeginRenderPhase, EcsDependsOn, LogicUpdatePhase); // BeginDrawing

    ecs_add_pair(world, BeginCameraPhase, EcsDependsOn, BeginRenderPhase); // EcsOnUpdate, BeginMode3D
    ecs_add_pair(world, UpdateCameraPhase, EcsDependsOn, BeginCameraPhase); // EcsOnUpdate, BeginMode3D
    //ecs_add_pair(world, CameraRenderPhase, EcsDependsOn, UpdateCameraPhase); // camera render 3d model
    ecs_add_pair(world, EndCameraPhase, EcsDependsOn, UpdateCameraPhase); // EndMode3D
    //ecs_add_pair(world, RenderScreenPhase, EcsDependsOn, EndCameraPhase); // 2D Screen

    ecs_add_pair(world, EndRenderPhase, EcsDependsOn, EndCameraPhase); // EcsPostUpdate, EndDrawing

    // ecs_system_init(world, &(ecs_system_desc_t){
    //   .entity = ecs_entity(world, { .name = "LogicSystem", 
    //     .add = ecs_ids(ecs_dependson(LogicUpdatePhase)) 
    //   }),
    //   .callback = Sys
    // });

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, { .name = "InputSystem", .add = ecs_ids(ecs_dependson(LogicUpdatePhase)) }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },
            { .id = ecs_pair(EcsChildOf, EcsWildcard), .oper = EcsNot }
        },
        .callback = InputSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, { .name = "UpdateTransformSystem", .add = ecs_ids(ecs_dependson(LogicUpdatePhase)) }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },           // Term 0: Entity has Transform3D
            { .id = ecs_id(Transform3D), .src.id = EcsUp,              // Term 1: Parent has Transform3D
              .trav = EcsChildOf, .oper = EcsOptional }                // Traversal in ecs_term_t
        },
        .callback = UpdateTransformSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, { .name = "RenderBeginSystem", 
        .add = ecs_ids(ecs_dependson(BeginRenderPhase)) 
      }),
      .callback = RenderBeginSystem
    });

    //note this has be in order of the ECS since push into array. From I guess.
    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, { .name = "BeginCameraSystem", .add = ecs_ids(ecs_dependson(BeginCameraPhase)) }),
      .callback = BeginCameraSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, { .name = "RenderSystem", .add = ecs_ids(ecs_dependson(UpdateCameraPhase)) }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },
            { .id = ecs_id(ModelComponent), .src.id = EcsSelf }
        },
        .callback = RenderSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, { .name = "EndCameraSystem", .add = ecs_ids(ecs_dependson(EndCameraPhase)) }),
      .callback = EndCameraSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, { .name = "RenderEndSystem", .add = ecs_ids(ecs_dependson(EndRenderPhase)) }),
        .callback = RenderEndSystem
    });

    // Use default pipeline, rely on phase dependencies
    // Alternatively, define a custom pipeline if needed
    // ecs_entity_t pipeline = ecs_pipeline_init(world, &(ecs_pipeline_desc_t){
    //   .query.terms = {
    //     { .id = EcsSystem }, // mandatory
    //     //{ .id = LogicUpdatePhase },
    //     // { .id = BeginRenderPhase },
    //     // { .id = BeginCameraPhase },
    //     // { .id = CameraRenderPhase },
    //     // { .id = EndCameraPhase },
    //     // { .id = RenderScreenPhase },
    //     // { .id = EndRenderPhase }
    //   }
    // });
    // ecs_set_pipeline(world, pipeline);


    Camera3D camera = {
        .position = (Vector3){10.0f, 10.0f, 10.0f},
        .target = (Vector3){0.0f, 0.0f, 0.0f},
        .up = (Vector3){0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    ecs_set_ctx(world, &camera, NULL);

    Model cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    ecs_entity_t node1 = ecs_new(world);
    ecs_set_name(world, node1, "NodeParent");
    ecs_set(world, node1, Transform3D, {
        .position = (Vector3){0.0f, 0.0f, 0.0f},
        .rotation = QuaternionIdentity(),
        .scale = (Vector3){1.0f, 1.0f, 1.0f},
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity()
    });
    ecs_set(world, node1, ModelComponent, {&cube});
    printf("Node1 entity ID: %llu (%s)\n", (unsigned long long)node1, ecs_get_name(world, node1));

    ecs_entity_t node2 = ecs_new(world);
    ecs_set_name(world, node2, "NodeChild");
    ecs_set(world, node2, Transform3D, {
        .position = (Vector3){2.0f, 0.0f, 0.0f},
        .rotation = QuaternionIdentity(),
        .scale = (Vector3){0.5f, 0.5f, 0.5f},
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity()
    });
    ecs_add_pair(world, node2, EcsChildOf, node1);
    ecs_set(world, node2, ModelComponent, {&cube});
    printf("Node2 entity ID: %llu (%s)\n", (unsigned long long)node2, ecs_get_name(world, node2));

    while (!WindowShouldClose()) {
        ecs_progress(world, 0);
    }

    UnloadModel(cube);
    ecs_fini(world);
    CloseWindow();
    return 0;
}