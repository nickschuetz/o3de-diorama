# Reference: Programmatic API (EBuses)

This is the programmer and agent view of Diorama: the typed EBuses that scripts
(Lua) and agents (Python, o3de-mcp) call to drive sprites and tilemaps. It is
organized by call and signature. For the same parameters organized by visual
effect (what each value looks like on screen, with screenshots and ranges), see
the parameter references: [Sprite component](./sprite-component.md) and
[Tilemap component](./tilemap-component.md). This document cross-links to those
rather than repeating the effect descriptions.

## The buses are the front door

`DioramaSpriteRequestBus` and `DioramaTilemapRequestBus` are the stable, typed,
agent-facing API for driving a sprite or a tilemap. They are a peer of the
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

`SetTextureByPath`, `SetSize`, `SetPivot`, `SetTint`, `SetBillboard`,
`SetDoubleSided`, `SetUVRegion`, `SetFlip`, `SetSortOffset`, `SetFrameGrid`,
`SetAnimationEnabled`, `SetPlayback`, `SetStartFrame`, `PlaySpriteSheet`,
`GetSpriteInfo`, `SetAtlasByPath`, `SetAtlasGrid`, `SetGridSize`, `SetTileSize`,
`SetTile`, `Fill`, `Clear`, `GetTilemapInfo`.

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
| `SetSize` | `width: float, height: float` | void | Each clamped to `>= 0`. | [Size](./sprite-component.md#size) |
| `SetPivot` | `x: float, y: float` | void | Each clamped to `0..1`. | [Pivot](./sprite-component.md#pivot) |
| `SetTint` | `r: float, g: float, b: float, a: float` | void | Each channel clamped to `0..1`. | [Tint](./sprite-component.md#tint) |
| `SetBillboard` | `enabled: bool` | void | None. | [Billboard](./sprite-component.md#billboard) |
| `SetDoubleSided` | `enabled: bool` | void | None. | [Double Sided](./sprite-component.md#double-sided) |
| `SetUVRegion` | `uMin: float, vMin: float, uMax: float, vMax: float` | void | Each clamped to `0..1`, then sorted so `min <= max` per axis (region is well formed regardless of argument order). | [UV Min](./sprite-component.md#uv-min) / [UV Max](./sprite-component.md#uv-max) |
| `SetFlip` | `horizontal: bool, vertical: bool` | void | None. | [Flip Horizontal](./sprite-component.md#flip-horizontal) / [Flip Vertical](./sprite-component.md#flip-vertical) |
| `SetSortOffset` | `sortOffset: float` | void | None (any value, positive or negative). | [Sort Offset](./sprite-component.md#sort-offset) |
| `SetFrameGrid` | `columns: int, rows: int, frameCount: int` | void | `columns`, `rows`, `frameCount` each clamped to `>= 1`. The effective frame count is additionally capped at `columns * rows` at read time. | [Columns](./sprite-component.md#columns) / [Rows](./sprite-component.md#rows) / [Frame Count](./sprite-component.md#frame-count) |
| `SetAnimationEnabled` | `enabled: bool` | void | None. | [Animated](./sprite-component.md#animated) |
| `SetPlayback` | `framesPerSecond: float, loop: bool` | void | `framesPerSecond` clamped to `>= 0`. | [Frames Per Second](./sprite-component.md#frames-per-second) / [Loop](./sprite-component.md#loop) |
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
| `SetAtlasGrid` | `columns: int, rows: int` | void | Each clamped to `>= 1`. | [Atlas columns and atlas rows](./tilemap-component.md#atlas-columns-and-atlas-rows) |
| `SetGridSize` | `columns: int, rows: int` | void | Each clamped to `>= 1`. | [Columns and rows](./tilemap-component.md#columns-and-rows) |
| `SetTileSize` | `width: float, height: float` | void | Each clamped to `>= 0`. | [Tile size](./tilemap-component.md#tile-size) |
| `SetTile` | `column: int, row: int, tileIndex: int` | void | Out-of-range cells are ignored. `tileIndex` of `-1` clears the cell. (0-based; row 0 is the top.) | [Tiles](./tilemap-component.md#tiles) |
| `Fill` | `tileIndex: int` | void | Sets every cell to `tileIndex`; `-1` clears all. | [Tiles](./tilemap-component.md#tiles) |
| `Clear` | (none) | void | Empties every cell (draws nothing). | [Tiles](./tilemap-component.md#tiles) |
| `SetTint` | `r: float, g: float, b: float, a: float` | void | Each channel clamped to `0..1`. | [Tint](./tilemap-component.md#tint) |
| `SetSortOffset` | `sortOffset: float` | void | None (any value). | [Sort offset](./tilemap-component.md#sort-offset) |
| `GetTilemapInfo` | (none) | `TilemapInfo` | Read-only. Safe to poll. | [Verify loop](#query-and-verify-the-verify-loop) |

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

The bus is reflected to the `BehaviorContext` with a handler
(`DioramaSpriteNotificationHandler`), so a script can connect a handler and
implement any subset of the three callbacks. The common use is waiting for
`OnTextureReady` after `SetTextureByPath` rather than polling `textureLoaded`,
and waiting for `OnAnimationFinished` on a one-shot clip.

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

## See also

- [Architecture](../architecture.md): how the config, presenter, and feature
  processor fit together, and how editor edits persist.
- [Sprite component reference](./sprite-component.md): every sprite parameter by
  visual effect, with ranges and screenshots.
- [Tilemap component reference](./tilemap-component.md): every tilemap parameter
  by visual effect.
- [Design: AI-native sprite API](../design/ai-sprite-api.md): the rationale for
  typed, forgiving, scalar-only, verify-via-Info buses.
