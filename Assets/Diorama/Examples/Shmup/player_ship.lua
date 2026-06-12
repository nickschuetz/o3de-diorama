-- Shmup player ship: fly with the "move" action, stay inside the play field, autofire
-- straight up, destroy enemies the bolts hit, and die (respawn) if the ship rams an enemy.
-- Put this on the player entity, which also has:
--   * a Sprite (the ship) and a 2D Collider on the PLAYER layer,
--   * a 2D Input Actions component with an Axis2D "move" action, and
--   * a 2D Bullet Emitter (the gun): authored as a single bolt aimed up (+Y) with Fire
--     On Activate and a Muzzle Offset to the nose, Target Mask = the ENEMY layer.
--
-- The gun's hits report to THIS entity (the emitter owner), so the player script applies
-- the damage: flash the struck enemy, and after a few hits destroy it and score.
--
-- Properties are read with `or` fallbacks: O3DE can leave self.Properties.<X> nil if the
-- script's declared properties changed after the component was authored (stale prefab
-- property metadata), so never depend on them being populated.

local PlayerShip = {
    Properties = {
        MoveSpeed = { default = 12.0, description = "World units/sec." },
        MinX = { default = -8.0 },
        MaxX = { default = 8.0 },
        MinY = { default = -5.0 },
        MaxY = { default = 5.0 },
        EnemyHits = { default = 5, description = "Bolt hits a normal-size enemy takes before it dies." },
        BaseEnemyWidth = { default = 1.6, description = "Sprite width of a normal enemy; bigger enemies scale up their hit count from this." },
        Lives = { default = 3, description = "Ramming an enemy costs a life; 0 = game over." },
        HalfSize = { default = 0.6, description = "Half the ship's hitbox (world units)." },
    },
}

local ENEMY_LAYER = 1 -- the collision layer the enemies' 2D Colliders sit on (default)

function PlayerShip:OnActivate()
    local p = self.Properties
    self.moveSpeed = p.MoveSpeed or 12.0
    self.minX = p.MinX or -8.0
    self.maxX = p.MaxX or 8.0
    self.minY = p.MinY or -5.0
    self.maxY = p.MaxY or 5.0
    self.enemyHits = p.EnemyHits or 5
    self.baseEnemyWidth = p.BaseEnemyWidth or 1.6
    self.lives = p.Lives or 3
    self.halfSize = p.HalfSize or 0.6

    self.enemies = {} -- key(tostring id) -> { id = EntityId, hits = int, flash = 0..1 }
    self.flash = 0.0 -- the player's own hurt-flash
    self.dead = false
    self.score = 0
    self.invuln = 0.0 -- seconds of post-respawn grace (no ram damage)
    ShmupRunning = true -- shared global the enemies read; false freezes the waves on game over

    local start = TransformBus.Event.GetWorldTranslation(self.entityId)
    self.startX, self.startY, self.startZ = start.x, start.y, start.z

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

-- "Kill" an enemy by recycling it to the top at a new X (endless waves, no spawnables).
-- Clears its hit-flash and our per-enemy hit tally.
function PlayerShip:Recycle(enemy)
    local p = TransformBus.Event.GetWorldTranslation(enemy)
    local nx = (math.random() * 2.0 - 1.0) * 7.0
    TransformBus.Event.SetWorldTranslation(enemy, Vector3(nx, 7.0, p.z))
    DioramaSpriteRequestBus.Event.SetFlash(enemy, 1.0, 1.0, 1.0, 0.0)
    self.enemies[tostring(enemy)] = nil
end

-- First sighting of an enemy: record it and decide how many hits it takes. Bigger
-- enemies are tougher: scale the hit count by the sprite's width relative to a normal
-- enemy, so an Odie (drawn larger) soaks a couple more bolts with no per-enemy config.
function PlayerShip:RegisterEnemy(target)
    local key = tostring(target)
    local e = self.enemies[key]
    if e == nil then
        local info = DioramaSpriteRequestBus.Event.GetSpriteInfo(target)
        local width = (info ~= nil and info.width) or self.baseEnemyWidth
        local maxHits = math.floor(self.enemyHits * width / self.baseEnemyWidth + 0.5)
        e = { id = target, hits = 0, flash = 0.0, maxHits = math.max(self.enemyHits, maxHits) }
        self.enemies[key] = e
    end
    return e
end

-- A player bolt struck `target`. Flash it; after its hit count, recycle it and score.
function PlayerShip:OnBulletHit(target)
    local e = self:RegisterEnemy(target)
    e.hits = e.hits + 1
    e.flash = 1.0
    -- Knockback: shove the enemy back (up, against its descent) a touch for impact.
    local p = TransformBus.Event.GetWorldTranslation(target)
    TransformBus.Event.SetWorldTranslation(target, Vector3(p.x, p.y + 0.3, p.z))
    if e.hits >= e.maxHits then
        self.score = self.score + 100
        self:Recycle(target)
        Debug.Log("Hit! Score: " .. tostring(self.score))
    end
end

-- The ship rammed `enemy`: both are destroyed, the player loses a life and respawns at
-- the start (or it is game over at zero lives).
function PlayerShip:Ram(enemy)
    self:Recycle(enemy) -- send the rammed enemy back to the top
    self.lives = self.lives - 1
    self.flash = 1.0
    DioramaSpriteRequestBus.Event.SetFlash(self.entityId, 1.0, 0.2, 0.2, 1.0) -- red hurt flash
    if self.lives <= 0 then
        self.dead = true
        -- Game over: cut the gun. The 2D Bullet Emitter autofires on its own, so stopping
        -- the script's tick is not enough; tell the emitter to stop too.
        DioramaBulletRequestBus.Event.Stop(self.entityId)
        ShmupRunning = false -- freeze the descending waves on game over
        Debug.Log("GAME OVER - press Space to play again. Final score: " .. tostring(self.score))
    else
        TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(self.startX, self.startY, self.startZ))
        self.invuln = 1.5 -- grace so you do not chain-die in the swarm on respawn
        Debug.Log("Crashed! Lives: " .. tostring(self.lives))
    end
end

-- Restart after game over: reset lives/score, revive, restart the gun, brief grace.
function PlayerShip:Restart()
    local id = self.entityId
    self.lives = self.Properties.Lives or 3
    self.score = 0
    self.dead = false
    self.flash = 0.0
    self.invuln = 1.5
    ShmupRunning = true -- resume the descending waves
    DioramaSpriteRequestBus.Event.SetFlash(id, 1.0, 0.2, 0.2, 0.0) -- clear the red
    TransformBus.Event.SetWorldTranslation(id, Vector3(self.startX, self.startY, self.startZ))
    DioramaBulletRequestBus.Event.Play(id) -- the gun was stopped on game over; restart it
    Debug.Log("Restart!")
end

function PlayerShip:OnTick(deltaTime, timePoint)
    local id = self.entityId
    local input = DioramaInputRequestBus.Event
    if self.dead then
        -- Game over: the wreck holds still and blinks red; press fire (Space) to restart.
        self.flash = self.flash - deltaTime * 2.0
        if self.flash <= 0.0 then
            self.flash = 1.0
        end
        DioramaSpriteRequestBus.Event.SetFlash(id, 1.0, 0.2, 0.2, self.flash)
        if input.WasPressedThisFrame(id, "fire") then
            self:Restart()
        end
        return
    end

    -- Move: Axis2D "move" scaled by speed, clamped to the play field. Read the Vector3
    -- by property (pos.x); pos:GetX() is nil in this engine's Lua.
    local mx = input.GetValue(id, "move")
    local my = input.GetValueY(id, "move")
    local pos = TransformBus.Event.GetWorldTranslation(id)
    local nx = pos.x + mx * self.moveSpeed * deltaTime
    local ny = pos.y + my * self.moveSpeed * deltaTime
    nx = math.max(self.minX, math.min(self.maxX, nx))
    ny = math.max(self.minY, math.min(self.maxY, ny))
    TransformBus.Event.SetWorldTranslation(id, Vector3(nx, ny, pos.z))

    -- Ram: if the ship box overlaps an enemy collider, the player dies. Skip while
    -- invincible (just after a respawn) so you do not chain-die in the swarm. OverlapBox
    -- returns every collider on the layer touching the box, so skip our own collider.
    if self.invuln > 0.0 then
        self.invuln = self.invuln - deltaTime
    else
        local hits = Diorama2DCollisionRequestBus.Broadcast.OverlapBox(nx, ny, self.halfSize, self.halfSize, ENEMY_LAYER)
        if hits ~= nil then
            for i = 1, #hits do
                if hits[i] ~= id then
                    self:Ram(hits[i])
                    break
                end
            end
        end
    end

    -- Decay the player's own hurt-flash and each struck enemy's hit-flash toward 0.
    if self.flash > 0.0 then
        self.flash = math.max(0.0, self.flash - deltaTime * 4.0)
        DioramaSpriteRequestBus.Event.SetFlash(id, 1.0, 0.2, 0.2, self.flash)
    end
    for key, e in pairs(self.enemies) do
        if e.flash > 0.0 then
            e.flash = math.max(0.0, e.flash - deltaTime * 5.0)
            DioramaSpriteRequestBus.Event.SetFlash(e.id, 1.0, 1.0, 1.0, e.flash)
        end
    end
end

return PlayerShip
