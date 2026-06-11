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
} // namespace Diorama
