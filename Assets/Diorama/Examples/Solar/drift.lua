----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Slow horizontal drift with wrap-around, for the diorama's comet: it streaks
-- across the sky and re-enters from the far side. Just TransformBus translation, so
-- it composes with whatever else is on the entity (e.g. its static emissive glow).
local Drift = {
    Properties = {
        Vx = { default = -0.6, description = "Horizontal speed in world units/sec (negative = leftward)" },
        Vy = { default = 0.0, description = "Vertical speed in world units/sec" },
        MinX = { default = -9.5, description = "Wrap: when x passes this, jump to MaxX" },
        MaxX = { default = 9.5, description = "Wrap: when x passes this, jump to MinX" },
    },
}

function Drift:OnActivate()
    self.vx = self.Properties.Vx or -0.6
    self.vy = self.Properties.Vy or 0.0
    self.minX = self.Properties.MinX or -9.5
    self.maxX = self.Properties.MaxX or 9.5
    self.pos = TransformBus.Event.GetWorldTranslation(self.entityId)
    self.tickHandler = TickBus.Connect(self)
end

function Drift:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function Drift:OnTick(deltaTime, scriptTime)
    local x = self.pos.x + self.vx * deltaTime
    local y = self.pos.y + self.vy * deltaTime
    if self.vx < 0 and x < self.minX then x = self.maxX end
    if self.vx > 0 and x > self.maxX then x = self.minX end
    self.pos = Vector3(x, y, self.pos.z)
    TransformBus.Event.SetWorldTranslation(self.entityId, self.pos)
end

return Drift
