----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Pulsing emissive glow for the solar-system diorama. Attach to any sprite with an
-- emissive material (crystal, mushrooms): each tick it oscillates the emissive
-- intensity between Min and Max so the glow "breathes", which the bloom post pass
-- turns into a soft pulsing halo. Pure DioramaSpriteRequestBus.SetEmissive, the same
-- verb the static scene uses, just animated.
local PulseEmissive = {
    Properties = {
        R = { default = 0.5 }, G = { default = 0.8 }, B = { default = 1.0 },
        Min = { default = 0.6, description = "Emissive intensity at the dim end" },
        Max = { default = 1.8, description = "Emissive intensity at the bright end" },
        Speed = { default = 1.6, description = "Pulse speed (radians/sec)" },
        Phase = { default = 0.0, description = "Phase offset so neighbours pulse out of sync" },
    },
}

function PulseEmissive:OnActivate()
    self.r = self.Properties.R or 0.5
    self.g = self.Properties.G or 0.8
    self.b = self.Properties.B or 1.0
    self.min = self.Properties.Min or 0.6
    self.max = self.Properties.Max or 1.8
    self.speed = self.Properties.Speed or 1.6
    self.t = self.Properties.Phase or 0.0
    self.tickHandler = TickBus.Connect(self)
end

function PulseEmissive:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function PulseEmissive:OnTick(deltaTime, scriptTime)
    self.t = self.t + deltaTime
    local k = 0.5 + 0.5 * math.sin(self.t * self.speed)
    local intensity = self.min + (self.max - self.min) * k
    DioramaSpriteRequestBus.Event.SetEmissive(self.entityId, self.r, self.g, self.b, intensity)
end

return PulseEmissive
