# Material-Driven Shadows — Massive Unit Shadow System

Single-draw-call, instance-batched blob shadows for 10,000+ units in RTS, top-down and horde games.

Virtual Shadow Maps and decal components both fall apart at scale: VSMs pay an enormous GPU and CPU
culling cost past a few hundred movers, and one decal per unit means one draw call per unit. Material-Driven
Shadows takes the opposite approach — every shadow that shares a mesh and a material lives in **one**
instanced static mesh component, and the projection itself is material math rather than rasterisation.

**What it does**

- One draw call per mesh/material pair, no matter how many units cast shadows.
- Per-instance custom data carries radius, opacity, height above ground and light stretch to the GPU.
- Shadows follow the scene's primary directional light in real time — a day/night cycle rotates and
  stretches all 10,000 shadows in the vertex shader, at zero extra CPU cost.
- Units that jump or fly slide and fade their shadow away automatically.
- Budgeted, round-robin ground traces: the per-frame trace cost is bounded regardless of unit count.
- Slope fitting is handled by the material's depth blend, not by per-frame CPU traces.

**What ships**

- A `UMaterialDrivenShadowComponent` you attach to anything that should cast a shadow.
- A `UMaterialDrivenShadowSubsystem` that batches, updates and profiles them all.
- Project settings for the default quad, master material, update rate, trace budget and culling.
- Full C++ source, Blueprint-exposed API, and a documented master-material recipe.

Supports **Unreal Engine 5.8**, Win64.
