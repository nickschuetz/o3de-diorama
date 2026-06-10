# Tilemap v2: rule tiles, animated tiles, per-tile collision

Status: rule tiles **shipped**; per-tile collision **shipped** (greedy-mesh core +
runtime collider registration); animated tiles **shipped** (pure frame-selection
core + presenter wiring + bus verbs). All three v2 items are in.

The base tilemap (atlas grid, multi-layer asset + builder, Tiled import, paint
tool, 4-bit and 47-blob autotiling, per-tile flip/rotate) is in. v2 closes the
three remaining items the roadmap called out.

## 1. Custom rule tiles (shipped)

The built-in autotiling assumes the art block is laid out in the gem's canonical
order (16 cells in edge-mask order, or 47 in blob order). An imported tileset is
often laid out differently, so the canonical index points at the wrong art. Rule
tiles let an author map each meaningful neighborhood to its own cell.

- A pure `RuleEntry { mask, offset }` and `RuleSetOffset(mask8, rules)` in
  [`TilemapAutotile.h`](../../Code/Source/Clients/TilemapAutotile.h): the neighbor
  mask is normalized the same way the blob scheme normalizes it (so an author lists
  only the 47 meaningful neighborhoods, not 256 raw masks), then matched against the
  rules in order; the first match's offset wins, else it falls back to the canonical
  blob index. Pure and unit tested.
- The rules live on the tilemap config (`m_autotileRules`, authored in the
  Inspector under "Autotiling"), and the `AutotileRules(baseTileIndex)` bus verb
  applies them, rewriting the integer grid exactly like `Autotile`/`AutotileBlob`.
  No rendering change, so it is fully exercised by the unit tests and the verify
  loop (read cells back through `GetTilemapInfo` / the tile getters).

## 2. Per-tile collision (core shipped; runtime pending)

A tile world should collide without hand-placing colliders. The expensive naive
approach is one collider per solid cell; the right approach is to merge runs of
solid cells into a few large boxes.

- The merge is a pure greedy mesh in
  [`TilemapCollision.h`](../../Code/Source/Clients/TilemapCollision.h):
  `MergeSolidCells(columns, rows, isSolid)` grows each unclaimed solid cell right
  then down into a maximal rectangle, producing a small, deterministic set of
  `CellBox`es; `ToWorldBox` turns a cell box into a centered world AABB. Pure and
  unit tested (a full grid collapses to one box; an L-shape covers exactly the solid
  cells with no overlap).
- **Runtime (shipped):** the config gains a **Solid Tiles** set (atlas indices that
  collide) and a **Collision Layer**. At activate (and whenever the tiles change) the
  runtime tilemap greedy-meshes its solid cells, converts each merged `CellBox` to a
  box `Collision2D::Collider` in the world X,Z plane (`BuildTilemapColliders`, unit
  tested), and registers the set with the collision world via the new
  `SetStaticColliders(owner, boxes)` path. These are **query-only static geometry**:
  they participate in overlap / raycast / push-out (so a moving collider blocks against
  the map) but do not generate per-box contact-begin/stay/end notifications, which keeps
  the change small and avoids spawning one entity per region. Assumes the tilemap is
  axis-aligned in the X,Z collision plane (its default orientation). A future option:
  opt-in contact events, and an oriented-plane mode for rotated maps.

## 3. Animated tiles (shipped)

Some cells should cycle through frames (water, torches, coins). The design and
what shipped:

- A config list of animated-tile definitions (`m_animatedTiles`, authored in the
  Inspector under "Animation"): each `TilemapAnimatedTileData` maps a painted
  `m_tileIndex` to a sequence of atlas `m_frames` played at `m_fps`, with a `m_loop`
  flag (wrap, or hold the last frame after one pass).
- A pure frame-selection core in
  [`TilemapAnimation.h`](../../Code/Source/Clients/TilemapAnimation.h):
  `FrameAtTime(elapsedSeconds, fps, frameCount, loop)` returns the frame index now
  (frame 0 for a static/single-frame/non-positive-fps input; modulo when looping;
  clamped to the last frame otherwise). Pure and unit tested.
- The presenter ([`TilemapPresenter`](../../Code/Source/Clients/TilemapPresenter.cpp))
  resolves each cell's painted index to its current frame's atlas cell through
  `DisplayTileIndex` (orientation flags preserved), and ticks while any animated tile
  exists: it advances one map-wide clock so every instance of a tile stays in phase,
  and re-pushes an animated cell only on the ticks where its frame actually changes.
  Non-animated cells and static maps cost nothing (no tick when the definition list
  is empty).
- AI parity: `DefineAnimatedTile(tileIndex, frames, fps, loop)` and
  `ClearAnimatedTiles()` on `DioramaTilemapRequestBus`, and `animatedTileCount` on
  `GetTilemapInfo` for read-back, so an agent authors and confirms animations without
  the Inspector.

## Why split this way

Rule tiles are pure grid math, so they ship and verify headlessly today, exactly
like the existing autotile verbs. Per-tile collision's geometry is equally pure and
shipped/tested; only the collider *spawning* is a runtime behaviour. Animated tiles
keep the same shape: the frame-selection math is a pure, unit-tested core, and the
presenter wiring on top is the thin render-path layer (its visible result is what the
on-screen pass confirms). The pure cores stay green and tested headlessly; the visual
wiring is verified with a monitor, consistent with how every visual Diorama feature
reaches "fully done."
