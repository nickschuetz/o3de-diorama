----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Projectile for the Diorama twin-stick sample game.
--
-- The player spawns this prefab facing the aim direction (a yaw rotation about
-- Z). On activate it launches itself along its forward axis through PhysX, lives
-- for a short time, and destroys any enemy it touches (and itself) on collision.
-- Standard O3DE: PhysX collision notifications, tag lookup, and game-entity
-- destruction; Diorama only supplies the visual.

local TwinStickProjectile = {
    Properties = {
        Speed = { default = 20.0, description = "Projectile speed in world units per second" },
        Lifetime = { default = 2.0, description = "Seconds before the projectile despawns" },
    },
}

function TwinStickProjectile:OnActivate()
    self.age = 0.0
    self.enemyTag = Crc32("Enemy")
    self.gameTag = Crc32("Game")

    -- Launch along the entity's forward (local +X), which the spawner aimed by
    -- setting the spawn yaw.
    local tm = TransformBus.Event.GetWorldTM(self.entityId)
    local forward = tm:GetBasisX()
    local speed = self.Properties.Speed
    RigidBodyRequestBus.Event.SetLinearVelocity(self.entityId, Vector3(forward.x * speed, forward.y * speed, 0.0))

    self.tickHandler = TickBus.Connect(self)
    self.collisionHandler = CollisionNotificationBus.Connect(self, self.entityId)
end

function TwinStickProjectile:OnDeactivate()
    if self.tickHandler then
        self.tickHandler:Disconnect()
    end
    if self.collisionHandler then
        self.collisionHandler:Disconnect()
    end
end

function TwinStickProjectile:Destroy()
    GameEntityContextRequestBus.Broadcast.DestroyGameEntityAndDescendants(self.entityId)
end

function TwinStickProjectile:OnTick(deltaTime, scriptTime)
    self.age = self.age + deltaTime
    if self.age >= self.Properties.Lifetime then
        self:Destroy()
    end
end

function TwinStickProjectile:OnCollisionBegin(otherEntityId, contacts)
    if otherEntityId == nil or not otherEntityId:IsValid() then
        return
    end
    -- Only react to enemies; ignore walls and the player.
    local isEnemy = TagComponentRequestBus.Event.HasTag(otherEntityId, self.enemyTag)
    if isEnemy then
        -- Award score: tell the "Game"-tagged controller an enemy was killed.
        -- The event reflects to scripting as "Get Entity By Tag"; O3DE's Lua binding
        -- strips spaces, so the callable name is GetEntityByTag (returns one entity).
        local game = TagGlobalRequestBus.Event.GetEntityByTag(self.gameTag)
        if game ~= nil and game:IsValid() then
            GameplayNotificationBus.Event.OnEventBegin(GameplayNotificationId(game, "EnemyKilled", "float"), 1.0)
        end
        GameEntityContextRequestBus.Broadcast.DestroyGameEntityAndDescendants(otherEntityId)
        self:Destroy()
    end
end

return TwinStickProjectile
