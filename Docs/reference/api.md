# Reference: Programmatic API (EBuses)

This is the programmer and agent view of Diorama: the typed EBuses that scripts
(Lua) and agents (Python, o3de-mcp) call to drive every Diorama feature: sprites,
tilemaps, the 2D camera, lights, particles, parallax, collision, UI, audio, and the
CRT overlay. It is organized by call and signature. For the same parameters organized by visual
effect (what each value looks like on screen, with screenshots and ranges), see
the parameter references: [Sprite component](./sprite-component.md) and
[Tilemap component](./tilemap-component.md). This document cross-links to those
rather than repeating the effect descriptions.

## The buses are the front door

The Diorama request buses (`DioramaSpriteRequestBus`, `DioramaTilemapRequestBus`,
`DioramaCamera2DRequestBus`, `DioramaLightRequestBus`, `DioramaParticleRequestBus`,
`DioramaParallaxRequestBus`, `Diorama2DColliderRequestBus`,
`Diorama2DCollisionRequestBus`, `DioramaUIRequestBus`, `DioramaAudioRequestBus`,
`DioramaCRTRequestBus`, `DioramaLookRequestBus`, `DioramaSkeletalRequestBus`,
`DioramaAsepriteRequestBus`) are the stable, typed, agent-facing API for driving the gem.
The Sprite and Tilemap buses are documented in full first; the rest follow the same
conventions and are listed after. They are a peer of the
editor Inspector over the same backing configuration (`SpriteComponentConfig`,
`TilemapComponentConfig`): a human edits a field in the Inspector, an agent
calls the matching verb on the bus, and both land on the same config and the
same live-update path. This AI to human parity is a design invariant, not a
convenience layer. See [the design rationale](../design/ai-sprite-api.md).

Three properties hold for every verb:

- **Addressed by entity id.** Both buses are `AZ::ComponentBus` (address policy
  by id, single handler). You call them with the sprite or tilemap entity's id,
  the same id o3de-mcp returns from entity creation and the same model
  `TransformBus` uses. There is no separate sprite-handle namespace.
- **Plain scalars only.** Every argument is a `float`, `int`, `bool`, or string.
  No `Vector2`, `Color`, `AssetId`, or other O3DE math or asset type has to be
  constructed by the caller. An LLM never builds a math type.
- **Forgiving.** Every setter validates and clamps its input. Out-of-range
  values are corrected, not rejected, and never crash. Bad cells are ignored.
  The exact clamping per verb is listed in the tables below.

## Calling the buses

### From Python (azlmbr)

The buses are reflected into the `diorama` module with `Common` scope, which is
what exports them to the editor Python bindings (`azlmbr`). Call an `Event` with
the verb name, the entity id, then the scalar arguments:

```python
import azlmbr.bus as bus
import azlmbr.diorama as diorama

diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", entityId, "diorama/textures/hero.png")
diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", entityId, 1.0, 1.0)
diorama.DioramaSpriteRequestBus(bus.Event, "PlaySpriteSheet", entityId, 4, 4, 16, 12.0, True)
```

### From Lua

The same verb name is the method on `.Event`, with the entity id first:

```lua
DioramaSpriteRequestBus.Event.SetTextureByPath(entityId, "diorama/textures/hero.png")
DioramaSpriteRequestBus.Event.SetSize(entityId, 1.0, 1.0)
DioramaSpriteRequestBus.Event.PlaySpriteSheet(entityId, 4, 4, 16, 12.0, true)
```

### Critical gotcha: event names must have no spaces

O3DE's Lua and BehaviorContext binding strips spaces from a reflected event's
display name and exposes the method by the space-stripped key. An event reflected
as `"Set Size"` would have to be called as `SetSize` in Lua, a silent mismatch
between the name you see in a reflection dump and the name you must type.

Diorama deliberately reflects every event name **without spaces**, so each verb
maps 1:1 in both Lua and Python, and the name in the reflection is exactly the
name you call. The reflected names are, verbatim from the `BehaviorContext`
reflection in `Code/Source/Clients/SpriteBus.cpp` and `TilemapBus.cpp`:

`SetTextureByPath`, `SetNormalMapByPath`, `SetSize`, `SetPivot`, `SetTint`,
`SetFlash`, `SetOutline`, `SetEmissive`, `SetBillboard`, `SetDoubleSided`,
`SetPointFilter`, `SetUVRegion`, `SetFlip`, `SetTranspose`, `SetSortOffset`,
`SetFrameGrid`, `SetAnimationEnabled`, `SetPlayback`, `SetPlaybackSpeed`,
`SetStartFrame`, `PlaySpriteSheet`, `GetSpriteInfo`, `SetAtlasByPath`,
`SetTilemapByPath`, `SetAtlasGrid`, `SetGridSize`, `SetTileSize`, `SetTile`,
`Fill`, `Clear`, `Autotile`, `AutotileBlob`, `GetTilemapInfo`.

The per-argument names and tooltips in the tables below are likewise reflected
into the C++ `BehaviorContext` (readable via `GetArgumentName` and
`GetArgumentToolTip`, and surfaced on Script Canvas node pins). Note that the
editor's generated Python stub (`azlmbr/diorama.pyi`) lists EBus event arguments
by type only, so those names live in the C++ reflection, not the stub; pass
arguments positionally in the order shown.

### No handler connected

The bus is addressed by entity id. If the target entity has no connected sprite
(or tilemap) component handler, the `Event` call simply does nothing: no handler
runs and no error is raised. A `bool`-returning verb such as `SetTextureByPath`
returns the EBus default (`false`) when nothing is connected. Confirm an action
landed by reading back `GetSpriteInfo` / `GetTilemapInfo` (see the verify loop).

## DioramaSpriteRequestBus

Addressed by the sprite entity's id. Effect column links to the matching
parameter in the [Sprite component reference](./sprite-component.md).

| Verb | Signature (after entity id) | Returns | Clamping / validation | Effect |
| ---- | --------------------------- | ------- | --------------------- | ------ |
| `SetTextureByPath` | `productPath: string` | `bool` | Empty string clears the texture and returns `true`. Otherwise resolves the path; returns `false` if it does not resolve to an asset. Accepts a source path too (see below). | [Texture](./sprite-component.md#texture) |
| `SetNormalMapByPath` | `productPath: string` | `bool` | Same resolve and empty-clears-and-returns-`true` rules as `SetTextureByPath`. Sets the tangent-space normal map for 2D lighting. | [Normal Map](./sprite-component.md#normal-map) |
| `SetSize` | `width: float, height: float` | void | Each clamped to `>= 0`. | [Size](./sprite-component.md#size) |
| `SetPivot` | `x: float, y: float` | void | Each clamped to `0..1`. | [Pivot](./sprite-component.md#pivot) |
| `SetTint` | `r: float, g: float, b: float, a: float` | void | Each channel clamped to `0..1`. | [Tint](./sprite-component.md#tint) |
| `SetFlash` | `r: float, g: float, b: float, amount: float` | void | Each channel and `amount` clamped to `0..1`. Blends the sprite toward the flash color by `amount` (hit flash); `amount = 0` is off. | [Flash](./sprite-component.md#flash) |
| `SetOutline` | `r: float, g: float, b: float, thickness: float` | void | Channels clamped to `0..1`; `thickness` clamped to `>= 0` (in texels). Draws a silhouette outline; `thickness = 0` is off. | [Outline](./sprite-component.md#outline) |
| `SetEmissive` | `r: float, g: float, b: float, intensity: float` | void | Channels clamped to `0..1`; `intensity` clamped to `>= 0`. Adds unlit emissive color (feeds bloom); `intensity = 0` is off. | [Emissive](./sprite-component.md#emissive) |
| `SetBillboard` | `enabled: bool` | void | None. | [Billboard](./sprite-component.md#billboard) |
| `SetDoubleSided` | `enabled: bool` | void | None. | [Double Sided](./sprite-component.md#double-sided) |
| `SetPointFilter` | `enabled: bool` | void | None. | [Point Filter](./sprite-component.md#point-filter-pixel-art) |
| `SetUVRegion` | `uMin: float, vMin: float, uMax: float, vMax: float` | void | Each clamped to `0..1`, then sorted so `min <= max` per axis (region is well formed regardless of argument order). | [UV Min](./sprite-component.md#uv-min) / [UV Max](./sprite-component.md#uv-max) |
| `SetFlip` | `horizontal: bool, vertical: bool` | void | None. | [Flip Horizontal](./sprite-component.md#flip-horizontal) / [Flip Vertical](./sprite-component.md#flip-vertical) |
| `SetTranspose` | `transpose: bool` | void | None. Swaps the UV diagonal (transpose); combine with flips to get all four 90-degree rotations of the region. | [Transpose](./sprite-component.md#transpose) |
| `SetSortOffset` | `sortOffset: float` | void | None (any value, positive or negative). | [Sort Offset](./sprite-component.md#sort-offset) |
| `SetFrameGrid` | `columns: int, rows: int, frameCount: int` | void | `columns`, `rows`, `frameCount` each clamped to `>= 1`. The effective frame count is additionally capped at `columns * rows` at read time. | [Columns](./sprite-component.md#columns) / [Rows](./sprite-component.md#rows) / [Frame Count](./sprite-component.md#frame-count) |
| `SetAnimationEnabled` | `enabled: bool` | void | None. | [Animated](./sprite-component.md#animated) |
| `SetPlayback` | `framesPerSecond: float, loop: bool` | void | `framesPerSecond` clamped to `>= 0`. | [Frames Per Second](./sprite-component.md#frames-per-second) / [Loop](./sprite-component.md#loop) |
| `SetPlaybackSpeed` | `speed: float` | void | Clamped to `>= 0`. Time-scale multiplier on animation advance (`1` = normal, `0` = paused, `0.2` = slow-motion); the global hit-stop / slow-motion knob. | [Playback Speed](./sprite-component.md#playback-speed) |
| `SetStartFrame` | `frame: int` | void | Clamped to `>= 0` on store; clamped to the valid frame range at read time. | [Start Frame](./sprite-component.md#start-frame) |
| `PlaySpriteSheet` | `columns: int, rows: int, frameCount: int, framesPerSecond: float, loop: bool` | void | `columns`/`rows`/`frameCount` clamped to `>= 1`; `framesPerSecond` clamped to `>= 0`; also sets `animEnabled = true`. | [Convenience: PlaySpriteSheet](./sprite-component.md#convenience-playspritesheet) |
| `GetSpriteInfo` | (none) | `SpriteInfo` | Read-only. Safe to poll. | [Verify loop](#query-and-verify-the-verify-loop) |

### SetTextureByPath accepts a source path too

`SetTextureByPath` (and the tilemap's `SetAtlasByPath`) take either:

- the O3DE **product path**, `"diorama/textures/hero.png.streamingimage"`, or
- the more intuitive **source path** you see on disk, `"diorama/textures/hero.png"`.

The resolver (`ResolveStreamingImageAssetId` in
`Code/Source/Clients/DioramaAssetUtils.h`) first tries the path as given; on a
miss, if the path does not already end in `.streamingimage`, it retries once with
that product suffix appended. This costs one extra catalog lookup only on a miss.
If neither resolves, the verb returns `false` and the texture is left unchanged.
Passing an empty string clears the texture and returns `true`.

### PlaySpriteSheet is the convenience verb

`PlaySpriteSheet` is the one-shot "play this sheet" intent. It is equivalent to
`SetFrameGrid(columns, rows, frameCount)` plus `SetPlayback(framesPerSecond,
loop)` plus `SetAnimationEnabled(true)`, applied in a single call. Use it to
start an animation; use the granular setters to adjust one aspect of a clip that
is already configured.

## DioramaTilemapRequestBus

Addressed by the tilemap entity's id. Effect column links to the matching
parameter in the [Tilemap component reference](./tilemap-component.md).

| Verb | Signature (after entity id) | Returns | Clamping / validation | Effect |
| ---- | --------------------------- | ------- | --------------------- | ------ |
| `SetAtlasByPath` | `productPath: string` | `bool` | Empty string clears the atlas and returns `true`. Otherwise resolves the path; returns `false` if it does not resolve. Accepts a source path too (same retry as `SetTextureByPath`). | [Atlas](./tilemap-component.md#atlas) |
| `SetTilemapByPath` | `productPath: string` | `bool` | Resolves a compiled `.dtilemapc` tilemap asset (built from a `.dtilemap` or Tiled `.tmj` source) and drives the whole map (all layers, grid, tiles, atlas) from it. Empty string detaches the asset and returns `true`; `false` if it does not resolve. | [Tilemap Asset](./tilemap-component.md#tilemap-asset) |
| `SetAtlasGrid` | `columns: int, rows: int` | void | Each clamped to `>= 1`. | [Atlas columns and atlas rows](./tilemap-component.md#atlas-columns-and-atlas-rows) |
| `SetGridSize` | `columns: int, rows: int` | void | Each clamped to `>= 1`. | [Columns and rows](./tilemap-component.md#columns-and-rows) |
| `SetTileSize` | `width: float, height: float` | void | Each clamped to `>= 0`. | [Tile size](./tilemap-component.md#tile-size) |
| `SetTile` | `column: int, row: int, tileIndex: int` | void | Out-of-range cells are ignored. `tileIndex` of `-1` clears the cell. (0-based; row 0 is the top.) | [Tiles](./tilemap-component.md#tiles) |
| `Fill` | `tileIndex: int` | void | Sets every cell to `tileIndex`; `-1` clears all. | [Tiles](./tilemap-component.md#tiles) |
| `Clear` | (none) | void | Empties every cell (draws nothing). | [Tiles](./tilemap-component.md#tiles) |
| `Autotile` | `baseTileIndex: int` | void | `baseTileIndex` clamped to `>= 0`. Rewrites every non-empty cell to `baseTileIndex` + a 4-bit edge mask (N=1,E=2,S=4,W=8) of its non-empty cardinal neighbors, so a 16-cell art block laid out in mask order connects itself. | [Tiles](./tilemap-component.md#tiles) |
| `AutotileBlob` | `baseTileIndex: int` | void | `baseTileIndex` clamped to `>= 0`. Like `Autotile` but the 47-tile blob scheme: a corner counts only when both its adjacent edges are present, so each cell becomes `baseTileIndex` + a blob index `0..46` (a 47-cell block that connects edges and corners). | [Tiles](./tilemap-component.md#tiles) |
| `SetTint` | `r: float, g: float, b: float, a: float` | void | Each channel clamped to `0..1`. | [Tint](./tilemap-component.md#tint) |
| `SetSortOffset` | `sortOffset: float` | void | None (any value). | [Sort offset](./tilemap-component.md#sort-offset) |
| `GetTilemapInfo` | (none) | `TilemapInfo` | Read-only. Safe to poll. | [Verify loop](#query-and-verify-the-verify-loop) |

The remaining buses follow the same conventions: per-entity `Event`s addressed by the
target entity id (except the global/query buses noted below), scalar arguments, a
`Get<Thing>Info` reader for the verify loop, and forgiving inputs (out-of-range scalars
are clamped, colors to `0..1`, sizes to `>= 0`). Verb names contain no spaces. Each is
the script front door to the matching component; see its how-to for usage in context.

## DioramaCamera2DRequestBus

Drives the **2D Camera Controller** on a camera entity (how-to [08-camera](../howto/08-camera.md)).

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `SetTarget` | `target: EntityId` | void | Entity the camera follows. |
| `SetSecondaryTarget` | `target: EntityId` | void | Second entity to frame; when valid the camera centers on the midpoint of the two (versus camera). Invalid id clears it. |
| `SetZoom` | `dolly: float` | void | Extra distance to pull the camera back from the play plane; turns auto-zoom off. |
| `SetAutoZoom` | `base, perSeparation, minDolly, maxDolly: float` | void | Auto-dolly as the two targets separate: `base` + `perSeparation` per world unit between targets, clamped to `minDolly..maxDolly`. `perSeparation > 0` enables it. |
| `SetFollowOffset` | `x: float, y: float, z: float` | void | Offset held from the target. |
| `SetSmoothTime` | `seconds: float` | void | Follow smoothing time (`0` snaps). |
| `SetDeadzone` | `halfX: float, halfY: float` | void | Half-extents of the no-move deadzone. |
| `SetLookahead` | `distance: float` | void | Lead the target by its motion. |
| `SetBounds` | `minX, minY, maxX, maxY: float` | void | Clamp the camera center to a world rect. |
| `ClearBounds` | (none) | void | Disable bounds clamping. |
| `AddTrauma` | `amount: float` | void | Add screen-shake trauma (`0..1`, decays). |
| `SetEnabled` | `enabled: bool` | void | Enable/disable the controller. |
| `GetCameraInfo` | (none) | `Camera2DInfo` | Read-only. Safe to poll. |

## DioramaLightRequestBus

Drives a **2D Light** (how-to [07-lighting](../howto/07-lighting.md)).

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `SetColor` | `r, g, b: float` | void | Light color (`0..1`). |
| `SetIntensity` | `intensity: float` | void | Brightness multiplier. |
| `SetRadius` | `radius: float` | void | Falloff radius (point light). |
| `SetDirectional` | `directional: bool` | void | Switch between point and directional. |
| `SetDirection` | `x, y, z: float` | void | Direction (directional mode). |
| `SetEnabled` | `enabled: bool` | void | Enable/disable the light. |
| `GetLightInfo` | (none) | `LightInfo` | Read-only. Safe to poll. |

## DioramaParticleRequestBus

Drives a **2D Particle Emitter** (how-to [09-particles](../howto/09-particles.md)).

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `Emit` | `count: int` | void | Emit `count` particles now. |
| `Burst` | (none) | void | Fire the configured burst count. |
| `Play` / `Stop` | (none) | void | Start/stop continuous emission. |
| `SetRate` | `particlesPerSecond: float` | void | Continuous emission rate. |
| `SetGravity` | `x, y: float` | void | Per-particle acceleration. |
| `SetLifetime` | `minSeconds, maxSeconds: float` | void | Particle lifetime range. |
| `SetSpeed` | `minSpeed, maxSpeed: float` | void | Initial speed range. |
| `SetSpin` | `minRadiansPerSecond, maxRadiansPerSecond: float` | void | Per-particle spin range (signed; negative = clockwise). |
| `SetDirection` | `degrees, spreadDegrees: float` | void | Emission direction and cone. |
| `SetStartColor` / `SetEndColor` | `r, g, b, a: float` | void | Color over life (`0..1`). |
| `SetStartSize` / `SetEndSize` | `size: float` | void | Size over life. |
| `SetTextureByPath` | `productPath: string` | `bool` | Particle texture; `false` if unresolved. |
| `GetParticleInfo` | (none) | `ParticleInfo` | Read-only. Safe to poll. |

## DioramaParallaxRequestBus

Drives a **2D Parallax Layer** (how-to [05-parallax-layers](../howto/05-parallax-layers.md)).

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `SetCamera` | `camera: EntityId` | void | Camera the layer scrolls against. |
| `SetFactor` | `factor: float` | void | Parallax factor (0 = locked, 1 = world-fixed). |
| `SetEnabled` | `enabled: bool` | void | Enable/disable scrolling. |
| `GetParallaxInfo` | (none) | `ParallaxInfo` | Read-only. Safe to poll. |

## Collision buses

2D collision for gameplay scripts. There are two request buses plus a
notification bus.

**`Diorama2DColliderRequestBus`** — per-entity collider config (addressed by entity id):

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `SetCircle` | `radius: float` | void | Make the collider a circle. |
| `SetBox` | `halfWidth, halfHeight: float` | void | Make the collider a box. |
| `SetOffset` | `x, z: float` | void | Local offset of the shape. |
| `SetLayer` | `layer: u32` | void | This collider's layer bit. |
| `SetCollidesWith` | `mask: u32` | void | Layer mask it collides against. |
| `SetTrigger` | `isTrigger: bool` | void | Trigger (overlap-only) vs solid. |
| `SetEnabled` | `enabled: bool` | void | Enable/disable the collider. |
| `GetColliderInfo` | (none) | `Collider2DInfo` | Read-only. Safe to poll. |

**`Diorama2DCollisionRequestBus`** — global world queries (Broadcast, no entity id):

| Verb | Signature | Returns | Effect |
| ---- | --------- | ------- | ------ |
| `OverlapCircle` | `x, z, radius: float, layerMask: u32` | `EntityId[]` | Entities overlapping the circle. |
| `OverlapBox` | `x, z, halfWidth, halfHeight: float, layerMask: u32` | `EntityId[]` | Entities overlapping the box. |
| `ComputeBoxPushOut` | `x, z, halfWidth, halfHeight: float, layerMask: u32, exclude: EntityId` | `Vector2` | Summed minimum-translation vector to push a box out of every overlapping collider on `layerMask`, excluding `exclude` (pass the caller's own id). Add it to the entity's position to resolve pushbox overlap; `(0, 0)` when nothing overlaps. |
| `Raycast2D` | `x, z, dirX, dirZ, maxDistance: float, layerMask: u32` | `Raycast2DResult` | First hit along the ray. |

`Diorama2DCollisionNotificationBus` delivers enter/exit events (see [Notifications](#notifications-event-driven)).

## DioramaUIRequestBus

Drives a **2D UI** element: text, bar, or panel (how-to [13-ui-hud](../howto/13-ui-hud.md)).

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `SetText` | `text: string` | void | Text content (Text kind). |
| `SetFontSize` | `pixels: float` | void | Font size. |
| `SetColor` | `r, g, b, a: float` | void | Foreground color (`0..1`). |
| `SetAnchor` | `anchor: int` | void | Screen anchor (corner/edge/center enum). |
| `SetOffset` | `x, y: float` | void | Pixel offset from the anchor. |
| `SetSize` | `width, height: float` | void | Element size (Bar/Panel). |
| `SetValue` | `value: float` | void | Fill fraction `0..1` (Bar kind). |
| `SetBackgroundColor` | `r, g, b, a: float` | void | Background color (Bar/Panel). |
| `SetVisible` | `visible: bool` | void | Show/hide. |
| `GetUIInfo` | (none) | `UIInfo` | Read-only. Safe to poll. |

## DioramaAudioRequestBus

Global audio via MiniAudio (how-to [15-audio](../howto/15-audio.md)). **Broadcast** (no entity id).

| Verb | Signature | Returns | Effect |
| ---- | --------- | ------- | ------ |
| `PlayOneShot` | `productPath: string, volume: float` | void | Play a sound once at `volume` (`0..1`). |
| `SetMasterVolume` | `volume: float` | void | Global volume (`0..1`). |

## DioramaCRTRequestBus

Drives the **CRT Overlay** scanline post effect (how-to [16-crt](../howto/16-crt.md)).

| Verb | Signature (after entity id) | Returns | Effect |
| ---- | --------------------------- | ------- | ------ |
| `SetEnabled` | `enabled: bool` | void | Toggle the overlay. |
| `SetScanlineDarkness` | `darkness: float` | void | Scanline strength (`0..1`). |
| `SetScanlineSpacing` | `pixels: float` | void | Pixels between scanlines. |

## DioramaLookRequestBus

Drives the **2D Look** component: one component applying tuned bloom + vignette via
Atom's `PostProcessFeatureProcessor` (how-to [14-glow](../howto/14-glow.md)).

| Verb | Signature (after entity id) | Returns | Clamping | Effect |
| ---- | --------------------------- | ------- | -------- | ------ |
| `SetBloomEnabled` | `enabled: bool` | void | None. | Toggle the glow. |
| `SetBloomThreshold` | `threshold: float` | void | `>= 0`. | HDR brightness above which a pixel glows. |
| `SetBloomKnee` | `knee: float` | void | `0..1`. | Softness of the bloom threshold edge. |
| `SetBloomIntensity` | `intensity: float` | void | `>= 0`. | Bloom strength. |
| `SetVignetteEnabled` | `enabled: bool` | void | None. | Toggle the edge darkening. |
| `SetVignetteIntensity` | `intensity: float` | void | `0..1`. | Vignette strength. |

## DioramaSkeletalRequestBus

Drives the **Skeletal Clip** component: a cutout (transform-hierarchy) keyframe
animation player. The character is the entity hierarchy under the component's
entity; each track names a descendant entity ("bone") and animates its local
transform over the clip (how-to [18-skeletal](../howto/18-skeletal.md)).

| Verb | Signature (after entity id) | Returns | Clamping | Effect |
| ---- | --------------------------- | ------- | -------- | ------ |
| `Play` | (none) | void | None. | Start or resume playback from the current time. |
| `Stop` | (none) | void | None. | Stop and hold the current pose. |
| `SetNormalizedTime` | `normalizedTime: float` | void | `0..1`. | Jump to a fraction of the duration and pose immediately. |
| `SetSpeed` | `speed: float` | void | None (negative = reverse). | Playback rate multiplier. |
| `SetLooping` | `looping: bool` | void | None. | Wrap at the end vs. hold the last frame. |
| `SetDuration` | `seconds: float` | void | `> 0`. | Clip length the normalized time maps onto. |
| `IsPlaying` | (none) | `bool` | n/a | True while the clip is advancing. |
## DioramaAsepriteRequestBus

Drives the **Aseprite Animation** component: it plays named animations (Aseprite
tags) imported from an "Export Sprite Sheet" JSON by driving a Sprite on the same
entity (its texture + per-frame UV), honoring per-frame durations and tag direction
(how-to [19-aseprite](../howto/19-aseprite.md)).

| Verb | Signature (after entity id) | Returns | Clamping | Effect |
| ---- | --------------------------- | ------- | -------- | ------ |
| `PlayTag` | `tagName: string` | void | Unknown name is ignored. | Play the named tag from its start. |
| `Play` | (none) | void | None. | Resume the current tag. |
| `Stop` | (none) | void | None. | Stop and hold the current frame. |
| `SetFrame` | `frameIndex: int` | void | Clamped to the sheet. | Show a frame and stop. |
| `SetSpeed` | `speed: float` | void | None (negative reverses). | Playback rate multiplier. |
| `SetLooping` | `looping: bool` | void | None. | Wrap the current tag vs. hold the last frame. |
| `IsPlaying` | (none) | `bool` | n/a | True while a tag is advancing. |
| `GetCurrentFrame` | (none) | `int` | n/a | The frame index currently shown. |

## Query and verify (the verify loop)

`GetSpriteInfo` returns a `SpriteInfo` and `GetTilemapInfo` returns a
`TilemapInfo`. Both are reflected as behavior classes (`DioramaSpriteInfo`,
`DioramaTilemapInfo`) with readable, named properties, so an agent or script
reads back exactly what it set, and more importantly reads the **resolved,
actual** state, not merely what was requested. The texture path is the resolved
catalog path; `textureLoaded` / `atlasLoaded` is whether the asset actually
streamed in; `visible` is whether the renderer is actually drawing it;
`currentFrame` is the frame on screen now; `filledTileCount` is how many cells
are actually non-empty. This is what lets an agent confirm an action took effect
**without a screenshot**.

### SpriteInfo fields

Reflected property names (read from `DioramaSpriteInfo`):

| Property | Type | Meaning |
| -------- | ---- | ------- |
| `texturePath` | string | Resolved texture product path, empty if none assigned. |
| `textureLoaded` | bool | True once the texture asset has streamed in. |
| `visible` | bool | True when registered with the feature processor and actually drawable. |
| `width`, `height` | float | Current quad size in world units. |
| `pivotX`, `pivotY` | float | Current normalized pivot. |
| `sortOffset` | float | Current draw-order bias. |
| `billboard` | bool | Faces the camera. |
| `doubleSided` | bool | Visible from behind. |
| `pointFilter` | bool | Nearest-neighbor (crisp pixel-art) sampling vs default linear. |
| `flipHorizontal`, `flipVertical` | bool | Region mirroring. |
| `animEnabled` | bool | Sprite-sheet playback is on. |
| `currentFrame` | int | Frame currently on screen (resolved from the presenter). |
| `frameCount` | int | Effective frame count (capped at `columns * rows`). |

### TilemapInfo fields

Reflected property names (read from `DioramaTilemapInfo`):

| Property | Type | Meaning |
| -------- | ---- | ------- |
| `atlasPath` | string | Resolved atlas product path, empty if none. |
| `atlasLoaded` | bool | True once the atlas asset has streamed in. |
| `hasSourceAsset` | bool | True when a compiled `.dtilemapc` tilemap asset drives this map (vs. cells set directly through the bus). |
| `visible` | bool | True when the layer is registered and actually drawing. |
| `columns`, `rows` | int | Current tilemap grid dimensions. |
| `atlasColumns`, `atlasRows` | int | Current atlas grid dimensions. |
| `tileWidth`, `tileHeight` | float | Current world size of one tile. |
| `filledTileCount` | int | Number of non-empty cells actually set. |
| `sortOffset` | float | Current draw-order bias. |

### Verify-loop example (Python)

Set, then read back and assert, with no screenshot:

```python
import azlmbr.bus as bus
import azlmbr.diorama as diorama

diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", entityId, "diorama/textures/hero.png")
diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", entityId, 2.0, 2.0)

info = diorama.DioramaSpriteRequestBus(bus.Event, "GetSpriteInfo", entityId)
assert info.textureLoaded   # asset actually streamed in
assert info.visible         # renderer is actually drawing it
assert info.width == 2.0     # the size took effect
```

Tilemap, confirming the painted cell count:

```python
diorama.DioramaTilemapRequestBus(bus.Event, "SetGridSize", entityId, 8, 8)
diorama.DioramaTilemapRequestBus(bus.Event, "Fill", entityId, 0)

info = diorama.DioramaTilemapRequestBus(bus.Event, "GetTilemapInfo", entityId)
assert info.atlasLoaded
assert info.filledTileCount == 64   # 8 * 8 cells, all set
```

## Notifications (event-driven)

`DioramaSpriteNotificationBus` lets scripts and agents subscribe to sprite state
changes instead of polling `GetSpriteInfo`. It is addressed by the same sprite
entity id as the request bus.

| Notification | Signature | Fires when |
| ------------ | --------- | ---------- |
| `OnTextureReady` | `productPath: string` | The texture finished loading (or changed and reloaded). |
| `OnVisibilityChanged` | `visible: bool` | Drawability changed: the sprite became visible or stopped being drawn. |
| `OnAnimationFinished` | (none) | A non-looping clip reached and held its last frame. |
| `OnAnimationFrame` | `frameIndex: int` | The displayed frame advanced (fires for both sprite-sheet and Aseprite playback). Drive per-frame gameplay (hitbox activation, footstep audio) off this rather than guessing frame timing. |

The bus is reflected to the `BehaviorContext` with a handler
(`DioramaSpriteNotificationHandler`), so a script can connect a handler and
implement any subset of the four callbacks. The common use is waiting for
`OnTextureReady` after `SetTextureByPath` rather than polling `textureLoaded`,
waiting for `OnAnimationFinished` on a one-shot clip, and hanging per-frame
gameplay logic (hitboxes, footsteps) on `OnAnimationFrame`.

There is no notification bus for the tilemap; poll `GetTilemapInfo` to confirm
tilemap state.

## Persistence note

Calling these buses **in the editor** persists to the saved prefab exactly like
an Inspector edit. The editor component connects the same handler and, on each
bus edit, marks the entity dirty through the undo or dirty system, so an
agent-authored scene bakes into the prefab and survives save and reload. A bus
edit is not a transient runtime-only change; it is a real authoring edit through
the same path a human's Inspector change takes. See
[the architecture document](../architecture.md) for the editor-component to
config to presenter mechanism.

## Rendering console variables (CVars)

Scene-wide rendering toggles, set from the console, a `.cfg`, or the settings
registry. See [the architecture document](../architecture.md#25d-depth-ordering-and-shadows)
for how they work.

| CVar | Default | Effect |
| ---- | ------- | ------ |
| `r_dioramaSpriteDepthSort` | `1` | Order sprites by camera distance each frame so nearer sprites draw in front, within their sort layer. |
| `r_dioramaSpriteShadows` | `1` | Draw soft ground shadows under billboarded sprites (above the floor, below the sprites). |
| `r_dioramaShadowAlpha` | `0.35` | Opacity of those ground shadows, `0..1`. |

## Runtime scripting notes

Gotchas when driving these buses from a gameplay (launcher) Lua `ScriptComponent`,
as opposed to from the editor:

- **Property defaults are not applied at runtime.** A runtime `ScriptComponent`
  does not inject a `.lua` property's declared default (only the editor does), so
  guard every property read with a fallback: `local speed = self.Properties.Speed or 8.0`.
  An unguarded read of an unbaked property is `nil` in a launcher build.
- **Spawned entities have no transform on their first tick.** An entity created
  via `SpawnableScriptMediator:SpawnAndParentAndTransform` reads the world origin
  on its first tick (the spawn transform is applied later). Publish the intended
  position yourself (a shared global or the ticket) and set it in `OnActivate`,
  rather than reading the transform back.
- **Mouse gives deltas, not a cursor position.** Game scripts receive mouse
  deltas only; no absolute cursor position or screen-to-world is reflected to
  scripts, so point-to-cursor aim is not available. Steer aim relatively off the
  deltas, or use a gamepad stick for absolute aim.

## See also

- [Architecture](../architecture.md): how the config, presenter, and feature
  processor fit together, and how editor edits persist.
- [Sprite component reference](./sprite-component.md): every sprite parameter by
  visual effect, with ranges and screenshots.
- [Tilemap component reference](./tilemap-component.md): every tilemap parameter
  by visual effect.
- [Design: AI-native sprite API](../design/ai-sprite-api.md): the rationale for
  typed, forgiving, scalar-only, verify-via-Info buses.
