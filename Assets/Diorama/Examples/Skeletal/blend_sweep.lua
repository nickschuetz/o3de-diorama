----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Drives a 1D blend tree by sweeping its parameter back and forth, so the demo shows the
-- continuous blend without any input. Put this on an entity that also has a Skeletal Clip
-- component with a Blend tree (1D) authored: each tick it sets the blend parameter to a
-- value oscillating between Min and Max, so the rig eases continuously between the two
-- bracketing clips (here arm-down at 0 and arm-up at 1). A game would instead feed a real
-- value such as walk speed via DioramaSkeletalRequestBus.Event.SetBlendParam.

local BlendSweep = {
    Properties = {
        Cycles = { default = 0.4, description = "Sweep cycles per second." },
        Min = { default = 0.0, description = "Low end of the blend parameter." },
        Max = { default = 1.0, description = "High end of the blend parameter." },
    },
}

function BlendSweep:OnActivate()
    self.t = 0.0
    self.tickHandler = TickBus.Connect(self)
end

function BlendSweep:OnTick(deltaTime, timePoint)
    self.t = self.t + deltaTime * self.Properties.Cycles
    -- A raised cosine eases 0 -> 1 -> 0 smoothly (no hard turnaround at the ends).
    local s = 0.5 - 0.5 * math.cos(self.t * 2.0 * math.pi)
    local value = self.Properties.Min + (self.Properties.Max - self.Properties.Min) * s
    DioramaSkeletalRequestBus.Event.SetBlendParam(self.entityId, value)
end

function BlendSweep:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

return BlendSweep
