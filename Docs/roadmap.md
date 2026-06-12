# Diorama roadmap: toward a credible 2D/2.5D engine

This is the feature roadmap beyond the current beta. It is organized by *why* a
feature matters, not just what it is. The guiding thesis: Diorama renders 2D
through **Atom**, so it can reach modern lighting, post, and material features
that pure-2D engines cannot. That is the moat; we lean into it.

Effort is a rough order of magnitude: **S** (days), **M** (1-2 weeks), **L**
(multi-week / a subsystem). Most of these are gem C++ plus GPU/shader work and
need on-screen verification, so they ship one at a time, not in a batch.

## Already shipped (the base)

World-space sprites, texture atlases, UV sub-regions, flips, billboarding,
sort layers, **automatic camera-distance depth sort**, **soft ground shadows**,
a batched Atom feature processor, sprite-sheet (flipbook) animation, an
atlas-grid tilemap component, and the typed per-feature buses (Lua / Python /
ScriptCanvas). An early 2.5D twin-stick sample (not yet polished) is kept out of
the published tree for now.

Most of the tiers below have since shipped (each tracked inline as **Shipped**):
2D lighting, the 2D camera (with versus framing + zoom), 2D collision (with pushbox
resolution), particles, sprite materials, parallax, audio, the 2D Look
post-profile + CRT overlay, the in-editor tilemap paint tool, autotiling (4-bit +
47-blob), a dedicated tilemap asset + builder (`.dtilemap` + Tiled `.tmj` import,
multi-layer), native `.aseprite` import (parser + AssetBuilder), skeletal cutout
animation, point-filter pixel art, sprite transpose/rotation, animation frame events,
and hit-stop, plus the `Diorama2DGame` project template. What remains are the deeper
versions (warping CRT pass, DragonBones mesh deform) and the flagship demo.

## Tier 1 - Differentiators (lean into Atom)

What makes a 2D game look modern/AAA, and what pure-2D engines do awkwardly:

- **2D dynamic lighting** (L). Normal-mapped sprites that take real Atom lights
  (point/spot/directional), with ambient and emissive control. The single
  loudest "this is a real modern 2D engine" feature. **Design done** (see
  [Docs/design/2d-lighting.md](design/2d-lighting.md)): the feature processor
  gathers scene lights on the CPU into a Diorama-owned SRG and the sprite shader
  loops them against a per-sprite normal map, which avoids Forward+ culling and
  any dependency on Atom pass SRGs being bound to our dynamic draw. Phased
  directional then point then spot. Implementation pending on-screen verification.
- **2D post-processing** (M). Expose Atom's bloom, color grading, vignette, and a
  CRT/scanline option for the 2D scene. Glowing sprites and filmic/retro looks
  mostly for free via Atom passes; the work is a clean, 2D-friendly control
  surface (a post profile component or CVars). **Design done**
  ([design/2d-post-processing.md](design/2d-post-processing.md)): Atom already
  post-processes our transparent sprites in HDR, so lean on stock PostFx components
  + an emissive hook for bloom, and ship only a CRT fullscreen pass.
  **In progress**: the **emissive hook shipped** -- sprites can write HDR > 1.0 and
  bloom through a stock PostFX Layer + Bloom (`SetEmissive(r,g,b,intensity)` on the
  sprite bus; per-sprite material constant; verified rendering over-bright glowing
  mascots). How-to: [howto/14-glow.md](howto/14-glow.md). A **CRT scanline overlay**
  also shipped (DioramaCRTComponent + DioramaCRTRequestBus + editor twin; screen-space
  AuxGeom scanlines + flat darkening, verified rendering; [howto/16-crt.md](howto/16-crt.md)).
  **Shipped**: the **2D Look** profile component (`DioramaLookComponent` +
  `DioramaLookRequestBus` + editor twin with a live edit-viewport preview) -- one
  component driving Atom's stock `PostProcessFeatureProcessor` for tuned bloom +
  vignette with 2D-friendly defaults, verified A/B in-editor; [howto/14-glow.md](howto/14-glow.md).
  Remaining: the true warping CRT *pass* (barrel curvature / chromatic aberration),
  which needs a fullscreen post-process pass injected into the render pipeline.
- **Per-sprite materials / effects** (M). A small material surface for outline,
  flash, dissolve, hue-shift, and additive/blend modes, so common effects are a
  property, not hand-rolled Lua. Outline + flash alone replace a lot of gameplay
  juice code. **Design done** ([design/2d-materials.md](design/2d-materials.md)):
  cheap effects ride the vertex stream to stay batched; richer ones key into the
  batch as variants.

## Tier 2 - Table-stakes (needed to ship real games)

- **2D physics + collision reachable from scripts** (L). Box/circle/polygon
  colliders, triggers, raycasts, and contact callbacks delivered to *game* Lua
  (the current PhysX collision path is Automation-scope only, which is why the
  sample is transform-driven). The single biggest gameplay gap; unblocks whole
  genres. **Design done** ([design/2d-collision.md](design/2d-collision.md)):
  gem-native 2D colliders + a `Common`-scoped contact/trigger bus, decoupled from
  PhysX; the Automation->Common scope fix is a separate candidate upstream
  contribution (needs go-ahead before filing).
- **Pixel-perfect / orthographic camera** (M). Integer scaling, pixel snapping,
  and an ortho projection. Non-negotiable for pixel-art games. Folded into the
  camera-component design below (a pixel-perfect mode).
- **2D camera component** (S-M). Follow, smoothing, deadzone, bounds, lookahead,
  and screen shake (a Cinemachine-2D analogue). Every 2D game needs it; cheap and
  high impact, and it absorbs the gameplay-juice work. **Shipped**
  ([design/2d-camera.md](design/2d-camera.md), [howto/08-camera.md](howto/08-camera.md)):
  a C++ `2D Camera Controller` with damped follow, deadzone, lookahead, world
  bounds, trauma shake, and a pixel-perfect mode, all on the
  `DioramaCamera2DRequestBus`. Extended with **versus framing** (`SetSecondaryTarget`
  centers on the midpoint of two targets) and **distance zoom/dolly** (`SetZoom`
  manual, `SetAutoZoom` auto-dollies as the targets separate) for fighting and
  co-op layouts.
- **Tilemap tooling** (L). In-editor tile *painting*, autotiling / rule tiles,
  multiple layers, animated tiles, and per-tile collision shapes. Tilemaps are
  data-driven only today. **Design done**
  ([design/2d-tilemap-tooling.md](design/2d-tilemap-tooling.md)): a custom editor
  Component Mode for discrete stamping (borrowing GradientSignal's undo pattern) +
  our own neighbor-bitmask autotiling. **Shipped (paint v1)**: an editor component
  mode on the Tilemap (left-drag paint with the active tile, right-drag erase, one
  undo step per stroke) on the pure tested `TilemapPaint.h` core + `LocalPositionToCell`;
  interactively verified ([howto/04-tilemap.md](howto/04-tilemap.md)). **Shipped
  (autotile v1)**: the `Autotile(baseTile)` bus verb on the pure tested
  `TilemapAutotile.h` core -- rewrites every non-empty cell to baseTile + a 4-bit edge
  mask of its cardinal neighbors so a 16-cell art block connects itself
  ([howto/04-tilemap.md](howto/04-tilemap.md#autotiling-borders-connect-themselves)).
  **Shipped (blob)**: `AutotileBlob(baseTile)` -- the 47-tile corner-aware scheme on the
  same `TilemapAutotile.h` core (a corner counts only when both adjacent edges are
  present, reducing 256 neighbor combos to exactly 47). **Shipped (asset + layers)**: a
  dedicated `DioramaTilemapAsset` (`.dtilemapc`) with a custom `.dtilemap` JSON builder
  and a Tiled `.tmj` importer, **multiple layers**, and **per-tile flip/rotate**
  (orientation bits packed above the atlas index), driven by the `SetTilemapByPath`
  bus verb and the editor's **Tilemap Asset** field. **Shipped (v2 rule tiles)**:
  custom **Autotile Rules** (normalized neighbor-mask -> display offset) on the pure
  `TilemapAutotile::RuleSetOffset` core, driven by the `AutotileRules` bus verb, for
  tilesets not laid out in the canonical blob order
  ([design/2d-tilemap-v2.md](design/2d-tilemap-v2.md)). **Shipped (per-tile
  collision)**: a **Solid Tiles** config set whose cells are greedy-meshed
  (`TilemapCollision.h`) into static box colliders registered with the 2D collision
  world (`SetStaticColliders`), so a moving collider blocks against the map via the
  overlap / raycast / push-out queries ([howto/04-tilemap.md](howto/04-tilemap.md)).
  **Shipped (animated tiles)**: an **Animated Tiles** config list (painted tile index
  -> a cycling sequence of atlas frames at an fps, with a loop flag) on the pure tested
  `TilemapAnimation::FrameAtTime` core, driven in the presenter off one map-wide clock
  (animated cells re-push only when their frame changes; a static map never ticks) and
  authored by the `DefineAnimatedTile` / `ClearAnimatedTiles` bus verbs
  ([design/2d-tilemap-v2.md](design/2d-tilemap-v2.md)). All three tilemap-v2 items are
  in. Remaining: opt-in tile contact events and an oriented-plane mode for rotated maps.
- **Skeletal 2D animation** (L). Bone deformation (Spine / DragonBones style), the
  AAA-2D animation standard. **Design done**
  ([design/2d-skeletal-animation.md](design/2d-skeletal-animation.md)): phased
  cutout (transform hierarchy) then open-format (DragonBones) 2D mesh deform
  through our renderer; EMotionFX stays an optional advanced evaluator, not a
  runtime dependency. **Shipped (cutout v1)**: `DioramaSkeletalClipComponent` +
  `DioramaSkeletalRequestBus` + editor twin with a non-destructive pose-scrub
  preview, on the pure tested `SkeletalClip.h` keyframe/easing core; bones are
  descendant entities matched by name, posed via TransformBus each tick
  ([howto/18-skeletal.md](howto/18-skeletal.md)). **Shipped (cross-fade)**: a named
  clip library + `CrossFadeTo(clip, duration)` that blends the current and target
  clips per bone over a transition, on the pure tested `SkeletalClip::BlendPose` +
  `CrossfadeWeight` cores (the same primitives back 1D blend trees later). Remaining:
  1D blend trees, and the DragonBones open-format mesh-deform path (v2, an open
  MIT-licensed format the builder can parse, reusing the same sampling core).
- **2D particle system** (M). A real emitter component (the sample's heart-burst
  pool, generalized): rate/burst, velocity/gravity/drag, size/color over life,
  blend modes. **Design done** ([design/2d-particles.md](design/2d-particles.md)):
  CPU-simulated pooled particles fed through the existing sprite batch path.

## Tier 3 - Pipeline and ease of use (indie + AI-friendly)

- **Aseprite / sprite-sheet import** (M). Slice sheets and import `.aseprite`
  files with frame tags. The indie art pipeline. **Design done**
  ([design/2d-aseprite-import.md](design/2d-aseprite-import.md)): a custom asset
  builder parses the open `.ase` format and delegates image compression to the
  existing image pipeline; v1 output plugs into the current flipbook animation.
  **Shipped (JSON v1)**: import Aseprite's *Export Sprite Sheet* PNG + JSON (both
  Hash and Array forms), play named tags with per-frame durations and direction
  (forward/reverse/ping-pong) via `DioramaAsepriteComponent` + `DioramaAsepriteRequestBus`;
  the editor twin does the import, the runtime drives a Sprite's texture + UV. Pure
  tested `AsepriteImport.h` core ([howto/19-aseprite.md](howto/19-aseprite.md)).
  **Native binary (phase 1) shipped**: `AsepriteBinary.h/.cpp` parses the native
  `.aseprite`/`.ase` container (header, frames, layers, cels incl. ZLIB-compressed via
  AzCore's portable inflate, tags) into an in-memory model and composites visible
  layers per frame to RGBA; bounds-checked against untrusted input. **Phase 2 shipped**:
  `DioramaAsepriteBuilder` registers `*.aseprite`/`*.ase`, parses the native binary,
  packs the composited frames to a grid atlas, and emits the image + sheet-metadata
  products that the Aseprite component/bus already consume. **Phase 3 shipped**: all
  three color depths (32-bpp RGBA, 16-bpp grayscale, 8-bpp indexed via the palette
  chunk + transparent index) and the separable layer blend modes (multiply/screen/
  overlay/darken/lighten/dodge/burn/hard+soft-light/difference/exclusion/add/subtract/
  divide), on the pure tested `BlendModeChannel` + crafted-file parser tests; the HSL
  modes (hue/sat/color/luminosity) remain a documented fall-back to normal.
- **In-editor sprite/atlas slicer + animation clip editor** (M). Define frames,
  fps, loop, and frame events visually. **Design done**
  ([design/2d-editor-authoring.md](design/2d-editor-authoring.md)): a Qt front-end
  over the config fields that already exist; auto-grid first, free-form rects later.
- **Tween / easing library** (S). **Done** - `Assets/Diorama/Scripts/tween.lua`:
  animate a value over time with easing (linear/quad/cubic/sine/back/elastic/
  bounce), driven by a per-owner tween group. Removes a lot of boilerplate.
- **Instant-start "New 2.5D Game" template** (S). Enabling the gem lands you in a
  working scene (camera, floor, a sprite, input). **Quick-start shipped**: a
  playable starter (a player you move with WASD / arrows / left stick, a ground,
  and a camera) via the reusable `player_move_2d.lua` controller +
  `diorama/input/diorama_move.inputbindings` + the `quickstart_demo.py` scene
  builder + [howto/17-quickstart.md](howto/17-quickstart.md). **Default-texture
  robustness shipped**: a Sprite with no texture now falls back to a bundled default
  in the feature processor, so a freshly added Sprite is visible (and tintable)
  instead of vanishing (capture-verified). **Project template shipped**: the
  gem-registered `Diorama2DGame` O3DE project template (under `Templates/`), so
  `o3de create-project --template-name Diorama2DGame` scaffolds a buildable 2.5D
  project with Diorama enabled + 2.5D starter assets + a `STARTING.md` guide;
  verified by running create-project ([howto/20-template.md](howto/20-template.md)).
  Remaining from the design ([design/2d-starter-template.md](design/2d-starter-template.md)):
  a fully pre-populated demo level baked into the template.
- **Editor live-preview** (S). **Done / verified.** Audited every editor twin: each
  reflects its `Config` as a single element wired `ChangeNotify -> OnConfigChanged`,
  and that handler re-pushes the whole config to the preview (`SetConfig` /
  `ApplyPreview`), so *every* Inspector property of a previewing component (Sprite,
  Tilemap, 2D Light, 2D Look, Skeletal, Aseprite) live-updates the viewport. The
  remaining components (input, collision, camera, particles, anim state machine, CRT,
  parallax) have no edit preview by nature -- they are runtime behaviors seen in
  play / game mode. The old "doesn't live-update" README note was stale and has been
  corrected to state the authored-vs-runtime boundary (it is by design, not a gap).
  Reflecting *runtime request-bus* mutations in the static edit preview is a non-goal
  (it would duplicate every bus on the editor twin and blur O3DE's authored/runtime
  split; agents author in edit mode via the component-property API, which already
  refreshes the preview).

## Recommended sequence

Ordered by credibility-per-effort, knocking them out one at a time with
verification between each:

1. **2D camera component** (S-M) - quickest credibility win, every game uses it,
   absorbs the deferred juice (follow + shake).
2. **2D dynamic lighting** (L) - the headline differentiator; start its design in
   parallel since it is the longest pole.
3. **2D physics/collision from scripts** (L) - unblocks gameplay genres.
4. **Per-sprite materials (outline/flash)** + **2D particles** (M each) - the
   "feels alive" tier.
5. **Pixel-perfect camera**, **tilemap painting/autotiling**, **post-processing**.
6. **Skeletal animation**, **Aseprite import**, **tween lib**, **template**,
   **live-preview fix** as they fit.

These are tracked as individual efforts rather than one batch: each is a gem
change that needs to be seen running before it counts as done.

## Backlog / follow-ups

- **Reproducible demo levels.** The per-feature demo *levels* (lighting,
  particles, camera, materials, parallax, sidescroller) currently exist only in
  the local sandbox project, authored by hand / by `scripts/prep_demo_camera.py`.
  Turn that hand-authoring into committed prefab-generator scripts so the levels
  rebuild from the repo, then they can be captured headlessly in CI via
  `scripts/capture_level.sh`.
- **Sidescroller polish.** Composed slice works (parallax + torch lights +
  embers + walking/spinning player) but needs a live-editor pass: tone down the
  torch intensity and fix the oblique camera framing to a clean flat side view.
- **Side-profile / directional character art.** The billboard player faces the
  camera, so it can only flip or coin-spin; a true side-scroller character needs
  a side-profile (or multi-frame directional) sprite asset.

## Gaps to AAA / indie parity (not yet tiered)

Diorama will not win on raw 2D feature parity with Unity or Godot. Its two real
differentiators are **Atom-quality visuals on 2D** (lean into lighting + post) and
**AI-friendliness** (a stable, typed, request-bus surface that an agent or Script
Canvas drives exactly like a human). The path to "worthy" is to close the missing
table-stakes below, then double down on those two and prove it with the flagship
([living-diorama.md](design/living-diorama.md) + [magic-eye](design/magic-eye-autostereogram.md)).

Missing table-stakes (no design doc yet):

- **Audio.** O3DE 26.05 ships the **MiniAudio** gem (lightweight, dependency-free;
  enabled in this project) with a per-entity Playback component + listener and a
  `MiniAudioPlaybackRequestBus` reflected for Lua/Python/Script Canvas. So audio is
  present and script-drivable; Diorama's job is the 2D-friendly path + conveniences,
  not a new engine. **In progress**: blessed-path how-to ([howto/15-audio.md](howto/15-audio.md)),
  a sample SFX (`diorama/audio/blip.wav`, processes to `.miniaudio`), Lua examples,
  and the **`DioramaAudioRequestBus::PlayOneShot(path, volume)`** convenience -- a
  global, `Common`-reflected fire-and-forget call backed by an 8-voice pool of
  MiniAudio Playback entities (the one verbose 2D gap, now closed). Builds green,
  133 tests pass, and **verified audible** on a workstation (the GameLauncher/editor
  with an audio device; headless capture has no audio output, so sound is the one
  thing the capture pipeline cannot check).
- **Screen-space UI / HUD: out of scope (LyShine owns it).** Diorama is world-space
  only (see [VISION.md](../VISION.md)); screen-space UI is LyShine's domain and
  Diorama does not overlap it. An early screen-space HUD component was removed for
  this reason. In-world HUD elements (a health bar or icon pinned above an entity)
  are just sprites and are covered by the sprite how-to; world-space text (floating
  damage numbers, nameplates) is a possible future feature (it needs a bitmap-font
  or text-to-texture approach, since the engine font interface is screen-space).
- **Animation state machine.** **Shipped (v1)**
  ([design/2d-animation-state-machine.md](design/2d-animation-state-machine.md),
  [howto/22-anim-state-machine.md](howto/22-anim-state-machine.md)): a
  parameter-driven graph (Bool/Float/Trigger parameters, conditions, exit-time and
  Any-State transitions) on the pure tested `AnimStateMachine.h` core, driven through
  `DioramaAnimStateMachineRequestBus` and playing the bound Sprite-sheet or Aseprite
  tag on the target each state change (with an `OnStateChanged` notification). Flipbook
  and skeletal give *clips*; this decides which one plays. Remaining (v2): blend trees
  / cross-fades.
- **Input action-mapping.** **Shipped**
  ([howto/23-input-actions.md](howto/23-input-actions.md)): a rebindable surface of
  named actions (Button / Axis1D / Axis2D, per-action dead zone + press threshold)
  over input channels, on the pure tested `InputActionMap.h` core, read through
  `DioramaInputRequestBus` (values + per-frame pressed/released edges) with
  `OnActionPressed`/`OnActionReleased` notifications. Replaces the twin-stick's raw
  input wiring. **Extended (motion inputs)**: a direction action quantized to numpad
  notation feeds the pure tested `MotionInput` core, so authored motions (`236`
  quarter-circle, `623` dragon-punch) are recognized within a per-motion window via
  `WasMotionPerformed` (buffered poll) + `OnMotionPerformed`.
- **Fighting building blocks v2.** **Shipped**
  ([howto/21-fighting.md](howto/21-fighting.md)): a first-class **2D Frame-Data
  Hitboxes** component (`DioramaHitboxComponent`) authors a rig of hitboxes/hurtboxes,
  each live on an animation-frame window (pure tested `HitboxFrames` core). Each frame
  it registers live hurtboxes as collision geometry and tests live hitboxes against a
  target layer, firing `OnHit`/`OnHurt` once per opponent per active window, with
  `SetFacing` mirroring and a read-only `GetHitboxInfo`. Plus motion inputs (above) and
  an impact-effects sample (`hit_response.lua`: hit-spark particle preset + material
  flash + camera trauma). Builds on the existing frame events, versus camera, hit-stop,
  and pushbox pieces. Charge-motion hold timing and rollback stay game-side.
- **Bullet-pattern emitter (danmaku / shmup).** **Shipped**
  ([howto/24-bullet-patterns.md](howto/24-bullet-patterns.md)): a **2D Bullet Emitter**
  component (`DioramaBulletEmitterComponent`) fires Ring / Fan / Spiral patterns at a
  fire rate (pure tested `BulletPattern` core), pools and integrates the bullets with
  `Particles2D`, renders the pattern in one draw call through the sprite batch, and
  hit-tests each live bullet against a target collision layer (`OnBulletHit` + consume).
  Driven by `DioramaBulletRequestBus`; fixed-capacity pool. Reproducible sample
  `Assets/Diorama/Examples/Shmup/enemy_danmaku.lua`. Damage/scoring stay game-side.
- **Side-scroller platforming (one-way platforms + ramps).** **Shipped**
  ([howto/25-platformer.md](howto/25-platformer.md)): the 2D collision world gains the
  platformer primitives on the pure tested `SlopeCollision` core. A `m_oneWay` flag on
  the 2D Collider (Inspector + `SetOneWay`) that `ComputeBoxPushOut` resolves with an
  upward-only push (`OneWayPushOut`); and `ProbeGroundY` ground-follow over flat ground
  and ramps, fed by `AddGroundSegment` (script/agent) or `SetGroundSegments` (C++). A
  side-scroller sample ships under `Assets/Diorama/Examples/SideScroller/`. Driving
  slope tiles straight from a tilemap is a follow-up (needs the tilemap per-tile
  collision to support the XY screen plane; today it projects to the XZ ground plane).
- **2.5D brawler depth lanes (beat-em-up).** **Shipped**
  ([howto/26-brawler.md](howto/26-brawler.md)): a **2.5D Depth Body** component
  (`DioramaDepthBodyComponent`) owns a character's depth (toward/away from the camera)
  and renders it as an up-screen lift + draw-order bias so a flat orthographic scene
  reads as 2.5D, on the pure tested `DepthLane` core. Driven by
  `DioramaDepthBodyRequestBus` (`SetDepth`/`MoveDepthToward`/`GetDepth`/`GetDepthInfo`).
  Depth-aware combat composes the frame-data hitbox component (gate `OnHit` by
  `SameLane`, or author hitboxes on the XZ floor plane). Brawler sample under
  `Assets/Diorama/Examples/Brawler/`.
- **Day/night cycle (lighting).** **Shipped**
  ([howto/28-day-night.md](howto/28-day-night.md)): a `DioramaDayNightComponent` advances a
  time-of-day clock and drives a target Diorama light's color, intensity, and direction
  over the day, on the pure tested `DayNightCycle` core (four-phase gradient + sun
  direction). A thin complementary layer over the lighting feature (no new light or
  rendering); clock knobs on `DioramaDayNightRequestBus`. Driving multiple lights (a fill /
  moon) and a sky/atmosphere tie-in are follow-ups; Atom owns skyboxes.
- **Grid intelligence cores (roguelike / tactics).** **Shipped**
  ([howto/27-grid-intelligence.md](howto/27-grid-intelligence.md)): three pure tested
  header algorithms over a tile grid -- A* pathfinding (`Pathfinding::FindPath`),
  movement range (`MovementRange::ComputeReachable`, Dijkstra flood with per-cell
  costs), and field of view (`FieldOfView::Compute`, recursive shadowcasting) for fog
  of war. Each takes a grid predicate (no engine coupling). FOV/fog is in-vision;
  movement-range and A* are general grid utilities. A tilemap-backed bus is a follow-up.
- **Off-screen culling.** **Shipped**: the sprite feature processor builds the view
  frustum each frame and skips any sprite whose bounding sphere is fully outside the
  side planes, on the pure tested `SpriteCull.h` core (conservative and
  reverse-depth-independent, so it never hides a visible sprite; toggled by
  `r_dioramaSpriteCull`). Saves the vertex packing and draw for off-screen sprites in
  large sparse scenes. (Game-state **save/load** is intentionally out of scope: it is
  generic game infrastructure served by O3DE's serialization, not world-space 2D
  rendering.)
- **Large / streaming worlds** (community ask). Diorama itself ships no
  chunk-pager or LOD; its components are plain per-entity components that register
  on activate and unregister on deactivate, so they already spawn and despawn
  cleanly with O3DE's spawnable/prefab streaming, and tilemap chunks can be
  hot-swapped at runtime via `SetTilemapByPath`. Off-screen sprite culling already
  ships (above). What is still missing is a region/grid paging layer on top of the
  spawnable system (stream prefabs and tilemap chunks by camera proximity), so an
  open 2.5D world with thousands of entities (grass, trees, walls all as entities,
  per the *Edentopia* approach) stays affordable. A natural complementary gem rather than
  core, but worth a design once a concrete world size/shape is in hand.

Recommended order (closes gaps, then leans into the differentiators): audio ->
post-processing (already designed, biggest "looks AAA" payoff) -> animation depth
(clip editor + Aseprite import and/or skeletal) -> finish the AI-friendly API
(the moat) -> starter template -> flagship demo.

## Beyond games: simulation and robotics

A 2D engine has no robotics-*simulation* story, but as a 2D content layer Diorama has
a narrow, honest supporting role: authoring and live-streaming the inherently-2D
content of a 3D sim (live occupancy grids / costmaps first, then fiducials and floor
markings the simulated sensors can see), feeding a fully open Fedora-RPM stack. It is
not a robotics tool, and the engine's robotics story should lead with the real
robotics tech (ROS 2 Gem, sensors, physics). Scope and limits in
[design/simulation-robotics.md](design/simulation-robotics.md).
