/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/containers/vector.h>

#include <cstddef>
#include <limits>

// Pure, header-only A* pathfinding on a 4-connected grid. It has no engine / component
// dependency: the caller supplies a passability predicate (often from a tilemap's
// solid-tile set), so the search is unit tested directly, the same pattern as
// TilemapCollision.h / DepthLane.h. Grid intelligence for tactics / roguelike games.
namespace Diorama::Pathfinding
{
    //! A grid cell in column/row coordinates.
    struct Cell
    {
        int m_col = 0;
        int m_row = 0;
    };

    inline bool operator==(const Cell& a, const Cell& b)
    {
        return a.m_col == b.m_col && a.m_row == b.m_row;
    }

    //! Find the shortest 4-connected path from `start` to `goal` (both inclusive) on a
    //! `columns` x `rows` grid. isPassable(col, row) returns whether a cell can be
    //! entered; it is only queried for in-bounds cells. Step cost is uniform (1), and
    //! the Manhattan distance is the (admissible) heuristic. On success fills outPath
    //! in order from start to goal and returns true; on an unreachable goal, an
    //! out-of-bounds / impassable endpoint, or empty grid, clears outPath and returns
    //! false. Ties break toward the cell visited first, so the path is deterministic.
    template<class IsPassable>
    inline bool FindPath(int columns, int rows, Cell start, Cell goal, IsPassable&& isPassable, AZStd::vector<Cell>& outPath)
    {
        outPath.clear();
        if (columns <= 0 || rows <= 0)
        {
            return false;
        }
        const auto inBounds = [columns, rows](int c, int r)
        {
            return c >= 0 && c < columns && r >= 0 && r < rows;
        };
        if (!inBounds(start.m_col, start.m_row) || !inBounds(goal.m_col, goal.m_row))
        {
            return false;
        }
        if (!isPassable(start.m_col, start.m_row) || !isPassable(goal.m_col, goal.m_row))
        {
            return false;
        }

        const size_t count = static_cast<size_t>(columns) * static_cast<size_t>(rows);
        const auto index = [columns](int c, int r)
        {
            return static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(c);
        };
        constexpr int Inf = std::numeric_limits<int>::max();

        AZStd::vector<int> g(count, Inf);
        AZStd::vector<int> cameFrom(count, -1);
        AZStd::vector<bool> closed(count, false);

        const auto heuristic = [&goal](int c, int r)
        {
            const int dc = c > goal.m_col ? c - goal.m_col : goal.m_col - c;
            const int dr = r > goal.m_row ? r - goal.m_row : goal.m_row - r;
            return dc + dr;
        };

        // Open set as parallel cell/f-score lists; pop the lowest f by linear scan
        // (grids here are small; no heap needed for a deterministic result).
        AZStd::vector<size_t> openCells;
        AZStd::vector<int> openF;
        g[index(start.m_col, start.m_row)] = 0;
        openCells.push_back(index(start.m_col, start.m_row));
        openF.push_back(heuristic(start.m_col, start.m_row));

        const int dCol[4] = { 1, -1, 0, 0 };
        const int dRow[4] = { 0, 0, 1, -1 };

        while (!openCells.empty())
        {
            // Find the open cell with the smallest f.
            size_t best = 0;
            for (size_t i = 1; i < openF.size(); ++i)
            {
                if (openF[i] < openF[best])
                {
                    best = i;
                }
            }
            const size_t current = openCells[best];
            openCells[best] = openCells.back();
            openF[best] = openF.back();
            openCells.pop_back();
            openF.pop_back();

            if (closed[current])
            {
                continue;
            }
            closed[current] = true;

            const int cc = static_cast<int>(current % static_cast<size_t>(columns));
            const int cr = static_cast<int>(current / static_cast<size_t>(columns));
            if (cc == goal.m_col && cr == goal.m_row)
            {
                // Reconstruct path back to start, then reverse.
                for (size_t node = current;; node = static_cast<size_t>(cameFrom[node]))
                {
                    outPath.push_back(Cell{ static_cast<int>(node % static_cast<size_t>(columns)),
                                            static_cast<int>(node / static_cast<size_t>(columns)) });
                    if (cameFrom[node] < 0)
                    {
                        break;
                    }
                }
                for (size_t i = 0, j = outPath.size(); i < j / 2; ++i)
                {
                    const Cell tmp = outPath[i];
                    outPath[i] = outPath[j - 1 - i];
                    outPath[j - 1 - i] = tmp;
                }
                return true;
            }

            for (int n = 0; n < 4; ++n)
            {
                const int nc = cc + dCol[n];
                const int nr = cr + dRow[n];
                if (!inBounds(nc, nr) || !isPassable(nc, nr))
                {
                    continue;
                }
                const size_t ni = index(nc, nr);
                if (closed[ni])
                {
                    continue;
                }
                const int tentative = g[current] + 1;
                if (tentative < g[ni])
                {
                    g[ni] = tentative;
                    cameFrom[ni] = static_cast<int>(current);
                    openCells.push_back(ni);
                    openF.push_back(tentative + heuristic(nc, nr));
                }
            }
        }
        return false; // goal never reached
    }
} // namespace Diorama::Pathfinding
