/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/containers/vector.h>

#include <cstddef>

// Pure, header-only core for the tilemap paint tool: which cells a brush, a
// rectangle, or a flood fill cover, given a grid size. It has no engine, editor,
// or viewport dependencies (cell coordinates come from
// TilemapComponentConfig::LocalPositionToCell at the call site), so it is unit
// tested directly, the same pattern as Collision2D.h / Camera2D.h / SpriteBatchPlan.
//
// All outputs are clamped to the grid [0,columns) x [0,rows): no produced cell can
// index out of bounds, whatever a viewport click resolves to.
namespace Diorama::TilemapPaint
{
    struct Cell
    {
        int m_col = 0;
        int m_row = 0;
    };

    inline bool InGrid(int col, int row, int columns, int rows)
    {
        return col >= 0 && col < columns && row >= 0 && row < rows;
    }

    //! Cells covered by a brushSize x brushSize square brush centered on (col, row),
    //! clamped to the grid. brushSize is clamped to at least 1. For even sizes the
    //! brush extends one extra cell toward +col/+row (a deterministic choice).
    inline void BrushCells(int col, int row, int brushSize, int columns, int rows, AZStd::vector<Cell>& out)
    {
        out.clear();
        const int size = brushSize < 1 ? 1 : brushSize;
        const int half = size / 2;
        const int startCol = col - half;
        const int startRow = row - half;
        for (int r = 0; r < size; ++r)
        {
            for (int c = 0; c < size; ++c)
            {
                const int cc = startCol + c;
                const int rr = startRow + r;
                if (InGrid(cc, rr, columns, rows))
                {
                    out.push_back(Cell{ cc, rr });
                }
            }
        }
    }

    //! Cells in the inclusive rectangle between two corners (in any order), clamped
    //! to the grid.
    inline void RectCells(int c0, int r0, int c1, int r1, int columns, int rows, AZStd::vector<Cell>& out)
    {
        out.clear();
        int minCol = c0 < c1 ? c0 : c1;
        int maxCol = c0 < c1 ? c1 : c0;
        int minRow = r0 < r1 ? r0 : r1;
        int maxRow = r0 < r1 ? r1 : r0;
        minCol = minCol < 0 ? 0 : minCol;
        minRow = minRow < 0 ? 0 : minRow;
        maxCol = maxCol >= columns ? columns - 1 : maxCol;
        maxRow = maxRow >= rows ? rows - 1 : maxRow;
        for (int r = minRow; r <= maxRow; ++r)
        {
            for (int c = minCol; c <= maxCol; ++c)
            {
                out.push_back(Cell{ c, r });
            }
        }
    }

    //! 4-connected flood fill from the seed cell over every cell whose current tile
    //! equals the seed's tile, bounded to the grid. tileAt(col, row) returns the
    //! current tile id at a cell (templated so the core stays dependency-free and
    //! testable). Returns nothing if the seed is out of grid.
    template<class TileAt>
    void FloodFillCells(int seedCol, int seedRow, int columns, int rows, TileAt tileAt, AZStd::vector<Cell>& out)
    {
        out.clear();
        if (!InGrid(seedCol, seedRow, columns, rows) || columns <= 0 || rows <= 0)
        {
            return;
        }
        const auto target = tileAt(seedCol, seedRow);

        AZStd::vector<bool> visited(static_cast<size_t>(columns) * static_cast<size_t>(rows), false);
        AZStd::vector<Cell> stack;
        stack.push_back(Cell{ seedCol, seedRow });
        visited[static_cast<size_t>(seedRow) * columns + seedCol] = true;

        const int dc[4] = { 1, -1, 0, 0 };
        const int dr[4] = { 0, 0, 1, -1 };
        while (!stack.empty())
        {
            const Cell cell = stack.back();
            stack.pop_back();
            out.push_back(cell);
            for (int i = 0; i < 4; ++i)
            {
                const int nc = cell.m_col + dc[i];
                const int nr = cell.m_row + dr[i];
                if (!InGrid(nc, nr, columns, rows))
                {
                    continue;
                }
                const size_t idx = static_cast<size_t>(nr) * columns + nc;
                if (!visited[idx] && tileAt(nc, nr) == target)
                {
                    visited[idx] = true;
                    stack.push_back(Cell{ nc, nr });
                }
            }
        }
    }

    //! One cell's before/after tile id, accumulated across a stroke so undo replays
    //! the whole stroke (not one cell at a time). m_oldTile lets undo restore exactly.
    struct Edit
    {
        int m_col = 0;
        int m_row = 0;
        int m_oldTile = -1;
        int m_newTile = -1;
    };
} // namespace Diorama::TilemapPaint
