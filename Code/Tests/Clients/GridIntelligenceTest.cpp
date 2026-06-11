/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>

#include <Clients/FieldOfView.h>
#include <Clients/MovementRange.h>
#include <Clients/Pathfinding.h>

namespace Diorama
{
    // ---- A* pathfinding ----------------------------------------------------------

    TEST(PathfindingTest, StraightPathOnOpenGridHasManhattanLength)
    {
        const auto open = [](int, int)
        {
            return true;
        };
        AZStd::vector<Pathfinding::Cell> path;
        ASSERT_TRUE(Pathfinding::FindPath(10, 10, { 0, 0 }, { 3, 2 }, open, path));
        EXPECT_EQ(path.size(), 6u); // 3 + 2 steps, inclusive of both ends = 6 cells
        EXPECT_EQ(path.front(), (Pathfinding::Cell{ 0, 0 }));
        EXPECT_EQ(path.back(), (Pathfinding::Cell{ 3, 2 }));
    }

    TEST(PathfindingTest, RoutesAroundAWall)
    {
        // A vertical wall at column 2 for rows 0..2, with a gap at row 3.
        const auto passable = [](int c, int r)
        {
            return !(c == 2 && r <= 2);
        };
        AZStd::vector<Pathfinding::Cell> path;
        ASSERT_TRUE(Pathfinding::FindPath(6, 6, { 0, 0 }, { 4, 0 }, passable, path));
        // It must detour through the row-3 gap, so it is longer than the straight 5.
        EXPECT_GT(path.size(), 5u);
        // No path cell sits on the wall.
        for (const auto& cell : path)
        {
            EXPECT_FALSE(cell.m_col == 2 && cell.m_row <= 2);
        }
    }

    TEST(PathfindingTest, UnreachableGoalReturnsFalse)
    {
        // Seal the goal cell off with walls on all four sides.
        const auto passable = [](int c, int r)
        {
            if (c == 5 && r == 5)
            {
                return true; // the goal itself
            }
            return !((c == 4 && r == 5) || (c == 6 && r == 5) || (c == 5 && r == 4) || (c == 5 && r == 6));
        };
        AZStd::vector<Pathfinding::Cell> path;
        EXPECT_FALSE(Pathfinding::FindPath(10, 10, { 0, 0 }, { 5, 5 }, passable, path));
        EXPECT_TRUE(path.empty());
    }

    TEST(PathfindingTest, OutOfBoundsAndSameCell)
    {
        const auto open = [](int, int)
        {
            return true;
        };
        AZStd::vector<Pathfinding::Cell> path;
        EXPECT_FALSE(Pathfinding::FindPath(5, 5, { -1, 0 }, { 3, 3 }, open, path));
        ASSERT_TRUE(Pathfinding::FindPath(5, 5, { 2, 2 }, { 2, 2 }, open, path));
        EXPECT_EQ(path.size(), 1u); // start == goal
    }

    // ---- Movement range ----------------------------------------------------------

    TEST(MovementRangeTest, OpenGridFloodsADiamond)
    {
        const auto cost1 = [](int, int)
        {
            return 1;
        };
        const auto cells = MovementRange::ComputeReachable(11, 11, 5, 5, 2, cost1);
        // Manhattan-disc of radius 2 on a 4-grid: 1 + 4 + 8 = 13 cells.
        EXPECT_EQ(cells.size(), 13u);
        for (const auto& c : cells)
        {
            const int manhattan = (c.m_col > 5 ? c.m_col - 5 : 5 - c.m_col) + (c.m_row > 5 ? c.m_row - 5 : 5 - c.m_row);
            EXPECT_EQ(c.m_cost, manhattan);
        }
    }

    TEST(MovementRangeTest, RespectsTerrainCostAndBlocks)
    {
        // Column 6 costs 2 to enter; column 7 is blocked (cost 0).
        const auto cost = [](int c, int)
        {
            if (c == 7)
            {
                return 0; // blocked
            }
            return c == 6 ? 2 : 1;
        };
        const auto cells = MovementRange::ComputeReachable(12, 3, 5, 1, 3, cost);
        const auto find = [&cells](int col, int row) -> int
        {
            for (const auto& c : cells)
            {
                if (c.m_col == col && c.m_row == row)
                {
                    return c.m_cost;
                }
            }
            return -1;
        };
        EXPECT_EQ(find(5, 1), 0); // start
        EXPECT_EQ(find(6, 1), 2); // costly tile
        EXPECT_EQ(find(7, 1), -1); // blocked: never reached
        EXPECT_EQ(find(4, 1), 1); // open neighbor
    }

    // ---- Field of view -----------------------------------------------------------

    TEST(FieldOfViewTest, OpenRoomSeesEverythingInRadius)
    {
        const auto clear = [](int, int)
        {
            return false;
        };
        AZStd::unordered_set<int> visible;
        const auto mark = [&visible](int c, int r)
        {
            visible.insert(r * 100 + c);
        };
        FieldOfView::Compute(11, 11, 5, 5, 3, clear, mark);
        EXPECT_TRUE(visible.count(5 * 100 + 5) > 0); // origin
        EXPECT_TRUE(visible.count(5 * 100 + 8) > 0); // 3 east, in radius
        EXPECT_TRUE(visible.count(2 * 100 + 5) > 0); // 3 north
        EXPECT_FALSE(visible.count(5 * 100 + 9) > 0); // 4 east, outside radius
    }

    TEST(FieldOfViewTest, WallCastsAShadow)
    {
        // A wall at (7,5) directly east of the origin at (5,5).
        const auto wallAt7 = [](int c, int r)
        {
            return c == 7 && r == 5;
        };
        AZStd::unordered_set<int> visible;
        const auto mark = [&visible](int c, int r)
        {
            visible.insert(r * 100 + c);
        };
        FieldOfView::Compute(11, 11, 5, 5, 5, wallAt7, mark);
        EXPECT_TRUE(visible.count(5 * 100 + 6) > 0); // before the wall: visible
        EXPECT_TRUE(visible.count(5 * 100 + 7) > 0); // the wall itself: visible
        EXPECT_FALSE(visible.count(5 * 100 + 8) > 0); // directly behind the wall: shadowed
        EXPECT_FALSE(visible.count(5 * 100 + 9) > 0);
    }
} // namespace Diorama
