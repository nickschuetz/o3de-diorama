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
    if frame == 5 then AudioBus.Broadcast.PlayOneShot("swing") end
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

## Known gaps (today)

- **Diagonal/rotated tiles and 90-degree sprite rotation** are not supported, so a
  Tiled diagonal-flip flag is dropped; H/V mirroring works.
- **Hit-stop** (freeze frames) is game-side: toggle `SetAnimationEnabled` or scale
  `SetPlayback` fps for the freeze.
- **Pushboxes** (characters shoving each other apart) are game-side via
  `OverlapBox`; the collision system detects, it does not resolve.
