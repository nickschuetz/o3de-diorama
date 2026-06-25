# How-To: Deterministic Simulation (fixed step, snapshots, rewind)

The deterministic-sim layer makes Diorama gameplay advance in **exact fixed frames**
with **seeded randomness**, **snapshot/restore**, and a **per-frame input history**,
so the same inputs replay to the same result on any machine running the same build.
This is the foundation for replays, training-mode rewind, and rollback netcode
readiness ([design](../design/2d-deterministic-sim.md)); the netcode itself stays
with O3DE Multiplayer or middleware.

Everything is opt-in: without a clock in the level, nothing changes.

## Build it

A one-command scene builder assembles this guide's demo in its own level
(a sim-clock mover with rotating snapshot autosaves; R rewinds about two seconds); finish any wiring the editor cannot script (noted in its output), then
enter game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/rewind_demo.py
```

## The 2D Simulation Clock

Add a **2D Simulation Clock** component to one entity in the level (a second clock
is rejected). It turns variable render time into fixed steps (default 60 per
second, with a catch-up cap after hitches) and broadcasts one tick per step:

```lua
function MyThing:OnActivate()
    self.handler = DioramaSimTickNotificationBus.Connect(self)
end

function MyThing:OnSimTick(frame, stepSeconds)
    -- runs exactly once per fixed step; frame counts from 0
end
```

Gameplay that moves on `OnSimTick` (instead of `OnTick`) simulates deterministically.
The clock is driven over `DioramaSimClockRequestBus`: `GetSimFrame`, `SetPaused`,
`StepOnce` (single-step while paused: a training-mode frame advance),
`SetStepsPerSecond`, and read-only `GetSimClockInfo`.

## Putting Diorama components on the clock

Five gameplay-relevant components carry a **Use Simulation Clock** checkbox (and a
matching `SetUseSimClock` verb on their request bus): the Sprite's sheet playback,
the Aseprite player, the animation state machine, the frame-data hitbox rig, and
the bullet emitter. Flagged on, they advance on the clock's fixed steps; the render
tick stands down while a clock is running (and takes back over if no clock exists,
so an editor preview still plays). The bullet emitter's render tick keeps pushing
the current pool to the renderer; firing, integration, and hit-testing move to the
fixed step, so hits resolve once per frame in deterministic order. A fighter built
from these pieces (state machine picking clips, clips driving `OnAnimationFrame`,
frames driving hitbox windows) steps frame-exactly under `StepOnce`.

## Super-freeze (the cinematic pause)

`FreezeFor(frames)` suppresses the clock's automatic stepping for that many fixed
steps, then resumes on its own. Because every gameplay system advances on `OnSimTick`,
they all hold for the duration with no extra wiring (the clock just stops ticking
them) - the super-flash / dramatic-pause effect. `GetSimClockInfo` reports `frozen`
and `freezeFramesRemaining`; a non-positive count clears an active freeze.

```lua
DioramaSimClockRequestBus.Broadcast.FreezeFor(36)  -- ~0.6s at 60 steps/sec
```

Two notes keep it clean. **To keep the attacker moving through the freeze**, drive its
super animation on the **render tick** (Use Simulation Clock off for that sprite) so it
advances while the sim world is held. **`StepOnce` ignores the freeze** - it always
forces a step - so rollback re-simulation and training-mode frame advance are
unaffected, and the freeze countdown is part of the snapshot so it rewinds correctly.
A darken scrim is a sample-side touch (a translucent world quad you tint for the
freeze); see `Assets/Diorama/Examples/Fighting/super_freeze.lua`.

## Seeded randomness

The clock hosts a deterministic RNG on `DioramaRandomRequestBus` (`SetSeed`,
`RandFloat`, `RandRange`, `RandInt`, `GetRandomDraws`). The same seed yields the
same sequence on every platform, and the RNG's position in its sequence is part of
every snapshot. Use it instead of `math.random` wherever the outcome should replay.
Not cryptographic: gameplay only.

## Snapshots: slots, hashes, restore

Mark each entity whose **world position** belongs to the simulation with a
**Simulation State** component; snapshot-capable Diorama components on marked
entities save and restore their own state with it: the frame-data hitbox rig, the
bullet emitter's live pool, and the playback positions of the Sprite sheet, the
Aseprite player, and the animation state machine.

```lua
DioramaSimClockRequestBus.Broadcast.SaveToSlot(0)        -- capture now
DioramaSimClockRequestBus.Broadcast.RestoreFromSlot(0)   -- jump back
local hash = DioramaSimClockRequestBus.Broadcast.GetStateHash()
```

`GetStateHash` is the determinism fingerprint: two runs in the same state hash
identically, so a test (or an agent) verifies correctness without a viewport. C++
rollback layers use the unreflected `CaptureFrame` / `RestoreFrame` buffer verbs on
the same bus and keep their own frame history.

## Per-frame input (the rollback interface)

On the **2D Input Actions** component, enable **Use Simulation Clock**. Input is
then sampled once per fixed step into a ring of recent frames (default 120):

```lua
local held = DioramaInputRequestBus.Event.WasPressedAtFrame(id, "punch", frame)
DioramaInputRequestBus.Event.InjectActionState(id, frame, "punch", 1.0, 0.0, true)
```

- Frame queries read the recorded history (training-mode input display, combo
  analysis).
- `InjectActionState` overwrites a frame's record. When that frame is simulated, or
  re-simulated after a restore, the injected state is used and press/release edges
  derive against the prior frame. This is how a rollback layer applies corrected
  remote inputs, how a replay plays back, and how a bot drives a fighter through
  the exact pipeline a human uses.
- On re-simulation the ring **replays** recorded frames rather than re-sampling the
  live devices; the ring itself deliberately survives a restore (the input log
  lives outside the state, the standard rollback model).

The rollback loop a netcode layer runs: capture every frame; on a late input for
frame F, `RestoreFrame(history[F])`, inject the corrections, then `StepOnce` back
to the present.

## Rewind demo

`Assets/Diorama/Examples/Simulation/rewind_demo.lua` autosaves a rotating snapshot
slot every 15 sim frames and restores the oldest on a `rewind` button press, so the
scene jumps about two seconds back and plays on. Wire it on any entity with a 2D
Input Actions component (`rewind` bound to a key) in a level with the clock, and
put a Simulation State marker on everything that should snap back.

## Verified by CI

`DeterminismTest.cpp` runs in the normal suite on every PR: a scripted run replays
from a restored snapshot to bit-identical per-frame hashes, and a mid-run rollback
re-advances to the exact end-state hash.

## Current limits (honest list)

- "Use Simulation Clock" is opt-in per component and defaults off; a mixed scene
  (some components on the clock, some on the render tick) is fine visually but
  only the flagged components replay deterministically.
- The motion-input direction history is not rewound by a restore; motion
  recognition converges within one buffer window. Gate motions on button edges
  (which replay exactly) where this matters.
- Determinism is same-binary: both peers run the same build (the standard rollback
  assumption), not cross-ISA bit-exactness.
