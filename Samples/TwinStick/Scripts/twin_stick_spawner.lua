----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Wave spawner for the Diorama twin-stick sample game.
--
-- Spawns the enemy prefab on a timer at random points around the edge of the
-- arena, and ramps the spawn rate up over time so waves get harder. Uses the
-- standard O3DE spawnable system (SpawnableScriptMediator), so the enemy is an
-- ordinary spawnable prefab; this is exactly how a developer wires runtime
-- spawning in O3DE.

-- Shared registry of live enemies (each enemy adds/removes itself; see
-- twin_stick_enemy.lua). We count it to cap concurrent enemies.
TwinStickEnemies = rawget(_G, "TwinStickEnemies") or {}

local function countAlive(t)
    local n = 0
    for _ in pairs(t) do
        n = n + 1
    end
    return n
end

-- Arena half-extents are a uniform game constant, kept here rather than as a
-- ScriptComponent property: the runtime applies no .lua property default, so an
-- unbaked property reads nil and logs a "property not found" stack trace.
local ARENA_HALF_X = 31.0   -- half the arena WIDTH; haters spawn on this rectangle's edge
local ARENA_HALF_Y = 17.0   -- half the arena HEIGHT

local TwinStickSpawner = {
    Properties = {
        EnemyPrefab1 = { default = SpawnableScriptAssetRef(), description = "Hater prefab (the spawner picks one of these at random per spawn)" },
        EnemyPrefab2 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab3 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab4 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab5 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab6 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab7 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab8 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        EnemyPrefab9 = { default = SpawnableScriptAssetRef(), description = "Hater prefab" },
        SpawnInterval = { default = 2.0, description = "Seconds between spawns at the start" },
        MinInterval = { default = 0.5, description = "Fastest spawn interval as waves ramp up" },
        RampPerSpawn = { default = 0.05, description = "Interval reduction after each spawn" },
        MaxAlive = { default = 30, description = "Cap on concurrently spawned enemies" },
    },
}

function TwinStickSpawner:OnActivate()
    self.mediator = SpawnableScriptMediator()
    self.timer = 0.0
    -- Guard every property with a default: a runtime (baked) ScriptComponent does
    -- NOT apply the .lua Property defaults (editor-time only), so an unbaked
    -- property is nil here.
    self.startInterval = self.Properties.SpawnInterval or 2.0
    self.minInterval = self.Properties.MinInterval or 0.5
    self.rampPerSpawn = self.Properties.RampPerSpawn or 0.05
    self.arenaHalfX = ARENA_HALF_X
    self.arenaHalfY = ARENA_HALF_Y
    self.maxAlive = self.Properties.MaxAlive or 30
    self.interval = self.startInterval
    -- Collect the hater prefabs into a list; SpawnOne picks one at random. Skip any
    -- that are unset (nil at runtime) so a partly-wired spawner still works.
    self.prefabs = {}
    local refs = { self.Properties.EnemyPrefab1, self.Properties.EnemyPrefab2, self.Properties.EnemyPrefab3,
                   self.Properties.EnemyPrefab4, self.Properties.EnemyPrefab5, self.Properties.EnemyPrefab6,
                   self.Properties.EnemyPrefab7, self.Properties.EnemyPrefab8, self.Properties.EnemyPrefab9 }
    for i = 1, 9 do
        if refs[i] ~= nil then
            table.insert(self.prefabs, refs[i])
        end
    end
    -- Spawn relative to the spawner's own world position (the arena center), so
    -- waves ring the arena wherever it is placed rather than the world origin.
    self.center = TransformBus.Event.GetWorldTranslation(self.entityId)
    self.tickHandler = TickBus.Connect(self)
end

function TwinStickSpawner:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
    end
end

function TwinStickSpawner:SpawnOne()
    -- Local ticket (the mediator retains it internally once spawned, as the player
    -- fire does); we do not track tickets ourselves -- the live-enemy count comes
    -- from the shared registry instead, so the cap reflects enemies actually alive.
    if #self.prefabs == 0 then
        return
    end
    -- Pick a random hater species to spawn.
    local ticket = self.mediator:CreateSpawnTicket(self.prefabs[math.random(1, #self.prefabs)])

    -- A random point on one of the arena rectangle's four edges, so waves pour in
    -- from all borders of the landscape floor (and varied spots along each).
    local hx = self.arenaHalfX
    local hy = self.arenaHalfY
    local ox, oy
    local edge = math.random(1, 4)
    if edge == 1 then
        ox, oy = -hx, (math.random() * 2.0 - 1.0) * hy      -- left
    elseif edge == 2 then
        ox, oy = hx, (math.random() * 2.0 - 1.0) * hy       -- right
    elseif edge == 3 then
        ox, oy = (math.random() * 2.0 - 1.0) * hx, -hy      -- bottom
    else
        ox, oy = (math.random() * 2.0 - 1.0) * hx, hy       -- top
    end
    local position = Vector3(self.center.x + ox, self.center.y + oy, self.center.z)

    -- Publish the spawn position so the enemy can place itself: SpawnAndParentAnd
    -- Transform does NOT apply the spawn transform by the enemy's first tick, so
    -- without this every enemy would start at the world origin and chase from the
    -- same corner (see twin_stick_enemy.lua OnActivate). FIFO queue, popped on spawn.
    local queue = rawget(_G, "TwinStickEnemySpawnQueue")
    if queue == nil then
        queue = {}
        _G.TwinStickEnemySpawnQueue = queue
    end
    table.insert(queue, { x = position.x, y = position.y, z = position.z })

    -- No parent (world space), no rotation, unit scale.
    self.mediator:SpawnAndParentAndTransform(ticket, EntityId(), position, Vector3(0.0, 0.0, 0.0), 1.0)
end

function TwinStickSpawner:OnTick(deltaTime, scriptTime)
    if rawget(_G, "TwinStickPaused") then return end
    self.timer = self.timer + deltaTime
    if self.timer < self.interval then
        return
    end
    -- Cap on enemies ALIVE right now (not total ever spawned), so waves keep coming
    -- as the player clears them.
    if countAlive(TwinStickEnemies) >= self.maxAlive then
        return
    end

    self.timer = 0.0
    self:SpawnOne()

    -- Ramp difficulty: each spawn shortens the interval down to MinInterval.
    self.interval = math.max(self.minInterval, self.interval - self.rampPerSpawn)
end

return TwinStickSpawner
