# How-To: Grid Intelligence (FOV, movement range, A* pathfinding)

Roguelikes and grid tactics games need three classic algorithms on a tile grid:
**field of view** (what a unit can see, feeding fog of war), **movement range**
(the blue tiles a unit can step to), and **A\* pathfinding** (the route between two
cells). Diorama ships all three as pure, unit-tested header cores that take a grid
predicate, so they work over a Diorama tilemap or any grid you describe.

These are gameplay-grid cores. **FOV / fog of war is in-vision** (it feeds visibility
rendering); **movement range and A\*** are general grid utilities kept here for
convenience and could move to a sibling gameplay gem later. None of them couples to
the engine: each takes a callback for the grid, so the algorithm stays testable and
reusable.

## Pathfinding (A\*)

```cpp
#include <Clients/Pathfinding.h>

// isPassable: can this cell be entered? (e.g. not a solid tilemap tile)
auto isPassable = [&](int col, int row) { return !tilemap.IsSolid(col, row); };

AZStd::vector<Diorama::Pathfinding::Cell> path;
if (Diorama::Pathfinding::FindPath(columns, rows, {sx, sy}, {gx, gy}, isPassable, path))
{
    // path is start -> goal inclusive; walk the unit along it.
}
```

Shortest 4-connected path, uniform step cost, Manhattan heuristic. Returns `false`
(empty path) when the goal is walled off or an endpoint is out of bounds / blocked.

## Movement range

```cpp
#include <Clients/MovementRange.h>

// stepInto: cost to ENTER a cell; <= 0 means blocked.
auto stepInto = [&](int col, int row) { return tilemap.IsSolid(col, row) ? 0 : terrainCost(col, row); };

auto reachable = Diorama::MovementRange::ComputeReachable(columns, rows, sx, sy, budget, stepInto);
for (const auto& cell : reachable) { /* cell.m_col, cell.m_row, cell.m_cost <= budget */ }
```

A Dijkstra flood honoring per-cell entry costs, so rough terrain costs more and walls
are skipped. Each returned cell carries the minimum cost to reach it, in nondecreasing
order, ready to highlight as the move overlay.

## Field of view + fog of war

```cpp
#include <Clients/FieldOfView.h>

auto isOpaque = [&](int col, int row) { return tilemap.BlocksSight(col, row); };
auto markVisible = [&](int col, int row) { visibleThisTurn.insert({col, row}); explored.insert({col, row}); };

Diorama::FieldOfView::Compute(columns, rows, ux, uy, sightRadius, isOpaque, markVisible);
```

Symmetric recursive shadowcasting over 8 octants: a wall is itself visible but blocks
everything behind it. Treat `markVisible` as idempotent: cells on the octant seams
(the axes and diagonals) can be reported by more than one octant, so insert into a set
rather than counting calls. Record each visible cell as **visible now** (full bright)
and as **explored** (dim) to drive a fog-of-war layer that hides the unexplored map and
dims what is remembered but out of sight.

Render the fog by tinting / hiding sprites or tilemap cells by their visibility state
(Diorama is world-space, so the fog is in the world, not a screen overlay).

## What is and isn't here

The cores are the algorithms; wiring them to a **specific** grid is the game's job
(pass the predicate). A convenience that reads a Diorama tilemap's solid-tile set as
the grid and exposes `FindPath` / `ComputeFOV` / `ComputeRange` over a bus for Lua /
agents is a natural follow-up; today the cores are C++ (the tests under
`GridIntelligenceTest.cpp` are runnable usage examples).
