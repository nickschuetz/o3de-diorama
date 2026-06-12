# How-To: Day/Night Cycle

A day/night cycle animates the scene's lighting over a time-of-day clock: a warm low sun
at dawn, bright white overhead at noon, deep blue and dim at midnight. Diorama ships it as
a thin complementary layer over the existing [2D lighting](07-lighting.md): the **Day/Night
Cycle** component advances a normalized clock and drives a target Diorama light's color,
intensity, and direction. It creates no new light and owns no new rendering.

## Setup

Drop a **2D Light** component (set its type to **Directional**, the sun) and a **Day/Night
Cycle** component on the same entity. That is the whole setup: the cycle drives the light
on its own entity by default.

Key config fields:

- **Time Of Day** (0..1): start time. 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
- **Cycle Seconds**: real seconds for a full 24-hour cycle (0 freezes the clock so you can
  set the time of day by hand).
- **Night / Dawn / Day / Dusk Color + Intensity**: the four phase keys the cycle
  interpolates between. The sun's elevation (height) is fixed per phase: below the horizon
  at midnight, low at dawn/dusk, high at noon.
- **Drive Color / Intensity / Direction**: turn off any aspect the cycle should leave alone
  (for example, drive only the color and aim the sun yourself).
- **Target Light**: leave empty to drive this entity's light, or point it at another entity
  that has a Diorama light.

## Driving the clock from script / an agent

Every knob is on `DioramaDayNightRequestBus`, addressed by the cycle entity's id, so a Lua
script, Python, Script Canvas, or an AI agent drives the exact same API the Inspector does:

```lua
-- Pause, scrub, and fast-forward the cycle.
DioramaDayNightRequestBus.Event.SetPaused(cycleEntity, true)
DioramaDayNightRequestBus.Event.SetTimeOfDay(cycleEntity, 0.5)   -- jump to noon
DioramaDayNightRequestBus.Event.StepHours(cycleEntity, 3.0)      -- advance 3 in-game hours
DioramaDayNightRequestBus.Event.SetCycleSeconds(cycleEntity, 8)  -- a fast 8s day

local info = DioramaDayNightRequestBus.Event.GetDayNightInfo(cycleEntity)
-- info.timeOfDay, info.sunIntensity, info.elevationDegrees, info.isDaytime, info.paused
```

`GetDayNightInfo` is the verify-loop payload: an agent confirms the time of day and the
resolved sun color / intensity / elevation without a viewport.

## One-command demo

`Docs/examples/daynight_demo.py` builds a small lit scene in its own level (a directional
sun with a Day/Night Cycle on a fast 12-second day, a ground strip and props it lights, and
two lamps) so you can watch the day pass in game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/daynight_demo.py
```

## Reacting to day and night

Game logic often keys off the cycle rather than the light: street lamps switch on at dusk,
enemies spawn at night, shops close. Poll `GetDayNightInfo().isDaytime` (it flips when the
sun crosses the horizon) and act on the transition. The runnable sample has two scripts:

- `Assets/Diorama/Examples/DayNight/daynight_controller.lua` on the sun entity scrubs and
  pauses the cycle from input and logs each sunrise / sunset.
- `Assets/Diorama/Examples/DayNight/daynight_lamp.lua` on a lamp entity reads the sun's
  cycle (cross-entity) and turns its own point light on at night, off in the day.

## Scope

The cycle drives the lights that modulate Diorama sprites; it is not a sky / atmosphere
system (Atom owns skyboxes and physical sky). Driving more than one light (a separate fill
or moon light) is a target you can point a second cycle at, or a follow-up that takes a
light list. Time-of-day gameplay (schedules, spawns) lives in game script over the bus.
