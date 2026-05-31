----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Chase AI for a twin-stick enemy in the Diorama sample game.
--
-- Each tick the enemy finds the player (the entity tagged "Player") and steers
-- straight toward it in the XY plane through PhysX velocity, so it collides with
-- walls and (later) projectiles. Like the player, it faces its movement
-- direction by flipping its Diorama sprite through DioramaSpriteRequestBus.
--
-- This is intentionally simple "pursue" steering; it is the hook where smarter
-- behaviors (flanking, separation between enemies) would go later.

local TwinStickEnemy = {
    Properties = {
        MoveSpeed = { default = 4.0, description = "Chase speed in world units per second" },
    },
}

function TwinStickEnemy:OnActivate()
    self.facingLeft = false
    -- The global tag bus is addressed by the tag's Crc32; "Player" matches the
    -- tag set on the player entity.
    self.playerTag = Crc32("Player")
    self.tickHandler = TickBus.Connect(self)
end

function TwinStickEnemy:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
    end
end

function TwinStickEnemy:OnTick(deltaTime, scriptTime)
    -- The "get entity by tag" event reflects to scripting as "Get Entity By Tag";
    -- O3DE's Lua binding strips the spaces, so the callable name is GetEntityByTag.
    -- It is addressed by the tag's Crc32 and returns the (single) tagged entity.
    local player = TagGlobalRequestBus.Event.GetEntityByTag(self.playerTag)
    if player == nil or not player:IsValid() then
        return
    end

    local target = TransformBus.Event.GetWorldTranslation(player)
    local mine = TransformBus.Event.GetWorldTranslation(self.entityId)
    local dx = target.x - mine.x
    local dy = target.y - mine.y
    local dist = math.sqrt(dx * dx + dy * dy)
    if dist < 0.01 then
        return
    end

    local speed = self.Properties.MoveSpeed
    RigidBodyRequestBus.Event.SetLinearVelocity(self.entityId, Vector3((dx / dist) * speed, (dy / dist) * speed, 0.0))

    local faceLeft = dx < 0.0
    if faceLeft ~= self.facingLeft then
        self.facingLeft = faceLeft
        DioramaSpriteRequestBus.Event.SetFlip(self.entityId, faceLeft, false)
    end
end

return TwinStickEnemy
