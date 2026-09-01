# Mesh-deform characters (DragonBones)

Smooth 2D skeletal animation where a textured mesh **bends and stretches** as its bones
move, instead of pivoting as rigid cutout parts. Diorama imports the open, Apache-2.0
**DragonBones** weighted-mesh format and CPU-skins it through the sprite renderer, so a
mesh-deform character composes with the gem's 2D lighting, sorting, and camera like any
other sprite. This is world-space content (a character in the scene), not screen-space UI.

Demo: `Docs/examples/seaweed_demo.py` builds a runnable `DioramaSeaweedDemo` level with a
swaying seaweed frond -- a strip mesh weighted to a chain of bones running an authored
`sway` clip. The rig is generated (IP-free) by `scripts/gen_seaweed_rig.py`.

## The component

Add **Skinned Sprite (mesh deform)** (category *Diorama*) to an entity. It loads a
DragonBones armature, poses its bones each frame, CPU-skins every mesh, and draws the
deformed result. Config:

| Field | What it does |
| --- | --- |
| **Source (ske.json)** | The DragonBones `*_ske.json` armature, via a file IO alias, e.g. `@products@/mygame/hero_ske.json`. The companion `*_tex.json` atlas is loaded automatically from the same path (`_ske` -> `_tex`). |
| **Armature** | Which armature to use from the document; empty selects the first. |
| **Texture** | The atlas PNG the mesh UVs address (a normal streaming-image asset). |
| **Scale** | Armature units to world units. DragonBones rigs are authored in pixels, so this is small (~0.008). |
| **Flip vertical** | DragonBones is y-down; on by default so the character stands upright. |
| **Billboard** | Face the camera like a sprite, or lie in the entity's plane. |
| **Point filter** | Nearest-neighbor sampling (crisp pixel art) vs bilinear. |
| **Sort offset** | Draw-order bias; parts are already layered back-to-front by their slot order. |
| **Tint** | Multiplied into every vertex. |
| **Animation** | Clip to play on activate (by name); empty leaves the bind pose. |
| **Auto play** | Start that clip automatically. |
| **Speed** | Playback rate (negative plays in reverse). |

## Getting a rig into the project

1. In **DragonBones Pro** (free), rig and animate your character, then **Export** the
   *DragonBones* data. You get three files: `name_ske.json` (armature + animations),
   `name_tex.json` (atlas layout), and `name_tex.png` (the atlas image).
2. Drop all three into a scanned asset folder (e.g. `YourProject/Assets/characters/`). The
   Asset Processor turns the PNG into a streaming-image asset and, from the `*_ske.json`, bakes
   a compiled rig product `name.dskinrigc` (the atlas UV remap is baked in at this step).
3. On the component, set **Texture** to the atlas image and **Scale** so the character is a
   sensible world size, then choose one of:
   - **Rig Asset** (recommended, shipping): point it at `name.dskinrigc`. The rig loads from the
     compact binary with no runtime JSON parsing. Leave **Source** empty.
   - **Source (ske.json)** (authoring / back-compat): point it at the `*_ske.json` product path
     (`@products@/...`). The rig parses the JSON at load. Used only when no Rig Asset is set.

   See [The compiled rig product](../reference/skinned-sprite-component.md#the-compiled-rig-product)
   for the trade-off.

The **weighted-mesh** family (bones + a mesh skinned to them) and **surface / FFD** rigs (a mesh
warped by a control-point grid) are both imported; see [how-to 32](32-surface-deform.md) for the
surface path and the animation-parameter layer.

## Playing animations

The clip names come from the DragonBones file. Play them by:

- **The Inspector** -- set **Animation** to a clip name with **Auto play** on.
- **Script / Script Canvas / agent** over `DioramaSkinnedSpriteRequestBus`:

```lua
DioramaSkinnedSpriteRequestBus.Event.PlayAnimation(self.entityId, "run", true)  -- name, looping
DioramaSkinnedSpriteRequestBus.Event.SetAnimationSpeed(self.entityId, 0.5)
DioramaSkinnedSpriteRequestBus.Event.StopAnimation(self.entityId)
```

You can also pose individual bones on top of (or instead of) a clip -- useful for aim,
recoil, or procedural secondary motion:

```lua
DioramaSkinnedSpriteRequestBus.Event.SetBoneRotation(self.entityId, "upperarm_r", 40.0)  -- degrees
DioramaSkinnedSpriteRequestBus.Event.SetBoneTranslation(self.entityId, "head", 0.0, -3.0)
DioramaSkinnedSpriteRequestBus.Event.ResetPose(self.entityId)
```

`GetSkinnedSpriteInfo` returns whether the rig loaded and is drawing, plus its bone, mesh,
and vertex counts.

For a fighting game or any deterministic scene, tick **Use Simulation Clock** (or call
`SetUseSimClock(true)`) so the character advances on the
[2D Simulation Clock](30-deterministic-sim.md)'s fixed steps and its play state rewinds
exactly; with no clock in the level it falls back to the render tick (editor preview
unchanged).

## Notes

- **Editor preview**: the character animates live in the viewport, but the O3DE editor only
  redraws on interaction, so the motion looks frozen until you move the mouse over the
  viewport. Enter game mode (Ctrl+G) for continuous playback.
- **Layering**: the character's parts draw back-to-front in the armature's slot order, so a
  near arm covers the torso and a far arm sits behind it, automatically.
- Every runtime knob is on `DioramaSkinnedSpriteRequestBus` (AI/human parity) and in the
  Inspector, and the math is Windows-friendly (`std::` only).
