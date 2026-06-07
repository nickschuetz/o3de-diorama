-- Pushbox: keep characters from standing inside each other. Put this on a
-- character that has a 2D Collider used as its pushbox (a box on a dedicated
-- "pushbox" layer). Each tick it asks the collision system for the net push-out
-- vector against the other pushboxes and nudges the entity by it. When both
-- characters run this, they part naturally (each takes its own half of the
-- separation), the standard fighting-game pushbox behavior.
--
-- The gem's collision system detects overlaps and computes the minimum-translation
-- vector (ComputeBoxPushOut); this script just applies it, so resolution stays a
-- gameplay decision rather than a hidden physics step.

local PushBox = {
    Properties = {
        HalfWidth = { default = 0.4, description = "Pushbox half-width (plane X)." },
        HalfHeight = { default = 0.5, description = "Pushbox half-height (plane Z)." },
        LayerMask = { default = 2, description = "Collision layer mask of other pushboxes to separate from." },
    },
}

function PushBox:OnActivate()
    self.tickHandler = TickBus.Connect(self)
end

function PushBox:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function PushBox:OnTick(deltaTime, scriptTime)
    local pos = TransformBus.Event.GetWorldTranslation(self.entityId)
    -- Collision plane is X/Z; query the pushbox at the character's position and
    -- exclude self so it never pushes out of its own collider.
    local push = Diorama2DCollisionRequestBus.Broadcast.ComputeBoxPushOut(
        pos.x, pos.z, self.Properties.HalfWidth, self.Properties.HalfHeight, self.Properties.LayerMask, self.entityId)
    if push.x ~= 0.0 or push.y ~= 0.0 then
        -- ComputeBoxPushOut returns an (X, Z) vector in the collision plane.
        TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(pos.x + push.x, pos.y, pos.z + push.y))
    end
end

return PushBox
