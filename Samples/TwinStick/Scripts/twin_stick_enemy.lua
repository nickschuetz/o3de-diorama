----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Chase AI for a twin-stick enemy in the Diorama sample game.
--
-- Each tick the enemy finds the player (the entity tagged "Player") and steers
-- straight toward it in the XY plane, moving through the TransformBus and facing
-- its movement direction by flipping its Diorama sprite.
--
-- Movement is transform-driven rather than physics-driven for the reasons spelled
-- out in twin_stick_player.lua (rigid bodies ignore authored placement, and PhysX
-- collision callbacks are not reflected to launcher Lua). Because there are no
-- physics contacts, the projectile cannot rely on OnCollisionBegin to know it hit
-- an enemy. Instead every live enemy registers itself in a shared Lua table keyed
-- by entity id; the projectile reads that table and tests distance. All game
-- ScriptComponents share one Lua VM (the default script context), so this global
-- table is visible to every script.
--
-- Steering combines "pursue" (toward the player) with "separation" (a push away
-- from nearby enemies) so the pack does not collapse into one stack on top of the
-- player. Separation reads a shared position cache that each enemy refreshes once
-- per tick (TwinStickEnemyPos), so the O(n^2) neighbour test costs one extra
-- TransformBus read per enemy per frame, not one read per pair.

TwinStickEnemies = rawget(_G, "TwinStickEnemies") or {}
-- entityId-key -> {x, y}: each enemy's last known position, refreshed by its owner
-- each tick and read by neighbours for separation.
TwinStickEnemyPos = rawget(_G, "TwinStickEnemyPos") or {}
_G.TwinStickEnemyPos = TwinStickEnemyPos
-- Per-target heart tally (hearts increment it) + the shared FX queue the pooled
-- effect manager drains (twin_stick_fx.lua). See twin_stick_projectile.lua.
TwinStickHits = rawget(_G, "TwinStickHits") or {}
_G.TwinStickHits = TwinStickHits
TwinStickFXQueue = rawget(_G, "TwinStickFXQueue") or {}
_G.TwinStickFXQueue = TwinStickFXQueue

-- Each predator ("hater") is its own prefab (grouper, albatross, orca, diver,
-- penguin, moray, sperm whale, dolphin, seal) with its texture, size and chase speed
-- baked in; the spawner picks one at random. This script drives the shared behaviour.

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi else return v end
end

-- Game-wide tuning shared by every hater. Only MoveSpeed and BaseSize vary per
-- creature (baked into each prefab below), so the rest live here as constants
-- rather than ScriptComponent properties. Reading a property the runtime
-- ScriptComponent has no baked value for emits a "property not found" warning and
-- a full stack trace on every access (the runtime does not apply a .lua property's
-- default), which otherwise spams the console once per spawned enemy.
local ARENA_CENTER_X = 8.0
local ARENA_CENTER_Y = 8.0
local ARENA_HALF_X = 31.0       -- half the playable arena WIDTH (matches the landscape floor)
local ARENA_HALF_Y = 17.0       -- half the playable arena HEIGHT
local SEPARATION_RADIUS = 2.0   -- haters closer than this push apart so they do not stack
local SEPARATION_WEIGHT = 1.5   -- strength of the separation push relative to the chase
local MAX_HITS = 3              -- hearts needed to fill a hater with love before it floats away
local GROW_STEP = 0.5           -- size added per heart (filling with air/love)
local FLOAT_UP_SPEED = 6.0      -- rise speed (units/sec) as a content hater floats to the surface
local FLOAT_DURATION = 1.8      -- seconds the float-away (rise + swell + fade) lasts

local TwinStickEnemy = {
    Properties = {
        MoveSpeed = { default = 4.0, description = "Chase speed in world units per second (baked per creature)" },
        BaseSize = { default = 3.8, description = "Target size before any hearts land, ~Obi's size (baked per creature)" },
    },
}

function TwinStickEnemy:OnActivate()
    self.facingLeft = false
    -- The two per-creature values are baked into each prefab; cache them once with a
    -- default (the runtime ScriptComponent does not apply a .lua property default).
    -- Everything else is a shared game constant (above), so no per-enemy property
    -- read -- which keeps the console clean and avoids per-spawn stack-trace churn.
    self.moveSpeed = self.Properties.MoveSpeed or 4.0
    self.baseSize = self.Properties.BaseSize or 3.8
    self.cx = ARENA_CENTER_X
    self.cy = ARENA_CENTER_Y
    self.halfX = ARENA_HALF_X
    self.halfY = ARENA_HALF_Y
    self.sepRadius = SEPARATION_RADIUS
    self.sepWeight = SEPARATION_WEIGHT
    self.sepRadiusSq = self.sepRadius * self.sepRadius
    self.maxHits = MAX_HITS
    self.growStep = GROW_STEP
    self.floatUpSpeed = FLOAT_UP_SPEED
    self.floatDuration = FLOAT_DURATION
    self.shownHits = 0
    self.content = false   -- true once full of love and floating to the surface
    self.dead = false
    self.size = self.baseSize
    -- target.png is white, so Tint sets the color: calm bubble -> "full of love" pink
    -- as hearts fill it (the script recolors as it grows, then fades on float-away).
    self.baseR, self.baseG, self.baseB = 1.0, 1.0, 1.0    -- white = the art's true colours
    self.happyR, self.happyG, self.happyB = 1.0, 0.5, 0.7  -- rosy "full of love" blush
    -- The global tag bus is addressed by the tag's Crc32; "Player" matches the
    -- tag set on the player entity.
    self.playerTag = Crc32("Player")
    -- Register in the shared enemy table so projectiles can find us by distance.
    self.key = tostring(self.entityId)
    TwinStickEnemies[self.key] = self.entityId

    -- Spawn position: an entity created via SpawnAndParentAndTransform does NOT have
    -- its spawn transform applied by its first tick (it reads the world origin), so
    -- if we chased from the transform every enemy would start at (0,0) -- the "all
    -- enemies spawn in the same lower-left spot" bug. The spawner publishes each
    -- spawn's world position to a FIFO queue; we pop ours, set it immediately, and
    -- stay authoritative over our position from here (we never read the transform
    -- back, the same fix the projectile uses). Spawns are interval-spaced so the
    -- queue holds exactly our entry.
    local q = rawget(_G, "TwinStickEnemySpawnQueue")
    if q ~= nil and #q > 0 then
        local s = table.remove(q, 1)
        self.px, self.py, self.pz = s.x, s.y, s.z
    else
        local p = TransformBus.Event.GetWorldTranslation(self.entityId)
        self.px, self.py, self.pz = p.x, p.y, p.z
    end
    TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(self.px, self.py, self.pz))

    -- Texture, size and speed are baked into this creature's prefab; start at the
    -- baked size and the calm tint (white = the art's true colours).
    self.size = self.baseSize
    DioramaSpriteRequestBus.Event.SetSize(self.entityId, self.size, self.size)
    DioramaSpriteRequestBus.Event.SetTint(self.entityId, self.baseR, self.baseG, self.baseB, 1.0)

    self.tickHandler = TickBus.Connect(self)
end

function TwinStickEnemy:OnDeactivate()
    if self.key then
        TwinStickEnemies[self.key] = nil
        TwinStickEnemyPos[self.key] = nil
    end
    if self.tickHandler then
        self.tickHandler:Disconnect()
    end
end

function TwinStickEnemy:Inflate(hits)
    -- Each heart fills the target with a little more air/love: swell, and lerp the
    -- color from the calm bubble toward "full of love" pink. A heart pops for feedback.
    self.shownHits = hits
    self.size = self.baseSize + hits * self.growStep
    DioramaSpriteRequestBus.Event.SetSize(self.entityId, self.size, self.size)
    local frac = hits / self.maxHits
    if frac > 1.0 then frac = 1.0 end
    DioramaSpriteRequestBus.Event.SetTint(self.entityId,
        self.baseR + (self.happyR - self.baseR) * frac,
        self.baseG + (self.happyG - self.baseG) * frac,
        self.baseB + (self.happyB - self.baseB) * frac, 1.0)
    table.insert(TwinStickFXQueue, { x = self.px, y = self.py, z = self.pz })
end

function TwinStickEnemy:BecomeContent()
    -- Full of love: stop being a target (no more hits, separation, or chasing) and
    -- begin floating up to the surface, with a happy burst of hearts.
    self.content = true
    self.floatT = 0.0
    TwinStickEnemies[self.key] = nil
    TwinStickEnemyPos[self.key] = nil
    TwinStickHits[self.key] = nil
    DioramaSpriteRequestBus.Event.SetTint(self.entityId, self.happyR, self.happyG, self.happyB, 1.0)
    for _ = 1, 5 do
        table.insert(TwinStickFXQueue, { x = self.px, y = self.py, z = self.pz })
    end
    -- Score: a hater befriended. Bump the shared global the HUD controller polls
    -- (twin_stick_game.lua), the same shared-table pattern used across the sample.
    _G.TwinStickBefriended = (rawget(_G, "TwinStickBefriended") or 0) + 1
end

function TwinStickEnemy:FloatAway(deltaTime)
    self.floatT = self.floatT + deltaTime
    -- Rise to the surface with a gentle wobble, swelling and fading as it goes.
    self.py = self.py + self.floatUpSpeed * deltaTime
    self.px = self.px + math.sin(self.floatT * 4.0) * 1.5 * deltaTime
    self.size = self.size + 1.2 * deltaTime
    DioramaSpriteRequestBus.Event.SetSize(self.entityId, self.size, self.size)
    local a = 1.0 - self.floatT / self.floatDuration
    if a < 0.0 then a = 0.0 end
    DioramaSpriteRequestBus.Event.SetTint(self.entityId, self.happyR, self.happyG, self.happyB, a)
    TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(self.px, self.py, self.pz))
    if self.floatT >= self.floatDuration and not self.dead then
        self.dead = true
        GameEntityContextRequestBus.Broadcast.DestroyGameEntityAndDescendants(self.entityId)
    end
end

function TwinStickEnemy:OnTick(deltaTime, scriptTime)
    if rawget(_G, "TwinStickPaused") then return end
    -- Already full of love: rise to the surface and fade out, no more chasing.
    if self.content then
        self:FloatAway(deltaTime)
        return
    end
    -- Absorb any hearts that landed: swell + recolor, and float away at MaxHits.
    local hits = TwinStickHits[self.key] or 0
    if hits ~= self.shownHits then
        self:Inflate(hits)
        if hits >= self.maxHits then
            self:BecomeContent()
            return
        end
    end

    -- "Get entity by tag" reflects to scripting as RequestTaggedEntities; O3DE's
    -- Lua binding also exposes the spaces-stripped name GetEntityByTag. It is
    -- addressed by the tag's Crc32 and returns the (single) tagged entity.
    local player = TagGlobalRequestBus.Event.GetEntityByTag(self.playerTag)
    if player == nil or not player:IsValid() then
        return
    end

    local target = TransformBus.Event.GetWorldTranslation(player)
    -- Our position is authoritative (self.px/py), seeded from the spawn point; we do
    -- not read it back from the transform (which would return the origin until the
    -- deferred spawn transform applies).
    local mine = { x = self.px, y = self.py, z = self.pz }

    -- Publish our position for neighbours' separation (reuse the table to avoid a
    -- per-frame allocation).
    local mp = TwinStickEnemyPos[self.key]
    if mp then
        mp.x = mine.x
        mp.y = mine.y
    else
        TwinStickEnemyPos[self.key] = { x = mine.x, y = mine.y }
    end

    -- Pursue: unit vector toward the player.
    local cdx = target.x - mine.x
    local cdy = target.y - mine.y
    local cdist = math.sqrt(cdx * cdx + cdy * cdy)
    if cdist > 0.01 then
        cdx = cdx / cdist
        cdy = cdy / cdist
    else
        cdx = 0.0
        cdy = 0.0
    end

    -- Separation: sum a push away from every neighbour inside SeparationRadius,
    -- weighted so closer neighbours push harder. Uses the cached positions, so no
    -- extra TransformBus reads here.
    local sx = 0.0
    local sy = 0.0
    for k, p in pairs(TwinStickEnemyPos) do
        if k ~= self.key then
            local ox = mine.x - p.x
            local oy = mine.y - p.y
            local d2 = ox * ox + oy * oy
            if d2 > 0.0001 and d2 < self.sepRadiusSq then
                local d = math.sqrt(d2)
                local w = (self.sepRadius - d) / self.sepRadius
                sx = sx + (ox / d) * w
                sy = sy + (oy / d) * w
            end
        end
    end

    -- Blend pursue + weighted separation into a single heading.
    local dirx = cdx + sx * self.sepWeight
    local diry = cdy + sy * self.sepWeight
    local len = math.sqrt(dirx * dirx + diry * diry)
    if len < 0.001 then
        return
    end
    dirx = dirx / len
    diry = diry / len

    -- Step along the heading and clamp inside the arena so enemies stay confined.
    local speed = self.moveSpeed
    local cx = self.cx
    local cy = self.cy
    local hx = self.halfX
    local hy = self.halfY
    local nx = clamp(mine.x + dirx * speed * deltaTime, cx - hx, cx + hx)
    local ny = clamp(mine.y + diry * speed * deltaTime, cy - hy, cy + hy)
    self.px = nx
    self.py = ny
    TransformBus.Event.SetWorldTranslation(self.entityId, Vector3(nx, ny, self.pz))

    local faceLeft = dirx < 0.0
    if faceLeft ~= self.facingLeft then
        self.facingLeft = faceLeft
        DioramaSpriteRequestBus.Event.SetFlip(self.entityId, faceLeft, false)
    end
end

return TwinStickEnemy
