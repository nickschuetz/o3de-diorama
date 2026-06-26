----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Auto-triggered super-freeze, a no-input visual demo of the 2D Simulation Clock's
-- FreezeFor verb. Put this on the same entity as a 2D Bullet Emitter whose
-- "Use Simulation Clock" is on (the demo builder sets that). It needs no entity
-- references: it drives its OWN emitter and slides its OWN transform.
--
-- Every FreezePeriod seconds it calls DioramaSimClockRequestBus.FreezeFor(frames):
-- the clock stops emitting OnSimTick, so the bullet emitter (on the sim clock) holds
-- and every bullet already in flight hangs frozen in mid-air. Meanwhile this script
-- keeps sliding the emitter on the RENDER tick (which the sim clock does not gate),
-- so the launcher glides on while the world it fired into is held. That contrast --
-- moving launcher, frozen bullets -- is the whole point of super-freeze. An optional
-- Scrim sprite is darkened for the duration to sell the cinematic pause.

local AutoSuperFreeze = {
    Properties = {
        FreezePeriod = { default = 2.5, description = "Seconds between auto super-freezes." },
        Frames = { default = 60, description = "Freeze duration in fixed sim steps (60 = 1.0s at 60/s)." },
        Speed = { default = 3.5, description = "Render-tick slide speed of the emitter, world units/sec." },
        Range = { default = 5.0, description = "Slide half-range, world units." },
        Scrim = { default = EntityId(), description = "Optional big translucent dark sprite shown during the freeze." },
        ScrimAlpha = { default = 0.45, description = "Darken-scrim opacity during the freeze." },
    },
}

function AutoSuperFreeze:OnActivate()
    self.sinceFreeze = 0.0
    self.frozen = false
    self.dir = 1.0
    self.startX = nil
    DioramaBulletRequestBus.Event.Play(self.entityId)
    self:ShowScrim(false)
    self.tickHandler = TickBus.Connect(self)
end

function AutoSuperFreeze:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function AutoSuperFreeze:ShowScrim(on)
    local scrim = self.Properties.Scrim
    if scrim:IsValid() then
        local a = on and self.Properties.ScrimAlpha or 0.0
        DioramaSpriteRequestBus.Event.SetTint(scrim, 0.0, 0.0, 0.05, a)
    end
end

function AutoSuperFreeze:Slide(deltaTime)
    local tm = TransformBus.Event.GetWorldTM(self.entityId)
    local p = tm:GetTranslation()
    if self.startX == nil then
        self.startX = p.x
    end
    p.x = p.x + self.dir * self.Properties.Speed * deltaTime
    if p.x > self.startX + self.Properties.Range then
        p.x = self.startX + self.Properties.Range
        self.dir = -1.0
    elseif p.x < self.startX - self.Properties.Range then
        p.x = self.startX - self.Properties.Range
        self.dir = 1.0
    end
    TransformBus.Event.SetWorldTranslation(self.entityId, p)
end

function AutoSuperFreeze:OnTick(deltaTime, timePoint)
    -- Render-tick motion: continues during the freeze, the whole point.
    self:Slide(deltaTime)

    if self.frozen then
        local info = DioramaSimClockRequestBus.Broadcast.GetSimClockInfo()
        if info == nil or not info.frozen then
            self.frozen = false
            self:ShowScrim(false)
        end
        return
    end

    self.sinceFreeze = self.sinceFreeze + deltaTime
    if self.sinceFreeze >= self.Properties.FreezePeriod then
        self.sinceFreeze = 0.0
        self.frozen = true
        DioramaSimClockRequestBus.Broadcast.FreezeFor(self.Properties.Frames)
        self:ShowScrim(true)
    end
end

return AutoSuperFreeze
