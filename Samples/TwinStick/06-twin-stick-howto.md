# How-To: Twin-Stick Shooter (capstone)

Teaching ladder rung 6. A small but complete 2.5D twin-stick shooter that ties
the whole ladder together: a tilemap arena, a sprite player with twin-stick
controls, chasing enemies spawned in waves, projectiles, and a HUD. It shows the
division of labor in an O3DE game -- **Diorama draws the world, LyShine draws the
UI, and standard O3DE (PhysX, Lua, input bindings, spawnables) runs the
gameplay.**

The game lives under [`Samples/TwinStick`](../../Samples/TwinStick); this guide
explains how the pieces fit. It is a real game built the way a developer (or an
agent) would build one, not a contrived demo.

## What Diorama provides, and what it does not

Diorama is a renderer. It draws the player, enemies, projectiles, and the tilemap
arena as world-space sprites, and exposes typed buses (`DioramaSpriteRequestBus`,
`DioramaTilemapRequestBus`) to configure them. Everything else is ordinary O3DE:

| Concern | Handled by |
| ------- | ---------- |
| Sprites, tilemap arena | **Diorama** |
| Movement + hit detection | Transform-driven Lua (`TransformBus`, distance checks) -- deliberately **not** PhysX (see note) |
| Spawning waves | The spawnable system (`SpawnableScriptMediator`) |
| Input | Input bindings + `InputEventNotificationBus` |
| Game logic | Lua scripts |
| HUD | **LyShine** |

> **Why no PhysX for gameplay?** Two engine facts make transform-driven movement
> the right call here: (1) a dynamic PhysX rigid body does not honor an entity's
> authored editor transform at simulation start (it initializes at the world
> origin), and (2) PhysX collision callbacks are reflected only in the Automation
> script scope, so a launcher Lua script never receives `OnCollisionBegin`. So the
> sample moves entities through `TransformBus`, confines them with a clamp to the
> arena, and detects hits by distance through a shared live-entity table. It is
> fully deterministic and is the same API an agent would script.

A nice consequence: gameplay Lua drives the same Diorama buses an agent would.
The player and enemies set their facing with `DioramaSpriteRequestBus.SetFlip`
from ordinary `OnTick` code.

## The pieces

The theme: you play **Obi**, a jolly octopus who fires **hearts**. The "enemies"
are ocean predators ("haters"); three hearts fill a hater with love and it floats
away happy. Kill them with kindness.

| Piece | Script / asset | Role |
| ----- | -------------- | ---- |
| Arena | Tilemap component | The ocean floor, one big tinted tile on a lower sort layer |
| Player (Obi) | `twin_stick_player.lua` | WASD/stick move + aim (transform-driven), fires hearts; flushes colour as a hater nears |
| Hater | `twin_stick_enemy.lua` | Chases Obi; swells + pinkens per heart; at 3 it floats away (befriended) |
| Spawner | `twin_stick_spawner.lua` | Spawns a random hater (9 species) on a ramping timer from the arena edges |
| Heart | `twin_stick_projectile.lua` | Flies along the aim, registers a hit on the nearest hater by distance |
| FX manager | `twin_stick_fx.lua` | One long-lived entity animating a pool of script-less heart-burst shards |
| Game / HUD | `twin_stick_game.lua` | Befriended count (LyShine HUD), pause (P), quit (Esc) |

## How a befriending flows through the game

Every live hater registers itself in a shared Lua table (all game scripts share
one VM), so pieces communicate through shared globals rather than physics events:

1. Obi holds fire; `twin_stick_player.lua` spawns a heart toward the aim via
   `SpawnableScriptMediator`, publishing the muzzle position + direction so the
   heart launches from the right place (a freshly spawned entity does not have its
   spawn transform on its first tick).
2. The heart flies along its direction and each tick tests its distance to every
   live hater. On an overlap it increments that hater's hit tally
   (`_G.TwinStickHits`) and consumes itself.
3. The hater reads its own tally: each heart makes it swell and pinken; at 3 it
   becomes content, releases a burst of hearts, bumps `_G.TwinStickBefriended`,
   and floats up to the surface, fading out.
4. `twin_stick_game.lua` polls `_G.TwinStickBefriended` each tick and updates the
   LyShine HUD ("Befriended: N").

Each step is decoupled through shared globals, so pieces can be developed and
tested independently with no physics dependency.

## The 2.5D techniques it showcases

- **Tilted 2.5D camera** over a receding ocean floor, with billboarded creatures
  standing up on it.
- **Automatic depth sorting** (`r_dioramaSpriteDepthSort`) so nearer creatures
  draw in front; the floor sits on a lower sort layer to stay behind.
- **Soft ground shadows** (`r_dioramaSpriteShadows`) grounding each creature.
- **LyShine HUD** composited over the Diorama world layer (engine UI + custom
  world-space 2.5D rendering in one frame).
- **Pause / quit** controls and a colour-shifting player (octopus chromatophores).

## Building it

Run the capstone assembler in the editor:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Samples/TwinStick/build_game.py
```

It creates the arena, player, camera, game controller, and spawner, and
configures every Diorama part through the typed buses. Then finish the standard
O3DE wiring listed in [`Samples/TwinStick/README.md`](../../Samples/TwinStick/README.md):
the Lua script and input assets, the enemy and projectile **spawnable prefabs**,
the spawner's `EnemyPrefab` and player's `ProjectilePrefab`, and the LyShine HUD
canvas (a `hud.uicanvas` with a text element named `ScoreText`).

## Why it is built this way

Runtime spawning, collision, input, and UI all use the documented O3DE
interfaces, so the sample is a faithful starting point you can grow: add enemy
types, pickups, multiple weapons, or a wave UI. Diorama keeps the visual layer
small and forgiving; the rest is the O3DE you already know.
