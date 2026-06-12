# How-To: Animation State Machine

You already know how to play one clip: a sprite sheet on the **Sprite**, a named
tag on the **Aseprite** component. The **2D Animation State Machine** is the layer
above that: it decides *which* clip plays right now from game state. Push a few
parameters (a `speed`, a `grounded` flag, a `jump` trigger) and the graph switches
states for you, the same way Unity's Animator or Godot's AnimationTree does, so you
stop hand-rolling clip-switching `if`/`else` in Lua.

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

- **Parameters** are the inputs your game sets. Three kinds:
  - **Bool** -- a latched flag (`grounded`).
  - **Float** -- a number you compare (`speed`).
  - **Trigger** -- a one-shot pulse (`jump`): set it, it fires one transition, then
    clears itself.
- **States** are the clips. Each state plays either an **Aseprite tag** or a
  **sprite sheet** (columns / rows / frame count / fps / loop) on the target entity.
- **Transitions** are the arrows between states, each guarded by **conditions** on
  your parameters (all must be true), with an optional **exit time** (wait until the
  current clip is this far along before switching).

## Build a walk/jump graph

1. Put a **Sprite** (or **Aseprite**) component on your character so there is
   something to animate. Add a **2D Animation State Machine** component
   (`Diorama` category) to the same entity. Leave **Target entity** empty to drive
   the sprite on this entity, or set it to drive a child.
2. Add **Parameters**:
   - `speed` (Float)
   - `grounded` (Bool, default on)
   - `jump` (Trigger)
3. Add **States**, each with its clip. For a sprite sheet, set columns/rows/frame
   count/fps/loop; for Aseprite, just set the **Aseprite tag**:
   - `idle` -- the idle clip, looping.
   - `run` -- the run clip, looping.
   - `jump` -- the jump clip, not looping.
   Set **Default state** to `idle` (empty = the first state).
4. Add **Transitions**:
   - `idle -> run` when `speed > 0.1`.
   - `run -> idle` when `speed < 0.1`.
   - Any State `-> jump` when the `jump` trigger fires and `grounded == true`
     (leave **From** empty for Any State).
   - `jump -> idle` with **Has exit time** on and **Exit time** `1.0` (return to
     idle when the jump clip finishes).

Transitions are checked in order, so list the higher-priority ones first.

## Drive it from a script

The machine is driven through `DioramaAnimStateMachineRequestBus` (Lua / Python /
Script Canvas), addressed by the entity:

```lua
local sm = DioramaAnimStateMachineRequestBus.Event
local id = self.entityId

-- Each frame, feed it your movement state:
sm.SetFloat(id, "speed", currentSpeed)
sm.SetBool(id, "grounded", isOnGround)

-- On the jump button:
sm.SetTrigger(id, "jump")

-- Force a state directly (e.g. on death), bypassing transitions:
sm.Play(id, "hurt")

-- Read back for logic or verification:
local state = sm.GetCurrentState(id)   -- "idle" / "run" / "jump" / ...
```

To react to state changes instead of polling, connect to
`DioramaAnimStateMachineNotificationBus` and handle `OnStateChanged(from, to)` --
that is where you spawn a hitbox on entering `attack`, play a footstep on entering
`run`, and so on.

## Notes

- **Triggers wait until they can fire.** A `SetTrigger` stays pending until a
  transition that uses it becomes eligible, then clears. Call `ResetTrigger` to
  cancel one you no longer want.
- **Exit time uses the clip duration.** A state's duration comes from its
  `frame count / fps`, or you can set a **Duration override** (useful for Aseprite
  tags, whose timing is authored in the file).
- **It only decides; it does not draw.** The state machine plays clips through the
  Sprite/Aseprite components, so everything you already know about those (point
  filtering, flips, tint, materials) still applies.
