# Design: skeletal 2D animation (cutout and mesh deform)

Status: **v1 (transform-hierarchy cutout) shipped** — `DioramaSkeletalClipComponent`
+ [how-to 18](../howto/18-skeletal.md). v2 (DragonBones mesh deform) and v3 (EMotionFX)
remain design.

## Goal

Smooth, bone-driven 2D animation: a character built from parts that bend and
deform, the Spine/DragonBones look, instead of (or alongside) the existing
flipbook sprite-sheet animation. The hard, valuable parts for a 2D engine are the
**deformation**, the **authoring format**, and the **tooling**, not the math.

## Key findings (O3DE 26.05)

- **No 2D skeletal anything** exists in the engine (no sprite-skin, cutout, or
  2D-bone path). LyShine is UI-only; Diorama today does flipbook only.
- **EMotionFX** is a mature *3D* skeletal system. Its pose evaluation is decoupled
  from rendering: you can create an `ActorInstance` + `AnimGraphInstance`, call
  `Output(pose)`, and read joint transforms on the CPU
  (`ActorInstance::GetTransformData()->GetCurrentPose()->GetWorldSpaceTransform(i)`,
  `Gems/EMotionFX/.../Pose.h`). But its assets are authored through a **3D pipeline**
  (FBX/glTF + armatures), and pulling EMotionFX into a lightweight 2D runtime is a
  heavy dependency.
- **SkinnedMeshFeatureProcessor** wants a 3D `ModelAsset` with bone weights and runs
  GPU compute skinning: shoehorning flat 2D art through a 3D-first pipeline is high
  friction and poor ease-of-use for a 2D author.
- **Spine runtime is proprietary** (paid license) and cannot ship in the gem.
  **DragonBones** is the de-facto **open** (Apache-2.0) 2D skeletal format with a
  free authoring tool.
- Our renderer already rebuilds vertex streams per frame (the shadow pass packs
  fresh quads each frame), so emitting a deformed N-vertex mesh instead of a 4-vertex
  quad is a clean, existing insertion point.

## Decision

A **self-contained 2D bone + mesh-deform path, phased**, that fits Diorama's
priorities (minimal runtime dependency, open assets, author in a 2D tool):

1. **v1 - cutout via transform hierarchy (ship first).** Build characters by
   parenting sprite entities into a transform tree (torso -> upper arm -> forearm)
   and animating their transforms. This needs **no new runtime code and no new
   dependency**: it works today with the transform-driven model and the new
   [tween library](../../Assets/Diorama/Scripts/tween.lua), and a clip is just keyed
   transforms. It already covers a large slice of indie 2D (limb articulation, simple
   rigs). We add a small clip player (keyframed local transforms per bone entity,
   eased) and document the workflow.
2. **v2 - mesh deformation from DragonBones JSON.** A `DioramaSkeletalSpriteComponent`
   loads an **open DragonBones export** (bones as a 2D transform hierarchy, a mesh of
   vertices with per-vertex bone indices + weights, and animation tracks). Each frame:
   evaluate the pose (our own small 2D bone evaluator), CPU-skin the mesh
   (`pos = sum_i weight_i * boneMatrix_i * bindPos`), and emit the deformed vertices
   through the existing `SpriteFeatureProcessor` variable-geometry path. Open format,
   free tool (DragonBones Pro), 2D-native, no EMotionFX dependency.
3. **Documented alternative - EMotionFX as a CPU pose evaluator.** For teams that
   already have a 3D skeletal pipeline, document how to drive a Diorama 2D mesh from
   EMotionFX joint transforms (read the posed skeleton, skin our 2D mesh). This keeps
   EMotionFX **optional**, not a runtime requirement, and gives advanced users its
   anim-graph state machines/blend trees without forcing the dependency on everyone.

Why not make EMotionFX the primary (the investigation's first suggestion): it brings
a heavy dependency and 3D-first authoring into a gem whose whole point is
lightweight, 2D-native, open-asset 2D. The anim-graph evaluation it offers is real
value, but for v1/v2 a simple clip player plus our state needs (and the tween lib)
cover the 2D cases, and we can integrate EMotionFX as the optional advanced evaluator
without coupling the runtime to it.

## Data model (v2)

```
DioramaSkeleton  : bones[]  { name, parentIndex, bindLocalTransform2D }
DioramaSkinnedMesh : vertices[] { bindPos2d, uv, influences[<=4]{ boneIndex, weight } },
                     indices[]
DioramaSkeletalClip : tracks[]  { boneIndex, keyframes[]{ time, translate, rotate, scale }, easing }
```

Loaded from a DragonBones JSON via a builder into a compact product asset (no runtime
JSON parsing, matching the VISION efficiency criterion). Deformation runs on the CPU
in v1/v2 (vertex counts for 2D characters are small); a GPU-skinning variant in our
sprite shader is a later option if profiling demands it.

## Security and performance

- Bone count, vertex count, influences-per-vertex, and clip sizes are validated and
  bounded in the builder and at load; no asset value sizes a GPU buffer unchecked.
- Skinning reuses per-emitter scratch buffers; no per-frame heap allocation, matching
  the render-loop rule.
- Influences capped at 4 per vertex (the common rig limit).

## Phasing

1. **v1**: transform-hierarchy cutout + a small eased clip player + a how-to. No new
   dependency. Covers simple rigs immediately.
2. **v2**: DragonBones JSON import + 2D mesh deform through the sprite renderer.
3. **v3**: optional EMotionFX-evaluator integration (advanced), inverse kinematics,
   and a GPU-skinning path if needed.

## Verification plan (needs the monitor)

- v1: a 3-bone arm parented hierarchy plays a waving clip smoothly with easing.
- v2: a DragonBones-exported character deforms (a bending limb shows mesh deform, not
  just rigid part rotation); UVs stay correct; no per-frame allocation.
- Confirm the runtime client still has no EMotionFX dependency unless the optional
  path is explicitly enabled.

## Open questions

- Exact DragonBones JSON subset to support first (bones + slots + mesh + bone-driven
  animation; defer free-form mesh deformation/IK to v3).
- Whether the v1 clip player belongs in Lua (ships fastest, matches the sample) or as
  a C++ component (better editor authoring); likely Lua v1, C++ promotion later.
