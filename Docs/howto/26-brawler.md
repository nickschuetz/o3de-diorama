# How-To: 2.5D Brawler (depth lanes + depth-aware combat)

A beat-em-up (Final Fight, Streets of Rage) is 2.5D: characters are 2D sprites,
but they move on a **floor** with depth (toward and away from the camera), and a
punch only connects with an enemy on the **same depth lane**. Diorama gives you the
depth lane and composes with the fighting building blocks for the combat.

## The depth lane

Add the **2.5D Depth Body** component (`DioramaDepthBodyComponent`) to each
character. It owns the character's **depth** (how far into the scene it stands) and
renders it two ways so a flat orthographic scene reads as 2.5D:

- **Lift**: a character deeper in is drawn higher up-screen (`Lift Per Unit`), faking
  perspective.
- **Sort**: nearer characters draw on top of farther ones (`Sort Per Unit`).

If you use a tilted / perspective 2.5D camera, it already does the lift and ordering,
so set `Lift Per Unit` to 0 and use the body just for the depth value.

Drive the lane from script:

```lua
DioramaDepthBodyRequestBus.Event.SetDepth(id, newDepth)             -- snap to a lane
DioramaDepthBodyRequestBus.Event.MoveDepthToward(id, target, step)  -- ease toward one
local d = DioramaDepthBodyRequestBus.Event.GetDepth(id)
```

The character moves left/right with its transform X and toward/away by changing its
depth. `brawler_player.lua` and `brawler_enemy.lua` under
`Assets/Diorama/Examples/Brawler/` do exactly this (the enemy lines up on the
player's lane before attacking).

## Depth-aware combat (two ways)

A punch must require both horizontal reach **and** depth alignment, or you would hit
enemies in the background. Two ways to get that:

1. **Gate the hit by lane.** Use the **2D Frame-Data Hitboxes** component (how-to 21)
   for the swing; in its `OnHit(target)`, compare depths and only apply damage when
   they are close:

   ```lua
   function Player:OnHit(target)
       local gap = math.abs(GetDepth(self.entityId) - GetDepth(target))
       if gap <= laneTolerance then applyDamage(target) end
   end
   ```

   The pure tested `DepthLane::SameLane(a, b, tolerance)` is the C++ form of that gate.

2. **Put the hitboxes on the floor plane.** Set the 2D Collider / hitbox plane to
   **XZ** (the ground). Then the 2D overlap is in floor space, so a hit needs overlap
   in both X and depth automatically: depth-awareness falls out of the existing
   collision with no extra gate. Rendering stays in the sprite plane.

Use (1) when your hitboxes are authored in screen space for visual alignment; use (2)
when your whole gameplay lives on the floor plane.

## What Diorama gives you here

Depth body (lane + lift + sort), frame-data hitboxes and frame events for the swings,
the 2D camera (follow + shake), sprite flip for facing, particles and material flash
for impact, and the depth sort for free with a tilted camera. The reusable depth math
(`SameLane`, `ScreenLift`, `SortBias`, `MoveToward`, `ClampToArena`) is the pure
tested `DepthLane` core. Health, AI states, and combos stay game-side.
