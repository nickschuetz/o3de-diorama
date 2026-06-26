----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Ping-pong horizontal mover for the afterimage-trail demo. Attach to a sprite that
-- also carries trail settings (Trail Ghosts > 0, set in the Inspector or via
-- DioramaSpriteRequestBus.SetTrail). Each tick it slides the entity left/right at a
-- brisk, even speed so the afterimage ghosts spread out behind it the way a dash or
-- super activation looks. Pure TransformBus translation, nothing trail-specific: the
-- trail is captured by the Sprite component itself from the entity's motion.

local TrailMover = {
    Properties = {
        Speed = { default = 6.0, description = "Horizontal speed in world units/sec" },
        Range = { default = 6.0, description = "How far left/right of the start position to travel, in world units" },
    },
}

function TrailMover:OnActivate()
    self.startTm = TransformBus.Event.GetWorldTM(self.entityId)
    self.startX = self.startTm:GetTranslation().x
    self.dir = 1.0
    self.tickHandler = TickBus.Connect(self)
end

function TrailMover:OnTick(deltaTime, timePoint)
    local tm = TransformBus.Event.GetWorldTM(self.entityId)
    local p = tm:GetTranslation()
    p.x = p.x + self.dir * self.Properties.Speed * deltaTime
    if p.x > self.startX + self.Properties.Range then
        p.x = self.startX + self.Properties.Range
        self.dir = -1.0
    elseif p.x < self.startX - self.Properties.Range then
        p.x = self.startX - self.Properties.Range
        self.dir = 1.0
    end
    TransformBus.Event.SetWorldTranslation(self.entityId, p)
end

function TrailMover:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

return TrailMover
