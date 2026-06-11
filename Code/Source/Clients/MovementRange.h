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

// Pure, header-only movement-range flood on a 4-connected grid: which cells a unit can
// reach within a movement budget, honoring per-cell entry costs (the "blue tiles" of a
// tactics game). No engine dependency: the caller supplies the entry-cost function, so
// the flood is unit tested directly (the Pathfinding.h / DepthLane.h pattern).
namespace Diorama::MovementRange
{
    //! A reachable cell and the minimum cost to reach it from the start.
    struct RangeCell
    {
        int m_col = 0;
        int m_row = 0;
        int m_cost = 0;
    };

    //! Cells reachable from `startCol,startRow` within `budget` movement points on a
    //! `columns` x `rows` grid. stepInto(col, row) returns the cost to ENTER a cell; a
    //! cost <= 0 means the cell is blocked (impassable). Uses Dijkstra so each returned
    //! cell carries the minimum cost to reach it (<= budget). The start cell is included
    //! at cost 0 when in bounds. Returns the cells in nondecreasing cost order.
    template<class StepCost>
    inline AZStd::vector<RangeCell> ComputeReachable(int columns, int rows, int startCol, int startRow, int budget, StepCost&& stepInto)
    {
        AZStd::vector<RangeCell> result;
        if (columns <= 0 || rows <= 0 || budget < 0)
        {
            return result;
        }
        if (startCol < 0 || startCol >= columns || startRow < 0 || startRow >= rows)
        {
            return result;
        }

        const size_t count = static_cast<size_t>(columns) * static_cast<size_t>(rows);
        const auto index = [columns](int c, int r)
        {
            return static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(c);
        };
        constexpr int Inf = std::numeric_limits<int>::max();

        AZStd::vector<int> best(count, Inf);
        AZStd::vector<bool> settled(count, false);

        // Frontier as parallel cell/cost lists; pop the lowest cost by linear scan
        // (ranges are small and budget-bounded, so no heap is needed).
        AZStd::vector<size_t> frontier;
        AZStd::vector<int> frontierCost;
        best[index(startCol, startRow)] = 0;
        frontier.push_back(index(startCol, startRow));
        frontierCost.push_back(0);

        const int dCol[4] = { 1, -1, 0, 0 };
        const int dRow[4] = { 0, 0, 1, -1 };

        while (!frontier.empty())
        {
            size_t pick = 0;
            for (size_t i = 1; i < frontierCost.size(); ++i)
            {
                if (frontierCost[i] < frontierCost[pick])
                {
                    pick = i;
                }
            }
            const size_t current = frontier[pick];
            const int currentCost = frontierCost[pick];
            frontier[pick] = frontier.back();
            frontierCost[pick] = frontierCost.back();
            frontier.pop_back();
            frontierCost.pop_back();

            if (settled[current])
            {
                continue;
            }
            settled[current] = true;

            const int cc = static_cast<int>(current % static_cast<size_t>(columns));
            const int cr = static_cast<int>(current / static_cast<size_t>(columns));
            result.push_back(RangeCell{ cc, cr, currentCost });

            for (int n = 0; n < 4; ++n)
            {
                const int nc = cc + dCol[n];
                const int nr = cr + dRow[n];
                if (nc < 0 || nc >= columns || nr < 0 || nr >= rows)
                {
                    continue;
                }
                const int enter = stepInto(nc, nr);
                if (enter <= 0)
                {
                    continue; // blocked
                }
                const int newCost = currentCost + enter;
                if (newCost > budget)
                {
                    continue; // out of movement
                }
                const size_t ni = index(nc, nr);
                if (newCost < best[ni])
                {
                    best[ni] = newCost;
                    frontier.push_back(ni);
                    frontierCost.push_back(newCost);
                }
            }
        }
        return result;
    }
} // namespace Diorama::MovementRange
