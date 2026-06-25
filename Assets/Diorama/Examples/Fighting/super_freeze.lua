-- Super-freeze: the cinematic pause when a super move activates. On the trigger
-- button this calls DioramaSimClockRequestBus.FreezeFor(frames), which suppresses the
-- 2D Simulation Clock's automatic stepping for that many fixed steps. Every gameplay
-- system advancing on OnSimTick (the other fighter's animation, hitboxes, bullets)
-- holds for the duration, because the clock simply stops ticking them - no per-entity
-- bookkeeping. Two things keep the moment readable:
--   1. The attacker keeps moving. Its super sprite is driven on the render tick (Use
--      Simulation Clock off), so it animates through the freeze while the sim world is
--      held. Point Attacker at that sprite entity.
--   2. A darken scrim. A big translucent dark sprite (Scrim) is shown for the freeze,
--      then hidden, so the frozen world reads as "behind" the super. This is just a
--      world-space sprite the script tints - the gem stays a thin primitive.
--
-- Put this on an entity with a 2D Input Actions component that has a button action
-- named "super". The clock entity must exist in the level.

local SuperFreeze = {
    Properties = {
        Frames = { default = 36, description = "Freeze duration in fixed sim steps (36 = 0.6s at 60/s)." },
        Scrim = { default = EntityId(), description = "Big translucent dark sprite shown during the freeze." },
        ScrimAlpha = { default = 0.5, description = "Darken-scrim opacity during the freeze." },
    },
}

function SuperFreeze:OnActivate()
    self.frozenFor = 0
    self.inputHandler = DioramaInputNotificationBus.Connect(self, self.entityId)
    self.tickHandler = TickBus.Connect(self)
    self:ShowScrim(false)
end

function SuperFreeze:OnDeactivate()
    if self.inputHandler ~= nil then
        self.inputHandler:Disconnect()
        self.inputHandler = nil
    end
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function SuperFreeze:ShowScrim(on)
    local scrim = self.Properties.Scrim
    if scrim:IsValid() then
        local a = on and self.Properties.ScrimAlpha or 0.0
        DioramaSpriteRequestBus.Event.SetTint(scrim, 0.0, 0.0, 0.05, a)
    end
end

-- Trigger the freeze on the super button. Tracks the duration locally so the scrim
-- can be lifted when the clock resumes (read back from GetSimClockInfo).
function SuperFreeze:OnActionPressed(action)
    if action == "super" then
        DioramaSimClockRequestBus.Broadcast.FreezeFor(self.Properties.Frames)
        self.frozenFor = self.Properties.Frames
        self:ShowScrim(true)
    end
end

-- Lift the scrim once the clock reports it is no longer frozen.
function SuperFreeze:OnTick(deltaTime, timePoint)
    if self.frozenFor > 0 then
        local info = DioramaSimClockRequestBus.Broadcast.GetSimClockInfo()
        if info == nil or not info.frozen then
            self.frozenFor = 0
            self:ShowScrim(false)
        end
    end
end

return SuperFreeze
