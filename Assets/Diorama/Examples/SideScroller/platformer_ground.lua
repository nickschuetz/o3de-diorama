-- Register the walkable ground (flat ledges + ramps) for a side-scroller, so the
-- platformer body's ProbeGroundY follows it. Put this on any entity in the level
-- (it authors global ground segments, not per-entity). Walls and one-way platforms
-- are separate 2D Collider entities; this is only the ground the body walks on.
--
-- Segments are in the world XY plane: AddGroundSegment(x0, x1, y0, y1) is a span from
-- x0 to x1 with surface height y0 at x0 and y1 at x1 (equal for flat, differing for a
-- ramp). An agent can author a level's terrain this way without any tilemap.

local PlatformerGround = {}

function PlatformerGround:OnActivate()
    local c = Diorama2DCollisionRequestBus.Broadcast
    c.ClearScriptGroundSegments()
    c.AddGroundSegment(-20.0, 6.0, 0.0, 0.0) -- flat floor
    c.AddGroundSegment(6.0, 10.0, 0.0, 2.0) -- ramp up to height 2
    c.AddGroundSegment(10.0, 20.0, 2.0, 2.0) -- raised flat shelf
end

function PlatformerGround:OnDeactivate()
    Diorama2DCollisionRequestBus.Broadcast.ClearScriptGroundSegments()
end

return PlatformerGround
