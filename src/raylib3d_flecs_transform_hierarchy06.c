// 
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
    bool isDirty;             // Flag to indicate if transform needs updating
} Transform3D;
ECS_COMPONENT_DECLARE(Transform3D);

// Pointer component for raylib Model
typedef struct {
    Model* model;             // Pointer to Model
} ModelComponent;
ECS_COMPONENT_DECLARE(ModelComponent);

typedef struct {
  bool isMovementMode;
  bool tabPressed;
  bool moveForward;
  bool moveBackward;
  bool moveLeft;
  bool moveRight;
} PlayerInput_T;
ECS_COMPONENT_DECLARE(PlayerInput_T);

// Comparison function for hierarchical ordering (parents before children)
// does not work.
static int compare_entity(ecs_entity_t e1, const void *ptr1, ecs_entity_t e2, const void *ptr2) {
  return (e1 < e2) ? -1 : (e1 > e2) ? 1 : 0;
}
//does not work.
static int compare_depth(ecs_entity_t e1, const void *ptr1, ecs_entity_t e2, const void *ptr2) {
  ecs_world_t *world = ((ecs_iter_t*)ptr1)->world;
  int depth1 = 0, depth2 = 0;
  ecs_entity_t parent = e1;
  while (parent && ecs_is_valid(world, parent)) {
      depth1++;
      parent = ecs_get_parent(world, parent);
  }
  parent = e2;
  while (parent && ecs_is_valid(world, parent)) {
      depth2++;
      parent = ecs_get_parent(world, parent);
  }
  return depth1 - depth2; // Lower depth (parents) first
}

// Helper function to update a single transform
void UpdateTransform(ecs_world_t *world, ecs_entity_t entity, Transform3D *transform) {
  // Get parent entity
  ecs_entity_t parent = ecs_get_parent(world, entity);
  const char *name = ecs_get_name(world, entity) ? ecs_get_name(world, entity) : "(unnamed)";
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
      printf("Root %s position (%.2f, %.2f, %.2f)\n", name, transform->position.x, transform->position.y, transform->position.z);
  } else {
      // Child entity: world matrix = local matrix * parent world matrix
      const Transform3D *parent_transform = ecs_get(world, parent, Transform3D);
      if (!parent_transform) {
          printf("Error: Parent %s lacks Transform3D for %s\n",
                 ecs_get_name(world, parent) ? ecs_get_name(world, parent) : "(unnamed)", name);
          transform->worldMatrix = transform->localMatrix;
          return;
      }

      // Validate parent world matrix
      float px = parent_transform->worldMatrix.m12;
      float py = parent_transform->worldMatrix.m13;
      float pz = parent_transform->worldMatrix.m14;
      if (fabs(px) > 1e6 || fabs(py) > 1e6 || fabs(pz) > 1e6) {
          printf("Error: Invalid parent %s world pos (%.2f, %.2f, %.2f) for %s\n",
                 ecs_get_name(world, parent) ? ecs_get_name(world, parent) : "(unnamed)",
                 px, py, pz, name);
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
      const char *parent_name = ecs_get_name(world, parent) ? ecs_get_name(world, parent) : "(unnamed)";
      printf("Child %s (ID: %llu), parent %s (ID: %llu)\n",
             name, (unsigned long long)entity, parent_name, (unsigned long long)parent);
      printf("Child %s position (%.2f, %.2f, %.2f), parent %s world pos (%.2f, %.2f, %.2f), world pos (%.2f, %.2f, %.2f)\n",
             name, transform->position.x, transform->position.y, transform->position.z,
             parent_name, px, py, pz, wx, wy, wz);
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

// Recursive function to process entity and its children
void ProcessEntityHierarchy(ecs_world_t *world, ecs_entity_t entity) {
  // Update the current entity's transform
  Transform3D *transform = ecs_get_mut(world, entity, Transform3D);
  bool wasUpdated = false;
  if (transform) {
      // Only update if dirty or parent is dirty
      ecs_entity_t parent = ecs_get_parent(world, entity);
      bool parentIsDirty = false;
      if (parent && ecs_is_valid(world, parent)) {
          const Transform3D *parent_transform = ecs_get(world, parent, Transform3D);
          if (parent_transform && parent_transform->isDirty) {
              parentIsDirty = true;
          }
      }
      if (transform->isDirty || parentIsDirty) {
          UpdateTransform(world, entity, transform);
          //ecs_set(world, entity, Transform3D, *transform); // Commit changes
          wasUpdated = true;
      }
  }

  // Skip processing children if this entity was not updated
  if (!wasUpdated) {
      return;
  }

  // Iterate through children
  ecs_iter_t it = ecs_children(world, entity);
  while (ecs_children_next(&it)) {
      for (int i = 0; i < it.count; i++) {
          ecs_entity_t child = it.entities[i];
          ProcessEntityHierarchy(world, child); // Recursively process child
      }
  }
}

// System to update all transforms in hierarchical order
void UpdateTransformHierarchySystem(ecs_iter_t *it) {
  // Process only root entities (no parent)
  Transform3D *transforms = ecs_field(it, Transform3D, 0);
  for (int i = 0; i < it->count; i++) {
      ecs_entity_t entity = it->entities[i];
      ProcessEntityHierarchy(it->world, entity);
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
  ModelComponent *m = ecs_field(it, ModelComponent, 1);

  for (int i = 0; i < it->count; i++) {
      ecs_entity_t entity = it->entities[i];
      if (!ecs_is_valid(it->world, entity)) {
          //printf("Skipping invalid entity ID: %llu\n", (unsigned long long)entity);
          continue;
      }
      const char *name = ecs_get_name(it->world, entity) ? ecs_get_name(it->world, entity) : "(unnamed)";
      if (!ecs_has(it->world, entity, Transform3D) || !ecs_has(it->world, entity, ModelComponent)) {
          //printf("Skipping entity %s: missing Transform3D or ModelComponent\n", name);
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
    PlayerInput_T *pi_ctx = ecs_singleton_ensure(it->world, PlayerInput_T);
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
            printf("Toggled mode to: %s\n", pi_ctx->isMovementMode ? "Movement" : "Rotation");
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

void render2d_hud_system(ecs_iter_t *it){
  PlayerInput_T *pi_ctx = ecs_singleton_ensure(it->world, PlayerInput_T);
  if (!pi_ctx) return;
  Transform3D *t = ecs_field(it, Transform3D, 0);
  float dt = GetFrameTime();

  for (int i = 0; i < it->count; i++) {
    DrawText(TextFormat("Entity %s Pos: %.2f, %.2f, %.2f", 
      ecs_get_name(it->world, it->entities[i]) ? ecs_get_name(it->world, it->entities[i]) : "unnamed", 
      t[i].position.x, t[i].position.y, t[i].position.z), 10, 130 + i * 20, 20, DARKGRAY);
  }
  DrawText(TextFormat("Entity Count: %d", it->count), 10, 10, 20, DARKGRAY);
  DrawText(pi_ctx->isMovementMode ? "Mode: Movement (WASD)" : "Mode: Rotation (QWE/ASD)", 10, 30, 20, DARKGRAY);
  DrawText("Tab: Toggle Mode | R: Reset", 10, 50, 20, DARKGRAY);
  DrawFPS(10, 70);
}

int main(void) {
    InitWindow(800, 600, "Transform Hierarchy with Flecs v4.x");
    SetTargetFPS(60);

    ecs_world_t *world = ecs_init();

    ECS_COMPONENT_DEFINE(world, Transform3D);
    ECS_COMPONENT_DEFINE(world, ModelComponent);
    ECS_COMPONENT_DEFINE(world, PlayerInput_T);

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

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, { .name = "user_input_system", .add = ecs_ids(ecs_dependson(LogicUpdatePhase)) }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },
            //{ .id = ecs_pair(EcsChildOf, EcsWildcard), .oper = EcsNot }
        },
        .callback = user_input_system
    });

    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, { .name = "render2d_hud_system", .add = ecs_ids(ecs_dependson(RenderPhase)) }),
      .query.terms = {
          { .id = ecs_id(Transform3D), .src.id = EcsSelf },
          { .id = ecs_pair(EcsChildOf, EcsWildcard), .oper = EcsNot }
      },
      .callback = render2d_hud_system
    });

    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, {
          .name = "UpdateTransformHierarchySystem",
          .add = ecs_ids(ecs_dependson(PreLogicUpdatePhase))
      }),
      .query.terms = {
          { .id = ecs_id(Transform3D), .src.id = EcsSelf },
          { .id = ecs_pair(EcsChildOf, EcsWildcard), .oper = EcsNot } // No parent
      },
      .callback = UpdateTransformHierarchySystem
    });


    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, { .name = "RenderBeginSystem", 
        .add = ecs_ids(ecs_dependson(BeginRenderPhase)) 
      }),
      .callback = RenderBeginSystem
    });

    //note this has be in order of the ECS since push into array. From I guess.
    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, {
        .name = "BeginCamera3DSystem", 
        .add = ecs_ids(ecs_dependson(BeginCamera3DPhase)) 
      }),
      .callback = BeginCamera3DSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, {
          .name = "Camera3DSystem", 
          .add = ecs_ids(ecs_dependson(Camera3DPhase))
        }),
        .query.terms = {
            { .id = ecs_id(Transform3D), .src.id = EcsSelf },
            { .id = ecs_id(ModelComponent), .src.id = EcsSelf }
        },
        .callback = Camera3DSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
      .entity = ecs_entity(world, {
        .name = "EndCamera3DSystem", 
        .add = ecs_ids(ecs_dependson(EndCamera3DPhase))
      }),
      .callback = EndCamera3DSystem
    });

    ecs_system_init(world, &(ecs_system_desc_t){
        .entity = ecs_entity(world, {
          .name = "EndRenderSystem", 
          .add = ecs_ids(ecs_dependson(EndRenderPhase)) 
        }),
        .callback = EndRenderSystem
    });

    Camera3D camera = {
        .position = (Vector3){10.0f, 10.0f, 10.0f},
        .target = (Vector3){0.0f, 0.0f, 0.0f},
        .up = (Vector3){0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    ecs_set_ctx(world, &camera, NULL);

    ecs_singleton_set(world, PlayerInput_T, {
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
    ecs_set(world, node1, ModelComponent, {&cube});
    printf("Node1 entity ID: %llu (%s)\n", (unsigned long long)node1, ecs_get_name(world, node1));
    printf("- Node1 valid: %d, has Transform3D: %d\n", ecs_is_valid(world, node1), ecs_has(world, node1, Transform3D));
    
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
    ecs_set(world, node2, ModelComponent, {&cube});
    printf("Node2 entity ID: %llu (%s)\n", (unsigned long long)node2, ecs_get_name(world, node2));
    printf("- Node2 valid: %d, has Transform3D: %d, parent: %s\n",
           ecs_is_valid(world, node2), ecs_has(world, node2, Transform3D),
           ecs_get_name(world, ecs_get_parent(world, node2)));
    
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
    ecs_set(world, node3, ModelComponent, {&cube});
    printf("Node3 entity ID: %llu (%s)\n", (unsigned long long)node3, ecs_get_name(world, node3));
    printf("- Node3 valid: %d, has Transform3D: %d, parent: %s\n",
         ecs_is_valid(world, node3), ecs_has(world, node3, Transform3D),
         ecs_get_name(world, ecs_get_parent(world, node3)));


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
    ecs_set(world, node4, ModelComponent, {&cube});
    // printf("Node4 entity ID: %llu (%s)\n", (unsigned long long)node4, ecs_get_name(world, node4));
    // printf("- Node4 valid: %d, has Transform3D: %d, parent: %s\n",
    //        ecs_is_valid(world, node4), ecs_has(world, node4, Transform3D),
    //        ecs_get_name(world, ecs_get_parent(world, node4)));

    while (!WindowShouldClose()) {
      ecs_progress(world, 0);
    }

    UnloadModel(cube);
    ecs_fini(world);
    CloseWindow();
    return 0;
}

