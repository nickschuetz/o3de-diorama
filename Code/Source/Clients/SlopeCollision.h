/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/containers/span.h>

// Pure, header-only core for side-scroller platforming: walking on flat ground and
// ramps (slopes), and dropping through / landing on one-way platforms. It has no
// engine / component / physics dependency (the component supplies the floor segments
// projected from the tilemap and applies the resolved ground height), so the geometry
// is unit tested directly, the same pattern as Collision2D.h / TilemapCollision.h.
//
// All values are in the collision plane: X is the horizontal walk axis, Y is up.
namespace Diorama::SlopeCollision
{
    //! A walkable floor span on world X in [m_x0, m_x1] (m_x0 < m_x1), with surface
    //! height m_y0 at m_x0 and m_y1 at m_x1, linear between. A flat tile top has
    //! m_y0 == m_y1; a ramp has them differ. The component builds one of these per
    //! exposed tile top (flat or slope) and feeds the set to ProbeGround.
    struct FloorSegment
    {
        float m_x0 = 0.0f;
        float m_x1 = 0.0f;
        float m_y0 = 0.0f;
        float m_y1 = 0.0f;
    };

    //! True when x lies within the segment's horizontal span (inclusive).
    inline bool ContainsX(const FloorSegment& s, float x)
    {
        return x >= s.m_x0 && x <= s.m_x1;
    }

    //! Surface height of a segment at world x. x outside the span clamps to the nearer
    //! end (so a body just past the edge still reads the edge height, not a wild slope
    //! extrapolation). A degenerate (zero-width) segment returns m_y0.
    inline float SurfaceYAt(const FloorSegment& s, float x)
    {
        const float width = s.m_x1 - s.m_x0;
        if (width <= 0.0f)
        {
            return s.m_y0;
        }
        float t = (x - s.m_x0) / width;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return s.m_y0 + (s.m_y1 - s.m_y0) * t;
    }

    //! Ground-follow probe for a body standing at column x with its feet at footY.
    //! Among the segments whose span contains x, consider every surface that is at or
    //! below footY + stepUp (so the body can step up a small lip / walk up a ramp) and
    //! no lower than footY - maxDrop (so it does not snap to a floor far below, e.g.
    //! across a gap). Returns the highest such surface in outGroundY and true; returns
    //! false (no snap) when nothing qualifies, leaving the body to fall.
    //!
    //! stepUp absorbs the per-frame rise of a ramp and small steps; maxDrop keeps the
    //! body falling off ledges instead of teleporting down. Both are world units.
    inline bool ProbeGround(AZStd::span<const FloorSegment> segments, float x, float footY, float maxDrop, float stepUp, float& outGroundY)
    {
        bool found = false;
        float best = 0.0f;
        const float ceiling = footY + stepUp;
        const float floor = footY - maxDrop;
        for (const FloorSegment& s : segments)
        {
            if (!ContainsX(s, x))
            {
                continue;
            }
            const float surface = SurfaceYAt(s, x);
            if (surface > ceiling || surface < floor)
            {
                continue; // too high to step onto, or too far below to snap to
            }
            if (!found || surface > best)
            {
                best = surface;
                found = true;
            }
        }
        if (found)
        {
            outGroundY = best;
        }
        return found;
    }

    //! One-way platform landing test. A body whose feet move from prevFootY (last
    //! frame) to currFootY (this frame) lands on a platform whose top is at `top` only
    //! when it was at or above the top last frame and is at or below it now: it is
    //! crossing the top downward. A body moving up, or already below the top, passes
    //! through. Returns whether it should be placed on top.
    inline bool ResolveOneWayLanding(float prevFootY, float currFootY, float top)
    {
        return prevFootY >= top && currFootY <= top;
    }
} // namespace Diorama::SlopeCollision
