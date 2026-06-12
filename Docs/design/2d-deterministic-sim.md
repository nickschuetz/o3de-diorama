# Design: deterministic fixed-tick simulation and rollback-ready state

Status: **shipped** (phases A-D, PRs #112/#115/#114; see
[howto/30-deterministic-sim.md](../howto/30-deterministic-sim.md)). The named
follow-up is migrating the render-tick components (bullet emitter, hitbox rig,
sprite/Aseprite animation) onto the sim clock. Companion doc:
[2d-box-interactions.md](2d-box-interactions.md) (typed boxes + attack payloads).
Driver: competitive fighting games of the SF6 / GGST class, which all ship rollback
netcode. Rollback has three prerequisites that must be designed into the simulation
layer and cannot be retrofitted: a fixed timestep, full state snapshot/restore, and
determinism. This doc makes the gem's 2D simulation provide all three.

The payoff is wider than fighting: deterministic replays, training-mode rewind,
kill-cams, golden-state CI tests ("run 600 frames, assert the state hash"), and an
agent verify loop that can confirm simulation correctness from a hash instead of a
screenshot.

## Scope boundary (complementary, not competing)

- The gem makes **its own 2D simulation** deterministic and snapshot-capable, and
  exposes the hooks a rollback layer needs. The netcode itself -- transport, input
  delay scheduling, prediction policy, matchmaking -- is **not ours**: O3DE's
  Multiplayer gem or GGPO-style middleware owns it.
- Determinism target is **same-binary**: both peers run the same build (the standard
  rollback assumption). No cross-ISA bit-exactness guarantees.
- Only gem-owned state is covered, plus the entity transform of participating
  entities. Engine systems outside the gem (PhysX, Atom) are out of scope; Diorama
  gameplay is transform-driven and gem-collided precisely so this boundary works.

## Problem (what blocks rollback today)

1. **Variable timestep.** Every sim-stateful component integrates on
   `AZ::TickBus::OnTick(deltaTime)`. Two machines render at different rates, so the
   same inputs produce different trajectories.
2. **No snapshot/restore.** Component state is private and non-restorable: the
   hitbox rig's frame / `m_alreadyHit` sets, the sprite animation's frame and time
   accumulator, the bullet pool. "Rewind 4 frames and re-simulate" is impossible.
3. **Nondeterministic ordering.** `Collision2DSystemComponent::OverlapShape` iterates
   an `AZStd::unordered_map` (`Collision2DSystemComponent.h:134`), so query *result
   order* depends on hash-map history. Any consumer that takes the first result
   (the shmup ram check does exactly this) diverges across runs.
4. **Unseeded randomness.** Samples use Lua `math.random`; there is no gem RNG that
   can be seeded, stepped, and snapshotted.
5. **Render-rate input.** Input is sampled per render frame, not once per sim frame,
   so input timing differs between machines and between runs.

## Decision

### 1. Sim clock (opt-in)

A **2D Simulation Clock** component, placeable (one per level): adding it is the
opt-in, every knob gets Inspector parity like the rest of the gem, and projects
that do not add it see zero cost and zero behavior change:

- Accumulates render delta time and emits
  `DioramaSimTickNotificationBus::OnSimTick(frame, stepSeconds)` at a fixed step
  (default 1/60 s, configurable), with a max catch-up step count per render frame
  to bound spiral-of-death.
- `GetSimFrame()` (monotonic frame counter), `SetPaused(bool)`, `StepOnce()`
  (training-mode single step, also the re-simulation primitive for rollback).
- Hit-stop and slow motion in deterministic games are expressed in **sim frames**
  (freeze N frames), not time-scale floats; the existing time-scale verbs remain for
  non-deterministic use.

Components with sim state gain a **Use Simulation Clock** toggle (Inspector +
bus verb, AI/human parity): when on, they integrate in `OnSimTick` with the fixed dt
and ignore the render tick. In scope for migration: the hitbox rig, the bullet
emitter, sprite-sheet / Aseprite animation, the animation state machine, and input.
Cosmetic components (particles, parallax, camera, CRT, day/night) deliberately stay
on the render tick: cosmetic divergence is acceptable and is standard practice in
rollback titles.

### 2. Snapshot / restore

- `DioramaSimStateRequests` (component bus): `SaveSimState(writer)` /
  `RestoreSimState(reader)` implemented by each migrated component. Chunks are
  tagged (component type id + version byte + size) and written **explicit
  little-endian** (Windows/portability rule).
- A `Simulation State` marker component declares an entity a snapshot participant.
- System verbs: `CaptureFrame()` walks participating entities and serializes entity
  world translation plus every gem chunk into one buffer; `RestoreFrame(buffer)`
  reverses it. The rollback layer owns the buffer history (typically 8 to 15
  frames); we own producing and consuming a frame image.
- `GetStateHash()`: a stable hash (FNV-1a) over the capture buffer. This is the
  desync detector for netplay middleware, the CI assertion, and the agent
  verify-loop readback.

**Restore treats the buffer as untrusted** (Security priority): every read is
bounds-checked, chunk sizes are sanity-capped, unknown type ids and version
mismatches reject the restore cleanly. No allocation sized from unvalidated fields.

### 3. Determinism audit (concrete fixes)

- **Query ordering:** sort `OverlapShape` results by `EntityId` before returning
  (result sets are small; the sort is noise). This also makes existing gameplay
  ("first hit wins") reproducible, deterministic mode or not.
- **Hit resolution order:** the hitbox component's per-target event order follows
  query order; fixed by the same sort. `m_alreadyHit` stays an unordered set
  (membership only, order never observed).
- **Seeded RNG:** a pure header core (xorshift or PCG32, unit tested) behind
  `DioramaRandomRequests`: `SetSeed(u64)`, `RandFloat()`, `RandRange(min, max)`.
  Its state participates in snapshot. Samples that need determinism migrate off
  `math.random`.
- **Float policy:** same-binary determinism makes floats safe if compilation is
  consistent: std:: math only (already a standing rule), no fast-math, document
  the `-ffp-contract` / `/fp` expectation in the build docs.

### 4. Input ring (the rollback interface)

`DioramaInputComponent` in sim-clock mode samples device state **once per sim
frame** into a per-frame record, kept in a ring buffer (default 120 frames):

- `GetActionAtFrame(action, frame)` reads history (training-mode input display,
  motion windows).
- `InjectFrame(frame, states)` overwrites a frame's record: this is how a rollback
  layer feeds corrected remote inputs before re-simulating, how a replay is played
  back, and how an AI/bot drives a fighter through the exact pipeline a human does
  (a direct [[ai-friendly-goal]] win).
- The motion/sequence detector consumes the ring with frame-indexed windows instead
  of wall-clock time.

### 5. The consumer's loop (for clarity, not ours to build)

Per frame: middleware captures state into its history, ticks the clock once. On a
late remote input for frame F: `RestoreFrame(history[F])`, `InjectFrame(F, ...)`,
`StepOnce()` repeated up to the present, continue. Everything it needs is the
public surface above.

## Phases

- **A (S): clock + ordering + RNG.** Sim clock component and bus, pause/step,
  query-order sort, seeded RNG core + bus. Tests: fixed-step accumulation and
  catch-up clamp, sorted query results, RNG sequence reproducibility.
- **B (M): snapshot.** Writer/reader, `SaveSimState`/`RestoreSimState` for hitbox,
  sprite/Aseprite animation, state machine, bullet pool, RNG; marker component;
  `CaptureFrame` / `RestoreFrame` / `GetStateHash`. Tests: round-trip equality,
  hash stability across runs, malformed-buffer rejection.
- **C (M): input ring.** Per-sim-frame sampling, ring, `GetActionAtFrame`,
  `InjectFrame`, frame-aligned motion windows. Tests: same seed + same injected
  inputs run twice gives identical hashes per frame; mid-stream injection
  deterministically diverges.
- **D (S): proof.** Headless determinism test in the normal AzTest suite (every PR
  proves the sim still deterministic: run N frames twice, compare hashes; save at
  F, advance k, restore, re-advance, compare). A runnable rewind demo (hold a key
  to rewind 60 frames) + how-to.

## Acceptance criteria

- **Security:** restore rejects malformed buffers (bounds, versions, caps); the
  snapshot format is internal, not a network protocol.
- **Performance:** capture is memcpy-grade with pre-sized buffers, no steady-state
  allocation; target well under 0.1 ms for a two-fighter scene; the clock adds no
  per-frame heap work.
- **Efficiency:** strictly opt-in; with the clock absent, every component runs
  exactly today's path.
- **Ease of use:** one clock component + one marker component + one toggle per
  component; every knob on a typed bus and in the Inspector; `GetSimFrame` /
  `GetStateHash` give agents and CI a no-viewport verify loop.

## Out of scope

Netcode transport and scheduling, prediction policy, cross-platform bit-exactness,
screen-space HUD (LyShine), determinism for engine systems outside the gem.
