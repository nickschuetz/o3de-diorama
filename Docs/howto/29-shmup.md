# How-To: A Small Shmup (dogfood vertical slice)

This is a small vertical shoot-em-up built by composing the gem's shipped features into
one playable game: a sprite ship you fly with the input map, the bullet emitter reused as
the ship's gun, 2D collision for the hits, a parallax starfield, and a camera over the XY
play plane. It is the gem's own dogfood: assembling the pieces into a real game is how the
integration bugs that unit tests miss get found.

## Build it

Run the scene builder against your host project:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/DioramaSandbox \
  --runpython /path/to/o3de-diorama/Docs/examples/shmup_demo.py
```

It creates the **DioramaShmup** level: a Player (Sprite + 2D Input Actions + 2D Bullet
Emitter + 2D Collider + the `player_ship.lua` behaviour), a stand-in Enemy, a star field,
and a camera. Two manual clicks remain (the editor cannot set a script asset or activate a
camera from a build script): assign `diorama/examples/shmup/player_ship.lua` to the
Player's Lua Script, select **ShmupCamera -> Be this camera**, then **Ctrl+G**.

Fly with **WASD / arrows / left stick**; the ship autofires straight up; line up under an
enemy to destroy it (5 hits), and the Console logs the kill.

## How it is wired

- **Gun = the danmaku emitter, reused.** The player's weapon is a 2D Bullet Emitter
  authored as a single bolt (`Pattern` Fan, `Count` 1, `Spread` 0, `Aim` 90 = up) with
  **Fire On Activate**. The emitter pools, moves, and hit-tests the bolts.
- **Combat lives on the player.** The emitter reports hits to *its own* entity, so
  `player_ship.lua` handles `OnBulletHit(target)`: it flashes the struck enemy
  (`DioramaSpriteRequestBus.SetFlash`), counts hits, and after a few destroys it
  (`GameEntityContextRequestBus.DestroyGameEntity`).
- **Movement** reads the `move` Axis2D action and clamps the ship to the play field.

## Gotchas this slice taught (read before scripting gameplay)

These are O3DE/Lua behaviors, not Diorama bugs, but they bite every game script:

- **Read a `Vector3`/`Vector2` by property, not method.** `pos.x` works; `pos:GetX()` is
  **nil** in this engine's Lua. Write transforms by constructing a new value:
  `TransformBus.Event.SetWorldTranslation(id, Vector3(nx, ny, pos.z))`.
- **Author a sibling component's config on the component, not from `OnActivate`.** A
  script's `OnActivate` may run before a sibling's request-bus handler connects, so
  `SetPattern(...)` etc. on the emitter from there are silently lost. Tune the gun in the
  Inspector (or the prefab); drive only behavior (movement, combat) from the script.
- **Read every `self.Properties.<X>` with an `or` fallback.** O3DE leaves a property nil
  if the script's declared properties changed after the component was authored (the prefab
  keeps stale property metadata). `self.speed = self.Properties.MoveSpeed or 12.0`.
- **`SetFlash` does not decay.** It sets a flash amount and holds it; fade it back toward 0
  yourself each tick for a hit *pulse* (see `flash_pulse.lua` and `player_ship.lua`).

## Where it stops (and what is next)

This slice is the core loop (move, autofire, hit, kill). The gun fires from the ship's
**nose** via the emitter's **Muzzle Offset** (a general spawn offset from the entity
origin, added to the 2D Bullet Emitter for this dogfood), which also avoids point-blank
ram-kills. A full game still adds enemy **waves** (several descending, recycling), a
world-space score readout, and player death.
