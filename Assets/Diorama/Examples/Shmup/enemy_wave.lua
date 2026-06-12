-- Shmup enemy: descend from the top of the play field, and when it falls past the bottom,
-- wrap back to the top at a new X. Put this on each enemy entity (Sprite + 2D Collider on
-- the ENEMY layer). A fixed pool of these gives endless waves with no spawnables: the
-- player's bolts "kill" an enemy by recycling it to the top (see player_ship.lua), and an
-- enemy that survives all the way down simply wraps around.
--
-- Property reads use `or` fallbacks (O3DE can leave self.Properties nil after a script's
-- declared properties change), and the transform is read by property (pos.y), since
-- pos:GetY() is nil in this engine's Lua.

local EnemyWave = {
    Properties = {
        DescendSpeed = { default = 2.5, description = "Downward speed (world units/sec)." },
        TopY = { default = 7.0, description = "Y to wrap to at the top." },
        BottomY = { default = -7.0, description = "Y below which the enemy wraps." },
        SpanX = { default = 7.0, description = "Half the horizontal spread of wrap positions." },
    },
}

function EnemyWave:OnActivate()
    local p = self.Properties
    self.speed = p.DescendSpeed or 2.5
    self.topY = p.TopY or 7.0
    self.bottomY = p.BottomY or -7.0
    self.spanX = p.SpanX or 7.0
    self.tickHandler = TickBus.Connect(self)
end

function EnemyWave:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function EnemyWave:OnTick(deltaTime, timePoint)
    local id = self.entityId

    -- One-shot: size the hitbox to the sprite so a bigger enemy is a bigger target.
    -- Done on the first tick, not OnActivate: a sibling component's request-bus handler
    -- may not be connected yet during OnActivate, so SetBox there can be silently lost.
    if not self.sized then
        self.sized = true
        local info = DioramaSpriteRequestBus.Event.GetSpriteInfo(id)
        if info ~= nil and info.width > 0.0 then
            Diorama2DColliderRequestBus.Event.SetBox(id, info.width * 0.45, info.height * 0.45)
        end
    end

    -- Freeze the wave on game over: the player script sets the shared ShmupRunning global.
    if ShmupRunning == false then
        return
    end
    local pos = TransformBus.Event.GetWorldTranslation(id)
    local ny = pos.y - self.speed * deltaTime
    local nx = pos.x
    if ny < self.bottomY then
        ny = self.topY
        nx = (math.random() * 2.0 - 1.0) * self.spanX
    end
    TransformBus.Event.SetWorldTranslation(id, Vector3(nx, ny, pos.z))
end

return EnemyWave
