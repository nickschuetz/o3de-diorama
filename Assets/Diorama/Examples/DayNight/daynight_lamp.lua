-- Day/night lamp: a light that switches on at night and off during the day, by reading
-- the day/night cycle. Put this on a lamp entity, which also has:
--   * a 2D Light component (Point) for the lamp glow.
-- Set the CycleEntity property to the sun entity that carries the Day/Night Cycle.
--
-- This shows the verify-loop the other way around: instead of the cycle driving a light,
-- a game script polls GetDayNightInfo (cross-entity) and reacts. The lamp's own light is
-- toggled through DioramaLightRequestBus on this entity.

local DayNightLamp = {
    Properties = {
        CycleEntity = { default = EntityId(), description = "Entity carrying the Day/Night Cycle." },
        OnIntensity = { default = 1.5, description = "Lamp brightness at night." },
    },
}

function DayNightLamp:OnActivate()
    self.tickHandler = TickBus.Connect(self)
    self.lit = nil -- unknown until the first tick, so the first state change always applies.
end

function DayNightLamp:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function DayNightLamp:OnTick(deltaTime, timePoint)
    if not self.Properties.CycleEntity:IsValid() then
        return
    end

    local info = DioramaDayNightRequestBus.Event.GetDayNightInfo(self.Properties.CycleEntity)
    local shouldLight = not info.isDaytime
    if shouldLight ~= self.lit then
        -- Off in the day, on at night (a lamp earns its keep after sunset).
        DioramaLightRequestBus.Event.SetIntensity(self.entityId, shouldLight and self.Properties.OnIntensity or 0.0)
        self.lit = shouldLight
    end
end

return DayNightLamp
