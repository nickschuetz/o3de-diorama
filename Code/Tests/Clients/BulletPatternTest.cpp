/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/containers/vector.h>

#include <Clients/BulletPattern.h>

namespace Diorama
{
    TEST(BulletPatternTest, Ring_EvenlySpacedAtFullSpeed)
    {
        AZStd::vector<AZ::Vector2> v;
        BulletPattern::Ring(4, 0.0f, 2.0f, v);
        ASSERT_EQ(v.size(), 4u);
        // 4 bullets at 0, 90, 180, 270 degrees, each at speed 2.
        EXPECT_NEAR(v[0].GetX(), 2.0f, 1e-4f); // +X
        EXPECT_NEAR(v[0].GetY(), 0.0f, 1e-4f);
        EXPECT_NEAR(v[1].GetX(), 0.0f, 1e-4f); // +Y
        EXPECT_NEAR(v[1].GetY(), 2.0f, 1e-4f);
        EXPECT_NEAR(v[2].GetX(), -2.0f, 1e-4f); // -X
        EXPECT_NEAR(v[3].GetY(), -2.0f, 1e-4f); // -Y
        // Every bullet has the same speed.
        for (const AZ::Vector2& vel : v)
        {
            EXPECT_NEAR(vel.GetLength(), 2.0f, 1e-4f);
        }
    }

    TEST(BulletPatternTest, Ring_RejectsNonPositiveCountOrSpeed)
    {
        AZStd::vector<AZ::Vector2> v;
        BulletPattern::Ring(0, 0.0f, 2.0f, v);
        EXPECT_TRUE(v.empty());
        BulletPattern::Ring(8, 0.0f, 0.0f, v);
        EXPECT_TRUE(v.empty());
    }

    TEST(BulletPatternTest, Fan_SingleBulletAimsStraight)
    {
        AZStd::vector<AZ::Vector2> v;
        const float aim = AZ::DegToRad(90.0f); // up
        BulletPattern::Fan(1, aim, AZ::DegToRad(60.0f), 3.0f, v);
        ASSERT_EQ(v.size(), 1u);
        EXPECT_NEAR(v[0].GetX(), 0.0f, 1e-4f);
        EXPECT_NEAR(v[0].GetY(), 3.0f, 1e-4f);
    }

    TEST(BulletPatternTest, Fan_SpansArcInclusiveOfEdges)
    {
        AZStd::vector<AZ::Vector2> v;
        const float aim = 0.0f; // +X
        const float spread = AZ::DegToRad(90.0f);
        BulletPattern::Fan(3, aim, spread, 1.0f, v);
        ASSERT_EQ(v.size(), 3u);
        // First edge at -45, middle at 0, last edge at +45.
        EXPECT_NEAR(std::atan2(v[0].GetY(), v[0].GetX()), AZ::DegToRad(-45.0f), 1e-4f);
        EXPECT_NEAR(std::atan2(v[1].GetY(), v[1].GetX()), 0.0f, 1e-4f);
        EXPECT_NEAR(std::atan2(v[2].GetY(), v[2].GetX()), AZ::DegToRad(45.0f), 1e-4f);
    }

    TEST(BulletPatternTest, AdvanceSpiral_AddsAndWraps)
    {
        EXPECT_NEAR(BulletPattern::AdvanceSpiral(0.0f, AZ::DegToRad(30.0f)), AZ::DegToRad(30.0f), 1e-4f);
        // Wraps past 2pi back into [0, 2pi).
        const float wrapped = BulletPattern::AdvanceSpiral(AZ::Constants::TwoPi - AZ::DegToRad(10.0f), AZ::DegToRad(30.0f));
        EXPECT_NEAR(wrapped, AZ::DegToRad(20.0f), 1e-3f);
    }
} // namespace Diorama
