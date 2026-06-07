# Design: tilemap tooling (in-editor painting, autotiling, layers, collision)

Status: **shipped** (in-editor painting, 4-bit + 47-blob autotiling, layers, plus a
dedicated `.dtilemap` asset + builder and Tiled `.tmj` import). See
[how-to 04](../howto/04-tilemap.md) and the CHANGELOG. **Note:** autotiling shipped
as the `Autotile` / `AutotileBlob` bus verbs, not the `DioramaTilesetRule` JSON asset
sketched below; treat that section as historical design rationale. Builds on the
data-driven tilemap component (`DioramaTilemapRequestBus`: SetTile, Fill,
SetGridSize, SetAtlasGrid, ...).

## Goal

Turn the tilemap from "set cells through a bus" into a tool you author by hand:
paint and erase tiles directly in the editor viewport, autotile borders so art
connects itself, stack layers, and generate collision from solid tiles. This is
the workflow indies expect from GameMaker/Godot/Tiled.

## Key findings (O3DE 26.05)

- O3DE has a reusable **PaintBrush** subsystem
  (`AzFramework/PaintBrush/`, `AzToolsFramework/Manipulators/PaintBrushManipulator.h`)
  with a polished viewport brush UX, but it is built for **continuous float values**
  (opacity/intensity blended through `valueLookupFn`/`blendFn`). Discrete tile
  indices fight that model.
- The lower-level **viewport interaction** API is mature and is the right fit for
  discrete stamping: `EditorBaseComponentMode` +
  `ViewportInteraction::MouseViewportRequests::HandleMouseInteraction` gives mouse
  down/move/up with a world-space ray; `ViewportScreenToWorldRay` converts a click
  to a ray we intersect with the tilemap plane to get a cell.
- **GradientSignal's** `ImageGradientModifier`
  (`Gems/GradientSignal/.../ImageGradientModification.h`) is the worked example to
  copy: a sparse modified-tile buffer + `ScopedUndoBatch` + `MarkEntityDirty` for
  stroke-level undo and prefab persistence.
- **No autotiling prior art** in the engine. We design it ourselves: a neighbor
  bitmask -> tile-index rule set.

## Decision

**Custom Component Mode for painting** (not the PaintBrush), borrowing
GradientSignal's undo/sparse-buffer pattern; **our own autotiling rule sets**.

Rationale: tiles are discrete, so we want a stamp ("set cell (x,y) to tile id"),
not a blended value. A custom `EditorBaseComponentMode` handler gives exact control
(left-drag paints the selected tile, right-drag erases, modifiers for rect-fill and
flood-fill, a brush-size for N x N stamps) while reusing the proven viewport-ray and
undo plumbing. We can adopt the PaintBrush *manipulator* later if we want its
brush-radius UX, but the value/blend model is the wrong abstraction for indices.

## Editor painting

- A `DioramaTilemapEditorComponent` enters a Tilemap **component mode** with a tile
  palette panel (pick the active tile from the bound atlas) and tools: Paint, Erase,
  Rectangle, Fill (flood), Pick (eyedropper), brush size.
- `HandleMouseInteraction`: take the mouse ray, intersect the tilemap plane
  (`ray vs plane`), convert the hit to integer cell coordinates (clamped to the
  grid), and stamp the active tile via the existing `SetTile`/`Fill` presenter path,
  so the editor and runtime stay one code path.
- Wrap each stroke in `AzToolsFramework::ScopedUndoBatch` and call `MarkEntityDirty`
  so paints are undoable and serialize into the prefab. Accumulate the stroke's
  touched cells (old + new tile id) in a sparse buffer, exactly as GradientSignal
  does, so undo replays a whole stroke, not per-cell.

## Autotiling (our design)

Tiles can belong to an **autotile group**. After a paint that touches a cell, we
re-evaluate the 3x3 neighborhood: build an 8-bit mask of which neighbors are in the
same group (N, NE, E, SE, S, SW, W, NW), and look up the display tile from a rule
set.

```
struct AutotileRule { uint8 neighborMask; uint16 tileId; };  // per group
// On edit of cell c:
//   for each of c and its 8 neighbors in the group:
//     mask = neighbor-membership bits
//     cell.displayTile = ruleSet.lookup(group, mask)   // fallback: nearest mask
```

- Ship rule sets for the common schemes: the 16-tile **4-bit edge** set (cardinal
  neighbors only, the simplest) and the 47-tile **blob** set (corners included).
- A `DioramaTilesetRule` asset (JSON) maps masks to atlas cells, authored once per
  tileset. Validated and bounded on load.

## Layers and collision

- **Layers**: multiple tilemap components on child entities, ordered by the existing
  `SortOffset`, give background/main/foreground layers with no new mechanism. The
  editor mode targets one active layer at a time.
- **Per-tile collision**: a tile can be flagged solid (a bitset over the atlas, or a
  per-cell flag). On bake/activate, solid cells generate colliders for the 2D
  collision system ([Docs/design/2d-collision.md](2d-collision.md)): merge runs of
  solid cells into a few box colliders (greedy strip merge) rather than one per
  cell, so a wall is a handful of boxes. This is the natural join point between the
  two Tier-2 features.

## Security and performance

- Cell coordinates from a viewport ray are clamped to the grid before any write; no
  click can index out of bounds.
- Grid dimensions, tile ids, and rule-set sizes are validated and bounded at load
  (consistent with the existing tilemap builder's untrusted-input stance).
- Autotile re-evaluation touches only the 3x3 around an edit, not the whole map.
- Collider generation merges runs (bounded box count), not one collider per cell.

## Phasing

1. **v1**: viewport painting (paint/erase/rect/fill/pick, brush size), stroke undo,
   prefab persistence. The core workflow win.
2. **v2**: autotiling (4-bit edge set first, then blob), `DioramaTilesetRule` asset
   + a small rule editor or a shipped default.
3. **v3**: layer-aware editing UX and per-tile collision generation into the 2D
   collision system.

## Verification plan (needs the editor)

- Left-drag paints a contiguous line of the active tile; right-drag erases; rect and
  fill behave; undo/redo restores exact prior state; save reloads identically.
- Autotiling: paint a blob and watch borders/corners resolve; erase re-resolves
  neighbors.
- Solid-tile collision: a painted wall stops a transform-driven actor via
  `OnContactBegin`, and the generated collider count is a small merged set.

## Open questions

- Tile palette UI: a dockable widget vs an inspector control; reuse any existing
  asset-thumbnail grid widget in AzToolsFramework if one fits.
- Whether to adopt the PaintBrush manipulator purely for its brush-radius viewport
  overlay while keeping our discrete stamping underneath.
