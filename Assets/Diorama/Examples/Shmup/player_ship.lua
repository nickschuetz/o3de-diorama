-- Shmup player ship: fly with the "move" action, stay inside the play field, autofire
-- straight up, and destroy enemies the bolts hit. Put this on the player entity, which
-- also has:
--   * a Sprite (the ship) and a 2D Collider on the PLAYER layer,
--   * a 2D Input Actions component with an Axis2D "move" action, and
--   * a 2D Bullet Emitter (the gun): authored as a single bolt aimed up (+Y) with Fire
--     On Activate, Target Mask = the ENEMY collision layer.
--
-- The gun's hits report to THIS entity (the emitter owner), so the player script applies
-- the damage: flash the struck enemy, and after a few hits destroy it and score.
--
-- Properties are read with `or` fallbacks: O3DE can leave self.Properties.<X> nil if the
-- script's declared properties changed after the component was authored (the prefab keeps
-- stale property metadata), so never depend on them being populated.

local PlayerShip = {
    Properties = {
        MoveSpeed = { default = 12.0, description = "World units/sec." },
        MinX = { default = -8.0 },
        MaxX = { default = 8.0 },
        MinY = { default = -5.0 },
        MaxY = { default = 5.0 },
        EnemyHits = { default = 5, description = "Bolt hits an enemy takes before it dies." },
    },
}

function PlayerShip:OnActivate()
    -- Cache config with safe defaults (see the note above).
    local p = self.Properties
    self.moveSpeed = p.MoveSpeed or 12.0
    self.minX = p.MinX or -8.0
    self.maxX = p.MaxX or 8.0
    self.minY = p.MinY or -5.0
    self.maxY = p.MaxY or 5.0
    self.enemyHits = p.EnemyHits or 5

    self.enemies = {} -- key(tostring id) -> { id = EntityId, hits = int, flash = 0..1 }

    self.tickHandler = TickBus.Connect(self)
    self.bulletHandler = DioramaBulletNotificationBus.Connect(self, self.entityId)
end

function PlayerShip:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
    if self.bulletHandler ~= nil then
        self.bulletHandler:Disconnect()
        self.bulletHandler = nil
    end
end

-- A player bolt struck `target`. Flash it; after enemyHits hits, destroy it and score.
function PlayerShip:OnBulletHit(target)
    local key = tostring(target)
    local e = self.enemies[key]
    if e == nil then
        e = { id = target, hits = 0, flash = 0.0 }
        self.enemies[key] = e
    end
    e.hits = e.hits + 1
    e.flash = 1.0 -- full white this frame; OnTick decays it back down (a hit pulse)
    if e.hits >= self.enemyHits then
        self.enemies[key] = nil
        GameEntityContextRequestBus.Broadcast.DestroyGameEntity(target)
        Debug.Log("Enemy destroyed!")
    end
end

function PlayerShip:OnTick(deltaTime, timePoint)
    local id = self.entityId

    -- Move: Axis2D "move" scaled by speed, clamped to the play field. Read the Vector3
    -- by property (pos.x); pos:GetX() is nil in this engine's Lua.
    local input = DioramaInputRequestBus.Event
    local mx = input.GetValue(id, "move")
    local my = input.GetValueY(id, "move")
    local pos = TransformBus.Event.GetWorldTranslation(id)
    local nx = pos.x + mx * self.moveSpeed * deltaTime
    local ny = pos.y + my * self.moveSpeed * deltaTime
    nx = math.max(self.minX, math.min(self.maxX, nx))
    ny = math.max(self.minY, math.min(self.maxY, ny))
    TransformBus.Event.SetWorldTranslation(id, Vector3(nx, ny, pos.z))

    -- Decay each struck enemy's hit-flash back toward 0 so a hit reads as a quick pulse.
    for key, e in pairs(self.enemies) do
        if e.flash > 0.0 then
            e.flash = math.max(0.0, e.flash - deltaTime * 5.0)
            DioramaSpriteRequestBus.Event.SetFlash(e.id, 1.0, 1.0, 1.0, e.flash)
        end
    end
end

return PlayerShip
