/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

// Pure, header-only field-of-view (line-of-sight) on a grid by recursive
// shadowcasting. It has no engine / component dependency: the caller supplies an
// opacity predicate (often a tilemap's wall tiles) and a visit callback, so the
// visibility is unit tested directly (the Pathfinding.h / MovementRange.h pattern).
// Grid intelligence for roguelike / tactics games: what a unit can see, and the
// visible set a fog-of-war layer then marks explored.
namespace Diorama::FieldOfView
{
    namespace Internal
    {
        inline int Abs(int v)
        {
            return v < 0 ? -v : v;
        }

        // Octant transform multipliers (Bjorn Bergstrom's recursive shadowcasting).
        constexpr int Mult[4][8] = {
            { 1, 0, 0, -1, -1, 0, 0, 1 },
            { 0, 1, -1, 0, 0, -1, 1, 0 },
            { 0, 1, 1, 0, 0, -1, -1, 0 },
            { 1, 0, 0, 1, -1, 0, 0, -1 },
        };

        template<class IsOpaque, class MarkVisible>
        inline void CastLight(
            int columns,
            int rows,
            int cx,
            int cy,
            int row,
            float startSlope,
            float endSlope,
            int radius,
            int xx,
            int xy,
            int yx,
            int yy,
            IsOpaque& isOpaque,
            MarkVisible& markVisible)
        {
            if (startSlope < endSlope)
            {
                return;
            }
            const int radius2 = radius * radius;
            float nextStartSlope = startSlope;
            for (int i = row; i <= radius; ++i)
            {
                bool blocked = false;
                for (int dx = -i, dy = -i; dx <= 0; ++dx)
                {
                    const float lSlope = (dx - 0.5f) / (dy + 0.5f);
                    const float rSlope = (dx + 0.5f) / (dy - 0.5f);
                    if (startSlope < rSlope)
                    {
                        continue;
                    }
                    if (endSlope > lSlope)
                    {
                        break;
                    }

                    const int sax = dx * xx + dy * xy;
                    const int say = dx * yx + dy * yy;
                    if ((sax < 0 && Abs(sax) > cx) || (say < 0 && Abs(say) > cy))
                    {
                        continue;
                    }
                    const int ax = cx + sax;
                    const int ay = cy + say;
                    if (ax < 0 || ax >= columns || ay < 0 || ay >= rows)
                    {
                        continue;
                    }

                    if (dx * dx + dy * dy <= radius2)
                    {
                        markVisible(ax, ay);
                    }

                    if (blocked)
                    {
                        if (isOpaque(ax, ay))
                        {
                            nextStartSlope = rSlope;
                            continue;
                        }
                        blocked = false;
                        startSlope = nextStartSlope;
                    }
                    else if (isOpaque(ax, ay) && i < radius)
                    {
                        blocked = true;
                        CastLight(columns, rows, cx, cy, i + 1, startSlope, lSlope, radius, xx, xy, yx, yy, isOpaque, markVisible);
                        nextStartSlope = rSlope;
                    }
                }
                if (blocked)
                {
                    break;
                }
            }
        }
    } // namespace Internal

    //! Compute the cells visible from (originCol, originRow) within `radius` (Euclidean)
    //! on a `columns` x `rows` grid, calling markVisible(col, row) for each. isOpaque(col,
    //! row) returns whether a cell blocks sight; it is only queried for in-bounds cells.
    //! The origin is always visible. Symmetric recursive shadowcasting over 8 octants:
    //! a wall is itself visible but blocks everything behind it. markVisible is
    //! idempotent-safe to call: cells on octant seams may be reported more than once,
    //! so the caller should record into a set rather than count calls.
    template<class IsOpaque, class MarkVisible>
    inline void Compute(int columns, int rows, int originCol, int originRow, int radius, IsOpaque&& isOpaque, MarkVisible&& markVisible)
    {
        if (columns <= 0 || rows <= 0 || radius < 0)
        {
            return;
        }
        if (originCol < 0 || originCol >= columns || originRow < 0 || originRow >= rows)
        {
            return;
        }
        markVisible(originCol, originRow);
        for (int oct = 0; oct < 8; ++oct)
        {
            Internal::CastLight(
                columns,
                rows,
                originCol,
                originRow,
                1,
                1.0f,
                0.0f,
                radius,
                Internal::Mult[0][oct],
                Internal::Mult[1][oct],
                Internal::Mult[2][oct],
                Internal::Mult[3][oct],
                isOpaque,
                markVisible);
        }
    }
} // namespace Diorama::FieldOfView
