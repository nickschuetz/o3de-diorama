-- Skinned-character rollback demo (2D Simulation Clock).
--
-- Put this on a Skinned Sprite (mesh-deform) entity that also carries a Simulation
-- State marker, in a level that has a 2D Simulation Clock. It self-drives a
-- three-beat loop so the rollback is visible with no input:
--   1. raise both arms, then SaveToSlot (this is the "saved" pose),
--   2. drop both arms (the sim now diverges from the saved pose),
--   3. RestoreFromSlot -> the arms snap back up to the saved pose.
--
-- RestoreFromSlot rewinds the clock's frame counter, so the beat timer runs off
-- this script's OWN tick count (which keeps advancing) rather than the clock frame
-- (which jumps back), otherwise the beat cadence would stutter after each restore.
--
-- The pose overrides (SetBoneRotation) are part of the entity's rollback snapshot,
-- so beat 3 restores the exact raised pose captured at beat 1: rollback-exact
-- skinned character animation, on screen.

local PuppetRollback = {
    Properties = {
        BeatFrames = { default = 45, description = "Sim frames each beat holds (45 = 0.75s at 60/s)." },
        Slot = { default = 0, description = "Snapshot slot to save into and restore from." },
        RaiseDegrees = { default = 150.0, description = "How far to swing the upper arms up." },
    },
}

function PuppetRollback:OnActivate()
    self.beatFrames = self.Properties.BeatFrames or 45
    self.slot = self.Properties.Slot or 0
    self.raise = self.Properties.RaiseDegrees or 150.0
    self.tick = 0
    self.lastBeat = -1
    -- Advance the rig on the sim clock (deterministic), and keep a clip playing so the
    -- mesh re-skins every step and a pose change shows immediately.
    DioramaSkinnedSpriteRequestBus.Event.SetUseSimClock(self.entityId, true)
    DioramaSkinnedSpriteRequestBus.Event.PlayAnimation(self.entityId, "idle", true)
    self.tickHandler = DioramaSimTickNotificationBus.Connect(self)
    Debug.Log("PuppetRollback: active on entity, beat=" .. tostring(self.beatFrames) .. " frames")
end

function PuppetRollback:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function PuppetRollback:SetArms(degrees)
    -- Mirror the two upper arms (left swings +, right swings -) so both raise together.
    DioramaSkinnedSpriteRequestBus.Event.SetBoneRotation(self.entityId, "armL_up", degrees)
    DioramaSkinnedSpriteRequestBus.Event.SetBoneRotation(self.entityId, "armR_up", -degrees)
end

function PuppetRollback:OnSimTick(frame, stepSeconds)
    self.tick = self.tick + 1
    local beat = math.floor(self.tick / self.beatFrames) % 3
    if beat == self.lastBeat then
        return
    end
    self.lastBeat = beat

    if beat == 0 then
        -- Raise the arms, then snapshot this pose.
        self:SetArms(self.raise)
        DioramaSimClockRequestBus.Broadcast.SaveToSlot(self.slot)
        Debug.Log("PuppetRollback: arms UP + SaveToSlot")
    elseif beat == 1 then
        -- Diverge: drop the arms.
        self:SetArms(0.0)
        Debug.Log("PuppetRollback: arms DOWN (diverge)")
    else
        -- Roll back: the arms snap to the saved raised pose.
        DioramaSimClockRequestBus.Broadcast.RestoreFromSlot(self.slot)
        Debug.Log("PuppetRollback: RestoreFromSlot -> arms SNAP up")
    end
end

return PuppetRollback
