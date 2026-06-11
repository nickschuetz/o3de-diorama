-- Hit response + impact effects: the receiving end of the frame-data hitbox
-- component (2D Frame-Data Hitboxes). That component fires OnHit on the attacker and
-- OnHurt on the target whenever an active hitbox overlaps an active hurtbox; this
-- script turns those events into damage and "juice": a hit-spark burst, a white
-- flash, a screen-shake pulse, and a bloom post pop, all over systems the gem
-- already ships. The flash and the bloom pop are spiked on a hit and eased back to
-- rest in OnTick (SetFlash / SetBloomIntensity hold their value, so the script owns
-- the decay -- the same pattern as the materials flash_pulse demo).
--
-- Put this on each fighter (the one that also has the 2D Frame-Data Hitboxes
-- component). Point Spark at a child Particle Emitter and Camera at the versus
-- camera. The OnActivate below configures the emitter as a reusable hit-spark
-- preset, so no art setup is needed beyond a small round/white particle texture.

local HitResponse = {
    Properties = {
        Spark = { default = EntityId(), description = "Child Particle Emitter used for the hit spark." },
        Camera = { default = EntityId(), description = "Versus camera entity (for screen shake)." },
        Look = { default = EntityId(), description = "Entity with a 2D Look component (for the bloom post pulse)." },
        Damage = { default = 8.0, description = "Damage applied to the target this hit." },
        Trauma = { default = 0.35, description = "Screen-shake trauma added on a landed hit." },
        FlashFade = { default = 0.18, description = "Seconds for the white hit flash to ease back to zero." },
        PulseBloom = { default = 2.5, description = "Bloom intensity spiked on a hit, decaying back to base." },
        BaseBloom = { default = 0.6, description = "Resting bloom intensity to decay back to." },
    },
}

function HitResponse:OnActivate()
    self.health = 100.0
    self.flash = 0.0
    self.bloom = self.Properties.BaseBloom
    self.hitboxHandler = DioramaHitboxNotificationBus.Connect(self, self.entityId)
    self.tickHandler = TickBus.Connect(self)
    self:ConfigureSparkPreset()
end

function HitResponse:OnDeactivate()
    if self.hitboxHandler ~= nil then
        self.hitboxHandler:Disconnect()
        self.hitboxHandler = nil
    end
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

-- Ease the hit flash and the bloom pulse back to rest so each is a brief pop, not a
-- permanent change. Both verbs hold their last value, so the decay lives here.
function HitResponse:OnTick(deltaTime, timePoint)
    if self.flash > 0.0 then
        local step = (self.Properties.FlashFade > 0.0) and (deltaTime / self.Properties.FlashFade) or 1.0
        self.flash = math.max(0.0, self.flash - step)
        DioramaSpriteRequestBus.Event.SetFlash(self.entityId, 1.0, 1.0, 1.0, self.flash)
    end

    if self.Properties.Look:IsValid() and self.bloom > self.Properties.BaseBloom then
        self.bloom = math.max(self.Properties.BaseBloom, self.bloom - 8.0 * deltaTime)
        DioramaLookRequestBus.Event.SetBloomIntensity(self.Properties.Look, self.bloom)
    end
end

-- Hit-spark preset: a short-lived radial burst, hot-white fading to transparent,
-- thrown slightly upward and outward. Tuned once here; reused by every Burst().
function HitResponse:ConfigureSparkPreset()
    local spark = self.Properties.Spark
    if not spark:IsValid() then
        return
    end
    DioramaParticleRequestBus.Event.SetLifetime(spark, 0.12, 0.25)
    DioramaParticleRequestBus.Event.SetSpeed(spark, 4.0, 9.0)
    DioramaParticleRequestBus.Event.SetDirection(spark, 90.0, 180.0) -- fan upward, wide spread
    DioramaParticleRequestBus.Event.SetGravity(spark, 0.0, -18.0)
    DioramaParticleRequestBus.Event.SetStartColor(spark, 1.0, 0.95, 0.7, 1.0)
    DioramaParticleRequestBus.Event.SetEndColor(spark, 1.0, 0.5, 0.2, 0.0)
    DioramaParticleRequestBus.Event.SetStartSize(spark, 0.35)
    DioramaParticleRequestBus.Event.SetEndSize(spark, 0.0)
end

-- This fighter's hitbox struck `target`. The target applies its own damage in
-- OnHurt (each fighter runs this script); on the attacker side we pop the camera so
-- a confirmed hit reads as impact. `target` is the entity struck, if a move needs it.
function HitResponse:OnHit(target)
    if self.Properties.Camera:IsValid() then
        DioramaCamera2DRequestBus.Event.AddTrauma(self.Properties.Camera, self.Properties.Trauma)
    end
end

-- This fighter's hurtbox was struck by `attacker`. Take damage, flash, spark.
function HitResponse:OnHurt(attacker)
    self.health = self.health - self.Properties.Damage

    -- Snap the white impact flash to full; OnTick eases it back to zero.
    self.flash = 1.0
    DioramaSpriteRequestBus.Event.SetFlash(self.entityId, 1.0, 1.0, 1.0, self.flash)

    -- Hit spark at our position.
    if self.Properties.Spark:IsValid() then
        DioramaParticleRequestBus.Event.Burst(self.Properties.Spark)
    end

    -- Spike bloom for a one-frame post "pop"; OnTick decays it back to base.
    if self.Properties.Look:IsValid() then
        self.bloom = self.Properties.PulseBloom
        DioramaLookRequestBus.Event.SetBloomIntensity(self.Properties.Look, self.bloom)
    end
end

return HitResponse
