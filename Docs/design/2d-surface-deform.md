# Design: surface and FFD mesh deformation (mesh-deform v3)

Status: **design**. Extends the shipped v2 weighted-mesh path
([2d-skeletal-animation.md](2d-skeletal-animation.md): `DioramaSkinnedSpriteComponent`
on the pure `MeshSkin` + `DragonBonesImport` cores). This is the "free-form mesh
deformation" that v2 explicitly deferred to v3.

## Goal

Render the two DragonBones deformation modes v2 skipped, through the same component
and the same sprite-render path:

- **Surface deformation** - a `surface` bone carrying a control-point grid that warps
  every mesh bound to it. This is what *modern* DragonBones 5.x rigs use (the
  `shizuku` class); most current assets are surface rigs, not weighted meshes, so this
  is the high-value half.
- **FFD (free-form deformation)** - a per-vertex deform animation channel that offsets
  mesh vertices over time. Smaller, and it composes with the existing weighted meshes.

Both are CPU deformation emitted through the `SpriteFeatureProcessor` variable-geometry
path, exactly like v2. No new component, no new runtime dependency.

## Format and algorithm (DragonBones 5.x)

Decoded against the reference runtime (DragonBonesJS `Surface`, `ObjectDataParser`) and
a real surface rig (`shizuku_ske.json`, Apache-2.0).

### Surface bone (static / bind)

A surface is a bone with `"type": "surface"` and fields `segmentX`, `segmentY`, and
`vertices` (flat x,y control points). It has no `transform` block; its pose lives in the
control points.

- Control-point count is **(segmentX + 1) * (segmentY + 1)** (81 for the common 8x8),
  a plain grid, not a bezier patch.
- The grid spans a canonical **+/-200 square** in the bone's local space, so
  `dX = 400 / segmentX`; control point (i, j) has undeformed position
  `(-200 + i*dX, -200 + j*dY)` and actual position from `vertices`.
- **Binding is by parenting**: a slot's mesh is warped by a surface when the slot's
  parent bone is a surface (there is no separate binding record). Surface-bound meshes
  are **non-weighted**; their vertices are expressed in the surface's local space.
- Surfaces can **nest** (a surface bone parented to another surface).

### Surface warp (how a vertex is deformed)

Piecewise-affine over the triangulated grid (NOT bilinear, NOT bezier). Each cell is
split into two triangles; each triangle carries an affine that maps its undeformed
corners onto the deformed control points:

1. Locate the cell: `iX = floor((x + 200) / dX)`, `iY = floor((y + 200) / dY)`.
2. Pick the triangle by the cell-diagonal sign test (`k = -dY/dX`).
3. Build the affine from the triangle's three deformed corners (rotation from the first
   edge, skew from the second, scaleX/scaleY from the edge lengths, translation so a
   corner maps exactly). This is the reference `_getAffineTransform`.
4. Apply: `deformed = (a*x + c*y + tx, b*x + d*y + ty)`.

Points in the +/-200..+/-1000 band are handled by edge-extrapolation cells; beyond
+/-1000 the plain bone transform is used. The reference is deliberately **C0** (no
cross-cell smoothing; it carries a `// TODO interpolation`). We **match the reference
exactly** for authoring parity rather than "improving" it with a smoother interpolant.

### Deform animation channels

- **Surface animation** drives the surface's control-point offsets (a deform channel on
  the surface bone, `TimelineType.Surface` = 50): per-control-point (dx, dy) deltas added
  to the bind control points before the warp.
- **FFD / mesh deform** offsets mesh vertices directly. Two encodings, both supported on
  read: the legacy `ffd` block, and the modern typed `timeline` entry
  (`TimelineType.SlotDeform` = 22) whose delta array is keyed `value` (or `vertices`).
  A frame's `offset` is a flat float index into the full delta array so only the moving
  sub-range is stored; the rest default to zero.
- **Application order is fixed**: deform deltas are added in the mesh's local/bind space
  first, then skinning or the surface warp is applied on top. Easing reuses the existing
  cubic-bezier curve evaluator.

## Decision

Extend the existing cores rather than fork a parallel path:

- A new pure header `SurfaceDeform.h` (sibling to `MeshSkin.h`) holds the grid warp and
  the trivial deform-add, so the risky geometry is unit-tested in isolation with no
  engine dependency, the same discipline that made v2 land cleanly.
- The parser (`DragonBonesImport`) gains a surface bone type, a non-weighted
  surface-bound mesh kind, and the deform channels.
- The presenter and `SpriteFeatureProcessor` mesh-draw path are unchanged in shape; they
  just receive vertices that have been deform-offset and surface-warped before submit.
- The runtime component and bus are the **same** `DioramaSkinnedSpriteComponent` /
  `DioramaSkinnedSpriteRequestBus`; a surface rig "just loads". Any new knob is added on
  both the bus and the Inspector (AI/human parity).

## Data model additions

```
SurfaceData    : { name, parentIndex, segmentX, segmentY, controlPoints[] (bind) }
SurfaceMesh    : { slotName, drawOrder, vertices[] (surface-local), uv[], indices[],
                   surfaceBoneIndex }              // non-weighted; sibling to SkinnedMesh
DeformTimeline : { targetName, kind (mesh-ffd | surface), frames[]{ startTime, duration,
                   tween/curve, offset, deltas[] } }
```

`SurfaceData` extends the bone list; `SurfaceMesh` is a second mesh collection alongside
the weighted `SkinnedMesh`; `DeformTimeline` extends `Animation` next to the existing
translate/rotate/scale bone tracks.

## Security and performance

- `segmentX`, `segmentY`, control-point counts, vertex/mesh counts, and deform-frame
  sizes are validated and bounded at parse; a rig cannot size a buffer unchecked.
- The per-triangle affine cache for a surface is rebuilt **only when its control points
  moved this frame** (a static surface costs nothing after the first build), matching the
  VISION efficiency criterion and the no-per-frame-heap-allocation render rule (scratch
  buffers reused).
- Warp math is `std::` only (atan2/sqrt), no POSIX-isms (Windows parity).

## Phasing (tasks #9-12)

1. **Phase A - pure cores + tests.** `SurfaceDeform.h` (grid warp incl. edge cells) plus
   the deform-add helper, unit-tested numerically (identity grid is a passthrough, a
   sheared grid matches a hand-computed result). Self-contained; no engine needed.
2. **Phase B - parser + fixtures.** Surface bones, non-weighted surface-bound meshes, and
   both deform channel encodings with `offset` expansion. Verified against `shizuku_ske.json`
   (Apache-2.0, verify-only, not committed to this repo). Unit tests.
3. **Phase C - integration.** Presenter rebuilds surface grids from bind plus animated
   control-point deltas, warps surface meshes, applies FFD deltas in the correct order,
   handles nested surfaces, and submits through the existing mesh-draw path. Visual
   verification at the monitor with a surface rig deforming live.
4. **Phase D - example and docs.** An original, IP-free rippling-water surface rig
   (generated by a script, the seaweed-example pattern) plus a demo level, a how-to, and
   CHANGELOG / roadmap / this doc updated to "shipped".

## Deferred

- **Weighted-mesh FFD** (a mesh that is both skinned and FFD-animated). The modern
  reference parser stubs this; only the legacy path implements it, and no public rig
  exercises the combination, so it is out of scope for this epic (add later via the legacy
  path if a real asset needs it).
- Smoother-than-reference (C1) surface interpolation, a compiled product mesh asset (vs
  reading JSON products directly), and the optional continuous editor-viewport redraw.

## Verification plan (needs the monitor)

- A surface rig (`shizuku`, verify-only) loads and its surface-bound meshes warp as the
  deform animation plays; no shard/gutter artifacts; parts layer correctly.
- The shipped rippling-water rig deforms smoothly and loops seamlessly.
- No per-frame heap allocation; static surfaces do not rebuild their affine cache.
- The runtime client still has no new dependency.
