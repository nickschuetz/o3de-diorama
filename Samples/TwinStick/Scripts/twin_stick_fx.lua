----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Hit-effect manager for the Diorama twin-stick sample.
--
-- A SINGLE long-lived entity (one TickBus handler) that drives every explosion by
-- reusing a pool of script-less shard sprites (twin_stick_fx_shard.lua). On a kill,
-- the projectile pushes the impact point onto _G.TwinStickFXQueue; each tick this
-- manager pulls free shards from the pool, launches them outward (random direction,
-- drag, shrink + fade), and recycles them when they expire.
--
-- This replaces the previous "spawn N short-lived script entities per kill" burst.
-- That pattern churned Lua EBus handlers, whose GC finalization corrupts the heap
-- (o3de/o3de#19802). Pooling script-less sprites under one manager handler keeps the
-- explosion with zero per-effect spawn/destroy/handler churn. If #19802 is ever
-- fixed this could revert to per-shard entities, but the pooled form is the better
-- runtime design and is the one to keep.

TwinStickFXPool = rawget(_G, "TwinStickFXPool") or {}
_G.TwinStickFXPool = TwinStickFXPool
TwinStickFXQueue = rawget(_G, "TwinStickFXQueue") or {}
_G.TwinStickFXQueue = TwinStickFXQueue

local function rand(lo, hi)
    return lo + math.random() * (hi - lo)
end

-- Explosion tuning shared by every burst, kept as constants rather than
-- ScriptComponent properties: the runtime applies no .lua property default, so an
-- unbaked property reads nil and logs a "property not found" stack trace per read.
local POOL_SIZE = 64        -- shard sprites pre-spawned and reused
local SHARDS_PER_BURST = 8  -- shards launched per explosion
local DURATION = 0.45       -- shard lifetime in seconds
local START_SIZE = 0.5      -- shard size at the moment of impact
local END_SIZE = 0.05       -- shard size at the end (shrinks as it fades)
local MIN_SPEED = 4.0       -- slowest outward shard speed
local MAX_SPEED = 11.0      -- fastest outward shard speed
local DRAG = 6.0            -- how quickly shards decelerate

local TwinStickFX = {
    Properties = {
        ShardPrefab = { default = SpawnableScriptAssetRef(), description = "Script-less FX shard sprite to pool" },
    },
}

function TwinStickFX:OnActivate()
    self.poolSize = POOL_SIZE
    self.shardsPerBurst = SHARDS_PER_BURST
    self.duration = DURATION
    self.startSize = START_SIZE
    self.endSize = END_SIZE
    self.minSpeed = MIN_SPEED
    self.maxSpeed = MAX_SPEED
    self.drag = DRAG

    self.free = {}    -- free shard entityIds
    self.known = {}   -- key -> true; shards already pulled into the free list
    self.active = {}  -- { id, vx, vy, px, py, pz, age, g }
    self.mediator = SpawnableScriptMediator()

    -- Pre-spawn the pool ONCE. Each shard self-registers in TwinStickFXPool on
    -- activate; we pull them into the free list over the next few ticks.
    for i = 1, self.poolSize do
        local ticket = self.mediator:CreateSpawnTicket(self.Properties.ShardPrefab)
        self.mediator:SpawnAndParentAndTransform(ticket, EntityId(), Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0), 1.0)
    end

    self.tickHandler = TickBus.Connect(self)
end

function TwinStickFX:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
    end
end

function TwinStickFX:CollectNewShards()
    for key, id in pairs(TwinStickFXPool) do
        if not self.known[key] then
            self.known[key] = true
            table.insert(self.free, id)
        end
    end
end

function TwinStickFX:LaunchShard(x, y, z)
    local id = table.remove(self.free)
    if id == nil then
        return  -- pool exhausted this frame; drop the extra shard (graceful)
    end
    local angle = rand(0.0, 2.0 * math.pi)
    local speed = rand(self.minSpeed, self.maxSpeed)
    local g = rand(0.45, 0.85)  -- red -> soft pink variation (the shard is a heart)
    TransformBus.Event.SetWorldTranslation(id, Vector3(x, y, z))
    DioramaSpriteRequestBus.Event.SetSize(id, self.startSize, self.startSize)
    DioramaSpriteRequestBus.Event.SetTint(id, 1.0, g, g, 1.0)
    table.insert(self.active, {
        id = id, vx = math.cos(angle) * speed, vy = math.sin(angle) * speed,
        px = x, py = y, pz = z, age = 0.0, g = g,
    })
end

function TwinStickFX:OnTick(deltaTime, scriptTime)
    if rawget(_G, "TwinStickPaused") then return end
    self:CollectNewShards()

    -- Drain explosion requests (projectiles push {x,y,z} on a kill).
    while #TwinStickFXQueue > 0 do
        local pos = table.remove(TwinStickFXQueue)
        for i = 1, self.shardsPerBurst do
            self:LaunchShard(pos.x, pos.y, pos.z)
        end
    end

    -- Animate active shards; recycle expired ones back to the free list.
    local i = 1
    while i <= #self.active do
        local s = self.active[i]
        s.age = s.age + deltaTime
        local t = s.age / self.duration
        if t >= 1.0 then
            DioramaSpriteRequestBus.Event.SetSize(s.id, 0.0, 0.0)  -- hide
            table.insert(self.free, s.id)
            table.remove(self.active, i)
        else
            local damp = 1.0 - math.min(1.0, self.drag * deltaTime)
            s.vx = s.vx * damp
            s.vy = s.vy * damp
            s.px = s.px + s.vx * deltaTime
            s.py = s.py + s.vy * deltaTime
            TransformBus.Event.SetWorldTranslation(s.id, Vector3(s.px, s.py, s.pz))
            local size = self.startSize + (self.endSize - self.startSize) * t
            DioramaSpriteRequestBus.Event.SetSize(s.id, size, size)
            DioramaSpriteRequestBus.Event.SetTint(s.id, 1.0, s.g, s.g, 1.0 - t)
            i = i + 1
        end
    end
end

return TwinStickFX
