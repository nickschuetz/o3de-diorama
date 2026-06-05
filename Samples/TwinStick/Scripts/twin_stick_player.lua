----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Twin-stick player movement for the Diorama sample game.
--
-- Top-down movement in the world XY plane: WASD / left-stick drive the player and
-- the mouse / right-stick aim sets the facing direction. The player is a Diorama
-- sprite laid flat on the ground; "facing" is a yaw rotation about world Z so the
-- sprite visibly points toward the aim.
--
-- Movement is driven directly through the TransformBus (SetWorldTranslation),
-- not through PhysX. Two engine facts make this the right call for this sample:
--   1. A dynamic PhysX rigid body does not honor an entity's authored editor
--      transform at simulation start (it initializes at the world origin), which
--      put the player -- and everything that keys off its position -- in the
--      lower-left corner.
--   2. PhysX collision callbacks are reflected only in the Automation script
--      scope, so a launcher Lua script never receives OnCollisionBegin. Hit
--      detection is therefore done by distance (see twin_stick_projectile.lua).
-- Transform-driven movement honors placement, is fully deterministic, and is the
-- same API an AI agent would script, so confinement is a simple clamp to the
-- arena bounds rather than physics walls.
--
-- The aim direction is published via the spawn rotation of each projectile so the
-- projectile script can fly toward where the player is aiming.

-- Version-proof atan2: math.atan2 was removed in Lua 5.3 and the two-argument
-- math.atan does not exist in 5.1, but one-argument math.atan exists everywhere.
local function atan2(y, x)
    if x > 0.0 then
        return math.atan(y / x)
    elseif x < 0.0 then
        if y >= 0.0 then
            return math.atan(y / x) + math.pi
        end
        return math.atan(y / x) - math.pi
    elseif y > 0.0 then
        return math.pi * 0.5
    elseif y < 0.0 then
        return -math.pi * 0.5
    end
    return 0.0
end

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi else return v end
end

-- Uniform game constants (not per-instance tuning), kept as locals rather than
-- ScriptComponent properties: the runtime applies no .lua property default, so an
-- unbaked property reads nil and logs a "property not found" stack trace.
local MOUSE_SENSITIVITY = 0.25  -- how strongly mouse deltas steer the aim (scripts get no cursor position, only deltas)
local THREAT_RANGE = 12.0       -- distance at which the nearest hater fully flushes Obi to his alarm colour
local ARENA_CENTER_X = 8.0
local ARENA_CENTER_Y = 8.0
local ARENA_HALF_X = 31.0       -- half the playable arena WIDTH (one tile inside the floor edge)
local ARENA_HALF_Y = 17.0       -- half the playable arena HEIGHT

local TwinStickPlayer = {
    Properties = {
        MoveSpeed = { default = 9.5, description = "Movement speed in world units per second" },
        AimDeadZone = { default = 0.05, description = "Ignore aim input below this magnitude" },
        TurnSpeed = { default = 8.0, description = "How fast the sprite rotates toward the aim, in radians per second (lower = slower/less sensitive)" },
        ProjectilePrefab = { default = SpawnableScriptAssetRef(), description = "Projectile prefab to fire" },
        FireCooldown = { default = 0.2, description = "Minimum seconds between shots" },
        MuzzleOffset = { default = 1.0, description = "Spawn distance ahead of the player" },
    },
}

function TwinStickPlayer:OnActivate()
    self.moveX = 0.0
    self.moveY = 0.0
    self.aimX = 0.0    -- right-stick X (absolute aim direction)
    self.aimY = 0.0    -- right-stick Y
    self.mouseDX = 0.0 -- accumulated mouse delta X this tick (relative aim steering)
    self.mouseDY = 0.0 -- accumulated mouse delta Y this tick
    -- Last non-zero aim, kept so firing has a direction even between flicks.
    self.aimDir = Vector3(1.0, 0.0, 0.0)
    self.aimYaw = 0.0      -- target yaw from the latest aim input
    self.facingYaw = 0.0   -- current (smoothed) sprite yaw, eased toward aimYaw

    -- Cache properties with defaults. A runtime (baked) ScriptComponent does NOT
    -- apply the .lua Property defaults (editor-time only), so any property not baked
    -- into the level/prefab is nil at runtime. The whole level is baked to a
    -- spawnable for the launcher, so guard every property the player reads.
    self.moveSpeed = self.Properties.MoveSpeed or 9.5
    self.aimDeadZone = self.Properties.AimDeadZone or 0.05
    self.turnSpeed = self.Properties.TurnSpeed or 8.0
    self.mouseSens = MOUSE_SENSITIVITY
    self.threatRange = THREAT_RANGE
    -- Octopus mood (octopus.png is white, so Tint sets the body color): calm orange
    -- when safe, flushing toward alarm red as the nearest target closes in.
    self.calmR, self.calmG, self.calmB = 1.0, 1.0, 1.0    -- white tint = the sprite's own colours
    self.alarmR, self.alarmG, self.alarmB = 1.0, 0.45, 0.35  -- multiply-flush toward red
    self.fireCooldown = self.Properties.FireCooldown or 0.2
    self.muzzleOffset = self.Properties.MuzzleOffset or 1.0
    self.arenaCx = ARENA_CENTER_X
    self.arenaCy = ARENA_CENTER_Y
    self.arenaHalfX = ARENA_HALF_X
    self.arenaHalfY = ARENA_HALF_Y

    -- Keep Obi billboarded: his body stays upright facing the camera (no spinning
    -- with the aim). We convey aim direction by flipping him left/right and by the
    -- projectiles, not by rotating the body.
    self.facingLeft = false
    DioramaSpriteRequestBus.Event.SetBillboard(self.entityId, true)

    -- Firing state.
    self.firing = false
    self.fireTimer = 0.0
    self.fireMediator = SpawnableScriptMediator()

    self.tickHandler = TickBus.Connect(self)

    self.idMoveX = InputEventNotificationId("move_x")
    self.idMoveY = InputEventNotificationId("move_y")
    self.idAimX = InputEventNotificationId("aim_x")
    self.idAimY = InputEventNotificationId("aim_y")
    self.idAimDX = InputEventNotificationId("aimdelta_x")
    self.idAimDY = InputEventNotificationId("aimdelta_y")
    self.idFire = InputEventNotificationId("fire")

    self.hMoveX = InputEventNotificationBus.Connect(self, self.idMoveX)
    self.hMoveY = InputEventNotificationBus.Connect(self, self.idMoveY)
    self.hAimX = InputEventNotificationBus.Connect(self, self.idAimX)
    self.hAimY = InputEventNotificationBus.Connect(self, self.idAimY)
    self.hAimDX = InputEventNotificationBus.Connect(self, self.idAimDX)
    self.hAimDY = InputEventNotificationBus.Connect(self, self.idAimDY)
    self.hFire = InputEventNotificationBus.Connect(self, self.idFire)
end

function TwinStickPlayer:OnDeactivate()
    if self.tickHandler then self.tickHandler:Disconnect() end
    if self.hMoveX then self.hMoveX:Disconnect() end
    if self.hMoveY then self.hMoveY:Disconnect() end
    if self.hAimX then self.hAimX:Disconnect() end
    if self.hAimY then self.hAimY:Disconnect() end
    if self.hAimDX then self.hAimDX:Disconnect() end
    if self.hAimDY then self.hAimDY:Disconnect() end
    if self.hFire then self.hFire:Disconnect() end
end

function TwinStickPlayer:OnPressed(value)
    local id = InputEventNotificationBus.GetCurrentBusId()
    if id == self.idMoveX then
        self.moveX = value
    elseif id == self.idMoveY then
        self.moveY = value
    elseif id == self.idAimX then
        self.aimX = value
    elseif id == self.idAimY then
        self.aimY = value
    elseif id == self.idAimDX then
        -- Mouse deltas can fire several times per frame; accumulate them.
        self.mouseDX = self.mouseDX + value
    elseif id == self.idAimDY then
        self.mouseDY = self.mouseDY + value
    elseif id == self.idFire then
        self.firing = true
    end
end

function TwinStickPlayer:OnHeld(value)
    self:OnPressed(value)
end

function TwinStickPlayer:OnReleased(value)
    local id = InputEventNotificationBus.GetCurrentBusId()
    if id == self.idMoveX then
        self.moveX = 0.0
    elseif id == self.idMoveY then
        self.moveY = 0.0
    elseif id == self.idAimX then
        self.aimX = 0.0
    elseif id == self.idAimY then
        self.aimY = 0.0
    elseif id == self.idFire then
        self.firing = false
    end
end

function TwinStickPlayer:FireProjectile()
    -- Spawn the projectile at the muzzle, rotated to face the aim direction; the
    -- projectile launches itself along that forward axis.
    local ticket = self.fireMediator:CreateSpawnTicket(self.Properties.ProjectilePrefab)
    local origin = TransformBus.Event.GetWorldTranslation(self.entityId)
    local offset = self.muzzleOffset
    local position = Vector3(
        origin.x + self.aimDir.x * offset,
        origin.y + self.aimDir.y * offset,
        origin.z)
    -- Publish this shot so the projectile launches from the muzzle along the aim,
    -- independent of when its spawn transform is applied (it is not applied on the
    -- projectile's first tick, which would otherwise send every shot from the world
    -- origin heading +X).
    _G.TwinStickShot = {
        x = position.x, y = position.y, z = origin.z,
        dx = self.aimDir.x, dy = self.aimDir.y,
    }
    -- Yaw (degrees) about Z so the projectile's local +X points along the aim.
    local yawDegrees = math.deg(atan2(self.aimDir.y, self.aimDir.x))
    self.fireMediator:SpawnAndParentAndTransform(
        ticket, EntityId(), position, Vector3(0.0, 0.0, yawDegrees), 1.0)
end

function TwinStickPlayer:OnTick(deltaTime, scriptTime)
    -- Frozen while paused (the game controller toggles this shared flag on P).
    if rawget(_G, "TwinStickPaused") then return end

    -- Movement: normalize so diagonals are not faster, step the world position in
    -- the XY plane, then clamp inside the arena so play stays confined.
    local pos = TransformBus.Event.GetWorldTranslation(self.entityId)
    local moveLen = math.sqrt(self.moveX * self.moveX + self.moveY * self.moveY)
    if moveLen > 0.1 then
        local speed = self.moveSpeed
        local nx = pos.x + (self.moveX / moveLen) * speed * deltaTime
        local ny = pos.y + (self.moveY / moveLen) * speed * deltaTime
        local cx = self.arenaCx
        local cy = self.arenaCy
        local hx = self.arenaHalfX
        local hy = self.arenaHalfY
        pos = Vector3(clamp(nx, cx - hx, cx + hx), clamp(ny, cy - hy, cy + hy), pos.z)
        TransformBus.Event.SetWorldTranslation(self.entityId, pos)
    end

    -- Aim: when the player aims, set the TARGET facing; the sprite then eases
    -- toward it (below) instead of snapping, so a fast mouse flick does not
    -- instantly spin the sprite. Mouse deltas arrive as one-shot per-frame values
    -- (and are normalized, so their magnitude does not affect turn speed); gamepad
    -- stick values are re-sent each tick.
    -- The right stick gives an ABSOLUTE aim direction (its position IS where you
    -- aim, resent each tick), so point-to-aim works directly. The mouse gives only
    -- per-frame DELTAS (O3DE exposes no cursor position or screen unproject to game
    -- scripts), so we STEER the aim by nudging the current direction and renormalizing
    -- -- proportional and persistent, instead of snapping to the last flick. The stick
    -- wins whenever it is engaged; otherwise the mouse steers.
    local stickLen = math.sqrt(self.aimX * self.aimX + self.aimY * self.aimY)
    if stickLen > self.aimDeadZone then
        self.aimDir = Vector3(self.aimX / stickLen, self.aimY / stickLen, 0.0)
        self.aimYaw = atan2(self.aimDir.y, self.aimDir.x)
    elseif self.mouseDX ~= 0.0 or self.mouseDY ~= 0.0 then
        local nx = self.aimDir.x + self.mouseDX * self.mouseSens
        local ny = self.aimDir.y + self.mouseDY * self.mouseSens
        local l = math.sqrt(nx * nx + ny * ny)
        if l > 0.0001 then
            self.aimDir = Vector3(nx / l, ny / l, 0.0)
            self.aimYaw = atan2(ny, nx)
        end
    end
    -- Stick values are resent each tick and mouse deltas are per-frame; clear both so
    -- a centered stick / still mouse stops changing the aim.
    self.aimX = 0.0
    self.aimY = 0.0
    self.mouseDX = 0.0
    self.mouseDY = 0.0

    -- Obi's body stays upright (no spin): just flip him to face the aim horizontally.
    -- (The rotate-to-aim version is in git history if we want it back.)
    local faceLeft = self.aimDir.x < 0.0
    if faceLeft ~= self.facingLeft then
        self.facingLeft = faceLeft
        DioramaSpriteRequestBus.Event.SetFlip(self.entityId, faceLeft, false)
    end

    -- Firing: spawn a projectile toward the aim direction, rate-limited by the
    -- cooldown. Holding fire auto-fires at the cooldown rate.
    if self.fireTimer > 0.0 then
        self.fireTimer = self.fireTimer - deltaTime
    end
    if self.firing and self.fireTimer <= 0.0 then
        self:FireProjectile()
        self.fireTimer = self.fireCooldown
    end

    -- Octopus mood: flush from calm to alarm color by how close the nearest target
    -- is. Read positions from the shared cache the targets publish (no extra bus
    -- calls); nearestSq starts at threatRange^2 so "no target in range" = fully calm.
    local epos = rawget(_G, "TwinStickEnemyPos")
    local nearestSq = self.threatRange * self.threatRange
    if epos ~= nil then
        for _, p in pairs(epos) do
            local dx = p.x - pos.x
            local dy = p.y - pos.y
            local d2 = dx * dx + dy * dy
            if d2 < nearestSq then nearestSq = d2 end
        end
    end
    local threat = 1.0 - math.sqrt(nearestSq) / self.threatRange
    if threat < 0.0 then threat = 0.0 end
    DioramaSpriteRequestBus.Event.SetTint(self.entityId,
        self.calmR + (self.alarmR - self.calmR) * threat,
        self.calmG + (self.alarmG - self.calmG) * threat,
        self.calmB + (self.alarmB - self.calmB) * threat, 1.0)
end

return TwinStickPlayer
