/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>

#include <cstddef>

// Pure, header-only core for fighting-game motion inputs: match a directional input
// history against a named motion (quarter-circle, dragon-punch, charge, etc.). It has
// no engine/component dependency (the input component records the 8-way direction each
// frame with a timestamp and calls this to test for a recognized motion), so the
// recognition is unit tested directly, the same pattern as InputActionMap.h.
//
// Directions use numpad notation, the fighting-game standard:
//   7 8 9      up-back   up    up-forward
//   4 5 6  =   back   neutral  forward
//   1 2 3      down-back down  down-forward
namespace Diorama::MotionInput
{
    //! Numpad direction (1..9; 5 is neutral). 0 means "no/unknown direction".
    using Direction = AZ::u8;

    //! One recorded input: the direction held, and the time (seconds) it was entered.
    struct Sample
    {
        Direction m_direction = 5;
        float m_time = 0.0f;
    };

    //! Quantize a 2D directional axis (x right-positive, y up-positive, each in
    //! roughly [-1,1]) into a numpad direction. A component reads the value of a
    //! configured Axis2D action each frame and calls this; the recognition then runs
    //! on the resulting direction history. `deadZone` is the magnitude below which an
    //! axis counts as centered, so a resting stick reads neutral (5), not a diagonal.
    inline Direction DirectionFromAxes(float x, float y, float deadZone)
    {
        const int h = (x > deadZone) ? 1 : (x < -deadZone ? -1 : 0);
        const int v = (y > deadZone) ? 1 : (y < -deadZone ? -1 : 0);
        // Numpad layout: 5 is neutral, +x moves +1, +y moves +3 (7 8 9 / 4 5 6 / 1 2 3).
        return static_cast<Direction>(5 + h + 3 * v);
    }

    //! Match the most recent inputs in `history` (oldest first) against `motion` (the
    //! required direction sequence, e.g. {2,3,6} for a quarter-circle-forward). The
    //! motion must appear as a subsequence ending at the latest sample, with each step
    //! entered within `windowSeconds` of the final input (so an old, stale start does
    //! not count). Intermediate diagonals may be skipped (a lenient match), but the
    //! ordered required steps must each appear. Returns true on a match.
    inline bool Matches(AZStd::span<const Sample> history, AZStd::span<const Direction> motion, float windowSeconds, float nowSeconds)
    {
        if (motion.empty())
        {
            return false;
        }
        if (history.empty())
        {
            return false;
        }
        // Walk the history backward, matching the motion from its last step to its
        // first. A required step matches the newest unmatched sample equal to it.
        size_t need = motion.size(); // 1-based index from the end we still must satisfy
        for (size_t i = history.size(); i-- > 0;)
        {
            const Sample& s = history[i];
            if (nowSeconds - s.m_time > windowSeconds)
            {
                break; // older than the window: stop scanning
            }
            if (s.m_direction == motion[need - 1])
            {
                --need;
                if (need == 0)
                {
                    return true; // matched every step within the window
                }
            }
        }
        return false;
    }
} // namespace Diorama::MotionInput
