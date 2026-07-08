/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <cmath>
#include <cstddef>

// Pure, header-only core for DragonBones 5.x surface deformation and FFD (free-form
// mesh deform): the math that warps a mesh vertex through a surface's control-point
// grid, and that offsets a bind vertex by an animated FFD delta. Like MeshSkin.h it has
// no engine, component, or asset dependency (the parser fills SurfaceGrid; the presenter
// supplies vertices and submits the result), so it is unit tested directly.
//
// This is the v3 (surface + FFD) foundation on top of v2 weighted-mesh skinning. A
// surface is a bone that carries a (segmentX+1) x (segmentY+1) grid of control points
// over a canonical +/-200 square in the bone's local space; meshes parented to a surface
// are warped by it. See Docs/design/2d-surface-deform.md.
namespace Diorama::SurfaceDeform
{
    //! Half-width of the canonical square a surface grid spans in its bone-local space
    //! (DragonBones fixes this at 200), so the grid covers [-200, 200] on each axis.
    static constexpr float CanonicalHalfSize = 200.0f;

    //! Full width of the canonical square (the grid is subdivided into segmentX columns
    //! and segmentY rows of this total span).
    static constexpr float CanonicalSize = 2.0f * CanonicalHalfSize;

    //! Beyond this distance from the origin (in surface-local space) the warp is skipped
    //! and the point is returned unchanged, matching the reference runtime's cutoff so a
    //! far-away vertex is not sent through a wild extrapolation of the boundary cell.
    static constexpr float FarFieldLimit = 1000.0f;

    //! A surface's control-point grid. m_controlPoints holds the DEFORMED grid-node
    //! positions in the surface's local space, row-major and X-major: control point
    //! (i, j) is at index i + j * (segmentX + 1), with i in [0, segmentX], j in
    //! [0, segmentY]. In the bind pose the points trace the canonical +/-200 grid;
    //! animation offsets them. Size is expected to be (segmentX + 1) * (segmentY + 1).
    struct SurfaceGrid
    {
        int m_segmentX = 0;
        int m_segmentY = 0;
        AZStd::vector<AZ::Vector2> m_controlPoints;
    };

    //! Number of control points a grid of these segments has: (segmentX+1)*(segmentY+1).
    inline int ControlPointCount(int segmentX, int segmentY)
    {
        return (segmentX + 1) * (segmentY + 1);
    }

    //! Fetch control point (i, j) with bounds safety (out-of-range returns zero). i is the
    //! column (0..segmentX), j the row (0..segmentY).
    inline AZ::Vector2 ControlPoint(const SurfaceGrid& grid, int i, int j)
    {
        const int stride = grid.m_segmentX + 1;
        const int index = i + j * stride;
        if (index < 0 || index >= static_cast<int>(grid.m_controlPoints.size()))
        {
            return AZ::Vector2::CreateZero();
        }
        return grid.m_controlPoints[index];
    }

    //! Warp a point given in the surface's canonical local space (the +/-200 grid space)
    //! to its deformed position, by piecewise-affine interpolation over the triangulated
    //! control-point grid. Each cell is split by its (i+1,j)->(i,j+1) diagonal into two
    //! triangles; the point's cell and triangle are located, and the deformed position is
    //! the barycentric blend of that triangle's three deformed control points. That blend
    //! is exactly the unique affine mapping the triangle's undeformed corners onto its
    //! deformed corners, so this reproduces the reference runtime's per-triangle affine
    //! warp (C0, no cross-cell smoothing) while extrapolating cleanly for points in the
    //! edge band. Degenerate grids and far-field points pass through unchanged. Pure.
    inline AZ::Vector2 WarpPoint(const SurfaceGrid& grid, const AZ::Vector2& p)
    {
        if (grid.m_segmentX <= 0 || grid.m_segmentY <= 0 ||
            static_cast<int>(grid.m_controlPoints.size()) < ControlPointCount(grid.m_segmentX, grid.m_segmentY))
        {
            return p; // degenerate / undersized grid: no warp
        }
        if (std::fabs(p.GetX()) > FarFieldLimit || std::fabs(p.GetY()) > FarFieldLimit)
        {
            return p; // far outside the surface: skip rather than extrapolate wildly
        }

        const float dX = CanonicalSize / static_cast<float>(grid.m_segmentX);
        const float dY = CanonicalSize / static_cast<float>(grid.m_segmentY);

        // Fractional grid coordinates: (u, v) = 0 at the bottom-left control point,
        // (segmentX, segmentY) at the top-right.
        const float u = (p.GetX() + CanonicalHalfSize) / dX;
        const float v = (p.GetY() + CanonicalHalfSize) / dY;

        // Cell index, clamped to the interior so an edge-band point uses the nearest cell
        // and extrapolates it (fu / fv then fall outside [0, 1]).
        int i = static_cast<int>(std::floor(u));
        int j = static_cast<int>(std::floor(v));
        i = i < 0 ? 0 : (i > grid.m_segmentX - 1 ? grid.m_segmentX - 1 : i);
        j = j < 0 ? 0 : (j > grid.m_segmentY - 1 ? grid.m_segmentY - 1 : j);
        const float fu = u - static_cast<float>(i);
        const float fv = v - static_cast<float>(j);

        const AZ::Vector2 a = ControlPoint(grid, i, j); // (i,   j)
        const AZ::Vector2 b = ControlPoint(grid, i + 1, j); // (i+1, j)
        const AZ::Vector2 c = ControlPoint(grid, i, j + 1); // (i,   j+1)

        // Lower-left triangle (a, b, c), selected when fu + fv <= 1. The blend collapses
        // to a*(1-fu-fv) + b*fu + c*fv, which is the affine taking the undeformed corners
        // (i,j),(i+1,j),(i,j+1) onto their deformed positions.
        if (fu + fv <= 1.0f)
        {
            return a * (1.0f - fu - fv) + b * fu + c * fv;
        }

        // Upper-right triangle (b, c, d): the point is past the cell diagonal.
        const AZ::Vector2 d = ControlPoint(grid, i + 1, j + 1); // (i+1, j+1)
        return b * (1.0f - fv) + c * (1.0f - fu) + d * (fu + fv - 1.0f);
    }

    //! Add an FFD deform delta to a bind position. FFD offsets are applied in the mesh's
    //! local / bind space, BEFORE skinning or the surface warp, so a deformed vertex is
    //! WarpPoint(grid, ApplyDeform(bindPos, delta)) for a surface mesh, or the skin of
    //! ApplyDeform(bindPos, delta) for a weighted mesh. Trivial, but names the order.
    inline AZ::Vector2 ApplyDeform(const AZ::Vector2& bindPos, const AZ::Vector2& delta)
    {
        return bindPos + delta;
    }

    //! Apply per-vertex FFD deltas to a run of bind positions in place (the common case:
    //! offset the whole mesh before skin/warp). Only the overlapping prefix is touched, so
    //! a shorter delta array leaves the trailing vertices at their bind positions.
    inline void ApplyDeform(AZStd::span<AZ::Vector2> bindPositions, AZStd::span<const AZ::Vector2> deltas)
    {
        const size_t count = bindPositions.size() < deltas.size() ? bindPositions.size() : deltas.size();
        for (size_t k = 0; k < count; ++k)
        {
            bindPositions[k] += deltas[k];
        }
    }
} // namespace Diorama::SurfaceDeform
