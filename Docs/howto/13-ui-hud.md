# How-To: 2D UI / HUD

Build a screen-space HUD -- text labels, bars/gauges, and solid panels -- that is
drivable from a script, Script Canvas, or an AI agent through one typed bus, exactly
the way sprites are. This is Diorama's UI **parity**: a HUD is a first-class gem
citizen, not a separate UI stack.

## Elements

Add a **2D UI Element** component (`Diorama` category) to an entity and pick a **Kind**:

- **Text** -- a label drawn with the engine font (`Text`, `Font Size`, `Color`).
- **Bar / Gauge** -- a background track plus a value-scaled fill (`Size`, `Value`
  0..1, `Color` = fill, `Background Color` = track). Health, cooldowns, progress.
- **Panel** -- a single solid-color quad (`Size`, `Color`). Backgrounds and frames.

Every element is anchored to a screen corner/edge (`Anchor`) with a pixel `Offset`,
laid out in a virtual reference resolution (default 1280x720) and scaled to the real
window, so a HUD holds its layout at any size. Text draws through O3DE's font
interface; bars and panels draw as screen-space quads (AuxGeom) with no extra render
pass.

## Drive it from script

The whole point: the HUD updates through `DioramaUIRequestBus`, addressed by the
element's entity id, with the same plain, clamped verbs an agent would call.

```lua
-- Set a score label and a health bar each from their own entity.
DioramaUIRequestBus.Event.SetText(scoreEntity, "Score: " .. tostring(score))
DioramaUIRequestBus.Event.SetValue(healthEntity, hp / maxHp) -- 0..1, clamped
```

The runnable example [`hud_bar_pulse.lua`](../../Assets/Diorama/Examples/UI/hud_bar_pulse.lua)
attaches to a Bar element and oscillates its fill via `SetValue`, the way gameplay
would as health changes:

```lua
function HudBarPulse:OnTick(deltaTime, scriptTime)
    self.t = self.t + deltaTime * self.speed
    local value = self.min + (self.max - self.min) * (0.5 + 0.5 * math.sin(self.t))
    DioramaUIRequestBus.Event.SetValue(self.entityId, value)
end
```

Full verb list: `SetText`, `SetFontSize`, `SetColor`, `SetAnchor`, `SetOffset`,
`SetSize`, `SetValue`, `SetBackgroundColor`, `SetVisible`, and `GetUIInfo` (resolved
state for verification without a screenshot). All are reflected `Common`, so editor
Python, launcher Lua, and Script Canvas drive them identically.

## Build a HUD in script

[`ui_hud_demo.py`](../examples/ui_hud_demo.py) builds a title label, a score label, a
health bar, and a corner panel into their own level, then leaves the bar pulsing via
the script above.

## Porting a score HUD off LyShine

If you have an existing HUD built on a LyShine canvas (say a score counter), you
can move it onto Diorama's UI so the whole game renders through one gem (the
parity proof end to end):

1. Add an entity with a **2D UI Element** (Kind **Text**, anchored Top-Left) for the
   counter, and optionally a **Bar** for a health/cooldown gauge.
2. In your gameplay script, replace the LyShine text update with
   `DioramaUIRequestBus.Event.SetText(counterEntity, "Score: " .. count)`.
3. Remove the LyShine canvas component.

The score then renders through the same gem bus as everything else, with no UI gem
in the loop.
