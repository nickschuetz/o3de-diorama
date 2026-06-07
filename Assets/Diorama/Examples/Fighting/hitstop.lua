-- Hit-stop: briefly freeze a character's animation on impact for that crunchy
-- fighting-game feel. Put this on a character that has a Sprite (or Aseprite)
-- component. Call Freeze(seconds) on a hit (e.g. from the attacker when a hitbox
-- connects, on both characters). While frozen the animation holds its frame
-- (playback speed 0); movement scripts should check IsFrozen() and skip their step.
--
-- It uses the gem's playback time-scale: SetPlaybackSpeed for sprite-sheet sprites
-- and SetSpeed for Aseprite playback, so it works whichever you use. The same knob
-- does slow motion: pass a speed between 0 and 1 instead of freezing.

local HitStop = {
    Properties = {},
}

function HitStop:OnActivate()
    self.frozenFor = 0.0
    self.tickHandler = TickBus.Connect(self)
end

function HitStop:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function HitStop:SetSpeed(speed)
    -- Drive both animation paths; only the one in use responds.
    DioramaSpriteRequestBus.Event.SetPlaybackSpeed(self.entityId, speed)
    DioramaAsepriteRequestBus.Event.SetSpeed(self.entityId, speed)
end

-- Freeze the animation for `seconds` (typical hit-stop is ~0.05-0.12s).
function HitStop:Freeze(seconds)
    self.frozenFor = math.max(self.frozenFor, seconds)
    self:SetSpeed(0.0)
end

function HitStop:IsFrozen()
    return self.frozenFor > 0.0
end

function HitStop:OnTick(deltaTime, scriptTime)
    if self.frozenFor <= 0.0 then
        return
    end
    self.frozenFor = self.frozenFor - deltaTime
    if self.frozenFor <= 0.0 then
        self.frozenFor = 0.0
        self:SetSpeed(1.0) -- resume normal playback
    end
end

return HitStop
