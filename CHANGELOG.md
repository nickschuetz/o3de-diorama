# Changelog

All notable changes to Diorama are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/). Before
1.0 (the 0.x line), minor releases may include breaking changes.

## [Unreleased]

### Added
- **Typed interaction boxes, phase C (box overlay).** The **2D Frame-Data Hitboxes**
  component can draw its live boxes as world-space translucent quads, color-coded by
  kind (hurt green, hit red, push yellow, throw purple, armor blue, proximity gray) -
  the training-mode box display for tuning frame data. Toggle it per rig (the **Show
  Box Overlay** Inspector checkbox or the **`SetShowOverlay`** bus verb) or globally
  with the **`d_dioramaHitboxOverlay`** console variable, which forces it on for every
  rig. The quads draw through the existing sprite batch path (one renderer handle per
  authored box, acquired only while the overlay is on, so a rig with it off costs the
  renderer nothing) and stay world-space, inside the gem's scope line. Visible in game
  mode and the launcher as the animation plays. Phase D (the sample wiring real frame
  data onto boxes) follows.
- **Typed interaction boxes, phase A (pure core).** `HitboxFrames` grows the full
  interaction vocabulary: `BoxKind` adds Pushbox, Throwbox, ThrowableBox, ArmorBox,
  and ProximityBox alongside the v1 Hurtbox/Hitbox; each box carries a
  `HitProperties` attack payload (damage, hitstun/blockstun/hitstop frames,
  pushback, guard height, launch, priority, opaque custom id) that the gem stores
  and delivers but never interprets; and a pure `Resolve(kindA, priorityA, kindB,
  priorityB)` interaction matrix decides Hit / Clash / Beaten / Absorbed / Throw /
  Proximity per side (pushbox separation stays positional, not an event). Unit
  tests cover every matrix cell in both argument orders plus an exhaustive
  silent-pair sweep; new payload fields default to v1 behavior
  ([design](Docs/design/2d-box-interactions.md)). Component authoring, events, the
  box overlay, and samples follow in phases B-D.
- **Typed interaction boxes, phase B (component).** The **2D Frame-Data Hitboxes**
  component now authors the new box kinds and attack payload per box in the
  Inspector, resolves the typed-box matrix between rigs each evaluation, and fires
  **`OnBoxEvent(boxEvent)`** on both parties with the result (Hit / Clash / Beaten /
  Absorbed / Throw / Proximity), attacker / defender ids and box indices, an
  approximate world contact point, and the attacking box's payload. The v1
  `OnHit` / `OnHurt` keep firing only on a real strike, so existing scripts are
  untouched. Hit-vs-hit contests resolve once per pair by clash priority (equal =
  Clash both sides, else Hit / Beaten); armor shadows a hurtbox (Absorbed instead of
  Hit); throwboxes grab throwable boxes; proximity boxes signal over hurtboxes.
  Pushboxes separate positionally through the existing minimum-translation path, with
  an opt-in **Auto Separate** that applies half the push-out so an overlapping pair
  converges (or read `pushOutX/Y` off `GetHitboxInfo` and apply it game-side).
  `GetHitboxInfo` reports the active count of every box kind; `DioramaBoxEvent` is
  reflected read-only to Lua / Script Canvas / Python, and the rig snapshot now
  rewinds the per-box contest bookkeeping alongside the hit set. Integration tests
  cover payload delivery, contact point, each interaction kind, the priority contest,
  and opposite-direction pushbox separation. The overlay and sample land in phases
  C-D.
- **Sim-clock migration: the five render-tick gameplay components can now advance
  on the 2D Simulation Clock's fixed steps.** The Sprite sheet playback, the
  Aseprite player, the 2D Animation State Machine, the 2D Frame-Data Hitboxes,
  and the 2D Bullet Emitter each gain a **Use Simulation Clock** checkbox and a
  matching **`SetUseSimClock`** verb on their request bus (live-togglable,
  AI/human parity). Flagged on, the component advances in `OnSimTick` and its
  render tick stands down while a clock runs (no clock: render tick advances as
  before, so editor previews still play); the bullet emitter's render tick keeps
  pushing the pool to the renderer while firing, integration, and hit-testing
  move to the fixed step. Deterministic advance no longer depends on renderer
  availability (headless runs simulate identically). Snapshot coverage grows
  three chunks: sprite playback position (`'SPRA'`), Aseprite playback position
  (`'ASEP'`), and the state-machine runtime (current state, time in state,
  parameter values; `'ANSM'`), so animation timelines rewind exactly with the
  rest of the simulation. `SpriteInfo` reports `useSimClock`. New
  `SimClockMigrationTest` suite: fixed-step advance via `StepOnce`, live verb
  toggling, and byte-identical chunk round trips.

## [0.3.0-beta] - 2026-06-12

### Added
- **Demo backfill: a one-command scene builder for every how-to that lacked one.**
  Ten new self-contained builders under `Docs/examples/` (pixel-art A/B, glow,
  CRT, world-space readout, audio one-shots, danmaku boss, platformer, brawler
  lanes, fighting rigs with a quarter-circle special, and the deterministic
  rewind demo), plus two small samples they wire: `audio_oneshot.lua` (input ->
  `PlayOneShot`) and `sim_mover.lua` (a mover that advances on the 2D Simulation
  Clock's fixed steps, so the rewind demo's restores line up exactly). Each
  builder assembles its scene in its own level and bakes what the editor cannot
  set live (Lua script assets, input action maps, the fighting demo's hitbox
  rig) into the saved prefab; remaining manual steps are printed. The twelve
  affected how-tos gain a Build it section, including the two that already had a
  builder nobody linked (`anim_input_demo.py`). All ten scenes were built and
  structurally verified through the live editor.
- **2D Simulation Clock** (deterministic sim, phase A of
  [Docs/design/2d-deterministic-sim.md](Docs/design/2d-deterministic-sim.md)). A new
  **2D Simulation Clock** component (`DioramaSimClockComponent`, one per level, opt-in:
  nothing changes without it) turns variable render time into **fixed simulation
  steps** on the pure tested `SimClock` accumulator core (configurable steps per
  second, catch-up cap with backlog drop) and broadcasts
  `DioramaSimTickNotificationBus::OnSimTick(frame, stepSeconds)` once per step: the
  deterministic heartbeat gameplay scripts advance on so the same inputs replay to the
  same result. Controlled over `DioramaSimClockRequestBus` (`GetSimFrame`, `SetPaused`,
  **`StepOnce`** frame advance for training-mode freeze and rollback re-simulation,
  `SetStepsPerSecond`, read-only `GetSimClockInfo`). The component also hosts a
  **seeded gameplay RNG** on the pure tested `SimRandom` core (splitmix64-spread
  xorshift64*, exact integer math, platform-identical) over `DioramaRandomRequestBus`
  (`SetSeed`, `RandFloat`, `RandRange`, `RandInt`, `GetRandomDraws`), so randomness can
  be seeded, replayed, and later snapshotted. Not cryptographic; gameplay only.
- **Per-sim-frame input ring + determinism proof** (deterministic sim, phases C and
  D). The **2D Input Actions** component gains a **Use Simulation Clock** mode: input
  is sampled once per fixed step into a ring of recent frames (default 120), with
  frame queries (`WasPressedAtFrame`, `GetValueAtFrame`, `GetValueYAtFrame`) and
  **`InjectActionState(frame, action, x, y, pressed)`**, which overwrites a frame's
  record so a rollback layer can apply corrected remote inputs, a replay can play
  back, and a bot can drive a fighter through the exact pipeline a human uses. On
  re-simulation after a restore the ring **replays** recorded frames instead of
  re-sampling live devices, and it deliberately survives a restore (the input log
  lives outside the state, the standard rollback model); press/release edges derive
  per sim frame, and motion windows use frame-derived time. Phase D ships the
  acceptance proof in the normal test suite (`DeterminismTest.cpp`: a scripted run
  replays from a restored snapshot to bit-identical per-frame hashes, and a mid-run
  rollback re-advances to the exact end hash, so every PR re-proves determinism), a
  **rewind sample** (`Assets/Diorama/Examples/Simulation/rewind_demo.lua`: rotating
  autosave slots + a rewind key), and the how-to
  ([Docs/howto/30-deterministic-sim.md](Docs/howto/30-deterministic-sim.md)) with an
  honest current-limits list (render-tick components' sim-clock migration is the
  noted follow-up).
- **Simulation snapshot/restore** (deterministic sim, phase B infrastructure). The
  clock gains frame capture: a new **Simulation State** marker component
  (`DioramaSimStateComponent`, no configuration) enrolls an entity, and the marker
  itself snapshots/restores the entity's **world translation** (the bulk of sim state
  in transform-driven Diorama gameplay); snapshot-capable components contribute their
  own tagged chunks through an internal participant bus. First component coverage
  ships with it: the **2D Frame-Data Hitboxes** rig (current frame, facing, and each
  box's active-window bookkeeping including its already-hit set, so a restored swing
  re-resolves hits exactly as the original did) and the **2D Bullet Emitter** (the
  live bullet pool with full per-bullet kinematics plus the spiral angle and fire
  state, so a restored frame replays the exact bullet field). Capture writes a
  canonical little-endian image on the pure tested `SimState`
  writer/reader core (participants sorted by entity id, so the image and its hash are
  run-independent); **restore treats the buffer as untrusted** (bounds-checked reads,
  magic/version/range validation, malformed input rejected, chunks for missing
  entities skipped). C++ rollback layers use `CaptureFrame`/`RestoreFrame` on
  `DioramaSimClockRequestBus`; scripts and agents use the reflected surface:
  **`GetStateHash`** (FNV-1a 64 determinism fingerprint), **`SaveToSlot(slot)`** and
  **`RestoreFromSlot(slot)`** (8 internal slots: training-mode rewind, replays,
  tests).
- **2D Bullet Emitter: muzzle offset.** A general `Muzzle Offset` (Vector2) on the emitter
  config (Inspector + `SetMuzzleOffset(x, y)` on `DioramaBulletRequestBus`): bullets spawn
  at the entity origin plus this offset, so a gun fires from a ship's nose, a turret's
  barrel, or any offset point instead of the entity center. A unit test verifies the spawn
  point moves. Motivated by the shmup dogfood (firing from the ship center caused
  point-blank ram-kills).
- **Shmup dogfood example** (a playable vertical slice). A small shoot-em-up assembled
  from the shipped features to integration-test the gem end to end and serve as a worked
  example: a sprite ship flown by the input map, the **2D Bullet Emitter reused as the
  ship's gun** (a single bolt, autofire, firing from the nose via the muzzle offset), 2D
  collision for the hits, a flash-and-knockback combat loop, a descending **enemy wave**
  that recycles to the top (no spawnables) with two bigger, tougher **Odie** (the O3DE
  mascot, scaled from sprite width so no per-enemy tuning is needed), **lives** with a ram
  cost, a game-over freeze and a Space restart, a parallax starfield, and a camera over the
  XY play plane. Ships as `Assets/Diorama/Examples/Shmup/{player_ship,enemy_wave}.lua`,
  procedural art (`Assets/Diorama/Textures/_gen_shmup_textures.py` -> `shmup_*.png`), and a
  level builder (`Docs/examples/shmup_demo.py`) that wires the scene and Lua scripts,
  documented with the gameplay-scripting gotchas it surfaced
  ([Docs/howto/29-shmup.md](Docs/howto/29-shmup.md)).
- **Day/night cycle** (lighting). A new **Day/Night Cycle** component
  (`DioramaDayNightComponent`) advances a normalized time-of-day clock and drives a target
  Diorama light's color, intensity, and direction over the day, on the pure tested
  `DayNightCycle` core (cyclic four-phase gradient `Sample` + sun `SunDirection` from
  elevation/azimuth). A thin complementary layer over the 2D lighting feature: it creates
  no light and owns no new rendering, just animates an existing one (self by default, or a
  `Target Light` entity). Phase colors / intensities (night, dawn, day, dusk) and cycle
  timing are authored in the Inspector; the clock is also on `DioramaDayNightRequestBus`
  (`SetTimeOfDay`, `SetCycleSeconds`, `SetPaused`, `StepHours`, `GetDayNightInfo`) for
  Lua / Python / Script Canvas / agents. Reproducible sample under
  `Assets/Diorama/Examples/DayNight/` (`daynight_controller.lua` scrubs the clock and logs
  sunrise/sunset; `daynight_lamp.lua` switches a lamp on at night) plus a one-command demo
  scene builder (`Docs/examples/daynight_demo.py`)
  ([Docs/howto/28-day-night.md](Docs/howto/28-day-night.md)).
- **Grid intelligence cores** (roguelike / tactics). Three pure, unit-tested
  header algorithms over a tile grid (each takes a grid predicate, so they work over a
  Diorama tilemap or any grid, with no engine coupling): **A\* pathfinding**
  (`Pathfinding::FindPath`, 4-connected, Manhattan heuristic, returns the route or
  fails on an unreachable goal); **movement range** (`MovementRange::ComputeReachable`,
  a Dijkstra flood honoring per-cell entry costs, the "blue tiles"); and **field of
  view** (`FieldOfView::Compute`, symmetric recursive shadowcasting over 8 octants, the
  visible set a fog-of-war layer marks explored). FOV/fog is in-vision (feeds visibility
  rendering); movement-range and A\* are general grid utilities kept here for
  convenience. A tilemap-backed bus exposure for Lua/agents is a noted follow-up; the
  tests under `GridIntelligenceTest.cpp` are runnable usage examples
  ([Docs/howto/27-grid-intelligence.md](Docs/howto/27-grid-intelligence.md)).
- **Side-scroller platforming** (one-way platforms + ramps). The 2D collision world
  gains the platformer primitives, on the pure tested `SlopeCollision` core. **One-way
  platforms**: a `m_oneWay` flag on the 2D Collider (Inspector **One Way** + `SetOneWay`
  bus verb) that `ComputeBoxPushOut` resolves with an upward-only push (`OneWayPushOut`),
  so a body lands on top but drops through from below / the sides. **Ground-follow over
  flat ground and ramps**: `ProbeGroundY(x, footY, maxDrop, stepUp)` returns the highest
  walkable surface within step-up / max-drop reach, fed by ground segments authored via
  `AddGroundSegment(x0, x1, y0, y1)` (flat when `y0==y1`, a ramp otherwise) or the
  C++/tilemap `SetGroundSegments`. Reproducible sample under
  `Assets/Diorama/Examples/SideScroller/` (`platformer_body.lua` + `platformer_ground.lua`)
  ([Docs/howto/25-platformer.md](Docs/howto/25-platformer.md)). Driving slope tiles
  straight from a tilemap is a noted follow-up (needs tilemap XY-plane collision).
- **2.5D brawler depth lanes** (beat-em-up). A new **2.5D Depth Body** component
  (`DioramaDepthBodyComponent`) owns a character's **depth** (toward/away from the
  camera) and renders it so a flat orthographic scene reads as 2.5D: it lifts the
  sprite up-screen (`Lift Per Unit`) and biases its draw order (`Sort Per Unit`) from
  depth, backed by the pure tested `DepthLane` core (`SameLane`, `ScreenLift`,
  `SortBias`, `MoveToward`, `ClampToArena`). Driven by `DioramaDepthBodyRequestBus`
  (`SetDepth`, `MoveDepthToward`, `GetDepth`, read-only `GetDepthInfo`); with a tilted
  camera set the lift to 0 and use it just for the lane value. Depth-aware combat
  composes the **2D Frame-Data Hitboxes** component: gate `OnHit` by `SameLane`, or put
  the hitboxes on the XZ floor plane so the 2D overlap is depth-aware automatically.
  Reproducible sample under `Assets/Diorama/Examples/Brawler/` (`brawler_player.lua` +
  `brawler_enemy.lua`) ([Docs/howto/26-brawler.md](Docs/howto/26-brawler.md)).
- **Bullet-pattern emitter** (danmaku / shmup). A new **2D Bullet Emitter** component
  (`DioramaBulletEmitterComponent`) fires shots of a **Ring / Fan / Spiral** pattern at
  a configurable rate (geometry from the pure tested `BulletPattern` core: `Ring` /
  `Fan` / `AdvanceSpiral`), simulates the bullets as a pooled CPU particle set
  (`Particles2D`), and renders the whole pattern in one draw call through the sprite
  batch. Each frame every live bullet is tested against a target collision layer; an
  overlap fires `OnBulletHit` and consumes the bullet. Driven by `DioramaBulletRequestBus`
  (`Fire`/`Play`/`Stop`, `SetPattern`/`SetFireRate`/`SetCount`/`SetSpeed`/`SetAim`/
  `SetSpread`/`SetSpin`, read-only `GetBulletInfo`). Fixed-capacity pool (hard-capped),
  so no script or asset value spawns unboundedly. Reproducible sample
  `Assets/Diorama/Examples/Shmup/enemy_danmaku.lua`
  ([Docs/howto/24-bullet-patterns.md](Docs/howto/24-bullet-patterns.md)). Damage/scoring
  stay game-side; the emitter reports the contact and removes the bullet.
- **Fighting building blocks v2** (motions, frame-data hitboxes, impact juice). The
  **2D Input Actions** component gains a motion-input layer: point it at an `Axis2D`
  direction action and author motions in numpad notation (`236` = quarter-circle, `623`
  = dragon-punch); `WasMotionPerformed(name)` (buffered poll) and the `OnMotionPerformed`
  notification recognize them order-sensitively within a per-motion window, on the pure
  tested `MotionInput` core (`Matches` + `DirectionFromAxes`). A new **2D Frame-Data
  Hitboxes** component (`DioramaHitboxComponent`) authors a rig of hitboxes/hurtboxes,
  each live on an animation-frame window (`HitboxFrames` core); each frame it registers
  live hurtboxes as collision geometry and tests live hitboxes against a target layer,
  firing `OnHit`/`OnHurt` once per opponent per active window, with `SetFacing` mirroring
  and a read-only `GetHitboxInfo`. Reusable samples ship under
  `Assets/Diorama/Examples/Fighting/` (`motion_special.lua`, `hit_response.lua` with a
  hit-spark particle preset + flash + screen-shake pulse)
  ([Docs/howto/21-fighting.md](Docs/howto/21-fighting.md)). Both reuse existing systems
  (input map, 2D collision, particles, material flash, camera trauma); the renderer
  provides the primitives, applying damage stays a gameplay decision.
- **Skeletal cross-fade** (animation depth v2). The cutout skeletal player gains a
  named-clip library and `CrossFadeTo(clipName, durationSeconds)`, which blends the
  current and target clips per bone over a transition (then promotes the target),
  backed by the pure tested `SkeletalClip::BlendPose` + `CrossfadeWeight` cores. A
  non-positive duration switches instantly; an unknown name is ignored. Additive: an
  empty clip library leaves the v1 single-clip behavior unchanged
  ([Docs/howto/18-skeletal.md](Docs/howto/18-skeletal.md)).
- **Off-screen sprite culling.** The sprite feature processor builds the view frustum
  each frame and skips any sprite whose bounding sphere is fully outside the side
  planes, on the pure tested `SpriteCull.h` core. Conservative and reverse-depth
  independent (it never hides a visible sprite); saves the vertex packing and draw for
  off-screen sprites in large sparse scenes. Toggled by the `r_dioramaSpriteCull` cvar.
- **Hit-stop / slow-motion + pushbox resolution** (fighting follow-ups). Sprites
  gain a `m_playbackSpeed` time-scale (Inspector + `SetPlaybackSpeed` bus verb;
  0 = freeze for hit-stop, <1 = slow motion), matching the Aseprite component's
  `SetSpeed`. The 2D collision system gains `ComputeBoxPushOut` (global bus): the
  net minimum-translation vector to separate a box from overlapping colliders on a
  layer, excluding a given entity, backed by a new pure `Collision2D::MinimumTranslation`
  core (circle/box/circle-box, least-penetration axis; unit-tested). Two reusable
  samples ship under `Assets/Diorama/Examples/Fighting/` (`hitstop.lua`,
  `pushbox.lua`). The renderer provides the primitives; freezing time and applying
  the push stay gameplay decisions.
- **Sprite UV transpose (anti-diagonal flip) + rotated tiles.** Sprites gain a
  `m_transpose` option (Inspector + `SetTranspose` bus verb) that reflects the
  sampled region across its anti-diagonal; combined with the H/V flips it produces
  all four 90-degree rotations and mirrors of a square cell. Tilemaps carry it as a
  per-tile `FlipDiagonal` bit, and the Tiled `.tmj` importer now maps Tiled's
  diagonal-flip flag onto it, so **rotated/flipped Tiled tiles import correctly**
  (previously the diagonal flag was dropped). The corner-UV math is unit-tested and
  flip-only behavior is unchanged.
- **Animation frame events** for frame-exact gameplay (fighting games, action games):
  `DioramaSpriteNotificationBus::OnAnimationFrame(frameIndex)` fires every time the
  displayed frame advances, on both sprite-sheet and Aseprite playback, and
  `OnAnimationFinished` now actually fires when a non-looping clip ends. A new
  example (`Assets/Diorama/Examples/Fighting/frame_hitbox.lua`) uses it to drive a
  per-frame attack hitbox (startup / active / recovery frames) through the existing
  2D Collider bus, the per-frame hitbox pattern fighters need.
- **Camera versus framing + zoom** on the 2D camera: `SetSecondaryTarget` frames the
  midpoint of two characters, and `SetZoom` / `SetAutoZoom` dolly the camera back
  from the play plane (manually, or automatically from the targets' separation so
  the view pulls out as fighters move apart). The single-target, no-zoom path is
  unchanged. Pure math (`Camera2D::Midpoint`, `SeparationDolly`) is unit-tested.
- **Dedicated tilemap asset + builder.** A tilemap can now be authored as a
  `.dtilemap` JSON source that the new `DioramaTilemapBuilder` compiles into a
  validated, compact `DioramaTilemapAsset` product (`.dtilemapc`); a Tilemap
  component references it (the **Tilemap Asset** Inspector field) instead of
  inlining a large tile array into the prefab, so the level stays small and the map
  loads without re-parsing. The builder treats the source as untrusted input,
  bounding dimensions, layer count, tile-array length, and tile indices
  (`TilemapAssetLimits`) and rejecting inconsistent maps, fulfilling the VISION
  security criterion for asset-sourced tilemap data. The asset is multi-layer from
  the start (Phase 1 renders the first layer; multi-layer rendering and a Tiled
  `.tmj` importer emitting the same product are planned follow-ups). The JSON
  parser (`TilemapSource::Parse`) is a pure, unit-tested core. The source format is
  Diorama's own open JSON, so it carries no third-party license obligation. See
  [Docs/howto/04-tilemap.md](Docs/howto/04-tilemap.md).
- Sprite **Point Filter (pixel art)** option: a per-sprite toggle for
  nearest-neighbor texture filtering so low-resolution pixel art stays crisp
  instead of being bilinearly blurred. Reachable from the Inspector and the
  `SetPointFilter` verb on `DioramaSpriteRequestBus` (and reported by
  `GetSpriteInfo`), and part of the sprite batch key so point- and linear-filtered
  sprites coexist in a scene. Adds a nearest sampler to the sprite shader, selected
  per draw. Paired with a no-mipmap import recipe (the built-in
  `UserInterface_Lossless` preset) and a new how-to
  ([Docs/howto/06-pixel-art.md](Docs/howto/06-pixel-art.md)) plus an example
  `pixel_sprite.png`, covering both halves of crisp pixel art (sampler + mipmaps).
  Came out of an O3DE community request for explicit nearest-neighbor filtering.

### Fixed
- **Deterministic 2D collision query order.** `OverlapBox` / `OverlapCircle` results
  are now sorted by entity id, and `Raycast2D` breaks exact distance ties on the lower
  entity id. Both previously followed `unordered_map` iteration order, so "the first
  hit" could differ between runs of the same scene: visible to any script that indexes
  the result list, and a blocker for replay/rollback determinism
  ([Docs/design/2d-deterministic-sim.md](Docs/design/2d-deterministic-sim.md)).
- **Deterministic push-out accumulation.** `ComputeBoxPushOut` now sums per-collider
  push vectors in sorted entity-id order (dynamic colliders and static-set owners
  both). Float addition is not associative, so summing in hash-map iteration order
  could differ across runs in the low bits: harmless to the eye, but a divergence
  seed for replays and rollback.
- **Sample scripts: broken `Vector3` access idiom.** The `brawler_*.lua` and
  `platformer_body.lua` examples read/wrote a transform with `pos:GetX()` / `pos:SetX()`,
  which is **nil** in this engine's Lua (so they failed at runtime, never play-tested).
  Fixed to property access (`pos.x`) and `Vector3(...)` construction, matching the working
  `walker.lua`. Surfaced by the shmup dogfood.
- **Bullet emitter unity-build robustness.** `DioramaBulletEmitterComponent.cpp` serializes
  a `Data::Asset<StreamingImageAsset>` field but was relying on a transitive include of the
  `Data::Asset<T>` serializer from a unity-build neighbor; it now includes
  `<AzCore/Asset/AssetSerializer.h>` directly (the same include the other asset-bearing
  components already carry), so it compiles regardless of unity grouping.

### Removed
- **Screen-space UI/HUD component.** The early `DioramaUIComponent` (text/bar/panel
  drawn screen-space) was removed: it competed with LyShine and broke the gem's
  `VISION.md` boundary (Diorama is world-space only; LyShine owns screen-space UI).
  In-world HUD elements (a health bar or icon pinned above an entity) are just sprites,
  documented in [Docs/howto/13-world-space-hud.md](Docs/howto/13-world-space-hud.md).

## [0.2.0-beta] - 2026-06-05

Beta milestone. The feature set is now broad across sprites, rendering, tilemaps,
2.5D, lighting, particles, post, camera, 2D collision, and scripting, and the gem
builds with its unit tests green (166) on both Linux and Windows (VS2026 / MSVC,
verified on a Windows host). The API and serialized formats may still change
before 1.0.

### Added
- Aseprite **runtime asset reference**: the Aseprite animation component can now
  reference a `.dioramasheet` product (emitted by the native `.aseprite`
  AssetBuilder) and load its frames/tags/atlas at runtime, so a native import is a
  drop-in animated sprite with no manual JSON step. The inline JSON-import path
  still works and is the fallback when no sheet asset is assigned.
- Tilemap asset follow-ups on the dedicated tilemap asset + builder:
  **multi-layer rendering** (every layer of a `DioramaTilemapAsset` now draws, each
  as its own batched layer at its own sort offset, not just the first), **native
  Tiled `.tmj` import** (the builder also compiles finite orthogonal Tiled maps into
  the same product, mapping GIDs to atlas cells, carrying horizontal/vertical tile
  flips, and resolving embedded or external `.tsj` tilesets; a from-scratch parser,
  no third-party library), **editor preview from the asset** (the
  EditorTilemapComponent loads and previews the referenced map, all layers, in the
  viewport), and a **`SetTilemapByPath`** verb on `DioramaTilemapRequestBus` so a
  script or agent can swap the whole map at runtime (`GetTilemapInfo` reports
  `hasSourceAsset`). `TiledImport::Parse` is pure and unit-tested.
- A first sample **cartoon solar-system diorama** showcase (`DioramaSolarSystem`),
  authored offline from procedural art (`scripts/gen_cartoon_*.py`,
  `scripts/gen_diorama_solar_level.py`): a layered 2.5D scene with a setting sun,
  planets, a rabbit-marked moon, and a comet over normal-mapped terrain.
- Animated version of the showcase via example Lua scripts over the typed buses
  (`Assets/Diorama/Examples/Solar/`): a panning parallax camera, a drifting comet
  with a particle trail, a particle campfire with a flickering dynamic point
  light, pulsing emissive glow, and twinkling stars.
- Arcade-styled feature-overview card (`scripts/gen_feature_card.py`) in the
  README "At a glance" section.
- `scripts/capture_level_noap.sh` gained a video (`.mp4`) capture mode.
- Going-public groundwork: `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` (Contributor
  Covenant 2.1), `SECURITY.md`, GitHub issue and pull-request templates, and an
  SBOM (`sbom.spdx.json`) with a CI freshness gate. Every `runpython` how-to
  gained Windows instructions alongside the Linux ones.
- Side-scroller vertical slice sample (`Docs/examples/sidescroller_demo.py` +
  `walker.lua` + how-to). A short side-scroll scene that composes the sprint's
  features at once: three parallax background layers, a follow camera, two
  2D point-light torches, an ember particle emitter, and an outlined walking
  player. Pure composition of the shipped feature components (no new engine code),
  doubling as an integration check that the features stack.
- 2D materials: hit-flash and outline (materials v1). Sprites gain a **Flash Color**
  + **Flash Amount** (the shader blends the lit sprite toward the flash color, for
  the classic damage/hit pop) and an **Outline Color** + **Outline Thickness** (a
  silhouette outline drawn in the transparent fringe, for selection/hit highlights;
  thickness is screen-relative via the UV derivative). `SetFlash(r,g,b,amount)` and
  `SetOutline(r,g,b,thickness)` verbs on `DioramaSpriteRequestBus` drive them from
  script / Python / Script Canvas. Both are part of the batch key (bound per draw),
  so a sprite using an effect splits into its own batch while the rest (the default,
  unchanged look) batch together. This is the per-draw material backbone for the
  planned dissolve / additive-blend effects. See
  [Docs/design/2d-materials.md](Docs/design/2d-materials.md).
- Normal-mapped 2D lighting (lighting v1b). Sprites gain an optional **normal map**
  slot: when set, the gem's 2D lights shape the flat art with a Lambertian N.L term
  (the side facing a light is brighter, and the highlight tracks a moving light)
  instead of only attenuating by distance; when unset, lighting is byte-for-byte the
  flat v1a path. The shader brings the tangent-space normal into world space using
  the billboard's camera basis (set per draw), so it is best suited to billboarded
  sprites. The normal map is part of the batch key (one bind per draw), so sprites
  sharing albedo + normal + sort still batch into one call. A new
  `SetNormalMapByPath` verb on `DioramaSpriteRequestBus` assigns it from script /
  Python / Script Canvas, and a `sphere_normal` sample texture ships so a flat
  sprite can light like a 3D ball. See
  [Docs/design/2d-lighting.md](Docs/design/2d-lighting.md).
- Gem-native 2D parallax layer component. A `2D Parallax Layer` component placed on
  a layer entity (sprite, tilemap, or a parent of several) offsets it from its
  authored position by a reference entity's movement scaled by `(1 - factor)`, so
  far layers (low factor) appear to follow the camera and near layers (high factor)
  stay put, giving 2.5D depth. It is the first-class C++ counterpart of the existing
  `parallax_layer.lua` script, with an editor twin and a typed
  `DioramaParallaxRequestBus` (Common scope): `SetCamera`, `SetFactor` (clamped
  0..1), `SetEnabled`, and `GetParallaxInfo`.
- Gem-native 2D particle emitter. A `2D Particle Emitter` component sprays pooled
  particles (sparks, smoke, dust, hearts, fountains): continuous rate and/or bursts,
  a point/cone/radial emission shape with spread, randomized lifetime and speed,
  gravity and drag, and size + color ramps over life. Each live particle is a
  billboarded sprite quad pushed through the existing batched sprite path (one draw
  call per emitter, and particles pick up 2D lighting for free) via a pre-acquired
  handle pool. The pool is fixed-capacity and every rate/range is clamped, so no
  script or asset value can spawn unboundedly. The simulation is the pure,
  unit-tested `Particles2D` core. A typed `DioramaParticleRequestBus` (Common scope)
  exposes `Emit`, `Burst`, `Play` / `Stop`, `SetRate`, `SetGravity`, `SetLifetime`,
  `SetSpeed`, `SetDirection`, `SetStartColor` / `SetEndColor`, `SetStartSize` /
  `SetEndSize`, `SetTextureByPath`, and `GetParticleInfo` to Lua, Python, and Script
  Canvas. See [Docs/design/2d-particles.md](Docs/design/2d-particles.md).
- Gem-native 2D camera controller. A `2D Camera Controller` component placed on a
  camera entity follows a target with frame-rate-independent smoothing, a deadzone
  (small target motion does not move the view), optional world bounds, lookahead
  (the view leads the target's motion), optional pixel-perfect snapping, and
  trauma-based screen shake (`shake = maxShake * trauma^2`, applied after follow so
  it never fights tracking, decaying over time). It writes only the camera entity's
  translation, so an authored 2.5D tilt is preserved, and works in any of the three
  world planes (XY / XZ / YZ). All the feel math is the pure, unit-tested
  `Camera2D` core. A typed `DioramaCamera2DRequestBus` (Common scope) exposes
  `SetTarget`, `SetFollowOffset`, `SetSmoothTime`, `SetDeadzone`, `SetLookahead`,
  `SetBounds` / `ClearBounds`, `AddTrauma` (the gameplay juice hook), `SetEnabled`,
  and `GetCameraInfo` to Lua, Python, and Script Canvas. See
  [Docs/design/2d-camera.md](Docs/design/2d-camera.md).
- Gem-native 2D dynamic lighting (v1a). A `2D Light` component (directional sun or
  point light: color, intensity, direction or attenuation radius, on/off) placed in
  the scene and gathered by the sprite feature processor each frame into the sprite
  shader's per-draw lighting constants, so sprites are modulated by the lights:
  a point light brightens nearby sprites in its color with distance falloff, a
  directional light tints the whole sprite layer. Bounded to 8 lights (the count is
  clamped on the CPU before it ever sizes a loop). CVar-gated by
  `r_dioramaSpriteLighting` (default on) and `r_dioramaSpriteAmbient`; a scene with
  no lights, or lighting off, renders the exact prior unlit look. Decoupled from
  Atom's CoreLights (whose CPU light data is private and whose SRGs are not
  guaranteed to bind to a dynamic draw), so it is self-contained and verifiable. The
  editor component drives a live viewport preview. Normal-mapped relief is the next
  increment (v1b); see [Docs/design/2d-lighting.md](Docs/design/2d-lighting.md).
- Gem-native 2D collision reachable from gameplay scripts. A `2D Collider` component
  (circle or box, layer/mask matrix, trigger flag) and a collision system that
  detects overlaps each tick and dispatches contact and trigger events through
  buses reflected at `Common` scope, so launcher Lua, Python, and Script Canvas all
  receive them: `Diorama2DCollisionNotificationBus` (OnContactBegin/Stay/End,
  OnTriggerEnter/Exit), the per-collider `Diorama2DColliderRequestBus`, and the
  global `Diorama2DCollisionRequestBus` (OverlapCircle, OverlapBox, Raycast2D). This
  is the launcher-reachable contact path PhysX's collision bus is not (it is
  Automation-scope only). Decoupled from PhysX, with a configurable collision plane
  (XY / XZ / YZ, default the XY screen plane) so it matches whichever plane a 2.5D
  game moves on. The collision math (overlap, raycast, sort-and-sweep broadphase,
  begin/stay/end tracking) is a pure, header-only core; unit tests plus
  component/system integration tests cover it, and the twin-stick sample uses it for
  befriend-on-contact (validated in a running GameLauncher).
- Pure, header-only math cores for the upcoming camera and particle features, the
  parts verifiable without a viewport: frame-rate-independent camera follow,
  deadzone, bounds, lookahead, pixel snap, and trauma-based screen shake; and
  per-particle integration (velocity, gravity, drag), aging, and size/color over
  life with a pooled step. Both unit tested.
- Post-alpha feature roadmap ([Docs/roadmap.md](Docs/roadmap.md)) and a set of
  vetted design docs under [Docs/design/](Docs/design/) for the next 2D/2.5D
  features: dynamic lighting, collision-from-scripts, post-processing, per-sprite
  materials, a 2D camera component, particles, tilemap painting/autotiling,
  skeletal animation, Aseprite import, an in-editor slicer, a starter template, and
  an editor live-preview audit. Each is grounded in engine investigation with a
  chosen approach, security/performance notes, and a phased plan.
- Tween / easing library for gameplay scripts
  (`Assets/Diorama/Scripts/tween.lua`): a dependency-free helper that animates a
  value over time with easing (linear, quad, cubic, sine, back, elastic, bounce),
  with a per-owner tween group you update each tick. Pure logic, so it behaves
  identically in the launcher and editor; useful for size/tint/position pops,
  fades, and camera moves without hand-rolled per-frame code.
- Hello Sprite (rung 1) and Animated Sprite (rung 2) how-to guides with runnable
  examples, completing the written teaching ladder (rungs 1-6). Rung 1 builds a
  single sprite through the typed bus; rung 2 plays the 2x2 sample atlas as a
  four-frame sheet via `PlaySpriteSheet`.
- Parallax and Layers guide (teaching ladder rung 5): how to order overlapping
  sprites and tilemaps into layers with Sort Offset, and a reusable parallax
  script (`Assets/Diorama/Scripts/parallax_layer.lua`) that scrolls a layer at a
  fraction of the camera's motion for a 2.5D depth effect. Includes a runnable
  example that builds background/midground/foreground layers.
- Twin-stick shooter sample game (teaching ladder rung 6): a complete 2.5D "kill
  them with kindness" game. You play Obi, a jolly octopus who fires hearts at
  ocean "haters" (nine species spawned in ramping waves from the arena edges);
  three hearts fill a hater with love and it floats away happy. It ships a tilted
  2.5D camera over an ocean-floor tilemap, a LyShine "Befriended" HUD composited
  over the Diorama world layer, pause (P) / quit (Esc, gamepad Start) controls, a
  colour-shifting octopus (chromatophores), and a pooled heart-burst FX manager.
  Player, chase AI, hearts, and the wave spawner are all transform-driven Lua over
  the Diorama buses (see Changed for why no PhysX), so ordinary gameplay drives the
  same AI-native bus an agent would.
- 2.5D rendering in the sprite feature processor: automatic camera-distance depth
  sort, so nearer sprites draw in front within their sort layer
  (`r_dioramaSpriteDepthSort`); and soft ground shadows under billboarded sprites
  (`r_dioramaSpriteShadows`, opacity `r_dioramaShadowAlpha`), drawn in one batch
  between the floor and the sprites.
- Tilemap component (teaching ladder rung 4): a world-space grid of cells, each
  drawing one cell of a shared atlas, rendered through the same batched sprite
  feature processor so a whole layer collapses into one draw call. Authored in
  the editor inspector (atlas/grid/appearance) and driven by an AI-native
  `DioramaTilemapRequestBus` (SetGridSize, SetAtlasGrid, SetTile, Fill, Clear,
  SetTileSize, SetTint, SetSortOffset, GetTilemapInfo) with named, documented,
  clamped verbs. Runtime and editor components share one config and presenter,
  so it draws identically in game and in the editor viewport. Unit-tested tile
  UV / grid layout math and EBus dispatch; how-to guide and runnable example.
- Sprite-sheet (flipbook) animation on the Sprite component: a uniform frame
  grid (columns, rows, frame count) played on a timer, with frames-per-second,
  looping or hold-on-last, and a start frame. Frames compose with the existing
  atlas UV sub-region and flips. Driven through the shared presenter so it plays
  identically at runtime and in the editor viewport.
- Unit tests for per-frame UV regions and the frame-advance logic.
- How-to guide and runnable example for the Sprite Atlas (teaching ladder rung
  3): sharing one atlas texture across sprites via UV sub-regions, with a
  scripted four-cell example scene and a how-to index.
- AI-native sprite API (`DioramaSpriteRequestBus`): a typed, reflected,
  queryable EBus so an agent can drive a sprite with plain scalar verbs
  (SetTextureByPath, SetSize, SetUVRegion, SetSortOffset, SetDoubleSided,
  PlaySpriteSheet, ...) instead of brittle EditContext property-path strings,
  plus a GetSpriteInfo query and a notification bus. Reflected to the
  BehaviorContext (callable from Python as `azlmbr.diorama`), with a runnable
  example (`Docs/examples/sprite_via_ai_bus.py`). See
  `Docs/design/ai-sprite-api.md`. Verb logic and GetSpriteInfo are unit tested;
  Python reachability and the GetSpriteInfo readback are confirmed. End-to-end
  editor-scripted verb application still depends on the sprite component being
  active, which the current editor `--runpython` flow does not guarantee; that
  activation path is the remaining follow-up.
- Per-argument names and descriptions on every `DioramaSpriteRequestBus` verb,
  reflected to the BehaviorContext. These name and document each argument for
  Script Canvas node pins and for any tool that introspects the reflection to
  build an agent-facing API schema. (The editor's generated Python stub lists
  EBus arguments by type only, so the names live in the C++ reflection rather
  than the `.pyi`.)
- Per-sprite "Double Sided" toggle (default on): a flat sprite is visible from
  both sides, matching how 2D sprite renderers behave elsewhere. Implemented
  geometrically (the renderer emits back-facing triangles for double-sided,
  non-billboard sprites) so it needs no separate shader and does not affect
  batching. Turn it off to hide a sprite when viewed from behind.
- Build/test CI: a self-hosted `build-test` workflow that compiles the gem
  through a host O3DE project and runs the unit tests, plus a reusable
  `scripts/ci_build_test.sh` and a self-hosted runner setup guide. Complements
  the always-on lint workflow.

### Changed
- Twin-stick gameplay reworked from PhysX to transform-driven movement with
  distance-based hit detection. Two engine facts drove it: a dynamic PhysX rigid
  body ignores the entity's authored editor transform at simulation start (it
  initializes at the world origin), and PhysX collision callbacks are reflected
  only in the Automation script scope, so a launcher Lua script never receives
  `OnCollisionBegin`. Entities now move via `TransformBus`, stay in bounds by a
  clamp, and detect hits by distance through a shared live-entity table. Kills are
  replaced by a "befriend" mechanic (three hearts and a hater floats away); the
  HUD counts befriended haters via a shared global rather than a cross-entity
  `GameplayNotification`.
- The sprite shader disables the depth test and back-face culling, so a flat floor
  renders correctly under a tilted 2.5D camera and draw order (painter's, by sort
  layer then camera distance) is authoritative.
- Sprite rendering now goes through a proper Atom scene feature processor
  (`SpriteFeatureProcessor`) instead of an immediate-mode per-sprite draw loop.
  Sprites that share a texture and sort layer are batched into a single draw call
  with one shader resource group, which scales to large sprite counts. The
  immediate-mode `SpriteRenderer` and its request bus are removed; components
  register with the scene's feature processor through the shared presenter, which
  retries scene resolution per tick so sprites whose scene is not ready at
  activation still appear once it is.
- Batching keys on the full texture asset id (no lossy hash) and a float sort
  offset (fractional 2.5D layers stay distinct), uses 32-bit indices (batches
  may exceed 16384 sprites), reuses per-frame scratch buffers, and caches each
  sprite's batch key, so the render loop performs no per-frame heap allocation.
- The batch plan is dirty-tracked: the grouping and sort are rebuilt only when a
  sprite is added, removed, or its texture/sort layer changes, so a static scene
  skips the per-frame scan and sort (vertex packing and submission still run each
  frame). Transform, animation, and texture-streaming changes do not trigger a
  rebuild.
- Unit tests for the batch-planning logic (grouping, sort-offset ordering,
  fractional-layer separation, invalid-texture handling, stable in-batch order,
  and buffer reuse).

### Fixed
- The 2.5D floor vanished under a pitched camera. Root-caused (by reading the
  engine source) to the sprite shader's depth test, not back-face culling and not
  frustum culling; disabling the depth test is the correct model for this
  draw-order-ordered sprite renderer, and the floor now renders under the tilt.
- The score HUD never updated. The reflected Lua name `UiCanvasBus.FindElementByName`
  is bound to `FindElementEntityIdByName` and returns the element's EntityId
  directly (so `:GetId()` on it was wrong); the count now also flows through a
  shared global polled by the HUD controller, matching how the rest of the sample
  communicates.
- Per-spawn "property not found" log spam from reading `.lua` properties that the
  runtime `ScriptComponent` has no baked value for (it does not apply `.lua`
  defaults). Uniform game constants moved to script locals; only genuinely
  per-instance values stay as baked, tunable properties.
- Deadlock that froze the editor (and would freeze a runtime client) the first
  frame a sprite became drawable. `SpriteFeatureProcessor::Render` runs as a
  render job the main thread blocks on, and it loaded the sprite shader with a
  blocking call (`LoadShader` -> `BlockUntilLoadComplete`); the main thread
  waited for the job while the job waited for the asset. The shader now loads
  asynchronously in `Activate`, and the per-frame init only polls readiness, so
  nothing blocks inside the render job. Verified on screen: sprites render and
  the editor stays responsive.

### Removed
- Orphaned pre-theme creature prefabs (Shark, Bird, Fish, Whale, and a generic
  Enemy base) and the unused shark texture, left over from before the ocean-hater
  cast. Nothing referenced them; the live cast is generated from one creature list.

### Contributed upstream
- Filed o3de/o3de#19804 with a fix in PR #19805: a use-after-free in
  `AzFramework::ScriptComponent::DestroyEntityTable` when a Lua-scripted entity is
  torn down during shutdown after the script context (and its `lua_State`) has been
  freed, found while testing the sample's quit path. Diorama complements the engine
  and contributes such findings back rather than patching around them.

## [0.1.0-alpha] - 2026-05-29

First alpha. World-space sprite rendering through Atom, with editor tooling and
the runtime/editor module split in place. The `gem.json` version tracks the
`0.1.0` release line; this git tag carries the `-alpha` pre-release marker.

### Added
- Diorama gem: world-space 2D/2.5D sprite rendering through Atom, with a
  lightweight runtime client module and a separate Qt editor module.
- Sprite component that renders a world-space quad via Atom dynamic draw.
- Editor viewport preview of sprites through a shared presenter.
- Atlas UV sub-regions (`UV Min` / `UV Max`), horizontal and vertical flip, and a
  2.5D sort offset for draw-order control.
- Unit tests covering sprite UV region and flip mapping (`SpriteUVTest`).
- Project documentation: README, VISION, examples/how-to outline, and license.

### Fixed
- Static `AZ::Name` initialization-order crash in `SpriteRenderer`. A
  namespace-scope `AZ::Name` constructed during shared-library load, before the
  AzCore `NameDictionary` existed, causing a SIGSEGV when the test library was
  loaded ahead of engine bootstrap. It is now constructed lazily on first use.

[Unreleased]: https://github.com/nickschuetz/o3de-diorama/compare/v0.3.0-beta...HEAD
[0.3.0-beta]: https://github.com/nickschuetz/o3de-diorama/releases/tag/v0.3.0-beta
[0.2.0-beta]: https://github.com/nickschuetz/o3de-diorama/releases/tag/v0.2.0-beta
[0.1.0-alpha]: https://github.com/nickschuetz/o3de-diorama/releases/tag/v0.1.0-alpha
