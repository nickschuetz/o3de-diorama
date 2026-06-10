# Reference: Tilemap Component

A tilemap is a grid of cells, where each cell draws one cell of a shared atlas
texture as a quad. Every cell is submitted through the same batched sprite
feature processor the [Sprite](./sprite-component.md) component uses, so a whole
layer that shares one atlas collapses into a single draw call.

A tilemap has two distinct grids:

- The **atlas grid** (`atlasColumns` x `atlasRows`) divides the atlas texture
  into numbered tile cells, left-to-right then top-to-bottom starting at 0. This
  is the same UV sub-region model as a sprite atlas.
- The **tilemap grid** (`columns` x `rows`) is the layout in the world. Each
  cell holds an atlas cell index, or the empty-tile sentinel `-1` for a cell
  that draws nothing.

The runtime `TilemapComponent` and the `EditorTilemapComponent` share one
configuration struct, `TilemapComponentConfig`. The editor authors values into
that struct and hands an identical copy to the runtime component through
`BuildGameEntity`, so what you set in the inspector is exactly what ships.

The Inspector and the `DioramaTilemapRequestBus` are peers over that same
configuration. Anything a human sets in the inspector, an agent can set over the
bus, and vice versa. The bus verbs are typed and forgiving: every value is
validated and clamped rather than crashing.

## Coordinate and layout model

Tiles lie in the entity's local X (columns) and Z (rows) plane, centered on the
entity origin. Y is always 0 for every tile, so an unrotated tilemap stands
upright in the X/Z plane (a wall). Rotate the entity -90 degrees about X to lay
the map flat as a floor.

Columns increase along **+X**. Rows increase along **-Z**, which places **row 0
at the top** (the most +Z row). The grid is centered so its middle sits at the
entity origin.

`GetTileLocalPosition(col, row)` returns the local-space center of a cell using
exactly this convention. With `columns` and `rows` from the grid helpers and
`tileSize` (width along X, height along Z):

```
x = (col + 0.5) * width  - columns * width  * 0.5
z = rows * height * 0.5  - (row + 0.5) * height
y = 0
```

So cell `(0, 0)` sits in the upper-left of the grid (most -X, most +Z), and
cell `(columns-1, rows-1)` sits in the lower-right (most +X, most -Z).

The atlas UV convention matches: `GetTileUVRegion` maps an atlas cell index to a
normalized sub-rectangle where v runs top-to-bottom (atlas cell row 0 is the top
of the texture), identical to the sprite UV convention.

## Parameters

The configuration exposes ten serialized fields. Eight appear in the inspector
across four groups (Source, Atlas, Grid, Appearance). The tile data array is
authored through the bus or a build script, not the raw inspector. Below, each
parameter lists its inspector label, its config field, and the matching bus
setter.

| Parameter | Inspector label | Config field | Type | Default | Range / clamp |
| --------- | --------------- | ------------ | ---- | ------- | ------------- |
| Tilemap asset | Tilemap Asset | `m_tilemapAsset` | `Asset<DioramaTilemapAsset>` | none (PreLoad) | optional compiled `.dtilemapc`; supplies grid/atlas/tiles when set |
| Atlas | Atlas | `m_atlas` | `Asset<StreamingImageAsset>` | none (PreLoad) | must resolve to an asset |
| Atlas columns | Atlas Columns | `m_atlasColumns` | `int` | 1 | inspector Min 1; helper clamps to >= 1 |
| Atlas rows | Atlas Rows | `m_atlasRows` | `int` | 1 | inspector Min 1; helper clamps to >= 1 |
| Columns | Columns | `m_columns` | `int` | 1 | inspector Min 1; helper clamps to >= 1 |
| Rows | Rows | `m_rows` | `int` | 1 | inspector Min 1; helper clamps to >= 1 |
| Tile size | Tile Size | `m_tileSize` | `Vector2` | `(1.0, 1.0)` | bus clamps negatives to 0 |
| Tiles | (not in inspector) | `m_tiles` | `vector<s32>` | empty | row-major, length `columns*rows`; `-1` = empty |
| Tint | Tint | `m_tint` | `Color` | `(1, 1, 1, 1)` | bus clamps channels to 0..1 |
| Sort offset | Sort Offset | `m_sortOffset` | `float` | `0.0` | unbounded |

### Tilemap asset

- Inspector label: **Tilemap Asset** ("Optional compiled tilemap (.dtilemapc)"),
  group Source.
- Config field: `m_tilemapAsset` (`AZ::Data::Asset<DioramaTilemapAsset>`,
  `PreLoad`).
- Bus setter: `bool SetTilemapByPath(AZStd::string_view productPath)`.

The optional compiled tilemap asset. A `.dtilemapc` product is built from either
a Diorama `.dtilemap` JSON source or a Tiled `.tmj` export (see the
[Tilemap how-to](../howto/04-tilemap.md) and the
[tilemap-tooling design](../design/2d-tilemap-tooling.md)). When an asset is
assigned it is authoritative: it supplies the atlas, the grid dimensions, the
tile data, and every additional layer, and the inline fields below act only as
the fallback for a tilemap authored directly (no asset). `SetTilemapByPath`
resolves and clears like the other path setters (empty string detaches and
returns `true`; an unresolved path returns `false`), and
`GetTilemapInfo().hasSourceAsset` reports whether an asset is currently driving
the map.

A compiled tilemap can carry **multiple layers** and **per-tile orientation**.
Each tile entry packs three orientation bits above its atlas index
(`FlipHorizontal`, `FlipVertical`, `FlipDiagonal` / anti-diagonal transpose in
`DioramaTilemapAsset`), so a single atlas cell can be mirrored or rotated in
90-degree steps per cell, the same UV scheme the Sprite component's flip and
transpose use. The low 28 bits are the atlas cell index. Layers beyond the first
render as additional batched sprite groups stacked by sort offset.

### Atlas

- Inspector label: **Atlas** ("Texture divided into tile cells"), group Atlas.
- Config field: `m_atlas` (`AZ::Data::Asset<AZ::RPI::StreamingImageAsset>`).
- Bus setter: `bool SetAtlasByPath(AZStd::string_view productPath)`.

The atlas is the texture sampled per tile. Every non-empty cell draws a
sub-rectangle of this one texture. The asset reference uses `PreLoad` load
behavior; the component requests the load when it activates.

`SetAtlasByPath` assigns the atlas by product path and returns `false` if the
path does not resolve to an asset, so an agent can detect a typo without a
screenshot. The resolved path and whether it has finished streaming are reported
back through `GetTilemapInfo` (`atlasPath`, `atlasLoaded`).

The atlas is what makes the single-draw-call batching possible: because every
cell in a layer samples the same texture and shares the layer sort key, the
feature processor batches them into one draw.

### Atlas columns and atlas rows

- Inspector labels: **Atlas Columns** ("Cells across the atlas") and **Atlas
  Rows** ("Cells down the atlas"), group Atlas. Both have inspector `Min` of 1.
- Config fields: `m_atlasColumns`, `m_atlasRows` (both `int`, default 1).
- Bus setter: `void SetAtlasGrid(int columns, int rows)` (each value clamped to
  >= 1).

These define how the atlas texture is divided into tile cells, numbered
left-to-right then top-to-bottom starting at 0. They determine how a tile index
maps to a UV sub-rect: atlas cell index `i` lives at atlas column `i %
atlasColumns` and atlas row `i / atlasColumns`, and `GetTileUVRegion` turns that
into a normalized rectangle of size `(1/atlasColumns, 1/atlasRows)`.

`GetAtlasColumns()` and `GetAtlasRows()` return the stored value clamped to at
least 1, so the renderer and bus never divide by zero even if a field is left at
0 or negative. `GetAtlasCellCount()` returns `GetAtlasColumns() *
GetAtlasRows()`, the highest valid tile index plus one. A tile index handed to
`GetTileUVRegion` is clamped into `[0, cellCount-1]`: a negative index resolves
to cell 0 and an oversized index to the last cell. (Note that the empty-tile
sentinel `-1` is never passed to UV resolution; an empty cell is skipped and
draws nothing.)

### Columns and rows

- Inspector labels: **Columns** ("Tilemap width in cells") and **Rows**
  ("Tilemap height in cells"), group Grid. Both have inspector `Min` of 1.
- Config fields: `m_columns`, `m_rows` (both `int`, default 1).
- Bus setter: `void SetGridSize(int columns, int rows)` (each value clamped to
  >= 1).

These are the tilemap grid dimensions in cells: how wide (columns, along +X) and
how tall (rows, along -Z) the map is in the world. `GetColumns()` and
`GetRows()` return the stored value clamped to at least 1. `GetTileCount()`
returns `GetColumns() * GetRows()`, the length the `tiles` array takes once
written.

`InBounds(col, row)` is true only when `0 <= col < GetColumns()` and `0 <= row <
GetRows()`. Writes to out-of-bounds cells are silently ignored.

Resizing the grid does not reshape an existing `tiles` array. The array is
matched to the grid size lazily, on the next write (see Tiles below). After a
grid resize, query through `GetTile` rather than indexing raw data: `GetTile`
returns `EmptyTile` for any cell that the array does not yet cover.

### Tile size

- Inspector label: **Tile Size** ("World size of one tile"), group Grid.
- Config field: `m_tileSize` (`AZ::Vector2`, default `(1.0, 1.0)`).
- Bus setter: `void SetTileSize(float width, float height)` (negative values
  clamped to 0).

`tileSize` is the world size of a single tile: X is the width along local X, Y
of the vector is the height along local Z. It scales both the quad each cell
draws and the spacing in `GetTileLocalPosition`, so the full grid spans
`columns * width` along X and `rows * height` along Z, centered on the origin.

The bus clamps negative width or height to 0. The serialized field itself
carries whatever was set; the clamp lives in the bus setter.

### Tiles

- Inspector label: none. `m_tiles` is intentionally not an edit element. An
  integer grid is not usefully editable as a flat array, so cell contents are
  authored through the bus or a build script.
- Config field: `m_tiles` (`AZStd::vector<AZ::s32>`, default empty).
- Bus setters: `void SetTile(int column, int row, int tileIndex)`,
  `void Fill(int tileIndex)`, `void Clear()`, plus the two autotile rewrites
  `void Autotile(int baseTileIndex)` (4-bit edge mask) and
  `void AutotileBlob(int baseTileIndex)` (47-tile corner-aware blob); see the
  [API reference](./api.md#dioramatilemaprequestbus) for their exact mask rules.

`tiles` is the row-major tile data: one entry per cell, length `columns * rows`,
ordered so the entry for `(col, row)` is at index `row * columns + col`. Each
entry is an atlas cell index, or the empty-tile sentinel.

The empty-tile sentinel is `EmptyTile = -1`. A cell holding `-1` draws nothing;
it is skipped at render time rather than resolved to an atlas cell.

The data model:

- `SetTile(col, row, value)` sets one cell. It is a no-op when `(col, row)` is
  out of bounds. On the first write, if the array length does not already match
  `GetTileCount()`, the array grows to the full grid size with new cells
  filled with `EmptyTile`, then the target cell is set. This is why an
  empty-by-default tilemap costs nothing until you paint into it.
- `Fill(value)` assigns every cell to the same value, sizing the array to
  exactly `GetTileCount()`.
- `Clear()` is `Fill(EmptyTile)`: every cell becomes empty (draws nothing).
- `GetTile(col, row)` returns the value at a cell, or `EmptyTile` when the cell
  is out of bounds or not yet covered by the array.
- `CountFilledTiles()` returns the number of in-bounds cells whose value is not
  `EmptyTile`. This is the value surfaced as `filledTileCount` in
  `GetTilemapInfo`.

Over the bus, `SetTile` takes `tileIndex = -1` to clear a single cell, and
`Fill(-1)` clears all cells (equivalent to `Clear()`). Out-of-range cells passed
to `SetTile` are ignored.

### Tint

- Inspector label: **Tint** ("Color multiplied into every tile"), group
  Appearance, Color UI handler.
- Config field: `m_tint` (`AZ::Color`, default `(1, 1, 1, 1)`).
- Bus setter: `void SetTint(float r, float g, float b, float a)` (channels
  clamped to 0..1).

`tint` is a color multiplied into every tile in the layer. Alpha drives
transparency for the whole layer. The default of opaque white `(1, 1, 1, 1)`
leaves the atlas unmodified. The bus clamps each channel to `0..1`.

### Sort offset

- Inspector label: **Sort Offset** ("Transparent draw-order bias for the
  layer"), group Appearance.
- Config field: `m_sortOffset` (`float`, default `0.0`).
- Bus setter: `void SetSortOffset(float sortOffset)` (larger draws on top).

`sortOffset` is a transparent draw-order bias applied to the whole layer. Larger
values draw on top. It is the lever for stacking tilemaps and sprites at
different depths for a 2.5D look (see the parallax guide). It
applies uniformly to every cell in the layer, which is part of why the layer
stays a single batch.

### Autotile Rules

- Inspector label: **Autotile Rules** ("Custom neighbor-mask -> offset rules
  consumed by AutotileRules"), group Autotiling.
- Config field: `m_autotileRules` (`AZStd::vector<TilemapAutotileRuleData>`, each
  entry a `{mask, offset}`).
- Bus verb: `void AutotileRules(int baseTileIndex)` (consumes these rules).

The built-in `Autotile` / `AutotileBlob` verbs assume the art block is laid out in
the gem's canonical order (16 cells in edge-mask order, or 47 in blob order). When
an imported tileset is laid out differently, list **Autotile Rules** mapping each
normalized neighbor **Mask** (0..255) to a display **Offset**, then call
`AutotileRules(baseTileIndex)`: each non-empty cell becomes `baseTileIndex` + the
first matching rule's offset, or the canonical blob index when no rule matches. The
rewrite is pure integer-grid math (no render change), so it is fully verifiable
through the tile getters.

### Solid Tiles and Collision Layer

- Inspector labels: **Solid Tiles** ("Atlas tile indices treated as solid") and
  **Collision Layer** ("Collision category bit the solid-tile boxes belong to"),
  group Collision.
- Config fields: `m_solidTiles` (`AZStd::vector<AZ::s32>`, default empty) and
  `m_collisionLayer` (`AZ::u32`, default `1`).
- No bus setter: authored in the Inspector (or a build script); the runtime
  registers the colliders.

Mark which atlas indices are **solid** to give a tile world collision without
hand-placing colliders. At activate (and whenever the tiles change) the runtime
greedy-meshes the solid cells into a few large boxes and registers them with the 2D
collision world on the chosen **Collision Layer**. They are query-only static
geometry: a moving collider resolves against them through `OverlapBox` /
`Raycast2D` / `ComputeBoxPushOut`, but they do not fire contact events. Per-tile
collision assumes the tilemap is axis-aligned in the X,Z collision plane (its
default, un-rotated orientation); a rotated collidable tilemap warns once. Empty
`Solid Tiles` = no collision.

### Animated Tiles

- Inspector label: **Animated Tiles** ("Painted tile index -> a cycling sequence of
  atlas frames"), group Animation.
- Config field: `m_animatedTiles` (`AZStd::vector<TilemapAnimatedTileData>`, each a
  `{tileIndex, frames, fps, loop}`).
- Bus verbs: `void DefineAnimatedTile(int tileIndex, const AZStd::vector<int>& frames, float fps, bool loop)`
  and `void ClearAnimatedTiles()`.

Define an **Animated Tile** to make every cell painted with a given **Tile Index**
cycle through a sequence of atlas **Frames** at a chosen **FPS** (with a **Loop**
flag: wrap, or hold the last frame after one pass). All cells sharing a definition
advance off one map-wide clock, so every instance stays in sync, and an animated
cell is re-pushed only on the ticks where its frame actually changes; a map with no
animated tiles does not tick at all. Painted orientation flags (flip/rotate) are
preserved onto each frame. The frame-selection timing is the pure, unit-tested
`Diorama::TilemapAnimation::FrameAtTime`. An agent authors animations with
`DefineAnimatedTile` (re-defining the same tile index replaces it; an empty frame
list removes it) and confirms them through `animatedTileCount` on `GetTilemapInfo`.

## Performance notes

A tilemap is one component that owns the whole grid and renders the layer in a
single draw call. Every non-empty cell registers with the same batched sprite
feature processor the Sprite component uses; because all cells share the atlas
texture and the layer sort key, the feature processor batches them into one
draw.

Placing one Sprite component per cell would author the same image but cost a
component (and a separate registration) per cell, and would not guarantee the
single-batch collapse. The tilemap also authors compactly: one atlas reference
plus an integer index per cell, rather than per-cell entities. Empty cells
(`-1`) are skipped entirely, so a sparse map only pays for the cells it paints.

## Verifying state (GetTilemapInfo)

`TilemapInfo GetTilemapInfo()` returns the resolved runtime state of the
tilemap, reporting what the tilemap is actually doing rather than what was
requested. It is safe to poll. This is the agent verify-loop: set values over
the bus, then read them back and confirm they took effect, no screenshot
required.

| Field | Type | Default | Meaning |
| ----- | ---- | ------- | ------- |
| `atlasPath` | `string` | empty | Resolved atlas product path, empty if none assigned. |
| `atlasLoaded` | `bool` | `false` | True once the atlas asset has streamed in. |
| `hasSourceAsset` | `bool` | `false` | True when a compiled `.dtilemapc` asset is driving the map (vs. cells set inline through the bus). |
| `visible` | `bool` | `false` | True when the layer is registered and drawable. |
| `columns` | `int` | 1 | Resolved tilemap width in cells. |
| `rows` | `int` | 1 | Resolved tilemap height in cells. |
| `atlasColumns` | `int` | 1 | Resolved atlas cells across. |
| `atlasRows` | `int` | 1 | Resolved atlas cells down. |
| `tileWidth` | `float` | 1.0 | Resolved tile width along local X. |
| `tileHeight` | `float` | 1.0 | Resolved tile height along local Z. |
| `filledTileCount` | `int` | 0 | Number of non-empty cells (`CountFilledTiles()`). |
| `animatedTileCount` | `int` | 0 | Number of animated-tile definitions active. |
| `sortOffset` | `float` | 0.0 | Resolved layer draw-order bias. |

The distinction between requested and actual matters in two places.
`atlasLoaded` becomes true only after streaming completes, so an agent can
`SetAtlasByPath`, then poll until `atlasLoaded` is true before relying on the
texture. `filledTileCount` reflects the true number of non-empty cells after any
clamping or out-of-bounds skips, so after painting a region an agent confirms
`filledTileCount` matches its intent.

## See also

- [Architecture overview](../architecture.md)
- [Sprite component reference](./sprite-component.md)
- [Diorama API reference](./api.md)
- [How-To: Tilemap](../howto/04-tilemap.md)
