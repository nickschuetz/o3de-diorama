/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/MathUtils.h>

// Pure, header-only core for the sprite afterimage trail: the fading-ghost effect
// behind a moving sprite (dash and super trails). The presenter captures recent
// poses on an interval and draws each as a faded copy; the only math worth testing
// independently is the per-ghost alpha falloff and the capture-interval decision, so
// they live here (no engine dependency), the same pattern as SpriteCull.h.
namespace Diorama::SpriteTrail
{
    //! Alpha of ghost `index` (0 = the freshest captured ghost): a geometric falloff
    //! `startAlpha * fade^index`, so each older ghost is fainter. Inputs are clamped to
    //! 0..1 and the result is clamped to 0..1. A loop (not pow) keeps it dependency-free
    //! and exact for the small ghost counts a trail uses.
    inline float GhostAlpha(float startAlpha, float fade, int index)
    {
        const float clampedFade = AZ::GetClamp(fade, 0.0f, 1.0f);
        float alpha = AZ::GetClamp(startAlpha, 0.0f, 1.0f);
        for (int i = 0; i < index; ++i)
        {
            alpha *= clampedFade;
        }
        return AZ::GetClamp(alpha, 0.0f, 1.0f);
    }

    //! Advance the capture accumulator by `deltaSeconds` and report how many ghost
    //! captures are due this step (usually 0 or 1; >1 only after a long hitch). The
    //! remaining sub-interval time is left in `accumulator` so spacing stays even. A
    //! non-positive interval captures every step (clamped to one capture). Catch-up is
    //! capped at `maxCaptures` so a huge dt cannot spin the ring.
    inline int CapturesDue(float& accumulator, float deltaSeconds, float intervalSeconds, int maxCaptures)
    {
        if (deltaSeconds <= 0.0f)
        {
            return 0;
        }
        accumulator += deltaSeconds;
        if (intervalSeconds <= 0.0f)
        {
            accumulator = 0.0f;
            return 1;
        }
        int captures = 0;
        while (accumulator >= intervalSeconds && captures < maxCaptures)
        {
            accumulator -= intervalSeconds;
            ++captures;
        }
        // Drop any backlog beyond the cap so the accumulator cannot grow unbounded.
        if (accumulator >= intervalSeconds)
        {
            accumulator = 0.0f;
        }
        return captures;
    }
} // namespace Diorama::SpriteTrail
