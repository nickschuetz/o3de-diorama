-- 2.5D brawler player: move left/right (X) and toward/away from the camera (depth),
-- and land attacks only on enemies sharing the depth lane (the "you must line up"
-- rule of a beat-em-up). Put this on the player entity, which also has:
--   * a 2.5D Depth Body component (owns the depth lane + sprite lift/sort),
--   * a 2D Frame-Data Hitboxes component (the attack hitbox), and
--   * a 2D Input Actions component with Axis1D actions "moveX" and "moveDepth".
--
-- Depth-aware collision here is done by gating the hitbox's OnHit on the lane gap.
-- (The other option is to put the hitboxes on the XZ floor plane, where the 2D
-- overlap is depth-aware automatically; see how-to 26.)

local BrawlerPlayer = {
    Properties = {
        MoveSpeed = { default = 5.0, description = "Left/right speed (world units/sec)." },
        DepthSpeed = { default = 4.0, description = "Toward/away speed (depth units/sec)." },
        LaneTolerance = { default = 0.6, description = "Max depth gap for a hit to land." },
        Damage = { default = 10.0 },
    },
}

function BrawlerPlayer:OnActivate()
    self.tickHandler = TickBus.Connect(self)
    self.hitboxHandler = DioramaHitboxNotificationBus.Connect(self, self.entityId)
end

function BrawlerPlayer:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
    if self.hitboxHandler ~= nil then
        self.hitboxHandler:Disconnect()
        self.hitboxHandler = nil
    end
end

function BrawlerPlayer:OnTick(deltaTime, timePoint)
    local id = self.entityId

    -- Left/right on the floor: move the transform X.
    local moveX = DioramaInputRequestBus.Event.GetValue(id, "moveX")
    if moveX ~= 0.0 then
        local pos = TransformBus.Event.GetWorldTranslation(id)
        pos:SetX(pos:GetX() + moveX * self.Properties.MoveSpeed * deltaTime)
        TransformBus.Event.SetWorldTranslation(id, pos)
        -- Face the way we move (flip the sprite).
        DioramaSpriteRequestBus.Event.SetFlip(id, moveX < 0.0, false)
    end

    -- Toward/away from the camera: change the depth lane. The depth body lifts and
    -- re-sorts the sprite, so the character reads as moving into / out of the scene.
    local moveDepth = DioramaInputRequestBus.Event.GetValue(id, "moveDepth")
    if moveDepth ~= 0.0 then
        local d = DioramaDepthBodyRequestBus.Event.GetDepth(id)
        DioramaDepthBodyRequestBus.Event.SetDepth(id, d + moveDepth * self.Properties.DepthSpeed * deltaTime)
    end
end

-- The player's active hitbox overlapped `target`. Land the hit only when they share
-- the depth lane: a punch connects with an enemy on your row, not one in the
-- background or foreground.
function BrawlerPlayer:OnHit(target)
    local myDepth = DioramaDepthBodyRequestBus.Event.GetDepth(self.entityId)
    local targetDepth = DioramaDepthBodyRequestBus.Event.GetDepth(target)
    if math.abs(myDepth - targetDepth) <= self.Properties.LaneTolerance then
        Debug.Log("Hit " .. tostring(target) .. " on the same lane for " .. tostring(self.Properties.Damage))
        -- A real game subtracts Damage from the target's health here.
    else
        Debug.Log("Whiffed: target is on a different depth lane")
    end
end

return BrawlerPlayer
