/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

// Pure, header-only core for 2.5D brawler depth lanes. A brawler treats the floor as
// a 2D plane: X is left/right, depth (often the world Z) is toward/away from the
// camera. Characters move on the floor; combat connects only when attacker and target
// share a depth lane; and in an orthographic view, sprites are lifted up-screen by
// their depth to fake perspective. These helpers cover that math with no engine /
// component dependency, so they are unit tested directly (the Collision2D / SlopeCollision
// pattern). A tilted/perspective camera does the lift and draw-ordering for free; the
// lift / sort helpers here are for the flat, front-on orthographic setup common in
// pixel brawlers.
namespace Diorama::DepthLane
{
    //! True when two depths are within `tolerance` of each other: a hit, grab, or
    //! pickup lands only when both characters are on (nearly) the same lane. This is
    //! the depth-aware-collision gate a brawler applies on top of a 2D hitbox overlap.
    //! A negative tolerance never matches.
    inline bool SameLane(float depthA, float depthB, float tolerance)
    {
        if (tolerance < 0.0f)
        {
            return false;
        }
        const float d = depthA - depthB;
        const float ad = d < 0.0f ? -d : d;
        return ad <= tolerance;
    }

    //! Up-screen lift for an orthographic 2.5D view: a character deeper in (larger
    //! depth) is drawn higher, faking perspective. Returns `depth * liftPerUnit`, added
    //! to the sprite's Y.
    inline float ScreenLift(float depth, float liftPerUnit)
    {
        return depth * liftPerUnit;
    }

    //! Draw-order bias from depth: nearer the camera (smaller depth) draws on top.
    //! Returns `-depth * sortPerUnit`, so a smaller depth yields a larger sort value.
    //! Feed it to the sprite sort offset when not relying on camera-distance sorting.
    inline float SortBias(float depth, float sortPerUnit)
    {
        return -depth * sortPerUnit;
    }

    //! Move `current` toward `target` by at most `maxDelta` (clamped to >= 0). Reaches
    //! the target exactly without overshoot; the primitive for lane snapping and smooth
    //! depth approach.
    inline float MoveToward(float current, float target, float maxDelta)
    {
        const float step = maxDelta < 0.0f ? 0.0f : maxDelta;
        const float diff = target - current;
        if (diff > step)
        {
            return current + step;
        }
        if (diff < -step)
        {
            return current - step;
        }
        return target;
    }

    //! Clamp a floor position to a rectangular arena [minX, maxX] x [minDepth, maxDepth]
    //! so characters cannot walk off the play field. Degenerate ranges (max < min) leave
    //! the value at the min edge.
    inline void ClampToArena(float& x, float& depth, float minX, float maxX, float minDepth, float maxDepth)
    {
        x = x < minX ? minX : (x > maxX ? maxX : x);
        depth = depth < minDepth ? minDepth : (depth > maxDepth ? maxDepth : depth);
    }
} // namespace Diorama::DepthLane
