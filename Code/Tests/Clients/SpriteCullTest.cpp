/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Math/Frustum.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/Math/Vector3.h>

#include <Clients/SpriteCull.h>

namespace Diorama
{
    namespace
    {
        // A right-handed perspective frustum for a camera at the origin looking down
        // -Z (so world space == view space and world-to-clip is just the projection).
        // 60-degree vertical FOV, square aspect, near 0.1, far 100.
        AZ::Frustum MakeTestFrustum()
        {
            AZ::Matrix4x4 proj;
            AZ::MakePerspectiveFovMatrixRH(proj, AZ::DegToRad(60.0f), 1.0f, 0.1f, 100.0f);
            return AZ::Frustum::CreateFromMatrixColumnMajor(proj, AZ::Frustum::ReverseDepth::False);
        }
    } // namespace

    TEST(SpriteCullTest, BoundingRadius_CircumscribesQuadAndScales)
    {
        // Half-diagonal of a 3x4 quad is 2.5; scale multiplies it; non-positive scale -> 1.
        EXPECT_NEAR(SpriteCull::BoundingRadius(3.0f, 4.0f, 1.0f), 2.5f, 1e-4f);
        EXPECT_NEAR(SpriteCull::BoundingRadius(3.0f, 4.0f, 2.0f), 5.0f, 1e-4f);
        EXPECT_NEAR(SpriteCull::BoundingRadius(3.0f, 4.0f, 0.0f), 2.5f, 1e-4f);
        EXPECT_NEAR(SpriteCull::BoundingRadius(3.0f, 4.0f, -5.0f), 2.5f, 1e-4f);
    }

    TEST(SpriteCullTest, CenteredSpriteIsVisible)
    {
        // A point straight ahead is inside the frustum. (This also asserts the plane
        // normals point inward: if they did not, an inside point would read as culled.)
        const AZ::Frustum frustum = MakeTestFrustum();
        EXPECT_TRUE(SpriteCull::IsVisible(frustum, AZ::Vector3(0.0f, 0.0f, -10.0f), 0.5f));
    }

    TEST(SpriteCullTest, FarOffToTheSideIsCulled)
    {
        const AZ::Frustum frustum = MakeTestFrustum();
        // At z = -10 the half-extent is ~5.8; points way past it on each axis are out.
        EXPECT_FALSE(SpriteCull::IsVisible(frustum, AZ::Vector3(50.0f, 0.0f, -10.0f), 0.5f)); // right
        EXPECT_FALSE(SpriteCull::IsVisible(frustum, AZ::Vector3(-50.0f, 0.0f, -10.0f), 0.5f)); // left
        EXPECT_FALSE(SpriteCull::IsVisible(frustum, AZ::Vector3(0.0f, 50.0f, -10.0f), 0.5f)); // up
        EXPECT_FALSE(SpriteCull::IsVisible(frustum, AZ::Vector3(0.0f, -50.0f, -10.0f), 0.5f)); // down
    }

    TEST(SpriteCullTest, RadiusKeepsAnEdgeSpanningSpriteVisible)
    {
        const AZ::Frustum frustum = MakeTestFrustum();
        // A small point just outside the right edge is culled, but the same point with
        // a large enough radius (its quad still pokes on screen) is conservatively kept.
        const AZ::Vector3 justOutside(7.0f, 0.0f, -10.0f);
        EXPECT_FALSE(SpriteCull::IsVisible(frustum, justOutside, 0.1f));
        EXPECT_TRUE(SpriteCull::IsVisible(frustum, justOutside, 5.0f));
    }
} // namespace Diorama
