# How-To: Side-Scroller Platforming (one-way platforms + ramps)

A side-scroller needs three things beyond walls: **ground-follow over flat ground
and ramps**, **one-way platforms** you jump up through and land on, and a body that
**resolves against walls**. Diorama's 2D collision world provides all three; the
body's movement stays a small game script (the renderer gives primitives, the game
decides how it moves). Everything is in the world **XY plane** (X right, Y up).

## Ground-follow (flat ground and ramps)

Register the walkable ground as a set of segments. `AddGroundSegment(x0, x1, y0, y1)`
is a span from `x0` to `x1` with surface height `y0` at `x0` and `y1` at `x1`: equal
for a flat ledge, differing for a ramp.

```lua
local c = Diorama2DCollisionRequestBus.Broadcast
c.AddGroundSegment(-20, 6, 0, 0)   -- flat floor
c.AddGroundSegment(6, 10, 0, 2)    -- ramp up to height 2
c.AddGroundSegment(10, 20, 2, 2)   -- raised shelf
```

Each frame the body probes the floor under its feet:

```lua
local probe = Diorama2DCollisionRequestBus.Broadcast.ProbeGroundY(x, footY, maxDrop, stepUp)
if probe.onGround and falling then y = probe.groundY + halfHeight; vy = 0 end
```

`stepUp` is how far above the feet a surface may be and still be stepped onto (so the
body walks up ramps and small lips); `maxDrop` is how far below it may be and still
snap (so the body falls off ledges instead of teleporting down). `ProbeGroundY`
returns the highest surface within that band, so overlapping ledges resolve to the
top one. An agent can author a whole level's terrain this way with no tilemap, and
the geometry is backed by the pure tested `SlopeCollision` core.

## One-way platforms

Mark any Diorama **2D Collider** as a one-way platform: check **One Way** in the
Inspector, or call `SetOneWay(true)`. The collision world then resolves it with an
**upward-only** push-out: a body landing from above is pushed onto the top, but a
body rising from below (a jump) or moving sideways passes straight through.

```lua
Diorama2DColliderRequestBus.Event.SetOneWay(platform, true)
```

The body resolves against all solid colliders, one-way included, with one call:

```lua
local push = Diorama2DCollisionRequestBus.Broadcast.ComputeBoxPushOut(x, y, halfW, halfH, layer, self.entityId)
x = x + push:GetX(); y = y + push:GetY()
```

The push-out is history-free (it gates on the body being above the platform center).
For pixel-exact "land only if last frame's feet cleared the top", combine it with
`SlopeCollision::ResolveOneWayLanding(prevFootY, currFootY, top)` in the body script.

## Putting it together

The shipped sample is two scripts under `Assets/Diorama/Examples/SideScroller/`:

- `platformer_ground.lua` registers the floor + a ramp + a shelf.
- `platformer_body.lua` is the body: gravity, `ProbeGroundY` ground-follow,
  `ComputeBoxPushOut` for walls and one-way platforms, and a jump. It reads `moveX` /
  `jump` from a 2D Input Actions component.

## Scope note

Ground segments and one-way are authored as data (the `AddGroundSegment` bus and the
collider **One Way** flag) and via regular 2D Colliders for walls. Driving **slope
tiles directly from a tilemap** (paint a ramp tile, get its collision) is a natural
follow-up: it needs the tilemap's per-tile collision, which currently projects to the
XZ ground plane, to also support the XY screen plane a side-scroller uses.
