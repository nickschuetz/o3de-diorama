-- Enemy danmaku driver: cycles a 2D Bullet Emitter through its patterns and turns
-- bullet hits into damage. Put this on an enemy entity that has the 2D Bullet
-- Emitter component, with its Target Mask set to the layer the player's collider is
-- on. The player just needs a Diorama 2D Collider on that layer; the emitter does the
-- pattern emission, motion, pooling, and hit-testing, and tells us OnBulletHit.
--
-- The emitter fires in the world XY plane (the screen plane), the danmaku default.

local EnemyDanmaku = {
    Properties = {
        CyclePeriod = { default = 3.0, description = "Seconds between pattern changes (0 = never cycle)." },
        Damage = { default = 1.0, description = "Damage applied to a struck target per bullet." },
    },
}

function EnemyDanmaku:OnActivate()
    -- Subscribe to this emitter's bullet hits (addressed by the emitter entity id).
    self.bulletHandler = DioramaBulletNotificationBus.Connect(self, self.entityId)
    self.tickHandler = TickBus.Connect(self)
    self.timer = 0.0
    self.patternIndex = 0

    -- Start firing continuously at the configured fire rate.
    DioramaBulletRequestBus.Event.Play(self.entityId)
end

function EnemyDanmaku:OnDeactivate()
    if self.bulletHandler ~= nil then
        self.bulletHandler:Disconnect()
        self.bulletHandler = nil
    end
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

-- Cycle ring -> fan -> spiral for visual variety.
function EnemyDanmaku:OnTick(deltaTime, timePoint)
    if self.Properties.CyclePeriod <= 0.0 then
        return
    end
    self.timer = self.timer + deltaTime
    if self.timer >= self.Properties.CyclePeriod then
        self.timer = self.timer - self.Properties.CyclePeriod
        self.patternIndex = (self.patternIndex + 1) % 3
        DioramaBulletRequestBus.Event.SetPattern(self.entityId, self.patternIndex) -- 0 ring, 1 fan, 2 spiral
    end
end

-- A bullet from this emitter struck `target`. Apply damage; the renderer reported the
-- contact and already consumed the bullet, so the response is the only thing left.
function EnemyDanmaku:OnBulletHit(target)
    -- A real game would look up the target's health component and subtract Damage.
    Debug.Log("Danmaku hit " .. tostring(target) .. " for " .. tostring(self.Properties.Damage))
end

return EnemyDanmaku
