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
Emitter + 2D Collider + the `player_ship.lua` behaviour), a descending enemy wave, a star
field, and a camera. The builder wires the input map, the gun, the camera rotation, and
the Lua scripts for you (in `patch_prefab`). One manual step remains, since a camera
cannot be made active from a build script: select **ShmupCamera -> Be this camera**, then
**Ctrl+G**.

Fly with **WASD / arrows / left stick**; the ship autofires straight up; line up under an
enemy to destroy it, and the Console logs the kill. The small fighters take 5 hits; the
two bigger **Odie** (the O3DE mascot) take ~7. Ram an enemy and you lose a life; at zero
lives the wreck blinks red and **Space** restarts.

## How it is wired

- **Gun = the danmaku emitter, reused.** The player's weapon is a 2D Bullet Emitter
  authored as a single bolt (`Count` 1, `Spread` 0, `Aim` 90 = up) with **Fire On
  Activate** and a **Muzzle Offset** to the nose. The emitter pools, moves, and hit-tests
  the bolts.
- **Combat lives on the player.** The emitter reports hits to *its own* entity, so
  `player_ship.lua` handles `OnBulletHit(target)`: it flashes the struck enemy
  (`DioramaSpriteRequestBus.SetFlash`), nudges it back for impact, counts hits, and after
  enough **recycles** it to the top of the field at a new X (endless waves, no spawnables).
- **Bigger enemies are tougher, from one rule.** On a bolt's first hit the player reads
  the enemy's sprite width (`GetSpriteInfo().width`) and scales the hit count by it
  relative to a normal enemy. An Odie is just a larger sprite, so it soaks ~7 instead of 5
  and (because `enemy_wave.lua` sizes each collider to its sprite) presents a bigger
  target. No per-enemy property, no special-case.
- **The wave** is a fixed pool of enemies running `enemy_wave.lua`: each descends and, when
  it falls past the bottom, wraps back to the top at a new X. Game over freezes the wave
  through a shared `ShmupRunning` flag the player script flips.
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

The slice now runs a full loop: move, autofire, hit, recycle, a descending **wave** (with
two tougher Odie), **knockback** on hit, **lives** with a ram cost, a game-over freeze, and
a **Space** restart. The gun fires from the ship's **nose** via the emitter's **Muzzle
Offset** (a general spawn offset from the entity origin, added to the 2D Bullet Emitter for
this dogfood), which also avoids point-blank ram-kills.

What a fuller game would still add: a world-space score readout (a label pinned to a world
entity, not screen-space HUD; that stays LyShine's job), enemies that fire back, and a
difficulty ramp.
