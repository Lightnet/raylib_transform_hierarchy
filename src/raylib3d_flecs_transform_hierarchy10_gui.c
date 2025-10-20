// flecs v4.1.1
// raylib 5.5
// raygui 4.0

// about 97 lines for transform 3d hierarchy functions.

#include <stdio.h>
#include "raylib.h"
#include "raymath.h"
#include "flecs.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// Transform3D component
typedef struct {
    Vector3 position;         // Local position
    Quaternion rotation;      // Local rotation
    Vector3 scale;            // Local scale
    Matrix localMatrix;       // Local transform matrix
    Matrix worldMatrix;       // World transform matrix
    bool isDirty;             // Flag to indicate if transform needs updating
} Transform3D;
ECS_COMPONENT_DECLARE(Transform3D);

// Pointer component for raylib Model
typedef struct {
    Model* model;             // Pointer to Model
} model_component_t;
ECS_COMPONENT_DECLARE(model_component_t);

typedef struct {
    ecs_entity_t id;  // Entity to edit (e.g., cube with CubeWire)
    int selectedIndex; // Index of the selected entity in the list
    // Later: Add fields for other GUI controls
} transform_3d_gui_t;
ECS_COMPONENT_DECLARE(transform_3d_gui_t);

typedef struct {
  bool isMovementMode;
  bool tabPressed;
  bool moveForward;
  bool moveBackward;
  bool moveLeft;
  bool moveRight;
} player_input_t;
ECS_COMPONENT_DECLARE(player_input_t);

typedef struct {
    Camera3D camera;
} main_context_t;
ECS_COMPONENT_DECLARE(main_context_t);

void transform_3D_gui_list_system(ecs_iter_t *it);

// Helper function to update a single transform
void UpdateTransform(ecs_world_t *world, ecs_entity_t entity, Transform3D *transform) {
    // Get parent entity
    ecs_entity_t parent = ecs_get_parent(world, entity);
    // const char *name = ecs_get_name(world, entity) ? ecs_get_name(world, entity) : "(unnamed)"; //check name for debug
    bool parentIsDirty = false;

    // Check if parent is dirty
    if (parent && ecs_is_valid(world, parent)) {
        const Transform3D *parent_transform = ecs_get(world, parent, Transform3D);
        if (parent_transform && parent_transform->isDirty) {
            parentIsDirty = true;
        }
    }

    // Skip update if neither this transform nor its parent is dirty
    if (!transform->isDirty && !parentIsDirty) {
        // printf("Skipping update for %s (not dirty)\n", name);
        return;
    }

    // Compute local transform
    Matrix translation = MatrixTranslate(transform->position.x, transform->position.y, transform->position.z);
    Matrix rotation = QuaternionToMatrix(transform->rotation);
    Matrix scaling = MatrixScale(transform->scale.x, transform->scale.y, transform->scale.z);
    transform->localMatrix = MatrixMultiply(scaling, MatrixMultiply(rotation, translation));

    if (!parent || !ecs_is_valid(world, parent)) {
        // Root entity: world matrix = local matrix
        transform->worldMatrix = transform->localMatrix;
        //   printf("Root %s position (%.2f, %.2f, %.2f)\n", name, transform->position.x, transform->position.y, transform->position.z);
    } else {
        // Child entity: world matrix = local matrix * parent world matrix
        const Transform3D *parent_transform = ecs_get(world, parent, Transform3D);
        if (!parent_transform) {
            //   printf("Error: Parent %s lacks Transform3D for %s\n",
            //          ecs_get_name(world, parent) ? ecs_get_name(world, parent) : "(unnamed)", name);
            transform->worldMatrix = transform->localMatrix;
            return;
        }

        // Validate parent world matrix
        float px = parent_transform->worldMatrix.m12;
        float py = parent_transform->worldMatrix.m13;
        float pz = parent_transform->worldMatrix.m14;
        if (fabs(px) > 1e6 || fabs(py) > 1e6 || fabs(pz) > 1e6) {
            //   printf("Error: Invalid parent %s world pos (%.2f, %.2f, %.2f) for %s\n",
            //          ecs_get_name(world, parent) ? ecs_get_name(world, parent) : "(unnamed)",
            //          px, py, pz, name);
            transform->worldMatrix = transform->localMatrix;
            return;
        }

        // Compute world matrix
        transform->worldMatrix = MatrixMultiply(transform->localMatrix, parent_transform->worldMatrix);
        // Extract world position
        float wx = transform->worldMatrix.m12;
        float wy = transform->worldMatrix.m13;
        float wz = transform->worldMatrix.m14;

        // Debug output
        //   const char *parent_name = ecs_get_name(world, parent) ? ecs_get_name(world, parent) : "(unnamed)";
        //   printf("Child %s (ID: %llu), parent %s (ID: %llu)\n",
        //          name, (unsigned long long)entity, parent_name, (unsigned long long)parent);
        //   printf("Child %s position (%.2f, %.2f, %.2f), parent %s world pos (%.2f, %.2f, %.2f), world pos (%.2f, %.2f, %.2f)\n",
        //          name, transform->position.x, transform->position.y, transform->position.z,
        //          parent_name, px, py, pz, wx, wy, wz);
    }
    // Mark children as dirty to ensure they update in the next frame
    ecs_iter_t it = ecs_children(world, entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            Transform3D *child_transform = ecs_get_mut(world, it.entities[i], Transform3D);
            if (child_transform) {
                child_transform->isDirty = true;
                //ecs_set(world, it.entities[i], Transform3D, *child_transform);
            }
        }
    }
    // Reset isDirty after updating
    transform->isDirty = false;
}

// Function to update a single entity and its descendants
void UpdateChildTransformOnly(ecs_world_t *world, ecs_entity_t entity) {
    Transform3D *transform = ecs_get_mut(world, entity, Transform3D);
    if (!transform) return;
    // Update the entity's transform
    UpdateTransform(world, entity, transform);
    // ecs_modified(world, entity, Transform3D);
    
    // Recursively update descendants
    // note delay in update to sync.
    // if comment this will delay sync since frame render tick.
    ecs_iter_t it = ecs_children(world, entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            UpdateChildTransformOnly(world, it.entities[i]);
        }
    }
}

// System to process specific child entities (e.g., triggered by GUI)
void UpdateTransform3DSystem(ecs_iter_t *it) {
    Transform3D *transform3d = ecs_field(it, Transform3D, 0);
    for (int i = 0; i < it->count; i++) {
        // ecs_entity_t entity = transform3d[i].id;
        ecs_entity_t entity = it->entities[i];
        if (ecs_is_valid(it->world, entity) && ecs_has(it->world, entity, Transform3D)) {
            UpdateChildTransformOnly(it->world, entity);
        }
    }
}

// Render begin system
void RenderBeginSystem(ecs_iter_t *it) {
  // printf("RenderBeginSystem\n");
  BeginDrawing();
  ClearBackground(RAYWHITE);
}

// Render begin system
void BeginCamera3DSystem(ecs_iter_t *it) {
  // printf("BeginCamera3DSystem\n");
  Camera3D *camera = (Camera3D *)ecs_get_ctx(it->world);
  if (!camera) return;
  BeginMode3D(*camera);
}

// Camera3d system for 3d model
void Camera3DSystem(ecs_iter_t *it) {
  Transform3D *t = ecs_field(it, Transform3D, 0);
  model_component_t *m = ecs_field(it, model_component_t, 1);

  for (int i = 0; i < it->count; i++) {
      ecs_entity_t entity = it->entities[i];
      if (!ecs_is_valid(it->world, entity)) {
          //printf("Skipping invalid entity ID: %llu\n", (unsigned long long)entity);
          continue;
      }
      const char *name = ecs_get_name(it->world, entity) ? ecs_get_name(it->world, entity) : "(unnamed)";
      if (!ecs_has(it->world, entity, Transform3D) || !ecs_has(it->world, entity, model_component_t)) {
          //printf("Skipping entity %s: missing Transform3D or model_component_t\n", name);
          continue;
      }
      // Check for garbage values in world matrix
      if (fabs(t[i].worldMatrix.m12) > 1e6 || fabs(t[i].worldMatrix.m13) > 1e6 || fabs(t[i].worldMatrix.m14) > 1e6) {
          // printf("Skipping entity %s: invalid world matrix (%.2f, %.2f, %.2f)\n",
          //        name, t[i].worldMatrix.m12, t[i].worldMatrix.m13, t[i].worldMatrix.m14);
          continue;
      }
      // printf("Rendering entity %s at world pos (%.2f, %.2f, %.2f)\n",
      //        name, t[i].worldMatrix.m12, t[i].worldMatrix.m13, t[i].worldMatrix.m14);
      if (!m[i].model) {
          // printf("Skipping entity %s: null model\n", name);
          continue;
      }
      m[i].model->transform = t[i].worldMatrix;
      bool isChild = ecs_has_pair(it->world, entity, EcsChildOf, EcsWildcard);
      DrawModel(*(m[i].model), (Vector3){0, 0, 0}, 1.0f, isChild ? BLUE : RED);
  }
  DrawGrid(10, 1.0f);

  // does not work here.
  // Camera3D *camera = (Camera3D *)ecs_get_ctx(it->world);
  // if (camera) {
  //     DrawText(TextFormat("Camera Pos: %.2f, %.2f, %.2f",
  //                         camera->position.x, camera->position.y, camera->position.z), 10, 90, 20, DARKGRAY);
  //     DrawText(TextFormat("Entities Rendered: %d", it->count), 10, 110, 20, DARKGRAY);
  // }
}

void EndCamera3DSystem(ecs_iter_t *it) {
  //printf("EndCamera3DSystem\n");
  Camera3D *camera = (Camera3D *)ecs_get_ctx(it->world);
  if (!camera) return;
  EndMode3D();
}

// Render end system
void EndRenderSystem(ecs_iter_t *it) {
  //printf("EndRenderSystem\n");
  EndDrawing();
}

// Input handling system
void user_input_system(ecs_iter_t *it) {
    player_input_t *pi_ctx = ecs_singleton_ensure(it->world, player_input_t);
    if (!pi_ctx) return;

    Transform3D *t = ecs_field(it, Transform3D, 0);
    float dt = GetFrameTime();
    //test user input
    for (int i = 0; i < it->count; i++) {
      const char *name = ecs_get_name(it->world, it->entities[i]);
      if (name) {
        bool isFound = false;
        if (strcmp(name, "NodeParent") == 0) {

          bool wasModified = false;

          if (IsKeyPressed(KEY_TAB)) {
            pi_ctx->isMovementMode = !pi_ctx->isMovementMode;
            // printf("Toggled mode to: %s\n", pi_ctx->isMovementMode ? "Movement" : "Rotation");
          }

          if (pi_ctx->isMovementMode) {
              if (IsKeyDown(KEY_W)){t[i].position.z -= 2.0f * dt;wasModified = true;}
              if (IsKeyDown(KEY_S)){t[i].position.z += 2.0f * dt;wasModified = true;}
              if (IsKeyDown(KEY_A)){t[i].position.x -= 2.0f * dt;wasModified = true;}
              if (IsKeyDown(KEY_D)){t[i].position.x += 2.0f * dt;wasModified = true;}
          } else {
              float rotateSpeed = 90.0f;
              if (IsKeyDown(KEY_Q)) {
                  Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 1, 0}, DEG2RAD * rotateSpeed * dt);
                  t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
                  wasModified = true;
              }
              if (IsKeyDown(KEY_E)) {
                  Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 1, 0}, -DEG2RAD * rotateSpeed * dt);
                  t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
                  wasModified = true;
              }
              if (IsKeyDown(KEY_W)) {
                  Quaternion rot = QuaternionFromAxisAngle((Vector3){1, 0, 0}, DEG2RAD * rotateSpeed * dt);
                  t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
                  wasModified = true;
              }
              if (IsKeyDown(KEY_S)) {
                  Quaternion rot = QuaternionFromAxisAngle((Vector3){1, 0, 0}, -DEG2RAD * rotateSpeed * dt);
                  t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
                  wasModified = true;
              }
              if (IsKeyDown(KEY_A)) {
                  Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 0, 1}, DEG2RAD * rotateSpeed * dt);
                  t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
                  wasModified = true;
              }
              if (IsKeyDown(KEY_D)) {
                  Quaternion rot = QuaternionFromAxisAngle((Vector3){0, 0, 1}, -DEG2RAD * rotateSpeed * dt);
                  t[i].rotation = QuaternionMultiply(t[i].rotation, rot);
                  wasModified = true;
              }
          }
          if (IsKeyPressed(KEY_R)) {
              t[i].position = (Vector3){0.0f, 0.0f, 0.0f};
              t[i].rotation = QuaternionIdentity();
              t[i].scale = (Vector3){1.0f, 1.0f, 1.0f};
              wasModified = true;
          }
          if (wasModified) {
            t[i].isDirty = true;
            printf("Marked %s as dirty\n", name);
          }
        }
      }
    }
}

// render 2d
void render2d_hud_system(ecs_iter_t *it){
    player_input_t *player_input = ecs_field(it, player_input_t, 0);

    DrawText(player_input->isMovementMode ? "Mode: Movement (WASD)" : "Mode: Rotation (QWE/ASD)", 10, 30, 20, DARKGRAY);
    DrawText("Tab: Toggle Mode | R: Reset", 10, 10, 20, DARKGRAY);
    DrawFPS(10, 50);
    
    ecs_query_t *query = ecs_query(it->world, {
        .terms = {{ ecs_id(Transform3D) }}
    });

    int entity_count = 0;
    ecs_iter_t transform_it = ecs_query_iter(it->world, query);
    while (ecs_query_next(&transform_it)) {
        entity_count += transform_it.count;
    }
    DrawText(TextFormat("Entity Count: %d", entity_count), 10, 70, 20, DARKGRAY);

    transform_it = ecs_query_iter(it->world, query);
    int row_count = 0;
    // note this will not over lap in case entity transform 3d since it has parrent and child add together
    // note the position is zero by default for some reason
    while (ecs_query_next(&transform_it)) {
        Transform3D *t = ecs_field(&transform_it, Transform3D, 0);
        for (int j = 0; j < transform_it.count; j++) {
            
            DrawText(TextFormat("Entity %s Pos: %.2f, %.2f, %.2f", 
                ecs_get_name(it->world, transform_it.entities[j]) ? ecs_get_name(it->world, transform_it.entities[j]) : "unnamed", 
                t[j].position.x, t[j].position.y, t[j].position.z), 10, 90 + row_count * 20, 20, DARKGRAY);
            row_count++;
        }
    }

}

// main
int main(void) {
    InitWindow(800, 600, "Transform Hierarchy with Flecs v4.x");
    SetTargetFPS(60);

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_DEFINE(world, Transform3D);
    ECS_COMPONENT_DEFINE(world, model_component_t);
    ECS_COMPONENT_DEFINE(world, player_input_t);
    ECS_COMPONENT_DEFINE(world, transform_3d_gui_t);
    ECS_COMPONENT_DEFINE(world, main_context_t);

    // Define custom phases
    ecs_entity_t PreLogicUpdatePhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t LogicUpdatePhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t BeginRenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t BeginCamera3DPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t Camera3DPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t EndCamera3DPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t RenderPhase = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t EndRenderPhase = ecs_new_w_id(world, EcsPhase);

    // order phase not doing correct
    ecs_add_pair(world, PreLogicUpdatePhase, EcsDependsOn, EcsPreUpdate); // start game logics
    ecs_add_pair(world, LogicUpdatePhase, EcsDependsOn, PreLogicUpdatePhase); // start game logics
    ecs_add_pair(world, BeginRenderPhase, EcsDependsOn, LogicUpdatePhase); // BeginDrawing
    ecs_add_pair(world, BeginCamera3DPhase, EcsDependsOn, BeginRenderPhase); // EcsOnUpdate, BeginMode3D
    ecs_add_pair(world, Camera3DPhase, EcsDependsOn, BeginCamera3DPhase); // 3d model only
    ecs_add_pair(world, EndCamera3DPhase, EcsDependsOn, Camera3DPhase); // EndMode3D
    ecs_add_pair(world, RenderPhase, EcsDependsOn, EndCamera3DPhase); // 2D only
    ecs_add_pair(world, EndRenderPhase, EcsDependsOn, RenderPhase); // render to screen

    ecs_system(world, {
        .entity = ecs_entity(world, { .name = "user_input_system", .add = ecs_ids(ecs_dependson(LogicUpdatePhase)) }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },
        },
        .callback = user_input_system
    });

    ecs_system(world, {
      .entity = ecs_entity(world, { .name = "render2d_hud_system", .add = ecs_ids(ecs_dependson(RenderPhase)) }),
      .query.terms = {
        { .id = ecs_id(player_input_t), .src.id = ecs_id(player_input_t) } // Singleton
        // { .id = ecs_id(Transform3D) },
        //   { .id = ecs_id(Transform3D), .src.id = EcsSelf },
        //   { .id = ecs_pair(EcsChildOf, EcsWildcard), .oper = EcsNot }
      },
      .callback = render2d_hud_system
    });

    ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "UpdateTransform3DSystem",
            .add = ecs_ids(ecs_dependson(PreLogicUpdatePhase))
        }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf }
        },
        .callback = UpdateTransform3DSystem
    });


    ecs_system(world,{
      .entity = ecs_entity(world, { .name = "RenderBeginSystem", 
        .add = ecs_ids(ecs_dependson(BeginRenderPhase)) 
      }),
      .callback = RenderBeginSystem
    });

    //note this has be in order of the ECS since push into array. From I guess.
    ecs_system(world, {
      .entity = ecs_entity(world, {
        .name = "BeginCamera3DSystem", 
        .add = ecs_ids(ecs_dependson(BeginCamera3DPhase)) 
      }),
      .callback = BeginCamera3DSystem
    });

    ecs_system(world, {
        .entity = ecs_entity(world, {
          .name = "Camera3DSystem", 
          .add = ecs_ids(ecs_dependson(Camera3DPhase))
        }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },
            { .id = ecs_id(model_component_t), .src.id = EcsSelf }
        },
        .callback = Camera3DSystem
    });

    ecs_system(world, {
      .entity = ecs_entity(world, {
        .name = "EndCamera3DSystem", 
        .add = ecs_ids(ecs_dependson(EndCamera3DPhase))
      }),
      .callback = EndCamera3DSystem
    });

    ecs_system(world, {
        .entity = ecs_entity(world, {
          .name = "EndRenderSystem", 
          .add = ecs_ids(ecs_dependson(EndRenderPhase)) 
        }),
        .callback = EndRenderSystem
    });

    // Register GUI list system in the 2D rendering phase
    // ECS_SYSTEM(world, transform_3D_gui_list_system, EcsOnUpdate, transform_3d_gui_t);

    ecs_system(world, {
        .entity = ecs_entity(world, { .name = "transform_3D_gui_list_system", .add = ecs_ids(ecs_dependson(LogicUpdatePhase)) }),
        .query.terms = {
            { .id = ecs_id(transform_3d_gui_t), .src.id = ecs_id(transform_3d_gui_t) } // Singleton
        },
        .callback = transform_3D_gui_list_system
    });

    Camera3D camera = {
        .position = (Vector3){10.0f, 10.0f, 10.0f},
        .target = (Vector3){0.0f, 0.0f, 0.0f},
        .up = (Vector3){0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    ecs_set_ctx(world, &camera, NULL);

    ecs_singleton_set(world, main_context_t, {
        .camera = camera
    });


    ecs_singleton_set(world, player_input_t, {
      .isMovementMode=true,
      .tabPressed=false
    });

    Model cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    ecs_entity_t node1 = ecs_entity(world, {
      .name = "NodeParent"
    });

    ecs_set(world, node1, Transform3D, {
        .position = (Vector3){0.0f, 0.0f, 0.0f},
        .rotation = QuaternionIdentity(),
        .scale = (Vector3){1.0f, 1.0f, 1.0f},
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity(),
        .isDirty = true
    });
    ecs_set(world, node1, model_component_t, {&cube});
    // printf("Node1 entity ID: %llu (%s)\n", (unsigned long long)node1, ecs_get_name(world, node1));
    // printf("- Node1 valid: %d, has Transform3D: %d\n", ecs_is_valid(world, node1), ecs_has(world, node1, Transform3D));
    
    ecs_entity_t node2 = ecs_entity(world, {
        .name = "NodeChild",
        .parent = node1
    });
    ecs_set(world, node2, Transform3D, {
        .position = (Vector3){2.0f, 0.0f, 0.0f},
        .rotation = QuaternionIdentity(),
        .scale = (Vector3){0.5f, 0.5f, 0.5f},
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity(),
        .isDirty = true
    });
    ecs_set(world, node2, model_component_t, {&cube});
    // printf("Node2 entity ID: %llu (%s)\n", (unsigned long long)node2, ecs_get_name(world, node2));
    // printf("- Node2 valid: %d, has Transform3D: %d, parent: %s\n",
    //        ecs_is_valid(world, node2), ecs_has(world, node2, Transform3D),
    //        ecs_get_name(world, ecs_get_parent(world, node2)));
    
    ecs_entity_t node3 = ecs_entity(world, {
        .name = "Node3",
        .parent = node1
    });
    ecs_set(world, node3, Transform3D, {
        .position = (Vector3){2.0f, 0.0f, 2.0f},
        .rotation = QuaternionIdentity(),
        .scale = (Vector3){0.5f, 0.5f, 0.5f},
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity(),
        .isDirty = true
    });
    ecs_set(world, node3, model_component_t, {&cube});
    // printf("Node3 entity ID: %llu (%s)\n", (unsigned long long)node3, ecs_get_name(world, node3));
    // printf("- Node3 valid: %d, has Transform3D: %d, parent: %s\n",
    //      ecs_is_valid(world, node3), ecs_has(world, node3, Transform3D),
    //      ecs_get_name(world, ecs_get_parent(world, node3)));


    ecs_entity_t node4 = ecs_entity(world, {
        .name = "NodeGrandchild",
        .parent = node2
    });
    ecs_set(world, node4, Transform3D, {
        .position = (Vector3){1.0f, 0.0f, 1.0f},
        .rotation = QuaternionIdentity(),
        .scale = (Vector3){0.5f, 0.5f, 0.5f},
        .localMatrix = MatrixIdentity(),
        .worldMatrix = MatrixIdentity(),
        .isDirty = true
    });
    ecs_set(world, node4, model_component_t, {&cube});
    // printf("Node4 entity ID: %llu (%s)\n", (unsigned long long)node4, ecs_get_name(world, node4));
    // printf("- Node4 valid: %d, has Transform3D: %d, parent: %s\n",
    //        ecs_is_valid(world, node4), ecs_has(world, node4, Transform3D),
    //        ecs_get_name(world, ecs_get_parent(world, node4)));

    ecs_singleton_set(world, transform_3d_gui_t, {
        .id = node1  // Reference the id entity
    });


    while (!WindowShouldClose()) {
      ecs_progress(world, 0);
    }

    UnloadModel(cube);
    ecs_fini(world);
    CloseWindow();
    return 0;
}

void transform_3D_gui_list_system(ecs_iter_t *it) {
    transform_3d_gui_t *gui = ecs_field(it, transform_3d_gui_t, 0);

    // Create a query for all entities with Transform3D
    ecs_query_t *query = ecs_query(it->world, {
        .terms = {{ ecs_id(Transform3D) }}
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
    Rectangle list_rect = {520, 10, 240, 200}; // Reduced height for more controls
    int scroll_index = 0;
    
    int prev_selected_index = gui->selectedIndex;
    GuiListView(list_rect, name_list, &scroll_index, &gui->selectedIndex);

    // Draw transform controls if an entity is selected
    if (gui->selectedIndex >= 0 && gui->selectedIndex < entity_count && ecs_is_valid(it->world, entity_ids[gui->selectedIndex])) {
        gui->id = entity_ids[gui->selectedIndex];
        Transform3D *transform = ecs_get_mut(it->world, gui->id, Transform3D);
        bool modified = false;

        if (transform) {
            Rectangle control_rect = {520, 220, 240, 360};
            GuiGroupBox(control_rect, "Transform Controls");

            // Position controls
            GuiLabel((Rectangle){530, 230, 100, 20}, "Position");
            float new_pos_x = transform->position.x;
            float new_pos_y = transform->position.y;
            float new_pos_z = transform->position.z;
            GuiSlider((Rectangle){530, 250, 200, 20}, "X", TextFormat("%.2f", new_pos_x), &new_pos_x, -10.0f, 10.0f);
            GuiSlider((Rectangle){530, 270, 200, 20}, "Y", TextFormat("%.2f", new_pos_y), &new_pos_y, -10.0f, 10.0f);
            GuiSlider((Rectangle){530, 290, 200, 20}, "Z", TextFormat("%.2f", new_pos_z), &new_pos_z, -10.0f, 10.0f);
            if (new_pos_x != transform->position.x || new_pos_y != transform->position.y || new_pos_z != transform->position.z) {
                transform->position.x = new_pos_x;
                transform->position.y = new_pos_y;
                transform->position.z = new_pos_z;
                modified = true;
            }

            // Rotation controls (using Euler angles)
            GuiLabel((Rectangle){530, 310, 100, 20}, "Rotation");
            Vector3 euler = QuaternionToEuler(transform->rotation);
            euler.x = RAD2DEG * euler.x;
            euler.y = RAD2DEG * euler.y;
            euler.z = RAD2DEG * euler.z;
            float new_rot_x = euler.x;
            float new_rot_y = euler.y;
            float new_rot_z = euler.z;
            GuiSlider((Rectangle){530, 330, 200, 20}, "X", TextFormat("%.2f", new_rot_x), &new_rot_x, -180.0f, 180.0f);
            GuiSlider((Rectangle){530, 350, 200, 20}, "Y", TextFormat("%.2f", new_rot_y), &new_rot_y, -180.0f, 180.0f);
            GuiSlider((Rectangle){530, 370, 200, 20}, "Z", TextFormat("%.2f", new_rot_z), &new_rot_z, -180.0f, 180.0f);
            if (new_rot_x != euler.x || new_rot_y != euler.y || new_rot_z != euler.z) {
                transform->rotation = QuaternionFromEuler(DEG2RAD * new_rot_x, DEG2RAD * new_rot_y, DEG2RAD * new_rot_z);
                modified = true;
            }

            // Scale controls
            GuiLabel((Rectangle){530, 390, 100, 20}, "Scale");
            float new_scale_x = transform->scale.x;
            float new_scale_y = transform->scale.y;
            float new_scale_z = transform->scale.z;
            GuiSlider((Rectangle){530, 410, 200, 20}, "X", TextFormat("%.2f", new_scale_x), &new_scale_x, 0.1f, 5.0f);
            GuiSlider((Rectangle){530, 430, 200, 20}, "Y", TextFormat("%.2f", new_scale_y), &new_scale_y, 0.1f, 5.0f);
            GuiSlider((Rectangle){530, 450, 200, 20}, "Z", TextFormat("%.2f", new_scale_z), &new_scale_z, 0.1f, 5.0f);
            if (new_scale_x != transform->scale.x || new_scale_y != transform->scale.y || new_scale_z != transform->scale.z) {
                transform->scale.x = new_scale_x;
                transform->scale.y = new_scale_y;
                transform->scale.z = new_scale_z;
                modified = true;
            }

            // Mark transform and descendants as dirty if modified
            if (modified) {
                transform->isDirty = true;
                UpdateChildTransformOnly(it->world, gui->id); // Update child and descendants immediately
                // ecs_modified(it->world, gui->id, Transform3D);

                // Mark all descendants as dirty
                ecs_iter_t child_it = ecs_children(it->world, gui->id);
                while (ecs_children_next(&child_it)) {
                    for (int j = 0; j < child_it.count; j++) {
                        if (ecs_has(child_it.world, child_it.entities[j], Transform3D)) {
                            Transform3D *child_transform = ecs_get_mut(child_it.world, child_it.entities[j], Transform3D);
                            if (child_transform) {
                                child_transform->isDirty = true;
                                ecs_modified(child_it.world, child_it.entities[j], Transform3D);
                            }
                        }
                    }
                }
            }
        }
    }

    GuiGroupBox(list_rect, "Entity List");

    // Clean up
    for (int j = 0; j < entity_count; j++) {
        RL_FREE(entity_names[j]);
    }
    RL_FREE(entity_names);
    RL_FREE(entity_ids);
    RL_FREE(name_list);
    ecs_query_fini(query);
}

