# Material-Driven Shadows — Documentation

Version 1.0.0 · Unreal Engine 5.8 · Win64 · Support: simulatedflow@gmail.com

---

## 1. What this plugin is

A shadow system for games that need far more shadow casters than the renderer can reasonably rasterise:
RTS armies, horde shooters, top-down survival, bullet-hell projectiles, tower defence.

Instead of asking the shadow-map pipeline to handle 10,000 movers, every shadow becomes **one instance in
one instanced static mesh component**. The CPU writes a single transform plus four custom-data floats per
unit per frame. Everything that makes the shadow look like a shadow — sliding along the light direction as
a unit rises, stretching when the sun is low, blending softly into uneven terrain — happens in the material.

The result: **one draw call per mesh/material pair**, regardless of unit count.

## 2. Installation

1. Copy the `MaterialDrivenShadows` folder into your project's `Plugins/` directory.
2. Regenerate project files and build, or let the editor compile the plugin on first open.
3. Enable **Material-Driven Shadows** under *Edit > Plugins* if it is not already enabled.
4. Open *Project Settings > Plugins > Material-Driven Shadows* and point **Default Shadow Material** at
   your master shadow material (see section 6 for the recipe). Everything else has working defaults.

## 3. Quick start

1. Add a **Material Driven Shadow** component to your unit's Blueprint or C++ actor.
2. Set **Shadow Radius** to roughly the unit's footprint (100 uu is a reasonable humanoid).
3. Press Play. The component registers itself with the world subsystem on `BeginPlay` and the subsystem
   does the rest — there is nothing to place in the level.

In C++:

```cpp
#include "MaterialDrivenShadowComponent.h"

AMyUnit::AMyUnit()
{
    ShadowComponent = CreateDefaultSubobject<UMaterialDrivenShadowComponent>(TEXT("Shadow"));
    ShadowComponent->SetupAttachment(RootComponent);
    ShadowComponent->ShadowRadius = 90.0f;
    ShadowComponent->BaseOpacity = 0.75f;
    ShadowComponent->MaxShadowStretch = 2.5f;
}
```

## 4. C++ / Blueprint API

### `UMaterialDrivenShadowComponent` (Scene Component)

Attach one per shadow caster. It never ticks and owns no primitive of its own.

| Property | Default | Meaning |
| --- | --- | --- |
| `ShadowRadius` | 100.0 | Base size in world units. Converted to instance scale via **Reference Mesh Radius**. |
| `BaseOpacity` | 0.8 | Opacity on the ground, before the height fade. |
| `MaxShadowStretch` | 2.0 | Cap on the stretch the material may apply at grazing sun angles. |
| `CustomShadowMesh` | none | Per-unit shadow quad. Units sharing a mesh share a draw call. |
| `CustomShadowMaterial` | none | Per-unit shadow material. |
| `ShadowOffset` | 0,0,0 | Extra offset in component space. |
| `GroundMode` | Trace Downward | How ground height is found — see below. |
| `MaxGroundTraceDistance` | 2000.0 | Reach of the downward trace. |
| `FadeStartHeight` | 250.0 | Height above ground where the shadow begins to fade. |
| `FadeEndHeight` | 1500.0 | Height above ground where the shadow is gone. |
| `bShadowEnabled` | true | Skip this shadow without releasing its instance slot. |
| `bAutoRegister` | true | Register on `BeginPlay`. |

Functions: `RegisterShadow`, `UnregisterShadow`, `IsShadowRegistered`, `SetShadowOpacity`,
`SetShadowRadius`, `SetShadowEnabled`, `SetMaxShadowStretch`, `SetGroundHeight`, `GetGroundHeight`,
`HasGroundHeight`, `SetShadowMeshAndMaterial`.

The setters are deliberately cheap: they write a float on the component and the value reaches the GPU on
the next subsystem pass. Nothing marks a render state dirty, so calling `SetShadowOpacity` on 10,000 units
in one frame costs 10,000 float stores.

#### Ground modes

| Mode | Cost | Use when |
| --- | --- | --- |
| **Owner Location** | free | Units never leave the floor. Height above ground is always 0. |
| **Trace Downward** | budgeted | Units jump, fly or walk over cliffs. Traces are round-robin across a per-frame budget. |
| **Manual** | free | You already know the terrain height — call `SetGroundHeight()` from your movement code. |

### `UMaterialDrivenShadowSubsystem` (Tickable World Subsystem)

Created automatically in every game and PIE world. Reachable from Blueprint through
`Get Material Driven Shadows` or a *Get World Subsystem* node.

| Function | Meaning |
| --- | --- |
| `SetShadowsEnabled(bool)` | Master switch. Disabling collapses every instance without destroying batches. |
| `AreShadowsEnabled()` | Current state of the master switch. |
| `GetStats()` | Batch count, slot usage, trace count and last-update milliseconds. |
| `SetLightDirectionOverride(FVector)` | Cast along a fixed direction instead of following the sun. |
| `ClearLightDirectionOverride()` | Resume following the scene's directional light. |
| `GetLightDirection()` / `GetShadowDirection()` | Current light vector and its ground-plane projection. |
| `GetNumBatches()` | Number of instanced components — this is your added draw-call count. |
| `RequestImmediateUpdate()` | Ignore the update interval for one tick. |

### `UMaterialDrivenShadowStatics` (Blueprint Function Library)

`GetMaterialDrivenShadows`, `SetMaterialDrivenShadowsEnabled`, `GetMaterialDrivenShadowStats`,
`SetMaterialDrivenShadowLightDirection`, `ClearMaterialDrivenShadowLightDirection`.

### Console variable

`MDS.Enabled 0|1` — hide or show every batched shadow at runtime without touching game code. Useful for
A/B profiling against the engine's own shadows.

## 5. Project settings

*Project Settings > Plugins > Material-Driven Shadows*, stored in `DefaultGame.ini`.

| Setting | Default | Meaning |
| --- | --- | --- |
| Enable Shadows | true | Master switch applied when a world starts. |
| Default Shadow Mesh | `/Engine/BasicShapes/Plane` | Flat, XY-aligned quad facing +Z. |
| Default Shadow Material | `/MaterialDrivenShadows/MaterialDrivenShadows/Materials/M_MaterialDrivenShadow` | Master material, shipped with the plugin. Empty keeps the mesh's own material. |
| Reference Mesh Radius | 50.0 | Radius of the default mesh at scale 1. The engine plane is 100×100 uu, hence 50. |
| Batch Mode | Instanced (ISM) | See section 7. |
| Update Interval | 0.0 | Seconds between updates. 0 = every frame; 0.033 halves the CPU cost at 30 Hz. |
| Max Ground Traces Per Update | 256 | Hard cap on line traces per update, shared by all shadows. |
| Ground Trace Channel | WorldStatic | Channel used by the downward trace. |
| Ground Offset | 2.0 | Lift above the detected ground, as cheap Z-fighting insurance. |
| Batch Bounds Scale | 4.0 | Culling headroom for the vertex-shader displacement. Keep ≥ your largest Max Shadow Stretch. |
| Instance Start/End Cull Distance | 0 / 0 | Per-instance distance culling. 0 disables it. |
| Light Refresh Interval | 2.0 | Seconds between re-scans for the primary directional light. |
| Fallback Light Direction | 0,0,-1 | Used when the level has no usable directional light. |

## 6. The master material

The plugin ships the C++ side of the system; the look lives in a material you own and can restyle. Create a
material named `M_MaterialDrivenShadow` and wire it up as follows.

**Material settings**

- **Blend Mode:** `Modulate` for a natural multiply onto the ground, or `Translucent` if you need the
  shadow to tint. `Translucent` gives you more control; `Modulate` is cheaper.
- **Shading Model:** `Unlit`.
- **Cast Shadow:** off (the subsystem also disables it on the component).
- **Used with Instanced Static Meshes:** on.
- **Disable Depth Test:** off. **Output Depth:** default.

**Per-instance custom data**

Add a `PerInstanceCustomData` node for each index the subsystem writes:

| Index | Name | Contents |
| --- | --- | --- |
| 0 | Radius | Shadow radius in world units. Scale your soft edge with this so different unit sizes look consistent. |
| 1 | Opacity | Final opacity in 0..1, height fade already applied. |
| 2 | Height Above Ground | World units the unit is above its ground point. |
| 3 | Stretch Factor | ≥ 1. Grows as the sun approaches the horizon, clamped per component. |

**Vertex shader — World Position Offset**

The subsystem has already yawed each instance so its **local +X points along the shadow direction**, which
makes the projection one-dimensional:

1. Take the vertex's local position (`Local Position` → `TransformPosition` local-to-world is not needed;
   use `Vertex Interpolator`-free local space via `ObjectLocalBounds`-relative UVs if you prefer).
2. **Stretch:** multiply the vertex's local X by `PerInstanceCustomData[3]`, then subtract the original X
   so you have a pure offset rather than an absolute position.
3. **Slide:** add `PerInstanceCustomData[2] * SlideScale` along the same local X axis, so the shadow walks
   away from a jumping unit rather than staying glued under it. `SlideScale` is a scalar parameter —
   0.5 is a good starting point and matches the geometric projection closely enough for a stylised look.
4. Transform the resulting local-space offset into world space with `TransformVector` (Local → World) and
   feed it into **World Position Offset**.

Because these offsets leave the mesh's own bounds, the subsystem sets the component's **Bounds Scale**
from the *Batch Bounds Scale* project setting. Raise it if shadows pop at screen edges.

**Pixel shader — shape, fade and slope fitting**

1. **Shape:** radial gradient from the mesh UVs (`TexCoord[0]` → `RadialGradientExponential`, or a
   soft-edged blob texture). Multiply by `PerInstanceCustomData[1]` for opacity.
2. **Slope fitting:** the quad is flat, so it will intersect hills and stairs. Feed a `DepthFade` node
   (Fade Distance ≈ 20–60 uu) into the opacity chain. Where the ground cuts through the quad, the shadow
   fades out instead of clipping — the shadow appears to hug the terrain with no CPU traces at all.
3. **Z-fighting:** add a small **Pixel Depth Offset** (5–15 uu) so the quad resolves in front of the
   ground it sits on. Combined with the *Ground Offset* setting this removes flicker on flat floors.
4. For `Modulate`, output `lerp(1, ShadowColor, Opacity)` into **Emissive Color**. For `Translucent
   Unlit`, feed `ShadowColor` into **Emissive Color** and `Opacity` into **Opacity**.

If the material is missing, batches fall back to the mesh's own material and the plugin logs a warning —
the system still runs, it just renders untextured quads.

## 7. Performance notes

**Why ISM is the default and not HISM.** The idea file for this plugin specified HISM. In UE 5.8, HISM's
`UpdateInstanceTransform` flags the CPU cluster tree out of date and schedules an async rebuild whenever an
instance *changes its translation*. With 10,000 shadows moving every frame that is a full tree rebuild per
frame — precisely the CPU cost the plugin exists to avoid. A plain ISM has no cluster tree: transform
updates are a memcpy into the instance buffer and culling happens per-instance on the GPU, which is both
faster and more accurate here. Both produce a single draw call.

`Batch Mode` therefore defaults to **Instanced (ISM)**. Switch it to **Hierarchical (HISM)** only when your
shadows barely move — static props, wreckage, buildings — where the cluster tree pays for itself.

**Instance slots are never removed.** Removing an ISM instance swaps indices around and would invalidate
every handle above it. Instead, unregistering a shadow collapses its instance to a negligible scale and
returns the slot to a free list, where the next shadow reuses it. Spawning and despawning units therefore
costs nothing but a list pop, and the instance buffer settles at your peak concurrent unit count.

**Two writes per batch per frame.** The whole update is one `BatchUpdateInstancesTransforms` and one
ranged `SetCustomData` per batch, both with `bMarkRenderStateDirty = false`. The render proxy is never
recreated; the instance data manager streams the deltas to the GPU scene.

**Trace budget.** In Trace Downward mode the per-frame trace count is capped by *Max Ground Traces Per
Update* and served round-robin. At the default 256, 10,000 units refresh their ground height about every
40 frames — invisible, because ground height changes slowly relative to frame rate.

**Tuning checklist for very large armies**

- Raise *Update Interval* to 0.033 (30 Hz) or 0.05 (20 Hz). Shadows are low-frequency detail.
- Switch ground-locked units to **Owner Location** and skip traces entirely.
- Set *Instance End Cull Distance* so distant shadows stop drawing.
- Keep every unit on the same mesh and material so they collapse into one batch. Check `GetNumBatches()`.

## 8. Verifying the claims

- **Draw calls:** `stat RHI` → *DrawPrimitive calls*. Toggle `MDS.Enabled 0` / `1` and compare. The delta
  should equal `GetNumBatches()`.
- **CPU cost:** `GetStats().LastUpdateMilliseconds` is the wall-clock cost of the whole subsystem pass.
- **Trace budget:** `GetStats().NumGroundTracesLastUpdate` must never exceed the configured maximum.
- **Light direction:** rotate the directional light during play. All shadows rotate and stretch together,
  and `LastUpdateMilliseconds` does not change — the stretch is vertex-shader work.

## 9. Troubleshooting

| Symptom | Cause |
| --- | --- |
| No shadows at all | No shadow mesh could be loaded, or `MDS.Enabled` is 0. Check the log for `LogMaterialDrivenShadows`. |
| White/untextured quads | The master material is missing. Assign *Default Shadow Material* in project settings. |
| Shadows pop at the screen edge | Raise *Batch Bounds Scale* above your largest `MaxShadowStretch`. |
| Shadows flicker on the floor | Increase *Ground Offset* or the material's Pixel Depth Offset. |
| Shadows clip through hills | The material is missing its `DepthFade` node — see section 6. |
| Shadow lags behind a fast unit | *Update Interval* is too high; lower it towards 0. |
| More than one draw call | Units use different meshes or materials. Consolidate, then re-check `GetNumBatches()`. |

---

*© 2026 Simulated Flow. All rights reserved.*
