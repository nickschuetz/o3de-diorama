----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Flickering firelight. Attach to an entity that also has a Diorama 2D Light
-- (Point): each tick it jitters the light's intensity with a couple of detuned
-- sines so nearby sprites (the rock, the ground) get a restless campfire glow via
-- DioramaLightRequestBus.SetIntensity. Pure light-intensity animation; the flame
-- itself is the particle emitter on the same entity.
local FlickerLight = {
    Properties = {
        Base = { default = 2.2, description = "Mean light intensity" },
        Amp = { default = 0.9, description = "Flicker amplitude" },
        Speed = { default = 9.0, description = "Flicker speed" },
    },
}

function FlickerLight:OnActivate()
    self.base = self.Properties.Base or 2.2
    self.amp = self.Properties.Amp or 0.9
    self.speed = self.Properties.Speed or 9.0
    self.t = 0.0
    self.tickHandler = TickBus.Connect(self)
end

function FlickerLight:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function FlickerLight:OnTick(deltaTime, scriptTime)
    self.t = self.t + deltaTime
    local f = 0.6 * math.sin(self.t * self.speed) + 0.4 * math.sin(self.t * self.speed * 1.7 + 1.3)
    local intensity = self.base + self.amp * f
    if intensity < 0.0 then intensity = 0.0 end
    DioramaLightRequestBus.Event.SetIntensity(self.entityId, intensity)
end

return FlickerLight
