# How-To: 2D Particle Emitter

The Diorama 2D Particle Emitter sprays pooled particles -- sparks, smoke, dust,
hearts, fountains -- each a billboarded sprite quad drawn through the existing
batched sprite path, so a whole burst is one draw call. The simulation
(velocity, gravity, drag, aging, size/color over life) is the unit-tested
`Particles2D` core; the pool is fixed-capacity, so nothing can spawn unboundedly.

If you just want to see it, run the example and enter game mode. It builds its
own level (`DioramaParticleDemo`), independent of any other scene:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/particles_demo.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Important: particles run at game time

The emitter simulates only **in game mode** (it ticks through the runtime
component). In the editor viewport you author it; the spray happens when you enter
game mode. This keeps the emitter from churning the renderer every editor frame.

## 1. Add an emitter

1. Create an entity where the effect should originate.
2. **Add Component -> Diorama -> 2D Particle Emitter**.
3. Configure it. Key fields:
   - **Texture**: the particle image (a soft dot, spark, heart). Tinted per
     particle by the start/end color.
   - **Max Particles**: hard pool cap (the pool never grows past this).
   - **Rate**: continuous emission (particles/sec); 0 = burst-only.
   - **Burst Count** / **Emit On Activate**: a one-shot burst when the game starts.
   - **Lifetime / Speed Min-Max**: randomized per particle.
   - **Direction / Spread**: emission cone in degrees (360 = radial).
   - **Gravity / Drag**: forces over life.
   - **Start/End Size**, **Start/End Color**: the over-life ramps (drop end alpha
     to fade out).

## 2. Drive it from script

The emitter has a typed request bus, `DioramaParticleRequestBus`, so a script or
agent can fire and tune effects at runtime:

```lua
DioramaParticleRequestBus.Event.Burst(emitter)            -- one-shot pop (e.g. on a hit)
DioramaParticleRequestBus.Event.Emit(emitter, 50)         -- spawn N now
DioramaParticleRequestBus.Event.SetRate(emitter, 120.0)   -- ramp a continuous stream
DioramaParticleRequestBus.Event.SetGravity(emitter, 0.0, -9.0)
DioramaParticleRequestBus.Event.SetStartColor(emitter, 1.0, 0.6, 0.1, 1.0)
DioramaParticleRequestBus.Event.SetEndColor(emitter, 1.0, 0.0, 0.0, 0.0)
DioramaParticleRequestBus.Event.Play(emitter)             -- / Stop(emitter)
```

`GetParticleInfo` reads back the live count, pool cap, and play state for
verification (the pool stays bounded by Max Particles).

## 3. Run the demo

After running `particles_demo.py`, **enter game mode** (Ctrl+G): the `Fountain`
entity sprays a continuous stream of particles that arc up, fall under gravity,
shrink, and fade. Tune the emitter's config and re-enter game mode to iterate.

## How it works

Each tick the runtime component emits (rate + bursts), steps the pooled particles
with `Particles2D::StepPool` (gravity, drag, integrate, age, retire the dead), and
pushes each live particle as a billboarded quad into the `SpriteFeatureProcessor`
through a pre-acquired handle pool -- so all the particles of one emitter batch
into a single draw, and they pick up 2D lighting for free. See
[Docs/design/2d-particles.md](../design/2d-particles.md) for the design and
[Code/Source/Clients/Particles2D.h](../../Code/Source/Clients/Particles2D.h) for
the pure simulation core.
