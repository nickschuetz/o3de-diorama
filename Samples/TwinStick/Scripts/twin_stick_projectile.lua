----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Projectile for the Diorama twin-stick sample game.
--
-- The player spawns this prefab facing the aim direction (a yaw rotation about
-- Z). On activate it captures its forward axis, then each tick it flies along
-- that axis through the TransformBus. It despawns after a short lifetime or once
-- it leaves the arena.
--
-- Hit detection uses the gem-native 2D collision bus: the heart carries a Diorama
-- 2D collider and befriends the first hater its collider overlaps, via
-- Diorama2DCollisionNotificationBus:OnContactBegin. Collision layers are set in the
-- prefabs so hearts only ever contact creatures. This is the launcher-reachable
-- contact path PhysX's collision bus is not (that one is Automation-scope only),
-- and it replaces the earlier per-tick distance scan over the enemy table.

TwinStickEnemies = rawget(_G, "TwinStickEnemies") or {}

-- The hit explosion is owned by the FX manager (twin_stick_fx.lua): on a kill we
-- just push the impact point onto this shared queue and the manager animates a
-- pooled, script-less shard burst. This avoids spawning per-effect script entities
-- here, which churned Lua EBus handlers and crashed the engine (o3de/o3de#19802).
TwinStickFXQueue = rawget(_G, "TwinStickFXQueue") or {}
_G.TwinStickFXQueue = TwinStickFXQueue

-- Registry of live projectiles (entityId-key -> id), so the game controller can
-- destroy any in-flight hearts on quit before the engine tears down the script VM
-- (the o3de/o3de#19804 shutdown use-after-free). Same shared-table pattern as the
-- enemy registry.
TwinStickProjectiles = rawget(_G, "TwinStickProjectiles") or {}
_G.TwinStickProjectiles = TwinStickProjectiles

-- Uniform game constants (not per-projectile): the hit radius and the arena bounds
-- the shot despawns past. Kept here rather than as ScriptComponent properties so the
-- runtime does not log a "property not found" stack trace per spawned shot for a
-- value it has no baked default for (only Speed and Lifetime are baked per prefab).
local HIT_RADIUS = 0.9          -- distance at which a heart counts as reaching a hater
local ARENA_CENTER_X = 8.0
local ARENA_CENTER_Y = 8.0
local ARENA_HALF_X = 31.0       -- half the arena WIDTH (matches the landscape floor)
local ARENA_HALF_Y = 17.0       -- half the arena HEIGHT

local TwinStickProjectile = {
    Properties = {
        Speed = { default = 20.0, description = "Projectile speed in world units per second" },
        Lifetime = { default = 2.0, description = "Seconds before the projectile despawns" },
    },
}

function TwinStickProjectile:OnActivate()
    self.age = 0.0
    self.dead = false
    self.pkey = tostring(self.entityId)
    TwinStickProjectiles[self.pkey] = self.entityId
    self.gameTag = Crc32("Game")
    -- Speed/Lifetime are baked per prefab; guard them with a default (the runtime
    -- ScriptComponent does not apply a .lua property default). The rest are shared
    -- game constants (above), so no per-shot property read -> no console spam.
    self.speed = self.Properties.Speed or 20.0
    self.lifetime = self.Properties.Lifetime or 2.0
    self.hitRadiusSq = HIT_RADIUS * HIT_RADIUS
    self.cx = ARENA_CENTER_X
    self.cy = ARENA_CENTER_Y
    self.halfX = ARENA_HALF_X
    self.halfY = ARENA_HALF_Y

    -- Launch from the player's published shot rather than our own transform. When
    -- an entity is created via SpawnAndParentAndTransform the spawn transform is
    -- NOT applied yet on the first tick (it reads the world origin facing +X), so
    -- the projectile would always fly from (0,0) heading +X regardless of where the
    -- player is or aims. Instead the player writes the muzzle position + aim
    -- direction to _G.TwinStickShot at fire time; we track our own position from it
    -- and write it to the transform each tick. (Shots are cooldown-spaced, so the
    -- single global is read fresh by each projectile.)
    local shot = rawget(_G, "TwinStickShot")
    if shot ~= nil then
        self.px, self.py, self.pz = shot.x, shot.y, shot.z
        self.velX = shot.dx * self.speed
        self.velY = shot.dy * self.speed
    else
        local p = TransformBus.Event.GetWorldTranslation(self.entityId)
        self.px, self.py, self.pz = p.x, p.y, p.z
        self.velX, self.velY = self.speed, 0.0
    end

    self.tickHandler = TickBus.Connect(self)

    -- Befriend on contact: the heart's 2D collider reports overlaps with haters
    -- through this bus (layers keep it from contacting anything else).
    self.contactHandler = Diorama2DCollisionNotificationBus.Connect(self, self.entityId)
end

function TwinStickProjectile:OnContactBegin(other)
    if self.dead then
        return
    end
    self:RegisterHit(other)
end

function TwinStickProjectile:OnDeactivate()
    if self.pkey then
        TwinStickProjectiles[self.pkey] = nil
    end
    if self.tickHandler then
        self.tickHandler:Disconnect()
    end
    if self.contactHandler then
        self.contactHandler:Disconnect()
    end
end

function TwinStickProjectile:Destroy()
    -- Guard against being destroyed twice (e.g. lifetime + a hit in the same tick),
    -- which double-frees the entity and corrupts the heap (HPHA assert / crash).
    if self.dead then
        return
    end
    self.dead = true
    GameEntityContextRequestBus.Broadcast.DestroyGameEntityAndDescendants(self.entityId)
end

-- Shared hit tally per target. A heart increments its target's count; the target
-- reads its own count to grow and, at MaxHits, floats away happy (twin_stick_enemy.lua).
-- The target owns its lifecycle, so the heart never destroys it -- which also avoids
-- two hearts "finishing" the same target in one frame.
TwinStickHits = rawget(_G, "TwinStickHits") or {}
_G.TwinStickHits = TwinStickHits

function TwinStickProjectile:RegisterHit(enemyId)
    -- The heart is absorbed into the target: tally one hit, then consume ourselves.
    local key = tostring(enemyId)
    TwinStickHits[key] = (TwinStickHits[key] or 0) + 1
    self:Destroy()
end

function TwinStickProjectile:OnTick(deltaTime, scriptTime)
    if rawget(_G, "TwinStickPaused") then return end
    -- Already despawning (destruction is queued, not immediate); do nothing more.
    if self.dead then
        return
    end

    -- Despawn on lifetime.
    self.age = self.age + deltaTime
    if self.age >= self.lifetime then
        self:Destroy()
        return
    end

    -- Integrate our own authoritative position (started at the muzzle) and write it
    -- to the transform for rendering. We never read the transform back, so the
    -- deferred spawn-transform application cannot send us to the origin.
    self.px = self.px + self.velX * deltaTime
    self.py = self.py + self.velY * deltaTime
    TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(self.px, self.py, self.pz))

    -- Despawn once it leaves the arena so stray shots do not live out their full
    -- lifetime off-screen.
    local cx = self.cx
    local cy = self.cy
    local hx = self.halfX
    local hy = self.halfY
    if self.px < cx - hx or self.px > cx + hx or self.py < cy - hy or self.py > cy + hy then
        self:Destroy()
        return
    end

    -- Hits are now delivered by the 2D collision bus (OnContactBegin), so there is
    -- no per-tick distance scan here; the tick only moves the heart and bounds it.
end

return TwinStickProjectile
