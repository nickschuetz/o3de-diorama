# Changelog

All notable changes to Diorama are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/). Before
1.0 (the 0.x line), minor releases may include breaking changes.

## [Unreleased]

### Added
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

[Unreleased]: https://github.com/nickschuetz/o3de-diorama/compare/v0.2.0-beta...HEAD
[0.2.0-beta]: https://github.com/nickschuetz/o3de-diorama/releases/tag/v0.2.0-beta
[0.1.0-alpha]: https://github.com/nickschuetz/o3de-diorama/releases/tag/v0.1.0-alpha
