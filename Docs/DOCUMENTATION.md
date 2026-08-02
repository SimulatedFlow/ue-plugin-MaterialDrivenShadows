# Material-Driven Shadows — Documentation

**Version 1.0.0 · Unreal Engine 5.8 · Win64 · Support: simulatedflow@gmail.com**

---

## Table of contents

1. [What this plugin is](#1-what-this-plugin-is)
2. [Supported engine and platforms](#2-supported-engine-and-platforms)
3. [Installation](#3-installation)
4. [Quick start](#4-quick-start)
5. [The demo map](#5-the-demo-map)
6. [API reference](#6-api-reference)
7. [Project settings](#7-project-settings)
8. [Code examples](#8-code-examples)
9. [The master material](#9-the-master-material)
10. [Performance notes](#10-performance-notes)
11. [Verifying the claims](#11-verifying-the-claims)
12. [Troubleshooting](#12-troubleshooting)
13. [Limitations](#13-limitations)

---

## 1. What this plugin is

A shadow system for games that need far more shadow casters than the renderer can reasonably rasterise:
RTS armies, horde shooters, top-down survival, bullet-hell projectiles, tower defence.

Instead of asking the shadow-map pipeline to handle 10,000 movers, every shadow becomes **one instance in
one instanced static mesh component**. The CPU writes a single transform plus four custom-data floats per
unit per update. Everything that makes the shadow look like a shadow — sliding along the light direction as
a unit rises, stretching when the sun is low, blending softly into uneven terrain — happens in the material.

The result: **one draw call per mesh/material pair**, regardless of unit count.

What ships:

| | |
| --- | --- |
| `UMaterialDrivenShadowComponent` | Scene component you attach to any shadow caster. |
| `UMaterialDrivenShadowSubsystem` | Tickable world subsystem that batches, updates and profiles them all. |
| `UMaterialDrivenShadowSettings` | Project settings (Developer Settings) under *Plugins*. |
| `UMaterialDrivenShadowStatics` | Blueprint function library for reaching the subsystem in one node. |
| `M_MaterialDrivenShadow` | Ready-to-use master material with the full vertex/pixel graph. |
| Demo map + Blueprints | Playable stress test with a live stats HUD. |
| Full C++ source | Runtime module, no editor-only dependency. |

## 2. Supported engine and platforms

| | |
| --- | --- |
| **Engine version** | Unreal Engine **5.8** |
| **Modules** | `MaterialDrivenShadows` — `Runtime`, `LoadingPhase: Default`, `PlatformAllowList: Win64` |
| **Supported target platform** | **Win64** (Development, Shipping, and Editor targets all build clean) |
| **Dependencies** | `Core`, `CoreUObject`, `Engine`, `RenderCore`, `RHI`, `DeveloperSettings`, `Projects` — engine modules only, no third-party or Marketplace dependencies |
| **Renderer** | Deferred and forward. Works with Lumen, VSMs and static lighting — the plugin never touches the shadow-map pipeline, it only draws instanced geometry. |
| **Nanite** | The shadow quads themselves are non-Nanite instanced meshes. Your units can be Nanite; that is independent. |
| **Blueprint only projects** | Supported — the plugin ships precompiled binaries, and the whole API is Blueprint-exposed. |

> The module is gated to `Win64` in the `.uplugin`. Other platforms are technically portable (nothing in the
> source is platform-specific), but they are not built, tested or supported in version 1.0.0. Adding a
> platform means adding it to `PlatformAllowList` and rebuilding.

## 3. Installation

1. Close the editor.
2. Copy the `MaterialDrivenShadows` folder into your project's `Plugins/` directory
   (create `Plugins/` if it does not exist).
3. Reopen the project. For C++ projects, regenerate project files and build once, or let the editor compile
   the plugin on first open.
4. Enable **Material-Driven Shadows** under *Edit ▸ Plugins ▸ Rendering* if it is not already enabled, and
   restart if prompted.
5. Optional: open *Edit ▸ Project Settings ▸ Plugins ▸ Material-Driven Shadows*. Everything has working
   defaults — the default mesh is the engine plane and the default material is the shipped
   `M_MaterialDrivenShadow`. You only need to touch this to restyle or to tune performance.

Nothing has to be placed in a level. The subsystem is created automatically in every game and PIE world.

## 4. Quick start

1. Add a **Material Driven Shadow** component to your unit's Blueprint or C++ actor.
2. Set **Shadow Radius** to roughly the unit's footprint (100 uu is a reasonable humanoid).
3. If your units never leave the floor, set **Ground Mode** to *Owner Location* — it skips ground traces
   entirely. Leave it on *Trace Downward* if they jump, fly or walk over cliffs.
4. Press Play.

That is the whole setup. The component registers itself with the world subsystem on `BeginPlay`, the
subsystem creates the instanced component on first use, and every unit sharing a mesh and material collapses
into one draw call.

To confirm it is working, open the console and type `stat RHI`, then toggle `MDS.Enabled 0` and `MDS.Enabled 1`.
The difference in *DrawPrimitive calls* should equal the number of batches, not the number of units.

## 5. The demo map

`/MaterialDrivenShadows/MaterialDrivenShadows/Maps/L_MaterialDrivenShadowsDemo`

A playable stress test. Press Play and use the on-screen buttons:

| Button | What it does |
| --- | --- |
| **+200 UNITS** | Spawns another 200 `BP_ShadowDemoUnit` actors, each with a shadow component. |
| **SHADOWS: ON / OFF** | Calls `SetMaterialDrivenShadowsEnabled` — the A/B comparison. |
| **SUN: NOON / LOW / SUNSET** | Rotates the directional light so you can watch every shadow stretch and rotate at once. |

The HUD (`WBP_ShadowDemoHUD`) reads `GetMaterialDrivenShadowStats()` every frame and shows shadow casters,
draw calls (ISM batches), instance slots, recycled free slots, ground traces this update, subsystem update
milliseconds and the current light stretch factor. It is about thirty nodes of Blueprint and is worth
copying into your own debug HUD.

Reference measurement from the shipped demo (editor PIE, 1080p): **1,101 shadow casters → 1 draw call,
0.91 ms subsystem update, 256 ground traces per update**.

Demo content lives under `Content/MaterialDrivenShadows/` and can be deleted from a shipping project without
touching the plugin code — only the master material `M_MaterialDrivenShadow` is referenced by the default
project settings.

## 6. API reference

### `UMaterialDrivenShadowComponent` (Scene Component)

Attach one per shadow caster. It never ticks and owns no primitive of its own.
Class group *Rendering*, spawnable from Blueprint.

**Properties** — all `EditAnywhere`, `BlueprintReadOnly`, category *Shadows*:

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ShadowRadius` | `float` | 100.0 | Base size in world units. Converted to instance scale via **Reference Mesh Radius**. |
| `BaseOpacity` | `float` | 0.8 | Opacity on the ground, before the height fade. |
| `MaxShadowStretch` | `float` | 2.0 | Cap on the stretch the material may apply at grazing sun angles. 1 keeps the shadow perfectly round. |
| `CustomShadowMesh` | `UStaticMesh*` | none | Per-unit shadow quad. Units sharing a mesh share a draw call. |
| `CustomShadowMaterial` | `UMaterialInterface*` | none | Per-unit shadow material. |
| `ShadowOffset` | `FVector` | 0,0,0 | Extra offset in the component's own space. |
| `GroundMode` | `EMaterialDrivenShadowGroundMode` | Trace Downward | How ground height is found — see below. |
| `MaxGroundTraceDistance` | `float` | 2000.0 | Reach of the downward trace. Only shown in *Trace Downward* mode. |
| `FadeStartHeight` | `float` | 250.0 | Height above ground where the shadow begins to fade. |
| `FadeEndHeight` | `float` | 1500.0 | Height above ground where the shadow is gone. Must exceed Fade Start Height. |
| `bShadowEnabled` | `bool` | true | Skip this shadow without releasing its instance slot. |
| `bAutoRegister` | `bool` | true | Register on `BeginPlay`. Turn off to control registration yourself. |

**Functions** — all Blueprint-callable, category *Material-Driven Shadows*:

| Function | Meaning |
| --- | --- |
| `RegisterShadow()` | Claim an instance slot. Safe to call twice; the second call is a no-op. |
| `UnregisterShadow()` | Release the slot back to the free list. |
| `IsShadowRegistered()` | True while this component owns a slot. |
| `SetShadowOpacity(float)` | Set base opacity. Takes effect on the next subsystem update. |
| `SetShadowRadius(float)` | Set radius in world units. |
| `SetShadowEnabled(bool)` | Enable/disable this one shadow, keeping its slot. |
| `SetMaxShadowStretch(float)` | Set the per-unit stretch cap. |
| `SetGroundHeight(float WorldZ)` | Supply the ground Z yourself (*Manual* mode). |
| `GetGroundHeight()` | Last known ground Z. |
| `HasGroundHeight()` | True once a trace hit or a manual value established a real ground height. |
| `SetShadowMeshAndMaterial(Mesh, Material)` | Move this shadow into a different batch, releasing the old slot cleanly. Pass null for either to fall back to the project default. |

The setters are deliberately cheap: they write a float on the component and the value reaches the GPU on the
next subsystem pass. Nothing marks a render state dirty, so calling `SetShadowOpacity` on 10,000 units in one
frame costs 10,000 float stores.

#### Ground modes (`EMaterialDrivenShadowGroundMode`)

| Mode | Cost | Use when |
| --- | --- | --- |
| **Owner Location (No Trace)** | free | Units never leave the floor. Height above ground is always 0. |
| **Trace Downward (Budgeted)** | budgeted | Units jump, fly or walk over cliffs. Traces are served round-robin from a per-update budget shared by the whole world. |
| **Manual (SetGroundHeight)** | free | You already know the terrain height — call `SetGroundHeight()` from your movement or terrain code. |

### `UMaterialDrivenShadowSubsystem` (Tickable World Subsystem)

Created automatically in every game and PIE world. Reachable from Blueprint through
**Get Material Driven Shadows** or a *Get World Subsystem* node, and from C++ through
`World->GetSubsystem<UMaterialDrivenShadowSubsystem>()`.

| Function | Meaning |
| --- | --- |
| `SetShadowsEnabled(bool)` | Master switch. Disabling collapses every instance without destroying batches. |
| `AreShadowsEnabled()` | Current state of the master switch. |
| `GetStats()` | `FMaterialDrivenShadowStats` — batch count, slot usage, trace count, timings. |
| `SetLightDirectionOverride(FVector)` | Cast along a fixed direction instead of following the sun. |
| `ClearLightDirectionOverride()` | Resume following the scene's directional light. |
| `GetLightDirection()` | Normalised direction the light travels in (pointing away from the sun). |
| `GetShadowDirection()` | That direction flattened onto the ground plane. |
| `GetNumBatches()` | Number of instanced components — this is your added draw-call count. |
| `RequestImmediateUpdate()` | Ignore the update interval for one tick. |

`RegisterShadowComponent()` / `UnregisterShadowComponent()` are C++-only and are called for you by the
component; you normally never touch them.

### `UMaterialDrivenShadowStatics` (Blueprint Function Library)

Every node takes a world context object.

| Node | Meaning |
| --- | --- |
| `GetMaterialDrivenShadows` | The subsystem, or null outside a game world. |
| `SetMaterialDrivenShadowsEnabled` | Master on/off. |
| `GetMaterialDrivenShadowStats` | Stats struct; returns defaults when there is no subsystem. |
| `SetMaterialDrivenShadowLightDirection` | Explicit light direction override. |
| `ClearMaterialDrivenShadowLightDirection` | Back to following the sun. |

### `FMaterialDrivenShadowStats` (Blueprint struct)

| Field | Meaning |
| --- | --- |
| `NumBatches` | Instanced components in play. Each one is exactly one draw call. |
| `NumRegisteredShadows` | Shadow components currently registered. |
| `NumInstanceSlots` | Slots allocated across all batches (registered + recycled). |
| `NumFreeSlots` | Allocated but currently unused slots waiting to be recycled. |
| `NumGroundTracesLastUpdate` | Traces performed in the last update. Never exceeds the configured budget. |
| `LastUpdateMilliseconds` | Wall-clock cost of the last subsystem update. |
| `ShadowDirection` | Normalised ground direction the shadows currently extend towards. |
| `LightStretchFactor` | 1 at high noon, rising towards the per-component maximum at grazing angles. |

### `FMaterialDrivenShadowHandle` (Blueprint struct)

`BatchIndex` + `InstanceIndex`, plus `IsValid()`. Returned by `GetShadowHandle()`. You rarely need it —
it exists so tooling and tests can point at a specific instance.

### Console variable

`MDS.Enabled 0|1` — hide or show every batched shadow at runtime without touching game code. Useful for
A/B profiling against the engine's own shadows.

## 7. Project settings

*Project Settings ▸ Plugins ▸ Material-Driven Shadows*, stored in `DefaultGame.ini` under
`[/Script/MaterialDrivenShadows.MaterialDrivenShadowSettings]`.

| Setting | Default | Meaning |
| --- | --- | --- |
| **Enable Shadows** | true | Master switch applied when a world starts. |
| **Default Shadow Mesh** | `/Engine/BasicShapes/Plane` | Flat, XY-aligned quad facing +Z. |
| **Default Shadow Material** | `/MaterialDrivenShadows/MaterialDrivenShadows/Materials/M_MaterialDrivenShadow` | Master material, shipped with the plugin. Empty keeps the mesh's own material. |
| **Reference Mesh Radius** | 50.0 | Radius of the default mesh at scale 1. The engine plane is 100×100 uu, hence 50. Change this if you swap in a mesh of a different size. |
| **Batch Mode** | Instanced (ISM) | See section 10. |
| **Update Interval** | 0.0 | Seconds between updates. 0 = every frame; 0.033 halves the CPU cost at 30 Hz. |
| **Max Ground Traces Per Update** | 256 | Hard cap on line traces per update, shared by all shadows. |
| **Ground Trace Channel** | WorldStatic | Channel used by the downward trace. |
| **Ground Offset** | 2.0 | Lift above the detected ground, as cheap Z-fighting insurance. |
| **Batch Bounds Scale** | 4.0 | Culling headroom for the vertex-shader displacement. Keep ≥ your largest Max Shadow Stretch. |
| **Instance Start / End Cull Distance** | 0 / 0 | Per-instance distance culling. 0 disables it. |
| **Light Refresh Interval** | 2.0 | Seconds between re-scans for the primary directional light. |
| **Fallback Light Direction** | 0,0,-1 | Used when the level has no usable directional light. |

## 8. Code examples

### Adding a shadow in C++

```cpp
#include "MaterialDrivenShadowComponent.h"

AMyUnit::AMyUnit()
{
    ShadowComponent = CreateDefaultSubobject<UMaterialDrivenShadowComponent>(TEXT("Shadow"));
    ShadowComponent->SetupAttachment(RootComponent);

    ShadowComponent->ShadowRadius     = 90.0f;
    ShadowComponent->BaseOpacity      = 0.75f;
    ShadowComponent->MaxShadowStretch = 2.5f;

    // Ground-locked infantry: skip the trace entirely.
    ShadowComponent->GroundMode = EMaterialDrivenShadowGroundMode::OwnerLocation;
}
```

### Fading a shadow out as a unit dies

```cpp
void AMyUnit::OnDeath(float Alpha)
{
    // Cheap: writes one float. Reaches the GPU on the next subsystem pass.
    ShadowComponent->SetShadowOpacity(FMath::Lerp(0.75f, 0.0f, Alpha));
}
```

### Feeding the ground height from your own terrain data

```cpp
// Manual mode — no line trace at all. Ideal when you already own a heightfield.
ShadowComponent->GroundMode = EMaterialDrivenShadowGroundMode::Manual;

void AMyUnit::Tick(float DeltaSeconds)
{
    const float GroundZ = MyTerrain->SampleHeight(GetActorLocation());
    ShadowComponent->SetGroundHeight(GroundZ);
}
```

### Reading the stats, e.g. for a debug HUD

```cpp
#include "MaterialDrivenShadowSubsystem.h"

if (UMaterialDrivenShadowSubsystem* Shadows = GetWorld()->GetSubsystem<UMaterialDrivenShadowSubsystem>())
{
    const FMaterialDrivenShadowStats& Stats = Shadows->GetStats();

    UE_LOG(LogTemp, Log, TEXT("%d shadows in %d draw call(s), %.2f ms, %d traces"),
        Stats.NumRegisteredShadows,
        Stats.NumBatches,
        Stats.LastUpdateMilliseconds,
        Stats.NumGroundTracesLastUpdate);
}
```

### A fixed, stylised shadow direction (top-down games)

```cpp
// Ignore the sun; always cast towards +X and slightly down.
if (UMaterialDrivenShadowSubsystem* Shadows = GetWorld()->GetSubsystem<UMaterialDrivenShadowSubsystem>())
{
    Shadows->SetLightDirectionOverride(FVector(1.0f, 0.0f, -2.0f));
}
```

### Same thing from Blueprint

`Set Material Driven Shadow Light Direction` (Light Direction = `1,0,-2`) →
`Clear Material Driven Shadow Light Direction` when you want the sun back.
For a quick A/B, `Set Material Driven Shadows Enabled` on a key press does exactly what the demo's
*SHADOWS: ON/OFF* button does.

### Swapping a unit onto a bigger shadow quad at runtime

```cpp
// e.g. an infantry unit boarding a vehicle
ShadowComponent->SetShadowMeshAndMaterial(VehicleShadowQuad, /*Material=*/nullptr);
ShadowComponent->SetShadowRadius(260.0f);
```

The old instance slot is released to its batch's free list and a slot in the vehicle batch is claimed.
Note that this creates a second batch — a second draw call — the first time a new mesh/material pair appears.

## 9. The master material

`M_MaterialDrivenShadow` ships ready to use and is already wired into the project settings. This section
documents its graph so you can restyle it, rebuild it from scratch, or write your own.

**Material settings**

- **Blend Mode:** `Translucent` (shipped) — gives you control over shadow tint. `Modulate` is cheaper and
  multiplies more naturally onto the ground; either works.
- **Shading Model:** `Unlit`.
- **Used with Instanced Static Meshes:** on.
- **Cast Shadow:** off (the subsystem also disables it on the component).

**Parameters you can tune** (create a Material Instance and override these):

| Parameter | Meaning |
| --- | --- |
| `ShadowColor` | Colour the shadow multiplies onto the ground. |
| `SlideScale` | How far the shadow walks away from a unit that is off the ground. 0.5 is a good stylised default. |
| `EdgeSharpness` | Falloff exponent of the blob's radial gradient. |
| `GroundBlendDistance` | `DepthFade` distance used for slope fitting, in world units (20–60 is the useful range). |

**Per-instance custom data**

The subsystem writes exactly four floats per instance. The material reads them through
`PerInstanceCustomData` at these indices (mirrored in C++ as
`MaterialDrivenShadows::CustomDataIndex_*` in `MaterialDrivenShadowTypes.h`):

| Index | Name | Contents |
| --- | --- | --- |
| 0 | Radius | Shadow radius in world units. Scale your soft edge with this so different unit sizes look consistent. |
| 1 | Opacity | Final opacity in 0..1, height fade already applied. |
| 2 | Height Above Ground | World units the unit is above its ground point. |
| 3 | Stretch Factor | ≥ 1. Grows as the sun approaches the horizon, clamped per component. |

**Vertex shader — World Position Offset**

The subsystem has already yawed each instance so its **local +X points along the shadow direction**, which
makes the projection one-dimensional:

1. Take the vertex's `Local Position`.
2. **Stretch:** multiply local X by `PerInstanceCustomData[3]`, then subtract the original X so you have a
   pure offset rather than an absolute position.
3. **Slide:** add `PerInstanceCustomData[2] * SlideScale` along the same local X axis, so the shadow walks
   away from a jumping unit rather than staying glued under it.
4. Transform the resulting local-space offset into world space (`TransformVector`, Local → World,
   instance transform source) and feed it into **World Position Offset**.

Because these offsets leave the mesh's own bounds, the subsystem sets the component's **Bounds Scale** from
the *Batch Bounds Scale* project setting. Raise it if shadows pop at screen edges.

**Pixel shader — shape, fade and slope fitting**

1. **Shape:** radial falloff from the mesh UVs, sharpened by `EdgeSharpness`. Multiply by
   `PerInstanceCustomData[1]` for opacity.
2. **Slope fitting:** the quad is flat, so it will intersect hills and stairs. A `DepthFade` node
   (`GroundBlendDistance` ≈ 20–60 uu) feeds the opacity chain: where the ground cuts through the quad, the
   shadow fades out instead of clipping. The shadow appears to hug the terrain **with no CPU traces at all**.
3. **Z-fighting:** the *Ground Offset* project setting (2 uu by default) lifts the quad off the floor. If you
   still see flicker on perfectly flat geometry, add a small **Pixel Depth Offset** (5–15 uu) to the graph.
4. Output: `ShadowColor` → **Emissive Color**, the opacity chain → **Opacity**. For a `Modulate` variant,
   output `lerp(1, ShadowColor, Opacity)` into **Emissive Color** instead.

If the material is missing, batches fall back to the mesh's own material and the plugin logs a warning under
`LogMaterialDrivenShadows` — the system still runs, it just renders untextured quads.

## 10. Performance notes

**Why ISM is the default and not HISM.** The original design for this plugin specified HISM. In UE 5.8,
HISM's `UpdateInstanceTransform` flags the CPU cluster tree out of date and schedules an async rebuild
whenever an instance *changes its translation*. With 10,000 shadows moving every frame that is a full tree
rebuild per frame — precisely the CPU cost the plugin exists to avoid. A plain ISM has no cluster tree:
transform updates are a memcpy into the instance buffer and culling happens per-instance on the GPU, which is
both faster and more accurate here. Both produce a single draw call.

`Batch Mode` therefore defaults to **Instanced (ISM)**. Switch it to **Hierarchical (HISM)** only when your
shadows barely move — static props, wreckage, buildings — where the cluster tree pays for itself.

**Instance slots are never removed.** Removing an ISM instance swaps indices around and would invalidate
every handle above it. Instead, unregistering a shadow collapses its instance to a negligible scale and
returns the slot to a free list, where the next shadow reuses it. Spawning and despawning units therefore
costs nothing but a list pop, and the instance buffer settles at your peak concurrent unit count.

**Two writes per batch per update.** The whole update is one `BatchUpdateInstancesTransforms` and one ranged
`SetCustomData` per batch, both with `bMarkRenderStateDirty = false`. The render proxy is never recreated;
the instance data manager streams the deltas to the GPU scene.

**Trace budget.** In Trace Downward mode the per-update trace count is capped by *Max Ground Traces Per
Update* and served round-robin. At the default 256, 10,000 units refresh their ground height about every
40 frames — invisible, because ground height changes slowly relative to frame rate.

**Tuning checklist for very large armies**

- Raise *Update Interval* to 0.033 (30 Hz) or 0.05 (20 Hz). Shadows are low-frequency detail.
- Switch ground-locked units to **Owner Location** and skip traces entirely.
- Set *Instance End Cull Distance* so distant shadows stop drawing.
- Keep every unit on the same mesh and material so they collapse into one batch. Check `GetNumBatches()`.
- Lower *Max Ground Traces Per Update* — 64 is still plenty for slow-moving ground units.

## 11. Verifying the claims

- **Draw calls:** `stat RHI` → *DrawPrimitive calls*. Toggle `MDS.Enabled 0` / `1` and compare. The delta
  should equal `GetNumBatches()`.
- **CPU cost:** `GetStats().LastUpdateMilliseconds` is the wall-clock cost of the whole subsystem pass.
  The demo map reports 0.91 ms for 1,101 casters at default settings.
- **Trace budget:** `GetStats().NumGroundTracesLastUpdate` must never exceed the configured maximum.
- **Light direction:** rotate the directional light during play. All shadows rotate and stretch together, and
  `LastUpdateMilliseconds` does not change — the stretch is vertex-shader work.
- **Slot recycling:** kill and respawn units and watch `NumFreeSlots` rise and fall while `NumInstanceSlots`
  stays at the peak. That is the allocator doing its job.

## 12. Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| No shadows at all | No shadow mesh could be loaded, or `MDS.Enabled` is 0, or *Enable Shadows* is off in project settings. Check the log for `LogMaterialDrivenShadows`. |
| White / untextured quads | The master material could not be loaded. Re-assign *Default Shadow Material* in project settings. |
| Shadows pop at the screen edge | Raise *Batch Bounds Scale* above your largest `MaxShadowStretch`. |
| Shadows flicker on the floor | Increase *Ground Offset*, or add Pixel Depth Offset to the material. |
| Shadows clip through hills | The material's `DepthFade` is missing or `GroundBlendDistance` is too small. |
| Shadow lags behind a fast unit | *Update Interval* is too high; lower it towards 0. |
| Shadow sits at the wrong height | Ground Mode is *Trace Downward* but the floor is not on the *Ground Trace Channel*, or it is further away than *Max Ground Trace Distance*. |
| More than one draw call | Units use different meshes or materials. Consolidate, then re-check `GetNumBatches()`. |
| Shadow is the wrong size | *Reference Mesh Radius* does not match your custom shadow mesh. The engine plane is 100×100 uu → 50. |
| Nothing happens in the editor viewport | The subsystem only runs in game and PIE worlds, by design. Press Play. |

## 13. Limitations

Stated plainly, so you know what you are buying:

- **These are blob shadows, not shadow maps.** They do not reproduce a unit's silhouette, do not self-shadow,
  and do not receive on walls. They are the right tool for 10,000 units; they are not a replacement for CSM
  or VSM on your hero characters.
- **Shadows land on the ground beneath a unit**, resolved by the chosen ground mode. They do not wrap around
  arbitrary geometry — the `DepthFade` blend hides intersections rather than projecting onto them.
- **One directional light.** The subsystem follows the scene's primary `ADirectionalLight` (or an explicit
  override). Point and spot lights do not drive these shadows.
- **One draw call per mesh/material pair**, not one per world. Mixing shadow meshes multiplies batches.
- **Win64 only** in version 1.0.0 (see section 2).

---

*© 2026 Simulated Flow. All rights reserved.*
