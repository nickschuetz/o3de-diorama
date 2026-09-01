# Reference: Skinned Sprite (mesh deform) component

The Skinned Sprite component renders a DragonBones mesh-deform character in world
space through Atom. Instead of a single flat quad (the [Sprite](./sprite-component.md)
component), it imports an armature exported in the open, Apache-2.0 DragonBones JSON
format and CPU-deforms its meshes each frame so a textured character *bends and
stretches* as its bones move. It handles three rig styles from the same component:

- **Weighted mesh** -- vertices skinned to multiple bones (see [how-to 31](../howto/31-mesh-deform.md)).
- **Surface** -- a mesh warped by a control-point grid, with nested surfaces and per-vertex
  FFD (see [how-to 32](../howto/32-surface-deform.md)).
- **Cutout parts** -- rigid parts posed by their bone transforms.

The deformed geometry draws through the sprite feature processor's mesh path, so it
composes with the gem's 2D lighting, depth sorting, and camera, and previews live in the
editor viewport.

## The shared configuration

Every parameter on this page lives on one struct, `DioramaSkinnedSpriteConfig`. The
editor component (`EditorDioramaSkinnedSpriteComponent`) authors that struct in the
Inspector, and at build time hands an identical copy to the runtime
`DioramaSkinnedSpriteComponent` through `BuildGameEntity`. There is one set of fields and
one set of semantics, whether you are in the editor or in a running game.

Unlike the Sprite component, whose appearance knobs (tint, flash, texture) are all on its
bus for gameplay effects, the Skinned Sprite's **appearance** fields are **author-time**:
they are set in the Inspector (or in the config handed to `BuildGameEntity`) and are not
re-settable per-field at runtime. The typed **`DioramaSkinnedSpriteRequestBus`** covers
what a rig needs *while playing* -- animation playback and per-bone pose overrides -- plus
a read-only `GetSkinnedSpriteInfo` snapshot, the AI/human parity Info-getter that lets a
script, Script Canvas, or agent inspect the loaded rig the same way it reads sprites and
cameras.

The rig is loaded at activation from the `Rig Asset` (a compiled `.dskinrigc` product,
decoded from a compact binary with the atlas UV remap already baked in) or, when no Rig
Asset is set, from `Source (ske.json)` (the armature JSON is parsed and the companion
`*_tex.json` atlas, same path with `_ske` swapped for `_tex`, remaps the mesh UVs). Either
way the meshes are then built. The `Texture` atlas image the UVs address is a separate
streaming-image asset reference.

## Parameters

Thirteen serialized parameters, grouped as the header and Inspector group them.

### Source / Rig

The rig geometry and clips come from one of two places. **Rig Asset** is the shipping path: a
compiled `.dskinrigc` product the AssetBuilder bakes from the DragonBones source, loaded through
the asset system with no JSON parsing at runtime. **Source (ske.json)** is the authoring / fallback
path: the raw `*_ske.json` read directly through the file IO aliases. When both are set, the Rig
Asset wins. See [The compiled rig product](#the-compiled-rig-product) below.

#### Rig Asset

| | |
| --- | --- |
| Inspector label | `Rig Asset` |
| Config field | `m_rigAsset` (`AZ::Data::Asset<DioramaSkinnedRigAsset>`) |
| Bus setter | none (author-time) |
| Default | none (unassigned) |
| Notes | Compiled DragonBones rig product (`.dskinrigc`) baked by the AssetBuilder from a `*_ske.json` (with its `*_tex.json` atlas UV remap already applied). Preferred over Source: the rig loads through the asset cache and decodes from a compact binary rather than parsing JSON at load, so the level neither ships nor re-parses the DragonBones text. Hot reloads: when the Asset Processor reprocesses the product, the rig rebuilds in place with playback preserved (see [The compiled rig product](#the-compiled-rig-product)). Leave unset to load the Source path directly. |

#### Source (ske.json)

| | |
| --- | --- |
| Inspector label | `Source (ske.json)` |
| Config field | `m_sourcePath` (`AZStd::string`) |
| Bus setter | none (author-time) |
| Default | empty (no rig) |
| Notes | Path to the DragonBones `*_ske.json` armature, resolved through the file IO aliases. Used only when no **Rig Asset** is set. For a shipped asset use the product path (`@products@/mygame/hero_ske.json`), which reads from the asset cache / pak; `@projectroot@/...` also works for a source file during authoring. The companion `*_tex.json` atlas is loaded from the same path with `_ske` swapped for `_tex`. |

#### Armature

| | |
| --- | --- |
| Inspector label | `Armature` |
| Config field | `m_armatureName` (`AZStd::string`) |
| Bus setter | none (author-time) |
| Default | empty (selects the first armature in the document) |
| Notes | Which armature to use when the document holds more than one. |

#### Texture

| | |
| --- | --- |
| Inspector label | `Texture` |
| Config field | `m_texture` (`AZ::Data::Asset<AZ::RPI::StreamingImageAsset>`) |
| Bus setter | none (author-time) |
| Default | none (unassigned) |
| Notes | The atlas texture the mesh UVs (0..1) address. The `*_tex.json` companion maps mesh UVs onto this image. |

### Appearance

#### Scale

| | |
| --- | --- |
| Inspector label | `Scale` |
| Config field | `m_scale` (`float`) |
| Bus setter | none (author-time) |
| Default | `0.01` |
| Range | `>= 0` |
| Notes | Armature-units-to-world-units. DragonBones rigs are authored in pixels (hundreds/thousands of units tall), so this is small. |

#### Flip vertical

| | |
| --- | --- |
| Inspector label | `Flip vertical` |
| Config field | `m_flipVertical` (`bool`) |
| Bus setter | none (author-time) |
| Default | `true` |
| Notes | DragonBones is y-down; flipping makes the character stand upright in O3DE's y-up world. |

#### Billboard

| | |
| --- | --- |
| Inspector label | `Billboard` |
| Config field | `m_billboard` (`bool`) |
| Bus setter | none (author-time) |
| Default | `true` |
| Notes | Face the camera like a sprite (`true`), or lie in the entity's local plane (`false`). |

#### Point filter

| | |
| --- | --- |
| Inspector label | `Point filter` |
| Config field | `m_pointFilter` (`bool`) |
| Bus setter | none (author-time) |
| Default | `true` |
| Notes | Nearest-neighbor texture sampling (crisp pixel art) vs bilinear. |

#### Sort offset

| | |
| --- | --- |
| Inspector label | `Sort offset` |
| Config field | `m_sortOffset` (`float`) |
| Bus setter | none (author-time) |
| Default | `0.0` |
| Notes | Draw-order bias; higher draws later (on top of lower-offset meshes/sprites). |

#### Tint

| | |
| --- | --- |
| Inspector label | `Tint` |
| Config field | `m_tint` (`AZ::Color`) |
| Bus setter | none (author-time) |
| Default | white (`1,1,1,1`) |
| Notes | Multiplied into every vertex color. |

### Animation

The rig plays authored DragonBones clips (per-bone translate / rotate / scale timelines
with cubic-bezier easing, surface-deform channels, and the animation-parameter composition:
type-40 progress, type-41 weight envelopes, and type-42 1D blends). These three fields seed
playback at activation; at runtime the bus verbs below take over.

#### Animation

| | |
| --- | --- |
| Inspector label | `Animation` |
| Config field | `m_animationName` (`AZStd::string`) |
| Bus equivalent | `PlayAnimation(name, looping)` at runtime |
| Default | empty (rig holds its bind pose) |
| Notes | The clip to play on activate. An empty name leaves the rig in its bind pose. |

#### Auto play

| | |
| --- | --- |
| Inspector label | `Auto play` |
| Config field | `m_autoPlay` (`bool`) |
| Bus equivalent | `PlayAnimation` / `StopAnimation` at runtime |
| Default | `true` |
| Notes | Start the `Animation` clip automatically on activate. With `false`, the rig waits in its bind pose until a `PlayAnimation` call. |

#### Speed

| | |
| --- | --- |
| Inspector label | `Speed` |
| Config field | `m_speed` (`float`) |
| Bus setter | `SetAnimationSpeed(speed)` |
| Default | `1.0` |
| Notes | Playback rate multiplier. Negative plays in reverse. |

#### Use simulation clock

| | |
| --- | --- |
| Inspector label | `Use simulation clock` |
| Config field | `m_useSimClock` (`bool`) |
| Bus setter | `SetUseSimClock(enabled)` (read back with `GetUseSimClock`) |
| Default | `false` |
| Notes | Advance on the 2D Simulation Clock's fixed steps (deterministic / rollback-exact) instead of the render tick, and capture the play state in the clock's snapshot. Falls back to the render tick with no clock in the level (editor preview included). See [how-to 30](../howto/30-deterministic-sim.md). |

## The compiled rig product

DragonBones ships a rig as two JSON files: `NAME_ske.json` (the armature, bones, meshes, and
clips) and `NAME_tex.json` (the atlas layout). Reading and parsing those at load, plus remapping
every mesh UV through the atlas, is work that never changes between runs. The **skinned-rig
AssetBuilder** does it once, offline:

1. It matches any `*_ske.json` in the project's asset scan folders.
2. It parses the armature, reads the companion `*_tex.json`, and bakes the atlas sub-texture UV
   remap into the mesh UVs.
3. It rejects a source with no drawable mesh (so a broken rig fails the build instead of shipping
   an empty product).
4. It writes a compact binary product, `NAME.dskinrigc` (a `DioramaSkinnedRigAsset`), whose
   payload is the fully-imported document.

At runtime, pointing the component's **Rig Asset** field at that product loads the rig through the
asset cache and decodes it with a fast, bounds-checked binary read -- no JSON parsing, no atlas
remap, and the DragonBones text never has to ship. That is the VISION efficiency criterion
"product assets load without runtime parsing". The decode treats the product as untrusted input:
every read is bounds-checked and every length capped, so a truncated or hostile `.dskinrigc` fails
the load cleanly (the rig simply does not build) rather than reading out of bounds.

The compiled rig **hot reloads**: the presenter watches its assigned product, so when the Asset
Processor reprocesses the `.dskinrigc` (you re-exported the `*_ske.json` or repacked the
`*_tex.json`), every live rig rebuilds in place -- in game mode and in the editor viewport preview
alike. Playback carries across the rebuild: the current clip, its elapsed time, and any per-bone
pose overrides are snapshotted with the rollback machinery and restored onto the new rig. If the
re-authored rig no longer has the playing clip (the clip list changed), the rig comes back stopped
in its bind pose rather than playing the wrong clip.

The `Source (ske.json)` path still works and is unchanged -- it is the authoring fallback and the
back-compat path for existing scenes (it is read once at activate / property edit and does not hot
reload; that is one more reason to prefer the product). When both a Rig Asset and a Source are set,
the Rig Asset wins.

**Using it:** the committed example rigs under `Assets/Diorama/Examples/Skinned/` are processed
automatically, so `water.dskinrigc`, `puppet.dskinrigc`, and `seaweed.dskinrigc` appear in the
asset browser. Drop one into **Rig Asset** (or assign it from the reflected property in a build
script) and leave **Source** empty. For your own rig, drop its `*_ske.json` + `*_tex.json` into a
scanned folder; the `.dskinrigc` appears once the AssetProcessor finishes.

## Runtime API

`DioramaSkinnedSpriteRequestBus` is reflected `Common`, so a script, Script Canvas, or an
agent drives the rig by entity id, the same way it drives every other Diorama feature.

| Verb | Parameters | Returns | Notes |
| --- | --- | --- | --- |
| `PlayAnimation` | `name: string, looping: bool` | void | Play a clip by name from the start; `looping` overrides the clip's own loop flag. An unknown name stops playback (rig holds its bind pose). |
| `StopAnimation` | -- | void | Stop playback, holding the bind pose (plus any bone overrides). |
| `SetAnimationSpeed` | `speed: float` | void | Playback rate multiplier; negative plays in reverse. |
| `SetBoneRotation` | `boneName: string, degrees: float` | void | Add an extra rotation at the named bone, on top of the animated (or bind) pose, rotating that bone and its descendants about the bone's own origin. The primitive that bends a limb; unknown bone names are ignored. |
| `SetBoneTranslation` | `boneName: string, x: float, y: float` | void | Add an extra translation (armature units) at the named bone, on top of its bind pose. Unknown bone names are ignored. |
| `ResetPose` | -- | void | Clear every pose override, returning the rig to its bind (or animated) pose. |
| `GetSkinnedSpriteInfo` | -- | `SkinnedSpriteInfo` | Read-only snapshot of the loaded rig and draw state. |
| `SetUseSimClock` | `enabled: bool` | void | Advance on the 2D Simulation Clock (deterministic / rollback-exact) vs the render tick; see the [Use simulation clock](#use-simulation-clock) parameter. |
| `GetUseSimClock` | -- | `bool` | Whether the rig advances on the simulation clock. |

Pose overrides (`SetBoneRotation` / `SetBoneTranslation`) layer *on top of* the playing
clip each frame, so gameplay can nudge a limb while an animation runs; `ResetPose` clears
them.

### SkinnedSpriteInfo

The read-only snapshot returned by `GetSkinnedSpriteInfo`, so a script can confirm the rig
loaded and is drawing without a screenshot.

| Field | Type | Meaning |
| --- | --- | --- |
| `loaded` | `bool` | The DragonBones armature parsed and built. |
| `visible` | `bool` | Registered with a feature processor and drawing (requires the texture to be ready; an untextured rig reports `false` but still renders its white silhouette). |
| `boneCount` | `int` | Bones in the armature. |
| `meshCount` | `int` | Meshes built (skinned + surface). |
| `vertexCount` | `int` | Total deformed vertices across all meshes. |

## See also

- [How-to 31: Mesh-deform characters](../howto/31-mesh-deform.md) -- weighted-mesh rigs.
- [How-to 32: Surface + FFD deform](../howto/32-surface-deform.md) -- surface rigs, nested
  surfaces, FFD, and the animation-parameter layer.
- [API reference](./api.md) -- the request bus alongside every other Diorama bus.
- [Sprite component](./sprite-component.md) / [Tilemap component](./tilemap-component.md)
  -- the other world-space renderables.
