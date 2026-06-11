# How-To: Fighting-Game Building Blocks (frame events, hitboxes, motions, versus camera)

A sprite 2.5D fighter needs a few things on top of the usual sprite + animation +
collision: **frame-exact triggers**, **frame-data hitboxes/hurtboxes**, **motion
inputs** (quarter-circles, dragon-punches), **impact juice**, and a **two-character
camera**. Diorama provides all of them.

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

## Frame-data hitboxes and hurtboxes (first-class component)

The **2D Frame-Data Hitboxes** component (`DioramaHitboxComponent`) authors a whole
rig of boxes in one place. Each box is a **hitbox** (attacking) or **hurtbox**
(vulnerable), has an in-plane offset and half extents, and is live only on an
inclusive animation-frame window (`startup -> active -> recovery`). Each frame the
component reads the displayed animation frame (from `OnAnimationFrame`, or a manual
`SetFrame`), registers the live hurtboxes as collision geometry, and tests the live
hitboxes against the target layer. When a hitbox overlaps an opponent's hurtbox it
fires `OnHit` on the attacker and `OnHurt` on the target over
`DioramaHitboxNotificationBus`, once per opponent per active window (one swing lands
once, not once per frame).

Add the component, set the **Hurt Layer** the rig's hurtboxes occupy and the
**Target Mask** its hitboxes strike (both default to the same bit, so two stock
fighters hit each other), then author the boxes:

| Box | Kind | Offset | Half extents | Frames |
|-----|------|--------|--------------|--------|
| body | Hurtbox | (0, 0.9) | (0.4, 0.9) | 0..99 |
| jab  | Hitbox  | (0.7, 1.0) | (0.4, 0.2) | 5..8 |

`Facing` (or `SetFacing(+1/-1)` at runtime) mirrors every box's X offset, so the rig
follows the character when it turns to face its opponent. An agent confirms setup
without a viewport via `GetHitboxInfo` (current frame, facing, active box counts).

```lua
-- Receiving end: apply damage and juice when struck. (See hit_response.lua.)
self.handler = DioramaHitboxNotificationBus.Connect(self, self.entityId)
function Fighter:OnHurt(attacker)
    self.health = self.health - 8
    DioramaSpriteRequestBus.Event.SetFlash(self.entityId, 1, 1, 1, 1) -- white impact flash
end
```

The full receiving script is `Assets/Diorama/Examples/Fighting/hit_response.lua`.
(The older `frame_hitbox.lua` toggles a single collider child by frame; it still
works for one-off cases, but the component is the blessed path for a real rig.)

## Motion inputs (quarter-circle, dragon-punch, charge sequences)

The **2D Input Actions** component recognizes directional motions. Point its
**Direction action** at an `Axis2D` action (the stick / WASD, up = +Y, forward =
+X), then author motions in numpad notation: `236` is a quarter-circle-forward,
`623` a dragon-punch, `41236` a half-circle-forward. Each motion has a **window**
(how recently every step must have been entered). The component quantizes the stick
each frame, records the direction history, and recognizes the motion order-sensitively
(intermediate diagonals may be skipped, so input is forgiving).

Read it the buffer-friendly way: `WasMotionPerformed("qcf")` stays true through the
window, so gate it on a button edge for the classic "do the motion, then press" feel.
`OnMotionPerformed` fires once the instant a motion completes for event-driven logic.

```lua
function Fighter:OnActionPressed(action)
    if action == "punch" then
        if DioramaInputRequestBus.Event.WasMotionPerformed(self.entityId, "qcf") then
            self:Fireball()   -- the motion upgraded the punch
        else
            self:Jab()
        end
    end
end
```

The full script is `Assets/Diorama/Examples/Fighting/motion_special.lua`. Charge
moves (hold-back-then-forward with a minimum hold time) need a hold-duration check on
top of the sequence; that stays a small game-side addition.

## Impact effects (hit spark + screen shake)

`hit_response.lua` also shows the impact "juice", built entirely on shipped systems:
a **hit-spark preset** (a Particle Emitter tuned to a short radial white-to-orange
burst, `Burst()` on a hit), a **white flash** on the struck sprite (`SetFlash`), a
**screen-shake pulse** on the versus camera (`AddTrauma`), and an **on-hit post pulse**
that spikes bloom (`DioramaLookRequestBus.SetBloomIntensity`) and decays it back over a
few frames. Combine with hit-stop (below) for a crunchy confirm.

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

- **Charge motions** (hold a direction for N frames, then release) need a
  hold-duration check on top of the sequence detector; the building block is here,
  the timing rule is a few lines of game code.
- **Rollback/determinism** is a game-framework concern, not the renderer's.
