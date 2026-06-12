-- Day/night controller: drive and observe the time-of-day cycle. Put this on the sun
-- entity, which also has:
--   * a 2D Light component (Directional), and
--   * a Day/Night Cycle component (drives that light over the day).
--
-- The cycle advances on its own; this script just shows the runtime knobs (all also on
-- the DioramaDayNightRequestBus for an agent) and logs each day<->night transition by
-- polling GetDayNightInfo. Wire a 2D Input Actions component with the digital actions
-- "pause", "fastForward", and "stepHours" to scrub time by hand.

local DayNightController = {
    Properties = {
        FastCycleSeconds = { default = 8.0, description = "Cycle length while fast-forwarding." },
        NormalCycleSeconds = { default = 120.0, description = "Cycle length otherwise." },
        StepHours = { default = 3.0, description = "Hours to jump on a step input." },
    },
}

function DayNightController:OnActivate()
    self.tickHandler = TickBus.Connect(self)
    -- Seed the "was it daytime last frame" latch from the current state.
    local info = DioramaDayNightRequestBus.Event.GetDayNightInfo(self.entityId)
    self.wasDaytime = info.isDaytime
    DioramaDayNightRequestBus.Event.SetCycleSeconds(self.entityId, self.Properties.NormalCycleSeconds)
end

function DayNightController:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function DayNightController:OnTick(deltaTime, timePoint)
    local id = self.entityId

    -- Hold "fastForward" to run the day quickly; release to return to normal speed.
    -- (Input actions come from a 2D Input Actions component on this same entity.)
    local fast = DioramaInputRequestBus.Event.IsPressed(id, "fastForward")
    DioramaDayNightRequestBus.Event.SetCycleSeconds(
        id, fast and self.Properties.FastCycleSeconds or self.Properties.NormalCycleSeconds)

    if DioramaInputRequestBus.Event.WasPressedThisFrame(id, "pause") then
        local info = DioramaDayNightRequestBus.Event.GetDayNightInfo(id)
        DioramaDayNightRequestBus.Event.SetPaused(id, not info.paused)
    end
    if DioramaInputRequestBus.Event.WasPressedThisFrame(id, "stepHours") then
        DioramaDayNightRequestBus.Event.StepHours(id, self.Properties.StepHours)
    end

    -- Log the moment the sun crosses the horizon either way.
    local info = DioramaDayNightRequestBus.Event.GetDayNightInfo(id)
    if info.isDaytime ~= self.wasDaytime then
        Debug.Log(info.isDaytime and "Sunrise: day has begun." or "Sunset: night has fallen.")
        self.wasDaytime = info.isDaytime
    end
end

return DayNightController
