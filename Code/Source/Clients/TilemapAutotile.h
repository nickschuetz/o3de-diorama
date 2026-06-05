/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>

#include <cstddef>

// Pure, header-only core for tilemap autotiling: turn a cell's neighborhood into a
// display-tile index so borders and corners connect themselves. It has no engine /
// component dependency (the caller supplies a membership predicate and applies the
// result through the tilemap bus), so it is unit tested directly, the same pattern as
// TilemapPaint.h / Collision2D.h.
//
// v1 is the 16-tile **4-bit edge** scheme (cardinal neighbors only): the autotile
// group's art is a block of 16 atlas cells laid out in edge-mask order, so the cell
// at baseTile + mask is the art for that neighbor configuration. The 47-tile blob
// scheme (corners included) is a documented follow-up that reuses NeighborMask8.
namespace Diorama::TilemapAutotile
{
    //! 8-neighbor membership bits (grid rows increase downward, so "north" = row - 1,
    //! matching TilemapPaint's row convention).
    enum Neighbor : AZ::u8
    {
        N = 1 << 0,
        NE = 1 << 1,
        E = 1 << 2,
        SE = 1 << 3,
        S = 1 << 4,
        SW = 1 << 5,
        W = 1 << 6,
        NW = 1 << 7,
    };

    //! 4-bit edge-mask bit values (cardinal neighbors only): the index into a 16-cell
    //! autotile block.
    enum Edge : AZ::u8
    {
        EdgeN = 1 << 0,
        EdgeE = 1 << 1,
        EdgeS = 1 << 2,
        EdgeW = 1 << 3,
    };

    //! Build the 8-bit membership mask of the eight neighbors of (col, row).
    //! isMember(c, r) returns whether that cell belongs to the autotile group; it is
    //! expected to return false for out-of-grid cells, so borders resolve to edge
    //! tiles. The cell's own membership is not part of the mask.
    template<class IsMember>
    inline AZ::u8 NeighborMask8(int col, int row, IsMember&& isMember)
    {
        AZ::u8 mask = 0;
        if (isMember(col, row - 1))
        {
            mask |= N;
        }
        if (isMember(col + 1, row - 1))
        {
            mask |= NE;
        }
        if (isMember(col + 1, row))
        {
            mask |= E;
        }
        if (isMember(col + 1, row + 1))
        {
            mask |= SE;
        }
        if (isMember(col, row + 1))
        {
            mask |= S;
        }
        if (isMember(col - 1, row + 1))
        {
            mask |= SW;
        }
        if (isMember(col - 1, row))
        {
            mask |= W;
        }
        if (isMember(col - 1, row - 1))
        {
            mask |= NW;
        }
        return mask;
    }

    //! Reduce an 8-bit neighbor mask to the 4-bit cardinal edge mask (0..15).
    inline AZ::u8 EdgeMask4(AZ::u8 mask8)
    {
        AZ::u8 edge = 0;
        if (mask8 & N)
        {
            edge |= EdgeN;
        }
        if (mask8 & E)
        {
            edge |= EdgeE;
        }
        if (mask8 & S)
        {
            edge |= EdgeS;
        }
        if (mask8 & W)
        {
            edge |= EdgeW;
        }
        return edge;
    }

    //! Display tile for a member cell: the group's 16-tile block (starting at
    //! baseTile) laid out in edge-mask order, so block cell baseTile + mask is the art
    //! for that neighbor configuration.
    inline int EdgeTileIndex(int baseTile, AZ::u8 edgeMask4)
    {
        return baseTile + static_cast<int>(edgeMask4);
    }
} // namespace Diorama::TilemapAutotile
