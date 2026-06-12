-- Rewind demo: continuous snapshots + a rewind key, on the 2D Simulation Clock.
-- Put this on any entity in a level that has:
--   * a 2D Simulation Clock component (the fixed-step heartbeat + snapshot slots),
--   * a Simulation State component on every entity whose position should rewind
--     (the marker snapshots/restores the entity's world translation),
--   * a 2D Input Actions component on THIS entity with a Button action "rewind"
--     (bind it to a key, e.g. keyboard_key_alphanumeric_R).
--
-- Every AutosaveFrames sim frames it saves a snapshot into a rotating slot, keeping
-- the last 8. Pressing "rewind" restores the OLDEST kept snapshot, so the scene
-- jumps back roughly AutosaveFrames * 8 frames (2 seconds at the defaults), then
-- play continues forward from there. Marked entities snap back; the seeded RNG and
-- the sim frame counter rewind with them.

local RewindDemo = {
    Properties = {
        AutosaveFrames = { default = 15, description = "Sim frames between autosaves (8 slots are kept)." },
    },
}

function RewindDemo:OnActivate()
    self.autosaveFrames = self.Properties.AutosaveFrames or 15
    self.savedCount = 0 -- how many autosaves have happened (slot = count % 8)
    self.tickHandler = DioramaSimTickNotificationBus.Connect(self)
end

function RewindDemo:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function RewindDemo:OnSimTick(frame, stepSeconds)
    -- Rotating autosave: slot 0..7, oldest is overwritten last.
    if frame % self.autosaveFrames == 0 then
        local slot = self.savedCount % 8
        DioramaSimClockRequestBus.Broadcast.SaveToSlot(slot)
        self.savedCount = self.savedCount + 1
    end

    -- Rewind: jump to the oldest kept snapshot. The clock's frame counter, the
    -- seeded RNG, and every marked entity's position return to that moment.
    if DioramaInputRequestBus.Event.WasPressedThisFrame(self.entityId, "rewind") then
        if self.savedCount > 0 then
            local kept = math.min(self.savedCount, 8)
            local oldest = (self.savedCount - kept) % 8
            if DioramaSimClockRequestBus.Broadcast.RestoreFromSlot(oldest) then
                -- Snapshots taken after the restored moment describe a future that
                -- no longer exists; start the rotation fresh from here.
                self.savedCount = 0
                local info = DioramaSimClockRequestBus.Broadcast.GetSimClockInfo()
                Debug.Log("Rewound to sim frame " .. tostring(info.frame))
            end
        end
    end
end

return RewindDemo
