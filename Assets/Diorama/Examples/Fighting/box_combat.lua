-- Typed-box combat: the payload-driven receiving end of the 2D Frame-Data Hitboxes
-- component. Where hit_response.lua reacts to the v1 OnHit/OnHurt, this script
-- listens to OnBoxEvent, which fires on both parties for every typed interaction and
-- carries the attacking box's whole payload. It reads the numbers the rig authored
-- (damage, hitstun, hitstop) and branches on the result kind, so one handler covers
-- strikes, armor absorbs, throws, and proximity-guard hints. The gem only delivers
-- the payload; applying it (the health, the hit-stop freeze, the thrown state) is
-- this game-side script's job.
--
-- Put it on each fighter (the entity that also has the 2D Frame-Data Hitboxes
-- component). Point Spark at a child Particle Emitter to place a burst at the contact
-- point. Hit-stop freezes this fighter's own animation for a few frames via
-- SetPlaybackSpeed(0); OnTick counts the frames down and restores normal speed.

local BoxCombat = {
    Properties = {
        Spark = { default = EntityId(), description = "Child Particle Emitter burst at the contact point." },
        StartHealth = { default = 100.0, description = "Starting health." },
    },
}

-- DioramaBoxEvent.result codes (see DioramaHitboxBus.h).
local RESULT_HIT = 1
local RESULT_CLASH = 2
local RESULT_BEATEN = 3
local RESULT_ABSORBED = 4
local RESULT_THROW = 5
local RESULT_PROXIMITY = 6

function BoxCombat:OnActivate()
    self.health = self.Properties.StartHealth
    self.hitstunFrames = 0
    self.hitstopFrames = 0
    self.thrown = false
    self.guardSuggested = false
    self.handler = DioramaHitboxNotificationBus.Connect(self, self.entityId)
    self.tickHandler = TickBus.Connect(self)
end

function BoxCombat:OnDeactivate()
    if self.handler ~= nil then
        self.handler:Disconnect()
        self.handler = nil
    end
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

-- Count down the hit-stop freeze (in sim-ish frames at ~60fps) and restore the
-- animation when it ends. Hitstun is just decremented here for the demo; a full game
-- would gate this fighter's movement/input on hitstunFrames > 0.
function BoxCombat:OnTick(deltaTime, timePoint)
    if self.hitstopFrames > 0 then
        self.hitstopFrames = self.hitstopFrames - 1
        if self.hitstopFrames <= 0 then
            DioramaSpriteRequestBus.Event.SetPlaybackSpeed(self.entityId, 1.0)
        end
    end
    if self.hitstunFrames > 0 then
        self.hitstunFrames = self.hitstunFrames - 1
    end
end

-- Spark at the world contact point the event reports (move the child emitter there,
-- then burst), so impacts land where the boxes actually overlapped.
function BoxCombat:SparkAt(x, y)
    local spark = self.Properties.Spark
    if spark:IsValid() then
        TransformBus.Event.SetWorldTranslation(spark, Vector3(x, y, 1.0))
        DioramaParticleRequestBus.Event.Burst(spark)
    end
end

-- One handler for every typed interaction. Each fighter runs this; act only when we
-- are the defender (or, for a clash, on either side).
function BoxCombat:OnBoxEvent(e)
    if e.result == RESULT_HIT and e.defender == self.entityId then
        -- Struck: take the authored damage, freeze briefly (hit-stop), flash, spark.
        self.health = self.health - e.damage
        self.hitstunFrames = e.hitstunFrames
        self.hitstopFrames = e.hitstopFrames
        if e.hitstopFrames > 0 then
            DioramaSpriteRequestBus.Event.SetPlaybackSpeed(self.entityId, 0.0) -- freeze; OnTick restores
        end
        DioramaSpriteRequestBus.Event.SetFlash(self.entityId, 1.0, 1.0, 1.0, 1.0)
        self:SparkAt(e.contactX, e.contactY)
    elseif e.result == RESULT_ABSORBED and e.defender == self.entityId then
        -- Armor ate the hit: no damage, a blue flash to read the absorb. A real game
        -- spends an armor charge here.
        DioramaSpriteRequestBus.Event.SetFlash(self.entityId, 0.3, 0.5, 1.0, 1.0)
    elseif e.result == RESULT_THROW and e.defender == self.entityId then
        -- Grabbed: enter a thrown state (the demo just flags it and stops animating).
        self.thrown = true
        DioramaSpriteRequestBus.Event.SetPlaybackSpeed(self.entityId, 0.0)
    elseif e.result == RESULT_PROXIMITY and e.defender == self.entityId then
        -- An attack is in range: a real game would enter proximity guard here.
        self.guardSuggested = true
    elseif e.result == RESULT_CLASH then
        -- Equal-priority hitboxes met: both fighters get a spark, neither takes damage.
        self:SparkAt(e.contactX, e.contactY)
    elseif e.result == RESULT_BEATEN and e.attacker == self.entityId then
        -- Our attack lost the priority contest: a real game would cancel into recovery.
        self.guardSuggested = false
    end
end

return BoxCombat
