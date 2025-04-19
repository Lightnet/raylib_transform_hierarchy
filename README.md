# Raylib Transform Hierarchy

# License: MIT

# Libraries:
- Raylib: 5.5
- Flecs: 4.0.5
# Overview

This project is a simple test to demonstrate how a transform hierarchy works in Raylib using C. It shows how to manage parent-child relationships to calculate transformations (position, rotation, scale) for objects in a scene, organized like a tree structure.

The code uses a tree-based approach where:
- A parent node (like a tree trunk) holds a transform (position, rotation, scale).
- Child nodes (like branches) inherit and combine their transforms with their parent's transform.
- Transformations are calculated from local (relative to the parent) to world (absolute) coordinates.
    

With help from Grok 3, you can generate transform hierarchy logic by providing a prompt like: "raylib c transform hierarchy" along with a cheat sheet.

# Transform Hierarchy Explained

A transform hierarchy organizes objects in a tree structure to manage their transformations:
- Nodes: Each node represents an object with a local transform (position, rotation, scale).
- Parent-Child Relationships: A parent node’s transform affects all its children. Children combine their local transform with their parent’s world transform to compute their final world transform.
- Tree Traversal: The hierarchy is updated by looping through the tree, starting from the root (topmost parent), to calculate each node’s world transform based on its parent’s transform.
    

# Example

Imagine a solar system:
- The Sun is the root node (parent).
- A Planet is a child of the Sun, orbiting around it.
- A Moon is a child of the Planet, orbiting the Planet while also moving with it around the Sun. The Moon’s final position depends on both the Planet’s and Sun’s transforms.

How It Works

1. Define Nodes: Each node has:
    - Local transform (position, rotation, scale).
    - A reference to its parent (if any).
    - A list of children.
        
2. Update Transforms:
    - Start at the root node.
    - Calculate the world transform by combining local transforms with the parent’s world transform (using matrix multiplication).
    - Recursively update all children.
        
3. Render: Use Raylib to draw objects at their computed world transforms.
    
# Visual Diagram

Below is a textual representation of the transform hierarchy tree. You can visualize it as a flowchart or use a tool like Mermaid to generate it.

```text
Root (e.g., Sun)
  ├── Child 1 (e.g., Planet)
  │     ├── Grandchild 1 (e.g., Moon)
  │     └── Grandchild 2
  └── Child 2
```

## Diagram Description:

- Nodes: Represented as circles or boxes labeled with the object (e.g., "Sun", "Planet", "Moon").
- Edges: Arrows pointing from parent to child, showing the hierarchy.
- Labels: Each node includes its local transform (e.g., "Pos: (x,y,z), Rot: θ, Scale: s").
- Flow: A dashed line shows the update order (root → children → grandchildren).
    

```mermaid
graph TD
    A[Root: Sun\nPos: (0,0,0)\nRot: 0\nScale: 1] --> B[Child 1: Planet\nPos: (5,0,0)\nRot: 45°\nScale: 0.5]
    A --> C[Child 2]
    B --> D[Grandchild 1: Moon\nPos: (1,0,0)\nRot: 30°\nScale: 0.2]
    B --> E[Grandchild 2]
```



# Getting Started

1. Dependencies: VS2022, CMake
2. Build: Compile the project using the provided source code.
3. Run: Execute the program to see a simple transform hierarchy in action.
4. Experiment: Modify the hierarchy (add/remove nodes) to understand parent-child relationships.
    
# Tips

- Use Grok 3 to generate or debug transform logic. Prompt example: "Create a Raylib C transform hierarchy with a parent and two children."
- Check Raylib’s matrix functions (MatrixTranslate, MatrixRotate, MatrixScale) for transform calculations.
- Ensure the update loop processes parents before children to maintain correct transform dependencies.

# Resources

- [Raylib Documentation](https://www.raylib.com/)
- [Flecs Documentation](https://www.flecs.dev/flecs/)
- [xAI Grok](https://x.ai/grok) for AI-assisted coding.