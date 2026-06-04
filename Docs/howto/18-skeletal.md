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
