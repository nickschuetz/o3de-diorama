# How-To: Bullet Patterns (danmaku / shmup emitter)

A bullet-hell or shoot-em-up needs to spray bullets in geometric patterns, move
hundreds of them cheaply, and know when one hits the player. The **2D Bullet
Emitter** component (`DioramaBulletEmitterComponent`) does all three.

## Build it

A one-command scene builder assembles this guide's demo in its own level
(a boss emitter cycling ring/fan/spiral over a dummy target); finish any wiring the editor cannot script (noted in its output), then
enter game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/danmaku_demo.py
```

## What it does

Add the component to an enemy (or a turret, a boss part, the player ship). Each
shot fires a **pattern** of bullets; the emitter pools and integrates them like
particles, renders the whole pattern in one draw call through the sprite batch,
and tests each live bullet against a collision layer. A bullet that overlaps a
collider on that layer fires `OnBulletHit` and is consumed.

Patterns (the pure tested `BulletPattern` core):

- **Ring** - `Count` bullets evenly spaced around a full circle, starting at `Aim`.
- **Fan** - `Count` bullets across a `Spread`-degree arc centered on `Aim`.
- **Spiral** - a ring (or single stream when `Count` is 1) fired each shot, rotated
  by `Spin Per Shot` degrees; the classic rotating-stream look.

Bullets live and collide in the world **XY plane** (the screen plane).

## Authoring

| Property | Meaning |
|----------|---------|
| Texture | The bullet sprite (a soft dot / orb). |
| Pattern | Ring / Fan / Spiral. |
| Count | Bullets per shot. |
| Speed | World units per second. |
| Fire Rate | Shots per second for continuous fire (0 = manual `Fire()` only). |
| Aim | Base angle in degrees (0 = +X, 90 = +Y). |
| Spread | Fan arc width (ignored by ring). |
| Spin Per Shot | Spiral rotation per shot (ignored by ring/fan). |
| Bullet Lifetime | Seconds before a bullet expires. |
| Bullet Radius | Collision + render half size. |
| Target Mask | Collision layer mask the bullets hit (0 = visual only). |
| Max Bullets | Hard pool cap; the pool never grows past it. |

Set **Target Mask** to the layer the player's collider is on; give the player a
Diorama **2D Collider** on that layer. That is the whole hit setup. The collider must
be on the **XY plane** (the 2D Collider default) so it shares the bullets' plane; a
collider set to XZ/YZ lives in a different 2D space and the bullets will pass through
it with no hit.

## Driving it from script

```lua
-- Start / stop continuous fire, fire one shot, retarget at run time.
DioramaBulletRequestBus.Event.Play(emitter)
DioramaBulletRequestBus.Event.SetPattern(emitter, 2)   -- 0 ring, 1 fan, 2 spiral
DioramaBulletRequestBus.Event.SetAim(emitter, angleToPlayer)
DioramaBulletRequestBus.Event.Fire(emitter)            -- one manual shot

-- React to hits (addressed by the emitter entity id).
self.handler = DioramaBulletNotificationBus.Connect(self, emitter)
function Enemy:OnBulletHit(target)
    -- apply damage to target; the bullet is already consumed
end
```

The shipped sample `Assets/Diorama/Examples/Shmup/enemy_danmaku.lua` cycles the
patterns and turns hits into damage. The player only needs a 2D Collider on the
emitter's target layer.

## Why a separate component (not the particle emitter)

The 2D Particle Emitter sprays *cosmetic* particles with random cones. The bullet
emitter shares the pooling + sprite-batch rendering but adds the two things bullets
need: deterministic **pattern geometry** and **collision** (each bullet is a hazard
that reports hits). Damage and scoring stay game-side; the emitter only reports the
contact and removes the bullet, the same division as the rest of Diorama.

## Performance

The pool is fixed-capacity (`Max Bullets`, hard-capped), so no script or asset value
can spawn unboundedly. A whole pattern of one texture is a single draw call. The
per-frame hit test is one overlap query per live bullet against the target layer;
keep the target layer to a few colliders (the player, maybe a shield) and it stays
cheap even with hundreds of bullets.
