/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/DepthLane.h>

namespace Diorama
{
    TEST(DepthLaneTest, SameLaneWithinTolerance)
    {
        EXPECT_TRUE(DepthLane::SameLane(2.0f, 2.3f, 0.5f)); // 0.3 apart, tol 0.5
        EXPECT_TRUE(DepthLane::SameLane(2.0f, 2.0f, 0.0f)); // exact
        EXPECT_FALSE(DepthLane::SameLane(2.0f, 3.0f, 0.5f)); // 1.0 apart, tol 0.5
        EXPECT_FALSE(DepthLane::SameLane(2.0f, 2.1f, -1.0f)); // negative tolerance never matches
    }

    TEST(DepthLaneTest, SameLaneIsSymmetric)
    {
        EXPECT_EQ(DepthLane::SameLane(1.0f, 1.6f, 0.5f), DepthLane::SameLane(1.6f, 1.0f, 0.5f));
        EXPECT_FALSE(DepthLane::SameLane(1.6f, 1.0f, 0.5f));
    }

    TEST(DepthLaneTest, ScreenLiftScalesWithDepth)
    {
        EXPECT_FLOAT_EQ(DepthLane::ScreenLift(0.0f, 0.25f), 0.0f);
        EXPECT_FLOAT_EQ(DepthLane::ScreenLift(4.0f, 0.25f), 1.0f); // deeper -> higher on screen
    }

    TEST(DepthLaneTest, SortBiasPutsNearerOnTop)
    {
        // Smaller depth (nearer) yields a larger sort value than a larger depth.
        const float near = DepthLane::SortBias(1.0f, 1.0f);
        const float far = DepthLane::SortBias(5.0f, 1.0f);
        EXPECT_GT(near, far);
    }

    TEST(DepthLaneTest, MoveTowardReachesWithoutOvershoot)
    {
        EXPECT_FLOAT_EQ(DepthLane::MoveToward(0.0f, 1.0f, 0.3f), 0.3f); // step toward
        EXPECT_FLOAT_EQ(DepthLane::MoveToward(0.0f, 1.0f, 5.0f), 1.0f); // clamps to target, no overshoot
        EXPECT_FLOAT_EQ(DepthLane::MoveToward(1.0f, 0.0f, 0.4f), 0.6f); // toward a smaller target
        EXPECT_FLOAT_EQ(DepthLane::MoveToward(2.0f, 2.0f, 1.0f), 2.0f); // already there
    }

    TEST(DepthLaneTest, ClampToArenaKeepsCharactersOnTheField)
    {
        float x = 12.0f;
        float depth = -3.0f;
        DepthLane::ClampToArena(x, depth, -10.0f, 10.0f, 0.0f, 4.0f);
        EXPECT_FLOAT_EQ(x, 10.0f); // clamped to maxX
        EXPECT_FLOAT_EQ(depth, 0.0f); // clamped to minDepth

        float x2 = 0.0f;
        float depth2 = 2.0f;
        DepthLane::ClampToArena(x2, depth2, -10.0f, 10.0f, 0.0f, 4.0f);
        EXPECT_FLOAT_EQ(x2, 0.0f); // inside: unchanged
        EXPECT_FLOAT_EQ(depth2, 2.0f);
    }
} // namespace Diorama
