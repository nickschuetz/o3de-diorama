/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/MathUtils.h>

// Pure, header-only core for the sprite palette recolor (team / alt colors): remap a
// pixel's luminance through a three-stop color ramp (shadow -> mid -> highlight), so
// one sprite sheet yields P1 / P2 variants by swapping the ramp, no index-encoded art
// needed. The shader (DioramaSprite.azsl) mirrors this math exactly; keeping it here
// lets the mapping be unit tested without a GPU, the same pattern as SpriteCull.h.
namespace Diorama::SpritePalette
{
    //! Perceptual luminance of a linear RGB color (Rec. 601 weights), matching the
    //! shader's dot product. The result is in 0..1 for inputs in 0..1.
    inline float Luminance(float r, float g, float b)
    {
        return 0.299f * r + 0.587f * g + 0.114f * b;
    }

    //! One channel of the three-stop ramp at `luma` (clamped 0..1): the lower half
    //! interpolates shadow -> mid, the upper half mid -> highlight. Call once per
    //! channel with that channel's three ramp values.
    inline float RampChannel(float luma, float shadow, float mid, float highlight)
    {
        const float t = AZ::GetClamp(luma, 0.0f, 1.0f);
        if (t < 0.5f)
        {
            return AZ::Lerp(shadow, mid, t * 2.0f);
        }
        return AZ::Lerp(mid, highlight, (t - 0.5f) * 2.0f);
    }
} // namespace Diorama::SpritePalette
