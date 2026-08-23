# Material-Driven Shadows — Fab Store Listing

*Copy-paste source for the Fab product page. Version 1.0.0 · UE 5.8 · Win64.*

---

## Headline

**Material-Driven Shadows — Massive Unit Shadow System**

*Sub-headline:* Single-draw-call blob shadows for 10,000+ units. One instanced mesh, four floats per unit,
zero shadow-map cost.

*Alternative short taglines*

- One draw call. Ten thousand shadows.
- Shadows for armies, not for hero characters.
- The shadow system that does not care how many units you spawn.

---

## Pitch (one paragraph)

Virtual Shadow Maps and decal components both fall apart at scale: VSMs pay an enormous GPU and CPU culling
cost past a few hundred movers, and one decal per unit means one draw call per unit — your RTS army, your
horde wave, your bullet-hell projectile swarm drowns the renderer before it drowns the player.
Material-Driven Shadows takes the opposite approach. Every shadow that shares a mesh and a material lives in
**one** instanced static mesh component, and the projection itself is material math rather than
rasterisation: the CPU writes a single transform plus four custom-data floats per unit, and the vertex and
pixel shaders do the rest — sliding the shadow along the light direction as a unit jumps, stretching it as
the sun drops towards the horizon, and blending it softly into hills and stairs with a depth fade instead of
per-frame ground traces. Drop the component on your unit Blueprint, press Play, and the whole army costs
**one draw call**. The included demo map measures **1,101 shadow casters in 1 draw call at 0.91 ms of total
subsystem CPU time**.

---

## Feature bullets

- **One draw call per mesh/material pair** — not per unit. 100 units or 10,000 units cost the same number of
  draw calls, and `GetNumBatches()` tells you exactly what that number is.
- **The projection lives in the material.** Per-instance custom data carries radius, opacity, height above
  ground and light-stretch to the GPU. Rotating your day/night sun re-stretches every shadow in the world in
  the vertex shader, at **zero** extra CPU cost.
- **Jump and fly handling out of the box.** Shadows slide away from an airborne unit and fade out as it
  climbs, driven by two tunable heights per component.
- **Slope fitting without CPU traces.** A depth-fade blend in the material makes the flat quad hug hills,
  ramps and stairs instead of clipping through them.
- **Bounded ground-trace cost.** Round-robin, budgeted downward traces (default 256 per update, world-wide)
  mean your trace cost is a constant you choose — not a function of unit count. Or skip traces entirely with
  *Owner Location* mode, or feed heights from your own terrain data with *Manual* mode.
- **Zero-cost spawn/despawn.** Instance slots are recycled through a free list, never removed. Spawning a
  unit is a list pop; the instance buffer settles at your peak concurrent count.
- **No render-state churn.** The entire per-frame update is one batched transform write plus one ranged
  custom-data write per batch, both without recreating the render proxy.
- **Nothing to place in the level.** A world subsystem creates and owns everything automatically. Add
  component, press Play.
- **Fully Blueprint-exposed.** Component, subsystem, stats struct and a function library — a Blueprint-only
  project can use every feature.
- **Built-in profiling.** A live stats struct (batches, slots, recycled slots, traces, update milliseconds,
  stretch factor) plus the `MDS.Enabled 0/1` console variable for instant A/B comparison against the
  engine's own shadows.
- **Tunable from Project Settings.** Default quad, master material, batch mode, update rate, trace budget,
  culling distances and light-refresh interval — all without touching code.
- **Ready-to-use master material** with documented parameters (`ShadowColor`, `SlideScale`, `EdgeSharpness`,
  `GroundBlendDistance`), plus a full recipe in the docs if you want to rebuild or restyle it.
- **Playable demo map** with a stats HUD and buttons to spawn units in batches of 200, toggle the system, and
  swing the sun from noon to sunset.
- **Full C++ source included**, Epic coding standard, engine modules only — no third-party libraries, no
  dependency on other Marketplace plugins.

---

## Technical details

**Code modules**

| Module | Type | Loading Phase | Platforms |
| --- | --- | --- | --- |
| `MaterialDrivenShadows` | Runtime | Default | Win64 |

**Number of C++ classes:** 4 (`UMaterialDrivenShadowComponent`, `UMaterialDrivenShadowSubsystem`,
`UMaterialDrivenShadowSettings`, `UMaterialDrivenShadowStatics`) plus 2 Blueprint structs
(`FMaterialDrivenShadowStats`, `FMaterialDrivenShadowHandle`) and 2 Blueprint enums.

**Network replicated:** No — shadows are purely cosmetic and are computed locally on every client.

**Supported Development Platforms:** Windows (Win64)
**Supported Target Build Platforms:** Windows (Win64)
**Engine version:** Unreal Engine 5.8
**Dependencies:** `Core`, `CoreUObject`, `Engine`, `RenderCore`, `RHI`, `DeveloperSettings`, `Projects` —
engine modules only.

**Included content:** master material `M_MaterialDrivenShadow`, demo map
`L_MaterialDrivenShadowsDemo`, demo unit and director Blueprints, stats HUD widget, two demo materials.

**Documentation:** `Docs/DOCUMENTATION.md` in the plugin — installation, quick start, full API reference,
code examples, master-material graph, performance tuning and troubleshooting.

**Documentation link:** https://github.com/SimulatedFlow/ue-plugin-MaterialDrivenShadows
**Support:** teufelsilvan@gmail.com

**Important / Additional notes**

- These are **projected blob shadows**, not shadow maps. They do not reproduce a unit's silhouette, do not
  self-shadow and do not receive on walls. Use them for crowds and keep CSM/VSM for hero characters.
- The system follows the scene's **primary directional light** (or an explicit direction override). Point and
  spot lights do not drive these shadows.
- One draw call per **mesh/material pair** — mixing shadow quads multiplies batches.
- Requires a shadow-receiving ground surface; shadows land on the ground point beneath each unit.

---

## Target audience

- **RTS and city-builder developers** with hundreds to thousands of simultaneously visible units.
- **Horde / survivor-like / bullet-hell developers** where enemy counts are the core mechanic and every
  millisecond of CPU is contested.
- **Top-down ARPG and twin-stick developers** who want readable, stylised shadows with a fixed light
  direction rather than physically correct ones.
- **Mobile, Switch and low-end PC targets** where the shadow-map pipeline is simply not affordable.
- **Tower-defence, auto-battler and simulation developers** with large static-ish populations.
- **Technical artists** who want the shadow look under their own control in a material rather than buried in
  renderer settings.

Skill level: drop-in for Blueprint users (add component, press Play); fully extensible for C++ teams.

---

## Price idea

**€ 49.00** (approx. **$ 54.99** USD) — one-time, per seat, Fab standard licence.

*Reasoning:* this sits above the €20–30 band of Blueprint-only blob-shadow packs because it is a real
engineering solution to a shipping-blocker problem (draw-call explosion in mass-unit games) with full C++
source, and below the €80–120 band of large gameplay frameworks, because it is a focused single-purpose
system. Teams that need it are usually already blocked by a profiler number, which makes the value obvious
and the price an easy decision.

*Launch strategy:* consider a **-30 % launch discount (€ 34.30) for the first two weeks** to build the
initial review count, then hold at €49.

---

## Suggested media set

| Slot | Image | Caption |
| --- | --- | --- |
| Featured (1920×1080) | `MDS_02_1100Units_OneDrawCall.png` | 1,101 shadow casters. 1 draw call. 0.91 ms. |
| Gallery 1 | `MDS_01_Noon_500Units.png` | Round, tight shadows with the sun overhead. |
| Gallery 2 | `MDS_03_Sunset_Stretch.png` | Sunset: every shadow stretches and rotates in the vertex shader. |
| Gallery 3 | `MDS_03_LowSun_Stretched.png` | Low sun, long shadows — still one draw call. |
| Gallery 4 | `MDS_04_ShadowsDisabled.png` | The same scene with `MDS.Enabled 0` — the honest A/B. |
| Icon | `Resources/Icon128.png` | Plugin icon. |

---

## Tags / keywords

`shadow` `shadows` `blob shadow` `performance` `optimization` `draw calls` `RTS` `top down` `horde`
`mass units` `instanced static mesh` `ISM` `crowd` `mobile` `material` `vertex shader` `subsystem` `C++`

---

*© 2026 Silvan Teufel. All rights reserved.*
