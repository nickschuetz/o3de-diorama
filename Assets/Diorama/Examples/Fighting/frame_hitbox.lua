-- Frame-driven hitbox: a fighting-game attack whose hitbox is active only on
-- specific animation frames. Put this on a character entity that has a Sprite (or
-- Aseprite) component playing an attack, and point "Hitbox" at a child entity that
-- has a Diorama 2D Collider (set to the "hit" layer). The collider is enabled and
-- positioned only while the animation is in the active-frame window, exactly how a
-- fighter authors "startup / active / recovery" frames.
--
-- It uses the gem's animation frame events (DioramaSpriteNotificationBus.OnAnimationFrame),
-- so the timing is frame-exact and works for both sprite-sheet and Aseprite playback.

local FrameHitbox = {
    Properties = {
        Hitbox = { default = EntityId(), description = "Child entity with a 2D Collider on the hit layer." },
        ActiveFrom = { default = 5, description = "First animation frame the hitbox is live." },
        ActiveTo = { default = 8, description = "Last animation frame the hitbox is live." },
        HalfWidth = { default = 0.4 },
        HalfHeight = { default = 0.3 },
        OffsetX = { default = 0.6, description = "Hitbox offset in front of the character (local X)." },
        OffsetZ = { default = 0.0 },
    },
}

function FrameHitbox:OnActivate()
    -- Listen for frame changes on this entity's sprite/aseprite animation.
    self.notifyHandler = DioramaSpriteNotificationBus.Connect(self, self.entityId)
    self:SetHitboxActive(false)
end

function FrameHitbox:OnDeactivate()
    if self.notifyHandler ~= nil then
        self.notifyHandler:Disconnect()
        self.notifyHandler = nil
    end
end

function FrameHitbox:SetHitboxActive(active)
    local hitbox = self.Properties.Hitbox
    if not hitbox:IsValid() then
        return
    end
    if active then
        Diorama2DColliderRequestBus.Event.SetBox(hitbox, self.Properties.HalfWidth, self.Properties.HalfHeight)
        Diorama2DColliderRequestBus.Event.SetOffset(hitbox, self.Properties.OffsetX, self.Properties.OffsetZ)
    end
    Diorama2DColliderRequestBus.Event.SetEnabled(hitbox, active)
end

-- Frame-exact: the hitbox is live only inside [ActiveFrom, ActiveTo].
function FrameHitbox:OnAnimationFrame(frameIndex)
    local live = frameIndex >= self.Properties.ActiveFrom and frameIndex <= self.Properties.ActiveTo
    self:SetHitboxActive(live)
end

-- Safety: clear the hitbox when the attack clip ends.
function FrameHitbox:OnAnimationFinished()
    self:SetHitboxActive(false)
end

return FrameHitbox
