-- 2.5D brawler enemy AI: walk toward the player on the floor (in X and in depth),
-- lining up on the player's lane so its attack can connect. Put this on an enemy
-- entity that has a 2.5D Depth Body component; point Target at the player.
--
-- It shows depth-lane navigation: the enemy closes the depth gap with MoveDepthToward
-- (so it ends up on the player's row) while walking in X, the core movement of a
-- beat-em-up grunt.

local BrawlerEnemy = {
    Properties = {
        Target = { default = EntityId(), description = "The player to chase." },
        MoveSpeed = { default = 2.5, description = "Left/right approach speed." },
        DepthSpeed = { default = 2.0, description = "Lane-change speed." },
        StopDistance = { default = 1.0, description = "How close in X it stops to attack." },
    },
}

function BrawlerEnemy:OnActivate()
    self.tickHandler = TickBus.Connect(self)
end

function BrawlerEnemy:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function BrawlerEnemy:OnTick(deltaTime, timePoint)
    local target = self.Properties.Target
    if not target:IsValid() then
        return
    end
    local id = self.entityId

    -- Match the player's depth lane so attacks can land.
    local targetDepth = DioramaDepthBodyRequestBus.Event.GetDepth(target)
    DioramaDepthBodyRequestBus.Event.MoveDepthToward(id, targetDepth, self.Properties.DepthSpeed * deltaTime)

    -- Close the horizontal gap until within StopDistance.
    local myPos = TransformBus.Event.GetWorldTranslation(id)
    local targetPos = TransformBus.Event.GetWorldTranslation(target)
    local dx = targetPos:GetX() - myPos:GetX()
    if math.abs(dx) > self.Properties.StopDistance then
        local dir = dx > 0.0 and 1.0 or -1.0
        myPos:SetX(myPos:GetX() + dir * self.Properties.MoveSpeed * deltaTime)
        TransformBus.Event.SetWorldTranslation(id, myPos)
        DioramaSpriteRequestBus.Event.SetFlip(id, dir < 0.0, false)
    else
        -- In range and (after MoveDepthToward) on the lane: a real game triggers the
        -- attack animation here, whose frame-data hitbox then strikes the player.
    end
end

return BrawlerEnemy
