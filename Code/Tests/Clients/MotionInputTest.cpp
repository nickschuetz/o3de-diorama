/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <Clients/MotionInput.h>

namespace Diorama
{
    namespace
    {
        AZStd::span<const MotionInput::Sample> Hist(const AZStd::vector<MotionInput::Sample>& h)
        {
            return AZStd::span<const MotionInput::Sample>(h.data(), h.size());
        }
        AZStd::span<const MotionInput::Direction> Mot(const AZStd::vector<MotionInput::Direction>& m)
        {
            return AZStd::span<const MotionInput::Direction>(m.data(), m.size());
        }
    } // namespace

    TEST(MotionInputTest, QuarterCircleForwardMatchesInOrderAndWindow)
    {
        // 2 (down), 3 (down-forward), 6 (forward) entered over the last 0.2s.
        const AZStd::vector<MotionInput::Sample> history = { { 5, 0.0f }, { 2, 0.30f }, { 3, 0.36f }, { 6, 0.42f } };
        const AZStd::vector<MotionInput::Direction> qcf = { 2, 3, 6 };
        EXPECT_TRUE(MotionInput::Matches(Hist(history), Mot(qcf), 0.3f, 0.42f));
    }

    TEST(MotionInputTest, FailsWhenStartIsOlderThanTheWindow)
    {
        // The down (2) was a full second before the final forward: stale.
        const AZStd::vector<MotionInput::Sample> history = { { 2, 0.0f }, { 3, 0.95f }, { 6, 1.0f } };
        const AZStd::vector<MotionInput::Direction> qcf = { 2, 3, 6 };
        EXPECT_FALSE(MotionInput::Matches(Hist(history), Mot(qcf), 0.3f, 1.0f));
    }

    TEST(MotionInputTest, MustEndOnTheFinalStep)
    {
        // Ends on 3, not 6: a quarter-circle-forward is not complete.
        const AZStd::vector<MotionInput::Sample> history = { { 2, 0.30f }, { 3, 0.36f } };
        const AZStd::vector<MotionInput::Direction> qcf = { 2, 3, 6 };
        EXPECT_FALSE(MotionInput::Matches(Hist(history), Mot(qcf), 0.3f, 0.40f));
    }

    TEST(MotionInputTest, OrderMatters)
    {
        // 6 then 3 then 2 is the reverse: not a quarter-circle-forward.
        const AZStd::vector<MotionInput::Sample> history = { { 6, 0.30f }, { 3, 0.36f }, { 2, 0.42f } };
        const AZStd::vector<MotionInput::Direction> qcf = { 2, 3, 6 };
        EXPECT_FALSE(MotionInput::Matches(Hist(history), Mot(qcf), 0.3f, 0.42f));
    }

    TEST(MotionInputTest, DirectionFromAxesMapsTheNumpadGrid)
    {
        const float dz = 0.3f;
        EXPECT_EQ(MotionInput::DirectionFromAxes(0.0f, 0.0f, dz), 5); // neutral
        EXPECT_EQ(MotionInput::DirectionFromAxes(1.0f, 0.0f, dz), 6); // forward
        EXPECT_EQ(MotionInput::DirectionFromAxes(-1.0f, 0.0f, dz), 4); // back
        EXPECT_EQ(MotionInput::DirectionFromAxes(0.0f, 1.0f, dz), 8); // up
        EXPECT_EQ(MotionInput::DirectionFromAxes(0.0f, -1.0f, dz), 2); // down
        EXPECT_EQ(MotionInput::DirectionFromAxes(1.0f, -1.0f, dz), 3); // down-forward
        EXPECT_EQ(MotionInput::DirectionFromAxes(-1.0f, 1.0f, dz), 7); // up-back
        EXPECT_EQ(MotionInput::DirectionFromAxes(1.0f, 1.0f, dz), 9); // up-forward
        EXPECT_EQ(MotionInput::DirectionFromAxes(-1.0f, -1.0f, dz), 1); // down-back
    }

    TEST(MotionInputTest, DirectionFromAxesRespectsTheDeadZone)
    {
        // A small drift below the dead zone reads as centered on that axis.
        EXPECT_EQ(MotionInput::DirectionFromAxes(0.2f, 0.0f, 0.3f), 5);
        EXPECT_EQ(MotionInput::DirectionFromAxes(0.2f, 0.9f, 0.3f), 8); // x centered, y up
    }

    TEST(MotionInputTest, DragonPunchMatchesForwardDownDownForward)
    {
        // 6 (forward), 2 (down), 3 (down-forward): the classic dragon-punch motion.
        const AZStd::vector<MotionInput::Sample> history = { { 6, 0.10f }, { 2, 0.16f }, { 3, 0.22f } };
        const AZStd::vector<MotionInput::Direction> dp = { 6, 2, 3 };
        EXPECT_TRUE(MotionInput::Matches(Hist(history), Mot(dp), 0.3f, 0.22f));
    }
} // namespace Diorama
