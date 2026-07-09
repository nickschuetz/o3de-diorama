# Surface + FFD deformation (DragonBones)

Free-form 2D mesh warping: a textured mesh is deformed by a **surface** (a control-point
grid) rather than skinned to bones. This is what modern DragonBones rigs use for soft,
fluid motion (breathing bodies, cloth, water, faces), and it composes with the gem's 2D
lighting, sorting, and camera like any other sprite. World-space content, not screen-space
UI.

It rides on the **same** `Skinned Sprite (mesh deform)` component as the weighted-mesh
path ([how-to 31](31-mesh-deform.md)); a surface rig just loads. See the design in
[Docs/design/2d-surface-deform.md](../design/2d-surface-deform.md).

Demo: `Docs/examples/water_demo.py` builds a runnable `DioramaWaterDemo` level with a
rippling water panel -- a flat quad bound to one surface bone, with a `ripple` clip that
runs a horizontal traveling wave across the control-point grid. The rig is generated
(IP-free) by `scripts/gen_water_rig.py`.

## What surface deformation is

- A DragonBones **`surface` bone** carries a `(segmentX+1) x (segmentY+1)` grid of control
  points over a canonical square. A mesh bound to that surface (its slot's parent is the
  surface bone) is warped by the grid: each vertex is mapped through the surface's
  triangulated cells to a deformed position. No per-vertex bone weights.
- A **surface-deform animation channel** offsets the control points over time, so the whole
  bound mesh warps smoothly.
- **FFD** (free-form deformation) is a related per-vertex channel that offsets mesh vertices
  directly, applied before the surface warp.

Weighted-mesh skinning (bones + weights) and surface deformation can both appear in one
armature; each mesh uses whichever its slot is set up for.

## Using it

Add **Skinned Sprite (mesh deform)** (category *Diorama*) and point **Source (ske.json)** at
a DragonBones export that contains surface bones. The companion `*_tex.json` atlas loads
automatically (`_ske` -> `_tex`). Set **Animation** to a clip that drives the surface deform
and enable **Auto play**. Because the deform changes the mesh geometry every frame, the
editor viewport animates continuously in edit mode (no need to enter game mode to preview).

Run the demo:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/water_demo.py
```

## Nested surfaces

A surface bone can be parented to another surface bone (rigs stack surfaces to build a whole
character). Diorama warps a nested surface's control points through its parent's grid,
parent-first, so a deeply nested surface is placed correctly and a deform on an ancestor
surface propagates down to everything beneath it (e.g. a breathing torso carries the clothes
and limbs stacked on it).

## Authoring a surface rig

- Export from **DragonBones Pro** with surface meshes, or generate one procedurally. The
  demo generator `scripts/gen_water_rig.py` shows the format: one `surface` bone
  (`segmentX`/`segmentY` + a flat `vertices` control-point grid), a mesh whose slot is
  parented to it, and a `timeline` entry of type `50` (surface) whose frames hold the
  per-control-point offsets. For a seamless loop, make the number of wave samples divide the
  cycle length and close the loop with a final frame equal to the first.

## The animation-parameter layer

Modern DragonBones idle clips rarely deform surfaces directly. They use a `type 40`
*AnimationProgress* channel to scrub separate `PARAM_*` sub-animations, and those hold the
surface deform. Diorama drives this: when the playing clip has type-40 channels, each
scrubs its target `PARAM_*` to `progress * duration`, that sub-animation's surface deltas
are sampled, and all of them compose **additively** onto the bind control points (so
parameters at their neutral value contribute nothing). The generated `flow` clip in
`scripts/gen_water_rig.py` demonstrates it: `flow` ripples the water entirely through a
type-40 channel driving `PARAM_WAVE`.

Not yet handled (future work): nested progress (a param driving another param), the type-41
*AnimationWeight* and type-42 *AnimationParameter* (blend) channels, and composing a
sub-animation's *bone* transforms (only its surface deform is composed). So a complex idle
animates its surface-deform parameters but not its bone-driven or nested ones.
