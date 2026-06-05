# Diorama roadmap: toward a credible 2D/2.5D engine

This is the feature roadmap beyond the current alpha. It is organized by *why* a
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
2D lighting, the 2D camera, 2D collision, particles, sprite materials, parallax,
UI/HUD, audio, the 2D Look post-profile + CRT overlay, the in-editor tilemap paint
tool, skeletal cutout animation, Aseprite import, and the `Diorama2DGame` project
template. What remains are the deeper versions (warping CRT pass, autotiling,
DragonBones mesh deform, native `.aseprite`) and the flagship demo.

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
  high impact, and it absorbs the gameplay-juice work. **Design done**
  ([design/2d-camera.md](design/2d-camera.md)): drive the camera transform with
  damped follow + trauma shake; sample script first, then a C++ component.
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
  present, reducing 256 neighbor combos to exactly 47). Remaining: a `DioramaTilesetRule`
  asset for custom mask->cell layouts, multiple layers, animated tiles, per-tile collision.
- **Skeletal 2D animation** (L). Bone deformation (Spine / DragonBones style), the
  AAA-2D animation standard. **Design done**
  ([design/2d-skeletal-animation.md](design/2d-skeletal-animation.md)): phased
  cutout (transform hierarchy) then open-format (DragonBones) 2D mesh deform
  through our renderer; EMotionFX stays an optional advanced evaluator, not a
  runtime dependency. **Shipped (cutout v1)**: `DioramaSkeletalClipComponent` +
  `DioramaSkeletalRequestBus` + editor twin with a non-destructive pose-scrub
  preview, on the pure tested `SkeletalClip.h` keyframe/easing core; bones are
  descendant entities matched by name, posed via TransformBus each tick
  ([howto/18-skeletal.md](howto/18-skeletal.md)). Remaining: the DragonBones
  open-format mesh-deform path (v2, reuses the same sampling core).
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
  layers per frame to RGBA; bounds-checked against untrusted input; v1 is 32-bpp RGBA.
  Remaining (phase 2): the asset builder that packs the composited frames into an atlas
  product (delegating compression to the image pipeline) and feeds the existing Aseprite
  component/bus; then indexed/grayscale color depths and non-normal blend modes.
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
- **Editor live-preview** (S). Make sprite property edits update the viewport live.
  **Design done** ([design/2d-live-preview.md](design/2d-live-preview.md)): the old
  "doesn't live-update" note is largely stale (the `ChangeNotify -> SetConfig ->
  UpdateSprite` path already works for Sprite and Tilemap); the work is a
  verification pass + correcting the README, plus a guardrail that new asset fields
  extend `SetConfig`'s reload detection. **Shipped for the newer components**: the
  2D Look twin previews bloom/vignette live in the edit viewport, and the Skeletal
  twin scrubs a pose non-destructively, both without entering game mode.

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
- **UI / HUD parity.** A first-class HUD/UI story that is *at parity* with the rest
  of Diorama: AI- and human-drivable through a typed request bus (the same
  parity model the sprite/tilemap buses follow), with an editor twin and demos,
  rather than a bolt-on. Health/score/menus an agent can build the same way it
  builds sprites. (LyShine is UI-only and outside Diorama's parity model today.)
  **In progress**: design in [design/2d-ui-hud.md](design/2d-ui-hud.md). Landed: the
  pure `UILayout2D.h` anchor/scale core (+ unit tests); `DioramaUIComponent` +
  `DioramaUIRequestBus` + the `EditorDioramaUIComponent` twin; **text** elements via
  `AzFramework::FontDrawInterface`; and **bar/gauge + solid-color panel** elements via
  screen-space AuxGeom triangles with an orthographic view-proj override (no new
  shader, no LyShine dependency). Bus: SetText/SetFontSize/SetColor/SetAnchor/
  SetOffset/SetSize/SetValue/SetBackgroundColor/SetVisible/GetUIInfo. Verified
  rendering a title + score + health bar + panel over the lighting demo. Next:
  textured-image panels (sprite path), a HUD demo + how-to, and porting the
  twin-stick score HUD off LyShine onto the Diorama bus.
- **Animation state machine.** Flipbook and skeletal give *clips*; character work
  needs transitions (idle -> run -> jump) with conditions and blends.
- **Input action-mapping.** A rebindable action surface; the twin-stick wires raw
  input directly today.
- **Save/load** of game state, and **off-screen culling** for bullet-hell / large
  tile scenes (the batch helps, but explicit culling is not there yet).

Recommended order (closes gaps, then leans into the differentiators): audio ->
post-processing (already designed, biggest "looks AAA" payoff) -> animation depth
(clip editor + Aseprite import and/or skeletal) -> finish the AI-friendly API
(the moat) -> UI/HUD parity -> starter template -> flagship demo.

## Beyond games: simulation and robotics

A 2D engine has no robotics-*simulation* story, but as a 2D content layer Diorama has
a narrow, honest supporting role: authoring and live-streaming the inherently-2D
content of a 3D sim (live occupancy grids / costmaps first, then fiducials and floor
markings the simulated sensors can see), feeding a fully open Fedora-RPM stack. It is
not a robotics tool, and the engine's robotics story should lead with the real
robotics tech (ROS 2 Gem, sensors, physics). Scope and limits in
[design/simulation-robotics.md](design/simulation-robotics.md).
