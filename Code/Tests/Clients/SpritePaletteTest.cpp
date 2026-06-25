/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/SpritePalette.h>

namespace Diorama
{
    TEST(SpritePaletteTest, LuminanceUsesRec601Weights)
    {
        EXPECT_FLOAT_EQ(SpritePalette::Luminance(0.0f, 0.0f, 0.0f), 0.0f);
        EXPECT_FLOAT_EQ(SpritePalette::Luminance(1.0f, 1.0f, 1.0f), 1.0f);
        EXPECT_FLOAT_EQ(SpritePalette::Luminance(1.0f, 0.0f, 0.0f), 0.299f);
        EXPECT_FLOAT_EQ(SpritePalette::Luminance(0.0f, 1.0f, 0.0f), 0.587f);
        EXPECT_FLOAT_EQ(SpritePalette::Luminance(0.0f, 0.0f, 1.0f), 0.114f);
    }

    TEST(SpritePaletteTest, RampHitsTheStopsAndInterpolatesBetween)
    {
        // shadow = 0.2, mid = 0.6, highlight = 1.0 on one channel.
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(0.0f, 0.2f, 0.6f, 1.0f), 0.2f); // shadow at black
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(0.5f, 0.2f, 0.6f, 1.0f), 0.6f); // mid at the seam
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(1.0f, 0.2f, 0.6f, 1.0f), 1.0f); // highlight at white
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(0.25f, 0.2f, 0.6f, 1.0f), 0.4f); // halfway shadow->mid
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(0.75f, 0.2f, 0.6f, 1.0f), 0.8f); // halfway mid->highlight
    }

    TEST(SpritePaletteTest, RampClampsLumaOutsideUnitRange)
    {
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(-1.0f, 0.2f, 0.6f, 1.0f), 0.2f); // below 0 -> shadow
        EXPECT_FLOAT_EQ(SpritePalette::RampChannel(2.0f, 0.2f, 0.6f, 1.0f), 1.0f); // above 1 -> highlight
    }
} // namespace Diorama
