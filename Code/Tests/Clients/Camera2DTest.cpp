/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/Camera2D.h>

namespace Diorama
{
    class Camera2DTest : public ::testing::Test
    {
    protected:
        static constexpr float Tol = 1e-3f;
    };

    TEST_F(Camera2DTest, SmoothFollow_ZeroSmoothTime_Snaps)
    {
        const AZ::Vector2 r = Camera2D::SmoothFollow(AZ::Vector2(0.0f, 0.0f), AZ::Vector2(5.0f, -2.0f), 0.0f, 0.016f);
        EXPECT_NEAR(r.GetX(), 5.0f, Tol);
        EXPECT_NEAR(r.GetY(), -2.0f, Tol);
    }

    TEST_F(Camera2DTest, SmoothFollow_MovesPartwayTowardTarget)
    {
        const AZ::Vector2 r = Camera2D::SmoothFollow(AZ::Vector2(0.0f, 0.0f), AZ::Vector2(10.0f, 0.0f), 0.25f, 0.1f);
        // Moves toward the target but does not overshoot or arrive.
        EXPECT_GT(r.GetX(), 0.0f);
        EXPECT_LT(r.GetX(), 10.0f);
    }

    TEST_F(Camera2DTest, SmoothFollow_Converges)
    {
        AZ::Vector2 c(0.0f, 0.0f);
        const AZ::Vector2 target(10.0f, 5.0f);
        for (int i = 0; i < 600; ++i)
        {
            c = Camera2D::SmoothFollow(c, target, 0.1f, 0.016f);
        }
        EXPECT_NEAR(c.GetX(), 10.0f, Tol);
        EXPECT_NEAR(c.GetY(), 5.0f, Tol);
    }

    TEST_F(Camera2DTest, SmoothFollow_FrameRateIndependent)
    {
        // One step of dt equals two steps of dt/2 (the point of exponential smoothing).
        const AZ::Vector2 start(0.0f, 0.0f);
        const AZ::Vector2 target(8.0f, 0.0f);
        const AZ::Vector2 oneBig = Camera2D::SmoothFollow(start, target, 0.3f, 0.2f);
        AZ::Vector2 twoSmall = Camera2D::SmoothFollow(start, target, 0.3f, 0.1f);
        twoSmall = Camera2D::SmoothFollow(twoSmall, target, 0.3f, 0.1f);
        EXPECT_NEAR(oneBig.GetX(), twoSmall.GetX(), Tol);
    }

    TEST_F(Camera2DTest, Deadzone_TargetInside_NoMove)
    {
        const AZ::Vector2 cam(0.0f, 0.0f);
        const AZ::Vector2 r = Camera2D::ApplyDeadzone(cam, AZ::Vector2(0.5f, -0.5f), AZ::Vector2(1.0f, 1.0f));
        EXPECT_NEAR(r.GetX(), 0.0f, Tol);
        EXPECT_NEAR(r.GetY(), 0.0f, Tol);
    }

    TEST_F(Camera2DTest, Deadzone_TargetOutside_MovesByExcess)
    {
        const AZ::Vector2 cam(0.0f, 0.0f);
        // Target 3 to the right, deadzone half 1: camera should move so target sits
        // on the +1 boundary, i.e. camera at x = 2.
        const AZ::Vector2 r = Camera2D::ApplyDeadzone(cam, AZ::Vector2(3.0f, 0.0f), AZ::Vector2(1.0f, 1.0f));
        EXPECT_NEAR(r.GetX(), 2.0f, Tol);
        EXPECT_NEAR(r.GetY(), 0.0f, Tol);
    }

    TEST_F(Camera2DTest, ClampToBounds_InsideUnchanged_OutsideClamped)
    {
        const AZ::Vector2 in = Camera2D::ClampToBounds(AZ::Vector2(2.0f, 2.0f), AZ::Vector2(-10.0f, -10.0f), AZ::Vector2(10.0f, 10.0f));
        EXPECT_NEAR(in.GetX(), 2.0f, Tol);
        const AZ::Vector2 out = Camera2D::ClampToBounds(AZ::Vector2(50.0f, -50.0f), AZ::Vector2(-10.0f, -10.0f), AZ::Vector2(10.0f, 10.0f));
        EXPECT_NEAR(out.GetX(), 10.0f, Tol);
        EXPECT_NEAR(out.GetY(), -10.0f, Tol);
    }

    TEST_F(Camera2DTest, ClampToBounds_BoundsSmallerThanView_Centers)
    {
        // min > max on X: that axis centers between them.
        const AZ::Vector2 r = Camera2D::ClampToBounds(AZ::Vector2(99.0f, 0.0f), AZ::Vector2(4.0f, -1.0f), AZ::Vector2(2.0f, 1.0f));
        EXPECT_NEAR(r.GetX(), 3.0f, Tol); // (4 + 2) / 2
    }

    TEST_F(Camera2DTest, Lookahead_StationaryIsZero_MovingLeads)
    {
        const AZ::Vector2 stay = Camera2D::Lookahead(AZ::Vector2(0.0f, 0.0f), 2.0f);
        EXPECT_NEAR(stay.GetLength(), 0.0f, Tol);
        const AZ::Vector2 lead = Camera2D::Lookahead(AZ::Vector2(5.0f, 0.0f), 2.0f);
        EXPECT_NEAR(lead.GetX(), 2.0f, Tol);
        EXPECT_NEAR(lead.GetY(), 0.0f, Tol);
    }

    TEST_F(Camera2DTest, PixelSnap_SnapsToGrid_AndDisables)
    {
        const AZ::Vector2 snapped = Camera2D::PixelSnap(AZ::Vector2(0.13f, -0.27f), 16.0f);
        // 0.13 * 16 = 2.08 -> 2 -> 0.125; -0.27 * 16 = -4.32 -> -4 -> -0.25
        EXPECT_NEAR(snapped.GetX(), 0.125f, Tol);
        EXPECT_NEAR(snapped.GetY(), -0.25f, Tol);

        const AZ::Vector2 off = Camera2D::PixelSnap(AZ::Vector2(0.13f, -0.27f), 0.0f);
        EXPECT_NEAR(off.GetX(), 0.13f, Tol);
    }

    TEST_F(Camera2DTest, Trauma_AddClampsAndDecayClamps)
    {
        EXPECT_NEAR(Camera2D::AddTrauma(0.8f, 0.5f), 1.0f, Tol); // clamps to 1
        EXPECT_NEAR(Camera2D::AddTrauma(0.2f, 0.3f), 0.5f, Tol);
        EXPECT_NEAR(Camera2D::DecayTrauma(0.1f, 1.0f, 0.5f), 0.0f, Tol); // clamps to 0
        EXPECT_NEAR(Camera2D::DecayTrauma(1.0f, 1.0f, 0.25f), 0.75f, Tol);
    }

    TEST_F(Camera2DTest, ShakeMagnitude_IsSquaredCurve)
    {
        EXPECT_NEAR(Camera2D::ShakeMagnitude(0.5f, 4.0f), 1.0f, Tol); // 4 * 0.25
        EXPECT_NEAR(Camera2D::ShakeMagnitude(1.0f, 4.0f), 4.0f, Tol);
        EXPECT_NEAR(Camera2D::ShakeMagnitude(0.0f, 4.0f), 0.0f, Tol);
    }

    TEST_F(Camera2DTest, ShakeOffset_AlongAngle)
    {
        const AZ::Vector2 r = Camera2D::ShakeOffset(2.0f, 0.0f); // angle 0 -> +x
        EXPECT_NEAR(r.GetX(), 2.0f, Tol);
        EXPECT_NEAR(r.GetY(), 0.0f, Tol);
    }

    TEST_F(Camera2DTest, Midpoint_CentersTwoTargets)
    {
        const AZ::Vector2 m = Camera2D::Midpoint(AZ::Vector2(-2.0f, 4.0f), AZ::Vector2(6.0f, 0.0f));
        EXPECT_NEAR(m.GetX(), 2.0f, Tol);
        EXPECT_NEAR(m.GetY(), 2.0f, Tol);
    }

    TEST_F(Camera2DTest, SeparationDolly_ScalesAndClamps)
    {
        // base 2 + separation * 0.5, clamped to [2, 6].
        EXPECT_NEAR(Camera2D::SeparationDolly(0.0f, 2.0f, 0.5f, 2.0f, 6.0f), 2.0f, Tol);
        EXPECT_NEAR(Camera2D::SeparationDolly(4.0f, 2.0f, 0.5f, 2.0f, 6.0f), 4.0f, Tol); // 2 + 2
        EXPECT_NEAR(Camera2D::SeparationDolly(100.0f, 2.0f, 0.5f, 2.0f, 6.0f), 6.0f, Tol); // clamps to max
        // perUnit <= 0 -> constant base (no separation term).
        EXPECT_NEAR(Camera2D::SeparationDolly(50.0f, 3.0f, 0.0f, 0.0f, 100.0f), 3.0f, Tol);
    }
} // namespace Diorama
