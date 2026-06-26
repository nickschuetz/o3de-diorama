# How-To: Skeletal Cutout Animation

Animate a 2D character built from sprite parts (a "cutout" rig): a torso, a head, two
arms, two legs, each its own entity, parented into a hierarchy. The **Skeletal Clip**
component plays a keyframed clip that moves each part's local transform over time, so
the whole rig walks, waves, or bounces. This is the classic paper-doll / cutout
technique (think of a marionette of sprites), driven entirely by the entity transform
hierarchy you already use for everything else in Diorama.

## Build the rig

1. Make a root entity for the character (this is where the Skeletal Clip component
   goes). Give it the sprite parts as **child entities**, named clearly:
   `torso`, `head`, `arm_left`, `arm_right`, `leg_left`, `leg_right`. Nest them the way
   they move together (parent `arm_left` under `torso`, etc.). Each part is an ordinary
   entity with a Diorama **Sprite** component.
2. Position each part at its rest pose using the normal Transform component. The clip
   animates *relative to nothing special*: it sets each bone's local transform outright,
   so author your keyframes as the local transforms you want at each moment.

## Add the clip

1. Add a **Skeletal Clip** component (`Diorama` category) to the root entity.
2. Set the clip's:
   - **Duration** -- length in seconds.
   - **Loop** -- wrap at the end, or hold the last frame.
   - **Speed** -- playback rate (negative plays in reverse).
   - **Auto play** -- start on activate.
3. Add a **Track** per part you want to animate. For each track:
   - **Bone (entity name)** -- the exact name of the child entity (e.g. `arm_left`).
     The component finds it by name anywhere in the hierarchy under the root.
   - **Keyframes** -- one per pose. Each keyframe has a **Time** (seconds), a
     **Translation**, a **Rotation** (Euler degrees), a **Scale** (uniform), and an
     **Ease** curve used to interpolate from that key to the next (Linear, Ease in,
     Ease out, Ease in-out, Smooth step).

Keys do not need to be entered in order; they are sorted by time when the clip plays.
Times before the first key hold the first pose; times after the last hold the last.

## Preview without game mode

The Skeletal Clip component previews directly in the editor viewport so you can pose
the rig while authoring:

1. Under **Editor preview**, tick **Preview pose**.
2. Drag **Preview time** (0..1) to scrub the clip. The bones move in the viewport.

The preview is non-destructive: each bone's original local transform is captured when
preview turns on and restored when you turn it off or deselect, so it never changes the
saved prefab. (Like all transform-only edits, the preview pose is not written to disk.)

## Drive it from script

`DioramaSkeletalRequestBus` is reflected `Common`, so a script, Script Canvas, or an
agent controls playback like any other Diorama feature:

```lua
DioramaSkeletalRequestBus.Event.Play(self.entityId)
DioramaSkeletalRequestBus.Event.SetSpeed(self.entityId, 2.0)   -- double time
DioramaSkeletalRequestBus.Event.SetLooping(self.entityId, true)

-- Pin a pose (e.g. an idle frame), or scrub from gameplay:
DioramaSkeletalRequestBus.Event.SetNormalizedTime(self.entityId, 0.5)
DioramaSkeletalRequestBus.Event.Stop(self.entityId)
```

`IsPlaying` reads back whether the clip is advancing, so a script can sequence one clip
after another without a screenshot.

## Cross-fade between clips

A character usually has more than one animation on the same rig (idle, walk, attack).
Author the alternatives under **Clips** in the component (each a name, duration, loop,
and a track per bone, on the *same* bones as the default clip), then blend to one with
`CrossFadeTo`:

```lua
-- Smoothly blend from the current clip to "walk" over 0.25s; the player samples both
-- clips and eases each bone's pose across the fade, then continues on "walk".
DioramaSkeletalRequestBus.Event.CrossFadeTo(self.entityId, "walk", 0.25)

-- A non-positive duration switches instantly (no blend), e.g. a hard cut to "hit":
DioramaSkeletalRequestBus.Event.CrossFadeTo(self.entityId, "hit", 0.0)
```

The blend is the pure `SkeletalClip::BlendPose` (lerp translation/scale, slerp rotation)
weighted by `CrossfadeWeight` over the duration, so the transition math is unit tested
headlessly. An unknown clip name is ignored. Clips share the rig: their tracks animate
the same bones, matched in order, so only the keyframes differ between clips.

## Blend by a parameter (1D blend tree)

A cross-fade is a one-shot transition; a **blend tree** holds a *continuous* mix driven
by a value. Author a **Blend tree (1D)** on the component: a list of entries, each a clip
(from the library, or empty for the default clip) at an **Anchor** value - for example
`idle` at 0, `walk` at 1, `run` at 2. At runtime set the blend parameter, and the player
blends the two clips that bracket it:

```lua
-- Drive locomotion by speed: 0 = pure idle, 1 = pure walk, 1.5 = walk/run mixed 50/50.
DioramaSkeletalRequestBus.Event.SetBlendParam(self.entityId, speed)
```

The two bracketing clips are **phase-synced** (a shared normalized phase sampled against
each clip's duration), so footfalls stay aligned as the mix shifts, and the blend weight
comes from the pure `SkeletalClip::ResolveBlend1D` (below the first anchor pins to the
first clip, above the last to the last, in between it lerps), unit tested headlessly. A
rig with no blend tree authored is unaffected - it stays on the single-clip / cross-fade
path. The blend tree and cross-fade are alternative modes: author one or the other on a
given rig.

Demo: `Docs/examples/blend_tree_demo.py` builds a runnable `DioramaBlendTree` level - a
one-bone rig with two single-pose clips (arm low at 0, arm high at 1); `blend_sweep.lua`
sweeps the parameter so in game mode the arm eases continuously between the two.

## Scope

This is the **v1 cutout player**: a transform hierarchy of sprite bones, keyframed local
transforms, eased interpolation. It deliberately reuses the entity hierarchy and Sprite
components rather than introducing a new asset type, so it composes with every other
Diorama 2D feature and stays fully open source (Apache-2.0 OR MIT, matching O3DE) with no
third-party runtime.

Mesh-deform skeletal animation (skinned bones that bend a single sprite, as authored in
tools that export the open DragonBones JSON format) is a planned follow-up. It will reuse
the same keyframe-sampling core (`SkeletalClip.h`) this player is built on; only the
deform step is new. We will parse the open format ourselves rather than bundle any
third-party runtime, keeping the licensing clean.
