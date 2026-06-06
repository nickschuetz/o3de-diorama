----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Solar-diorama capture camera: makes itself the active orthographic view AND
-- gently pans along X so the parallax layers (sky/bodies) separate from the
-- fixed-in-world foreground. Combines make_active_camera's framing + view-deadlock
-- guard with camera_pan's oscillating pan.
local SolarCamera = {
    Properties = {
        Orthographic = { default = true },
        OrthographicHalfWidth = { default = 8.2 },
        PanRange = { default = 1.5, description = "Half-distance the camera pans along X" },
        PanSpeed = { default = 0.22, description = "Pan oscillation speed (radians/sec)" },
    },
}

function SolarCamera:Frame()
    if self.ortho then
        CameraRequestBus.Event.SetOrthographic(self.entityId, true)
        CameraRequestBus.Event.SetOrthographicHalfWidth(self.entityId, self.hw)
    end
    CameraRequestBus.Event.MakeActiveView(self.entityId)
end

function SolarCamera:OnActivate()
    self.ortho = self.Properties.Orthographic
    if self.ortho == nil then self.ortho = true end
    self.hw = self.Properties.OrthographicHalfWidth or 8.2
    self.range = self.Properties.PanRange or 1.5
    self.speed = self.Properties.PanSpeed or 0.22
    self.start = TransformBus.Event.GetWorldTranslation(self.entityId)
    self.t = 0.0
    self.ticks = 0
    self:Frame()
    self.tickHandler = TickBus.Connect(self)
end

function SolarCamera:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function SolarCamera:OnTick(deltaTime, scriptTime)
    -- Re-assert the active ortho view for the first few frames (view-activation
    -- deadlock guard), same as make_active_camera.
    if self.ticks < 3 then
        self:Frame()
        self.ticks = self.ticks + 1
    end
    self.t = self.t + deltaTime
    local x = math.sin(self.t * self.speed) * self.range
    TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(self.start.x + x, self.start.y, self.start.z))
end

return SolarCamera
