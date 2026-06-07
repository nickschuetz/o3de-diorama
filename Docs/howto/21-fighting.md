# How-To: Fighting-Game Building Blocks (frame events, hitboxes, versus camera)

A sprite 2.5D fighter needs three things on top of the usual sprite + animation +
collision: **frame-exact triggers**, **per-frame hitboxes**, and a **two-character
camera**. Diorama provides all three.

## Frame-exact animation events

`DioramaSpriteNotificationBus` fires `OnAnimationFrame(frameIndex)` every time the
displayed frame advances, for both sprite-sheet and Aseprite playback, and
`OnAnimationFinished()` when a non-looping clip ends. That is the hook fighters use
to activate a hitbox, play a sound, open a cancel window, or start hit-stop on an
exact frame.

```lua
function Attack:OnActivate()
    self.handler = DioramaSpriteNotificationBus.Connect(self, self.entityId)
end
function Attack:OnAnimationFrame(frame)
    if frame == 5 then DioramaAudioRequestBus.Broadcast.PlayOneShot("diorama/audio/swing.wav", 1.0) end
end
function Attack:OnAnimationFinished()
    -- move recovery is over; return to neutral
end
```

For Aseprite, `frameIndex` is the document frame, so a move authored as a tag has
predictable frame numbers.

## Per-frame hitboxes

A fighter's hitbox is live only on specific frames. Combine frame events with the
2D Collider bus (`Diorama2DColliderRequestBus`): on a child entity that has a 2D
Collider on a "hit" layer, enable and position the box only inside the active-frame
window. The shipped example does exactly this:

```
Assets/Diorama/Examples/Fighting/frame_hitbox.lua
```

Point its `Hitbox` property at the collider child and set `ActiveFrom`/`ActiveTo`
to the move's active frames. Hurtboxes work the same way on a different layer, and
the collision layer/mask matrix keeps strikes, throws, and projectiles separate.

## Versus camera (two characters + zoom)

The 2D Camera Controller can frame two fighters and zoom with their distance:

```lua
-- Frame both characters and pull the view out as they separate.
DioramaCamera2DRequestBus.Event.SetTarget(cam, player1)
DioramaCamera2DRequestBus.Event.SetSecondaryTarget(cam, player2)
DioramaCamera2DRequestBus.Event.SetAutoZoom(cam, 6.0, 0.4, 6.0, 14.0)
-- base 6 + 0.4 per unit of separation, clamped to a 6..14 dolly.
```

- `SetSecondaryTarget` centers the camera on the **midpoint** of the two targets.
- `SetAutoZoom(base, perSeparation, minDolly, maxDolly)` pulls the camera back from
  the play plane as the pair separates (and back in as they close).
- `SetZoom(dolly)` sets a fixed pull-back instead (e.g. a dramatic close-up on a
  super, then back to auto-zoom).

Zoom is a dolly along the camera's out-of-plane axis, so it works for both
orthographic and perspective 2.5D setups and never touches the authored tilt.

## What Diorama gives you for free here

Hit-flash (`SetFlash`), sparks (particles), screen shake (`AddTrauma`), bloom and
CRT post, parallax stages, a LyShine HUD for health/meter, and MiniAudio for
SFX/voice. Combine those with the three pieces above and you have the core of a
2.5D sprite fighter.

## Hit-stop and slow motion

Freeze the animation on impact for that crunchy feel: set the playback time-scale
to 0, then back to 1 after a few frames. `SetPlaybackSpeed` does it for sprite-sheet
sprites and `SetSpeed` for Aseprite (both clamp `>= 0`); a value between 0 and 1 is
slow motion. The shipped `Assets/Diorama/Examples/Fighting/hitstop.lua` wraps this:
call `Freeze(seconds)` on a hit, and movement scripts check `IsFrozen()` to skip
their step so the whole character holds.

## Pushboxes (characters don't overlap)

Give each character a 2D Collider as its pushbox (a box on a dedicated "pushbox"
layer) and run `Assets/Diorama/Examples/Fighting/pushbox.lua`. Each tick it calls
`Diorama2DCollisionRequestBus::ComputeBoxPushOut(x, z, halfW, halfH, layerMask,
self)` for the net minimum-translation vector to separate from the other pushboxes
(excluding itself) and nudges the entity by it. When both characters run it, each
takes its half of the separation and they part naturally. The collision system
computes the vector; applying it stays a gameplay decision (so it never fights your
movement code).

## Known gaps (today)

- **Motion-input parsing/buffering** (quarter-circles, charge) and
  **rollback/determinism** are game-framework concerns, not the renderer's.
