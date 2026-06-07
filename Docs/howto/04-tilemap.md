# How-To: Tilemap

Teaching ladder rung 4. Builds on the [Sprite Atlas](03-sprite-atlas.md) guide:
a tilemap is an atlas applied across a grid of cells, drawn as one batched layer.

## What you will build

A `Tilemap` component that fills a grid of cells, each showing one cell of a
shared atlas texture, and an arena floor painted entirely through the AI-native
`DioramaTilemapRequestBus`. This is the kind of arena foundation a top-down
shooter or RPG map builds on.

## The tilemap model

A tilemap has two grids:

- The **atlas grid** (`atlasColumns` x `atlasRows`) divides the atlas texture
  into tile cells, numbered left-to-right, top-to-bottom starting at 0. This is
  exactly the UV sub-region model from the atlas guide.
- The **tilemap grid** (`columns` x `rows`) is the layout in the world. Each
  cell holds an atlas cell index, or `-1` for an empty (undrawn) cell.

Each tile is `tileSize` world units. Cells lie in the entity's local X (columns)
and Z (rows) plane, centered on the entity origin, with row 0 at the top (+Z).
Rotate the entity -90 degrees about X to lay the map flat as a floor, or leave it
upright as a wall.

## Why a tilemap (and not many sprites)

You could place one Sprite per cell, but a tilemap is a single component that
owns the whole grid, authors compactly (one atlas + an index per cell), and,
because every cell shares the atlas and sort key, renders in **one draw call**.
Internally the tilemap registers each non-empty cell with the same batched
sprite feature processor the Sprite component uses, so it reuses that entire
tested render path; the batching collapses the layer into one draw.

## The API

The `DioramaTilemapRequestBus` mirrors the inspector with typed, forgiving verbs:

| Verb | Effect |
| ---- | ------ |
| `SetAtlasByPath(productPath)` | Assign the atlas texture |
| `SetAtlasGrid(columns, rows)` | Divide the atlas into cells |
| `SetGridSize(columns, rows)` | Set the tilemap dimensions |
| `SetTileSize(width, height)` | World size of one tile |
| `SetTile(column, row, tileIndex)` | Set one cell (`-1` clears it) |
| `Fill(tileIndex)` | Set every cell |
| `Clear()` | Empty every cell |
| `SetTint(r, g, b, a)` | Tint the whole layer |
| `SetSortOffset(sortOffset)` | Transparent draw-order bias |
| `GetTilemapInfo()` | Resolved state (loaded, visible, filled count) |

Every value is validated and clamped (dimensions to at least 1, tile size to at
least 0), so a bad argument is corrected rather than crashing.

## Steps (in the editor)

1. Create an entity and add the **Tilemap** component (Diorama category).
2. In **Atlas**, set the atlas texture and its columns/rows.
3. In **Grid**, set the tilemap columns/rows and the tile size.
4. Paint cells. You can paint by hand in the viewport (next section), through the
   request bus, or from a build script (see the runnable example); the inspector
   exposes the atlas, grid, appearance, and paint settings, not the raw cell array.

## Painting in the editor

The Tilemap component has a viewport paint mode, the same workflow as
GameMaker/Godot/Tiled:

1. In the component's **Painting** group, set **Active Tile** (the atlas cell
   index a left-drag paints) and **Brush Size** (the square brush edge in cells).
2. Click **Edit** to enter the paint Component Mode (the button changes to
   **Done**; the viewport shows the Tilemap mode).
3. **Left-drag** over the grid to paint the active tile; **right-drag** to erase.
   Each stroke (mouse down to up) is a single undo step, so **Ctrl+Z** removes a
   whole stroke. Paints persist into the prefab on save.
4. Click **Done** (or press **Esc**) to leave the mode.

The cell under the cursor is found by intersecting the mouse ray with the
tilemap's grid plane, so painting works at any camera angle. The brush, rectangle,
and flood-fill cell math is the pure, unit-tested `TilemapPaint` core shared with
any future scripted authoring.

## Runnable example

[`../examples/tilemap.py`](../examples/tilemap.py) builds an 8x8 checkerboard
arena with a blue border from the 2x2 sample atlas, entirely through the bus,
then verifies the result with `GetTilemapInfo` (no screenshot needed):

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/tilemap.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Autotiling (borders connect themselves)

If your tileset has a 16-cell **autotile block** (one cell per combination of which
cardinal edges touch a same-group neighbor), you can paint plain solid cells and let
the tilemap pick the right border art:

```lua
-- Paint a region (any solid tile), then resolve borders against the 16-cell block
-- that starts at atlas cell 64:
DioramaTilemapRequestBus.Event.Fill(self.entityId, 64)
DioramaTilemapRequestBus.Event.Autotile(self.entityId, 64)
```

`Autotile(baseTileIndex)` walks every **non-empty** cell, builds a 4-bit mask of its
non-empty cardinal neighbors (`N=1, E=2, S=4, W=8`), and sets the cell to
`baseTileIndex + mask`. So lay your block out in mask order: cell `base + 0` is the
isolated tile, `base + 15` is fully surrounded, `base + 10` (`E|W`) is a horizontal
middle piece, and so on. Cells off the edge of the grid count as empty, so the map
border resolves to edge art. Re-run it after editing; it is stable (an already-tiled
cell stays in the block).

### Corners too: the 47-tile blob scheme

For art that also resolves *corners* (inner and outer), use `AutotileBlob` with a
**47-cell** block instead:

```lua
DioramaTilemapRequestBus.Event.AutotileBlob(self.entityId, 64)
```

It uses the same membership rule, but a diagonal neighbor only counts when both of its
adjacent edges are present (an unsupported corner is not a real connection), which
reduces the 256 neighbor combinations to exactly 47 tiles. Each cell becomes
`baseTileIndex + index`, where `index` is `0..46` in the gem's canonical order
(ascending normalized-mask value): cell `base + 0` is isolated, `base + 46` is fully
surrounded. Lay the 47-cell block out in that order.

Pick **4-bit edge** (16 tiles) for simple platforms and **blob** (47 tiles) when you
have corner art.

## Verifying without a screenshot

`GetTilemapInfo` returns the resolved state: the resolved atlas path, whether the
atlas has loaded, whether the layer is drawing (`visible`), the grid and atlas
dimensions, the tile size, and `filledTileCount` (how many cells are non-empty).
An agent paints cells and confirms `filledTileCount` matches its intent, closing
the loop without eyeballing pixels.

## Dedicated tilemap asset (for larger maps)

Authoring tiles inline on the component (above) is fine for small grids, but a
large map bloats the prefab/level and is re-parsed at load. For bigger maps,
author the map as a **`.dtilemap`** source file: the gem's tilemap builder
compiles it into a compact, validated **`.dtilemapc`** asset that a Tilemap
component references through its **Tilemap Asset** field. The map loads as data
instead of inlining a tile array, and the builder validates the map (bounds,
indices) at build time.

A `.dtilemap` source is JSON. A top-level `tiles` array is shorthand for one layer:

```json
{
  "columns": 8,
  "rows": 6,
  "atlas": "diorama/textures/tileset.png",
  "atlasColumns": 4,
  "atlasRows": 4,
  "tileWidth": 1.0,
  "tileHeight": 1.0,
  "tiles": [ 0, 1, 1, 2, -1, -1, 0, 0 ]
}
```

`tiles` is row-major, one atlas-cell index per cell (`-1` is an empty cell). The
asset is multi-layer-capable; supply a `layers` array (each with its own `name`,
`sortOffset`, `tint`, and `tiles`) instead of the top-level `tiles` when you want
several layers in one file:

```json
{
  "columns": 8, "rows": 6, "atlas": "diorama/textures/tileset.png",
  "atlasColumns": 4, "atlasRows": 4,
  "layers": [
    { "name": "ground", "sortOffset": 0.0, "tiles": [ 0, 0, 0 ] },
    { "name": "decor",  "sortOffset": 1.0, "tiles": [ -1, 3, -1 ] }
  ]
}
```

Save it next to your assets (for example `Assets/Diorama/Maps/level1.dtilemap`),
let the Asset Processor build it, then on a Tilemap component set **Tilemap Asset**
to the resulting `level1` asset. When an asset is assigned it supplies the grid,
atlas, and tiles; the inline fields are the fallback when no asset is set.

The source treats your map as untrusted: dimensions, layer count, tile-array
length, and every tile index are bounded and the map is rejected if inconsistent,
so a malformed file fails the build instead of feeding bad data to the renderer.

**Every layer renders.** A multi-layer asset draws all its layers, each as its own
batched layer at its own sort offset (layer 0 is the one the request bus edits).

### Import from Tiled

If you author maps in [Tiled](https://www.mapeditor.org/), export them as JSON
(`.tmj`) and the same builder imports them into the same `DioramaTilemapAsset`. It
supports finite **orthogonal** maps with integer tile layers; Tiled GIDs map to
atlas cells (`0` is empty), and **horizontal, vertical, and diagonal** flip flags
are all carried through (diagonal becomes an anti-diagonal transpose, so rotated
tiles import correctly). The tileset may be **embedded** in the map or an **external
`.tsj`** referenced by `source` (resolved relative to the map; `.tsx` XML tilesets
are not supported, export as `.tsj`). Object/image layers are skipped. The world
tile size defaults to 1.0 (Tiled's pixel sizes describe the atlas, not world
units); set it on the component if you need a different scale.

### Swapping the map from a script

`DioramaTilemapRequestBus` has **`SetTilemapByPath`**: pass a `.dtilemapc` product
path and the whole map (grid, atlas, layers) loads from the asset, so a script or
agent can change levels at runtime. `GetTilemapInfo` reports `hasSourceAsset` so you
can confirm an asset is driving the map.

## Next

Rung 5 (Parallax and Layers) stacks tilemaps and sprites at different sort
offsets and depths for a 2.5D look. From there a tilemap makes a natural arena
floor under sprite-based players, enemies, and projectiles in a top-down game.
