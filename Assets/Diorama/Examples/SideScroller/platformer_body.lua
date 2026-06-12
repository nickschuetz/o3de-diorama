-- Side-scroller platformer body: gravity, ground-follow over flat ground and ramps,
-- wall + one-way-platform resolution, and jumping, all over the Diorama 2D collision
-- world. Put this on the player entity. It expects:
--   * a Diorama 2D Collider on the GroundLayer (the body box, used for push-out),
--   * a 2D Input Actions component with an Axis1D action "moveX" and a Button "jump",
--   * the floor and ramps registered with AddGroundSegment (see platformer_ground.lua),
--   * solid walls / one-way platforms as 2D Collider entities on the GroundLayer
--     (check "One Way" on the collider for drop-through platforms).
-- The body moves in the world XY plane (X right, Y up).

local PlatformerBody = {
    Properties = {
        MoveSpeed = { default = 6.0, description = "Horizontal speed (world units/sec)." },
        JumpSpeed = { default = 12.0, description = "Upward speed on jump." },
        Gravity = { default = -30.0, description = "Downward acceleration." },
        HalfWidth = { default = 0.4 },
        HalfHeight = { default = 0.5 },
        GroundLayer = { default = 1, description = "Collision layer mask of walls / platforms to push out of." },
        StepUp = { default = 0.4, description = "Max ledge / ramp rise the feet snap up to per frame." },
        MaxDrop = { default = 0.5, description = "Max fall the feet snap down to (falls off ledges past this)." },
    },
}

function PlatformerBody:OnActivate()
    self.vy = 0.0
    self.onGround = false
    self.tickHandler = TickBus.Connect(self)
end

function PlatformerBody:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function PlatformerBody:OnTick(deltaTime, timePoint)
    local id = self.entityId
    local pos = TransformBus.Event.GetWorldTranslation(id)
    -- Read the Vector3/Vector2 by property (.x/.y); :GetX() is nil in this engine's Lua.
    local x = pos.x
    local y = pos.y

    -- Horizontal movement from input.
    local moveX = DioramaInputRequestBus.Event.GetValue(id, "moveX")
    x = x + moveX * self.Properties.MoveSpeed * deltaTime

    -- Gravity and vertical integration.
    self.vy = self.vy + self.Properties.Gravity * deltaTime
    y = y + self.vy * deltaTime

    -- Ground-follow over flat ground and ramps: probe at the feet and, while falling,
    -- snap the feet onto the surface within reach.
    self.onGround = false
    local footY = y - self.Properties.HalfHeight
    local probe = Diorama2DCollisionRequestBus.Broadcast.ProbeGroundY(x, footY, self.Properties.MaxDrop, self.Properties.StepUp)
    if probe.onGround and self.vy <= 0.0 then
        y = probe.groundY + self.Properties.HalfHeight
        self.vy = 0.0
        self.onGround = true
    end

    -- Walls and one-way platforms: push the body box out of solid colliders. One-way
    -- colliders only push up (landing on top), so the body drops through from below.
    local push = Diorama2DCollisionRequestBus.Broadcast.ComputeBoxPushOut(
        x, y, self.Properties.HalfWidth, self.Properties.HalfHeight, self.Properties.GroundLayer, id)
    x = x + push.x
    y = y + push.y
    if push.y > 0.0 and self.vy < 0.0 then
        self.vy = 0.0
        self.onGround = true
    end

    -- Jump.
    if self.onGround and DioramaInputRequestBus.Event.WasPressedThisFrame(id, "jump") then
        self.vy = self.Properties.JumpSpeed
        self.onGround = false
    end

    TransformBus.Event.SetWorldTranslation(id, Vector3(x, y, pos.z))
end

return PlatformerBody
