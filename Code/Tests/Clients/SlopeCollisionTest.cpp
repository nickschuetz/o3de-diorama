/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <Clients/SlopeCollision.h>

namespace Diorama
{
    namespace
    {
        AZStd::span<const SlopeCollision::FloorSegment> Seg(const AZStd::vector<SlopeCollision::FloorSegment>& v)
        {
            return AZStd::span<const SlopeCollision::FloorSegment>(v.data(), v.size());
        }
    } // namespace

    TEST(SlopeCollisionTest, SurfaceYInterpolatesAlongARamp)
    {
        // Ramp rising from y=0 at x=0 to y=2 at x=4.
        const SlopeCollision::FloorSegment ramp{ 0.0f, 4.0f, 0.0f, 2.0f };
        EXPECT_FLOAT_EQ(SlopeCollision::SurfaceYAt(ramp, 0.0f), 0.0f);
        EXPECT_FLOAT_EQ(SlopeCollision::SurfaceYAt(ramp, 2.0f), 1.0f); // midpoint
        EXPECT_FLOAT_EQ(SlopeCollision::SurfaceYAt(ramp, 4.0f), 2.0f);
    }

    TEST(SlopeCollisionTest, SurfaceYClampsOutsideTheSpan)
    {
        const SlopeCollision::FloorSegment ramp{ 0.0f, 4.0f, 0.0f, 2.0f };
        EXPECT_FLOAT_EQ(SlopeCollision::SurfaceYAt(ramp, -3.0f), 0.0f); // clamps to left end
        EXPECT_FLOAT_EQ(SlopeCollision::SurfaceYAt(ramp, 9.0f), 2.0f); // clamps to right end
    }

    TEST(SlopeCollisionTest, ProbeGroundPicksHighestSegmentWithinReach)
    {
        // A flat floor at y=0 and a platform top at y=1, both under x=2.
        const AZStd::vector<SlopeCollision::FloorSegment> floors = {
            { 0.0f, 5.0f, 0.0f, 0.0f },
            { 1.0f, 3.0f, 1.0f, 1.0f },
        };
        float ground = -99.0f;
        // Standing at footY=1.2, both are within stepUp/maxDrop: take the higher (1.0).
        EXPECT_TRUE(SlopeCollision::ProbeGround(Seg(floors), 2.0f, 1.2f, 4.0f, 0.5f, ground));
        EXPECT_FLOAT_EQ(ground, 1.0f);
    }

    TEST(SlopeCollisionTest, ProbeGroundIgnoresSurfacesTooHighToStepOnto)
    {
        // A wall-top at y=3 is above the step-up reach from footY=0, so it is ignored.
        const AZStd::vector<SlopeCollision::FloorSegment> floors = {
            { 0.0f, 5.0f, 0.0f, 0.0f },
            { 1.0f, 3.0f, 3.0f, 3.0f },
        };
        float ground = -99.0f;
        EXPECT_TRUE(SlopeCollision::ProbeGround(Seg(floors), 2.0f, 0.0f, 4.0f, 0.5f, ground));
        EXPECT_FLOAT_EQ(ground, 0.0f); // the flat floor, not the unreachable top
    }

    TEST(SlopeCollisionTest, ProbeGroundFallsWhenNothingIsWithinMaxDrop)
    {
        const AZStd::vector<SlopeCollision::FloorSegment> floors = { { 0.0f, 5.0f, 0.0f, 0.0f } };
        float ground = -99.0f;
        // Feet high above the floor (footY=10) with a small maxDrop: nothing to snap to.
        EXPECT_FALSE(SlopeCollision::ProbeGround(Seg(floors), 2.0f, 10.0f, 1.0f, 0.5f, ground));
    }

    TEST(SlopeCollisionTest, ProbeGroundOffTheSegmentFindsNoGround)
    {
        const AZStd::vector<SlopeCollision::FloorSegment> floors = { { 0.0f, 5.0f, 0.0f, 0.0f } };
        float ground = -99.0f;
        EXPECT_FALSE(SlopeCollision::ProbeGround(Seg(floors), 9.0f, 0.2f, 4.0f, 0.5f, ground)); // x past the span
    }

    TEST(SlopeCollisionTest, OneWayLandsOnlyWhenCrossingTheTopDownward)
    {
        // Was above (1.1), now at/below (0.9) the top (1.0): land.
        EXPECT_TRUE(SlopeCollision::ResolveOneWayLanding(1.1f, 0.9f, 1.0f));
        // Moving up through the top: pass through.
        EXPECT_FALSE(SlopeCollision::ResolveOneWayLanding(0.9f, 1.1f, 1.0f));
        // Already below the top and staying below: pass through (inside the platform).
        EXPECT_FALSE(SlopeCollision::ResolveOneWayLanding(0.5f, 0.4f, 1.0f));
    }
} // namespace Diorama
