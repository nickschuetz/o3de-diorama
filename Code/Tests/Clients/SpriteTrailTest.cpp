/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/SpriteTrail.h>

namespace Diorama
{
    TEST(SpriteTrailTest, GhostAlphaFallsOffGeometrically)
    {
        // Freshest ghost is at startAlpha; each older one is scaled by fade.
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(0.5f, 0.6f, 0), 0.5f);
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(0.5f, 0.6f, 1), 0.3f);
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(0.5f, 0.6f, 2), 0.18f);
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(0.5f, 0.6f, 3), 0.108f);
    }

    TEST(SpriteTrailTest, GhostAlphaClampsInputsAndResult)
    {
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(2.0f, 0.6f, 0), 1.0f); // startAlpha clamped to 1
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(0.5f, 2.0f, 1), 0.5f); // fade clamped to 1 -> no falloff
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(0.5f, -1.0f, 1), 0.0f); // fade clamped to 0 -> all-but-first vanish
        EXPECT_FLOAT_EQ(SpriteTrail::GhostAlpha(-1.0f, 0.6f, 0), 0.0f); // startAlpha clamped to 0
    }

    TEST(SpriteTrailTest, CapturesDueOnTheInterval)
    {
        float acc = 0.0f;
        // Below the interval: nothing captured, time banked.
        EXPECT_EQ(SpriteTrail::CapturesDue(acc, 0.03f, 0.05f, 8), 0);
        EXPECT_NEAR(acc, 0.03f, 1e-6f);
        // Crossing it captures once and keeps the remainder for even spacing.
        EXPECT_EQ(SpriteTrail::CapturesDue(acc, 0.03f, 0.05f, 8), 1);
        EXPECT_NEAR(acc, 0.01f, 1e-6f);
    }

    TEST(SpriteTrailTest, CapturesDueHandlesMultiAndCap)
    {
        float acc = 0.0f;
        // A big step yields several captures, capped at maxCaptures (the ghost count),
        // with any backlog beyond the cap dropped so the accumulator can't grow.
        EXPECT_EQ(SpriteTrail::CapturesDue(acc, 1.0f, 0.05f, 4), 4);
        EXPECT_FLOAT_EQ(acc, 0.0f);

        // A non-positive interval captures exactly once per step.
        float acc2 = 0.0f;
        EXPECT_EQ(SpriteTrail::CapturesDue(acc2, 0.016f, 0.0f, 4), 1);
        // A non-positive dt captures nothing.
        EXPECT_EQ(SpriteTrail::CapturesDue(acc2, 0.0f, 0.05f, 4), 0);
    }
} // namespace Diorama
