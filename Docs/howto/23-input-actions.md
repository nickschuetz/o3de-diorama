# How-To: Input Action Mapping

Wiring raw input channels straight into gameplay ("if the W key is down, move up")
works until you want to rebind keys, support a gamepad, or read the same intent
from two places. The **2D Input Actions** component gives you a rebindable layer:
you define named **actions** ("move", "jump", "fire") and bind each to one or more
input channels, then your scripts read the actions by name over a typed bus,
exactly like every other Diorama feature. Rebinding is editing the bindings, not
the gameplay code.

## Build it

A one-command scene builder assembles this guide's demo in its own level
(an input-driven character whose state machine switches clips from the action map); finish any wiring the editor cannot script (noted in its output), then
enter game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/anim_input_demo.py
```

## The pieces

- An **action** has a name, a **kind**, and a list of **bindings**:
  - **Button** -- digital press (0..1), pressed past a threshold (`jump`, `fire`).
  - **Axis 1D** -- one signed axis in [-1,1] (`throttle`, `look_x`).
  - **Axis 2D** -- a 2D vector, x/y each in [-1,1], with a radial dead zone
    (`move`, `aim`).
- A **binding** names an input **channel**, a **scale** (use `-1` for the negative
  side of an axis), and, for a 2D action, which **axis** (X or Y) it feeds.

Channel names are the standard O3DE ones, for example:

| Input | Channel name |
| ----- | ------------ |
| W / A / S / D | `keyboard_key_alphanumeric_W` (etc.) |
| Arrow keys | `keyboard_key_navigation_arrow_up` (etc.) |
| Space | `keyboard_key_edit_space` |
| Gamepad A | `gamepad_button_a` |
| Left stick | `gamepad_thumbstick_l_x`, `gamepad_thumbstick_l_y` |

## Set up move + jump

1. Add a **2D Input Actions** component (`Diorama` category) to your player (or a
   dedicated input entity).
2. Add a `move` action, kind **Axis 2D**, dead zone `0.2`, with bindings:
   - `keyboard_key_alphanumeric_D`, scale `+1`, axis **X**
   - `keyboard_key_alphanumeric_A`, scale `-1`, axis **X**
   - `keyboard_key_alphanumeric_W`, scale `+1`, axis **Y**
   - `keyboard_key_alphanumeric_S`, scale `-1`, axis **Y**
   - `gamepad_thumbstick_l_x`, scale `+1`, axis **X**
   - `gamepad_thumbstick_l_y`, scale `+1`, axis **Y**
3. Add a `jump` action, kind **Button**, with bindings:
   - `keyboard_key_edit_space`, scale `+1`
   - `gamepad_button_a`, scale `+1`

That is the whole rebinding surface: to remap jump to another key, change the
binding; the gameplay code below never changes.

## Read it from a script

Actions are read through `DioramaInputRequestBus` (Lua / Python / Script Canvas),
addressed by the component's entity:

```lua
local input = DioramaInputRequestBus.Event
local id = self.entityId

-- Axis 2D: a movement vector.
local moveX = input.GetValue(id, "move")    -- X
local moveY = input.GetValueY(id, "move")   -- Y

-- Button: held vs. just-pressed (for a jump you only want the edge).
if input.WasPressedThisFrame(id, "jump") then
    -- start a jump
end
if input.IsPressed(id, "fire") then
    -- auto-fire while held
end
```

To react to presses without polling, connect to `DioramaInputNotificationBus` and
handle `OnActionPressed(action)` / `OnActionReleased(action)`.

This pairs naturally with the [Animation State Machine](22-anim-state-machine.md):
feed `move` magnitude into its `speed` parameter and pulse its `jump` trigger from
`WasPressedThisFrame`, and your character animates itself.

## Notes

- **Dead zone is per action.** A 1D axis rescales so the usable range still reaches
  1 past the dead zone; a 2D axis uses a radial dead zone so diagonals are not
  clipped.
- **Press threshold** decides when an axis-as-button counts as pressed (default
  `0.5`), which is what drives `IsPressed` / the edges for axis actions.
- **It never consumes input.** Other listeners (UI, debug) still receive the same
  events, so adding this component does not steal input from the rest of the game.
