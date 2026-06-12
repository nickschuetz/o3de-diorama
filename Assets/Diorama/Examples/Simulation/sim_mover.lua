-- Deterministic mover: advances on the 2D Simulation Clock's fixed steps (not the
-- render tick), reading the "move" Axis2D action from this entity's 2D Input
-- Actions component. Same per-frame inputs always land on the same positions,
-- which is what makes the rewind demo's restores line up exactly.

local SimMover = {
    Properties = {
        Speed = { default = 7.0, description = "Move speed in world units per second." },
    },
}

function SimMover:OnActivate()
    self.speed = self.Properties.Speed or 7.0
    self.tickHandler = DioramaSimTickNotificationBus.Connect(self)
end

function SimMover:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function SimMover:OnSimTick(frame, stepSeconds)
    local id = self.entityId
    local mx = DioramaInputRequestBus.Event.GetValue(id, "move")
    local my = DioramaInputRequestBus.Event.GetValueY(id, "move")
    if mx ~= 0.0 or my ~= 0.0 then
        local pos = TransformBus.Event.GetWorldTranslation(id)
        TransformBus.Event.SetWorldTranslation(
            id, Vector3(pos.x + mx * self.speed * stepSeconds, pos.y + my * self.speed * stepSeconds, pos.z))
    end
end

return SimMover
