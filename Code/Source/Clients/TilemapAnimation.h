/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>

// Pure, header-only core for animated tilemap tiles: given a shared elapsed-time
// clock, decide which frame of a cycling tile is showing. It has no engine /
// component / render dependency (the presenter supplies the clock and applies the
// chosen frame's atlas index to the cell draw), so the timing is unit tested
// directly, the same pattern as TilemapAutotile.h / TilemapCollision.h.
//
// All cells that share an animated-tile definition advance off the same clock, so
// every instance (e.g. every water tile) stays in sync, which is the usual look and
// also the cheapest: the presenter re-pushes the affected cells only when the frame
// actually changes.
namespace Diorama::TilemapAnimation
{
    //! Frame index (0..frameCount-1) shown at elapsedSeconds for a sequence of
    //! frameCount frames played at fps. A non-positive fps or a single/empty frame
    //! list holds frame 0 (static). When loop is true the sequence wraps; otherwise
    //! it holds the last frame once it finishes.
    inline int FrameAtTime(float elapsedSeconds, float fps, int frameCount, bool loop)
    {
        if (frameCount <= 1 || fps <= 0.0f || elapsedSeconds <= 0.0f)
        {
            return 0;
        }
        // Whole frames elapsed (floor; elapsedSeconds > 0 so truncation is the floor).
        const AZ::s64 advanced = static_cast<AZ::s64>(elapsedSeconds * fps);
        if (loop)
        {
            return static_cast<int>(advanced % frameCount);
        }
        return advanced >= frameCount ? (frameCount - 1) : static_cast<int>(advanced);
    }
} // namespace Diorama::TilemapAnimation
