/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/Collision2D.h>

namespace Diorama
{
    using Collision2D::Collider;
    using Collision2D::ContactTracker;
    using Collision2D::PairKey;
    using Collision2D::ShapeType;

    namespace
    {
        Collider MakeCircle(AZ::u64 id, float x, float y, float r, AZ::u32 layer = 1, AZ::u32 mask = 0xFFFFFFFFu)
        {
            Collider c;
            c.m_id = id;
            c.m_center = AZ::Vector2(x, y);
            c.m_shape.m_type = ShapeType::Circle;
            c.m_shape.m_radius = r;
            c.m_layer = layer;
            c.m_collidesWith = mask;
            return c;
        }

        Collider MakeBox(AZ::u64 id, float x, float y, float hx, float hy, AZ::u32 layer = 1, AZ::u32 mask = 0xFFFFFFFFu)
        {
            Collider c;
            c.m_id = id;
            c.m_center = AZ::Vector2(x, y);
            c.m_shape.m_type = ShapeType::Box;
            c.m_shape.m_halfExtents = AZ::Vector2(hx, hy);
            c.m_layer = layer;
            c.m_collidesWith = mask;
            return c;
        }
    } // namespace

    // ---- overlap math --------------------------------------------------------

    using Collision2DOverlapTest = ::testing::Test;

    TEST_F(Collision2DOverlapTest, CircleCircle_Overlapping)
    {
        EXPECT_TRUE(Collision2D::Overlaps(MakeCircle(1, 0.0f, 0.0f, 1.0f), MakeCircle(2, 1.5f, 0.0f, 1.0f)));
    }

    TEST_F(Collision2DOverlapTest, CircleCircle_ExactlyTouching_CountsAsOverlap)
    {
        // Centers 2 apart, radii 1 + 1 = 2: touching is inclusive.
        EXPECT_TRUE(Collision2D::Overlaps(MakeCircle(1, 0.0f, 0.0f, 1.0f), MakeCircle(2, 2.0f, 0.0f, 1.0f)));
    }

    TEST_F(Collision2DOverlapTest, CircleCircle_Separated)
    {
        EXPECT_FALSE(Collision2D::Overlaps(MakeCircle(1, 0.0f, 0.0f, 1.0f), MakeCircle(2, 2.01f, 0.0f, 1.0f)));
    }

    TEST_F(Collision2DOverlapTest, BoxBox_Overlapping)
    {
        EXPECT_TRUE(Collision2D::Overlaps(MakeBox(1, 0.0f, 0.0f, 1.0f, 1.0f), MakeBox(2, 1.5f, 0.5f, 1.0f, 1.0f)));
    }

    TEST_F(Collision2DOverlapTest, BoxBox_SeparatedOnYOnly_DoesNotOverlap)
    {
        // X overlaps but Y is clearly apart: AABB requires both axes.
        EXPECT_FALSE(Collision2D::Overlaps(MakeBox(1, 0.0f, 0.0f, 1.0f, 1.0f), MakeBox(2, 0.5f, 3.0f, 1.0f, 1.0f)));
    }

    TEST_F(Collision2DOverlapTest, CircleBox_CenterInsideBox)
    {
        EXPECT_TRUE(Collision2D::Overlaps(MakeCircle(1, 0.0f, 0.0f, 0.25f), MakeBox(2, 0.0f, 0.0f, 1.0f, 1.0f)));
    }

    TEST_F(Collision2DOverlapTest, CircleBox_NearCorner_JustReaches)
    {
        // Box corner at (1,1); circle centered at (1.5,1.5) with r ~0.71 reaches it
        // (corner distance = sqrt(0.5) ~= 0.707).
        EXPECT_TRUE(Collision2D::Overlaps(MakeBox(2, 0.0f, 0.0f, 1.0f, 1.0f), MakeCircle(1, 1.5f, 1.5f, 0.72f)));
    }

    TEST_F(Collision2DOverlapTest, CircleBox_BeyondCorner_DoesNotReach)
    {
        EXPECT_FALSE(Collision2D::Overlaps(MakeBox(2, 0.0f, 0.0f, 1.0f, 1.0f), MakeCircle(1, 1.5f, 1.5f, 0.70f)));
    }

    // ---- layer / mask matrix -------------------------------------------------

    using Collision2DLayerTest = ::testing::Test;

    TEST_F(Collision2DLayerTest, DefaultLayers_Interact)
    {
        EXPECT_TRUE(Collision2D::LayersInteract(MakeCircle(1, 0, 0, 1), MakeCircle(2, 0, 0, 1)));
    }

    TEST_F(Collision2DLayerTest, OneSidedMask_DoesNotInteract)
    {
        // a (layer bit0) collides with everything; b (layer bit1) collides only
        // with bit1, so it does NOT include a's bit0 -> mutual opt-in fails.
        Collider a = MakeCircle(1, 0, 0, 1, /*layer*/ 0x1, /*mask*/ 0xFFFFFFFFu);
        Collider b = MakeCircle(2, 0, 0, 1, /*layer*/ 0x2, /*mask*/ 0x2);
        EXPECT_FALSE(Collision2D::LayersInteract(a, b));
    }

    TEST_F(Collision2DLayerTest, DisjointCategories_DoNotInteract)
    {
        Collider a = MakeCircle(1, 0, 0, 1, 0x1, 0x1);
        Collider b = MakeCircle(2, 0, 0, 1, 0x2, 0x2);
        EXPECT_FALSE(Collision2D::LayersInteract(a, b));
    }

    TEST_F(Collision2DLayerTest, MutualOptIn_Interacts)
    {
        Collider a = MakeCircle(1, 0, 0, 1, 0x1, 0x2);
        Collider b = MakeCircle(2, 0, 0, 1, 0x2, 0x1);
        EXPECT_TRUE(Collision2D::LayersInteract(a, b));
    }

    // ---- ray casts -----------------------------------------------------------

    using Collision2DRaycastTest = ::testing::Test;
    static constexpr float RayEps = 1e-4f;

    TEST_F(Collision2DRaycastTest, Circle_HitStraightOn)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(-5.0f, 0.0f), AZ::Vector2(1.0f, 0.0f), 100.0f, MakeCircle(1, 0.0f, 0.0f, 1.0f), t);
        EXPECT_TRUE(hit);
        EXPECT_NEAR(t, 4.0f, RayEps); // enters at x=-1
    }

    TEST_F(Collision2DRaycastTest, Circle_MissesOffAxis)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(-5.0f, 3.0f), AZ::Vector2(1.0f, 0.0f), 100.0f, MakeCircle(1, 0.0f, 0.0f, 1.0f), t);
        EXPECT_FALSE(hit);
    }

    TEST_F(Collision2DRaycastTest, Circle_OriginInside_HitsAtZero)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(0.0f, 0.0f), AZ::Vector2(1.0f, 0.0f), 100.0f, MakeCircle(1, 0.0f, 0.0f, 1.0f), t);
        EXPECT_TRUE(hit);
        EXPECT_NEAR(t, 0.0f, RayEps);
    }

    TEST_F(Collision2DRaycastTest, Circle_BeyondMaxDistance_Misses)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(-5.0f, 0.0f), AZ::Vector2(1.0f, 0.0f), 3.0f, MakeCircle(1, 0.0f, 0.0f, 1.0f), t);
        EXPECT_FALSE(hit); // entry at t=4 is past maxDistance 3
    }

    TEST_F(Collision2DRaycastTest, Circle_PointingAway_Misses)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(-5.0f, 0.0f), AZ::Vector2(-1.0f, 0.0f), 100.0f, MakeCircle(1, 0.0f, 0.0f, 1.0f), t);
        EXPECT_FALSE(hit);
    }

    TEST_F(Collision2DRaycastTest, Box_HitStraightOn)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(-5.0f, 0.0f), AZ::Vector2(1.0f, 0.0f), 100.0f, MakeBox(1, 0.0f, 0.0f, 1.0f, 1.0f), t);
        EXPECT_TRUE(hit);
        EXPECT_NEAR(t, 4.0f, RayEps); // enters at x=-1
    }

    TEST_F(Collision2DRaycastTest, Box_ParallelOutsideSlab_Misses)
    {
        float t = -1.0f;
        // Ray travels along X at y=5, box spans y in [-1,1]: parallel, outside.
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(-5.0f, 5.0f), AZ::Vector2(1.0f, 0.0f), 100.0f, MakeBox(1, 0.0f, 0.0f, 1.0f, 1.0f), t);
        EXPECT_FALSE(hit);
    }

    TEST_F(Collision2DRaycastTest, Box_OriginInside_HitsAtZero)
    {
        float t = -1.0f;
        const bool hit =
            Collision2D::RaycastCollider(AZ::Vector2(0.0f, 0.0f), AZ::Vector2(1.0f, 0.0f), 100.0f, MakeBox(1, 0.0f, 0.0f, 1.0f, 1.0f), t);
        EXPECT_TRUE(hit);
        EXPECT_NEAR(t, 0.0f, RayEps);
    }

    // ---- broadphase + full contact pass -------------------------------------

    class Collision2DContactsTest : public ::testing::Test
    {
    protected:
        AZStd::vector<PairKey> m_pairs;
        AZStd::vector<AZStd::pair<AZ::u32, AZ::u32>> m_candidates;
        AZStd::vector<AZ::u32> m_order;

        void Compute(const AZStd::vector<Collider>& colliders)
        {
            Collision2D::ComputeContacts(colliders, m_pairs, m_candidates, m_order);
        }
        bool HasPair(AZ::u64 a, AZ::u64 b) const
        {
            const PairKey want = PairKey::Make(a, b);
            for (const PairKey& p : m_pairs)
            {
                if (p == want)
                {
                    return true;
                }
            }
            return false;
        }
    };

    TEST_F(Collision2DContactsTest, TwoOverlappingCircles_OnePair)
    {
        Compute({ MakeCircle(10, 0.0f, 0.0f, 1.0f), MakeCircle(20, 1.0f, 0.0f, 1.0f) });
        EXPECT_EQ(m_pairs.size(), 1u);
        EXPECT_TRUE(HasPair(10, 20));
    }

    TEST_F(Collision2DContactsTest, FarApartOnX_NoPair_BroadphasePrunes)
    {
        Compute({ MakeCircle(10, 0.0f, 0.0f, 1.0f), MakeCircle(20, 100.0f, 0.0f, 1.0f) });
        EXPECT_TRUE(m_pairs.empty());
    }

    TEST_F(Collision2DContactsTest, OverlapOnXButNotOnY_NoPair)
    {
        // X intervals overlap (broadphase candidate) but they are far apart in Y,
        // so the narrowphase rejects: proves broadphase is a superset, not final.
        Compute({ MakeCircle(10, 0.0f, 0.0f, 1.0f), MakeCircle(20, 0.5f, 50.0f, 1.0f) });
        EXPECT_TRUE(m_pairs.empty());
    }

    TEST_F(Collision2DContactsTest, LayerMatrixExcludesPair)
    {
        Collider a = MakeCircle(10, 0.0f, 0.0f, 1.0f, 0x1, 0x1);
        Collider b = MakeCircle(20, 1.0f, 0.0f, 1.0f, 0x2, 0x2); // disjoint categories
        Compute({ a, b });
        EXPECT_TRUE(m_pairs.empty());
    }

    TEST_F(Collision2DContactsTest, DisabledColliderExcluded)
    {
        Collider a = MakeCircle(10, 0.0f, 0.0f, 1.0f);
        Collider b = MakeCircle(20, 1.0f, 0.0f, 1.0f);
        b.m_enabled = false;
        Compute({ a, b });
        EXPECT_TRUE(m_pairs.empty());
    }

    TEST_F(Collision2DContactsTest, ThreeInARow_AdjacentPairsOnly)
    {
        // Circles at x=0,1.5,3 with r=1: 0-1 overlap, 1-2 overlap, 0-2 do not.
        Compute({ MakeCircle(10, 0.0f, 0.0f, 1.0f), MakeCircle(20, 1.5f, 0.0f, 1.0f), MakeCircle(30, 3.0f, 0.0f, 1.0f) });
        EXPECT_EQ(m_pairs.size(), 2u);
        EXPECT_TRUE(HasPair(10, 20));
        EXPECT_TRUE(HasPair(20, 30));
        EXPECT_FALSE(HasPair(10, 30));
    }

    // ---- contact tracker (begin / stay / end) -------------------------------

    class Collision2DTrackerTest : public ::testing::Test
    {
    protected:
        ContactTracker m_tracker;
        AZStd::vector<PairKey> m_began;
        AZStd::vector<PairKey> m_ended;

        void Step(const AZStd::vector<PairKey>& current)
        {
            m_tracker.Update(current, m_began, m_ended);
        }
    };

    TEST_F(Collision2DTrackerTest, FirstContact_Begins)
    {
        Step({ PairKey::Make(1, 2) });
        EXPECT_EQ(m_began.size(), 1u);
        EXPECT_TRUE(m_ended.empty());
        EXPECT_TRUE(m_tracker.IsActive(PairKey::Make(1, 2)));
    }

    TEST_F(Collision2DTrackerTest, PersistingContact_NeitherBeginsNorEnds)
    {
        Step({ PairKey::Make(1, 2) });
        Step({ PairKey::Make(1, 2) });
        EXPECT_TRUE(m_began.empty());
        EXPECT_TRUE(m_ended.empty());
        EXPECT_EQ(m_tracker.ActiveCount(), 1u);
    }

    TEST_F(Collision2DTrackerTest, SeparatedContact_Ends)
    {
        Step({ PairKey::Make(1, 2) });
        Step({});
        EXPECT_TRUE(m_began.empty());
        EXPECT_EQ(m_ended.size(), 1u);
        EXPECT_FALSE(m_tracker.IsActive(PairKey::Make(1, 2)));
    }

    TEST_F(Collision2DTrackerTest, PairOrderIndependent)
    {
        // (1,2) then (2,1) is the same contact; must not re-begin.
        Step({ PairKey::Make(1, 2) });
        Step({ PairKey::Make(2, 1) });
        EXPECT_TRUE(m_began.empty());
        EXPECT_TRUE(m_ended.empty());
    }

    TEST_F(Collision2DTrackerTest, MixedBeginAndEndInOneStep)
    {
        Step({ PairKey::Make(1, 2) }); // 1-2 active
        Step({ PairKey::Make(3, 4) }); // 1-2 ends, 3-4 begins
        EXPECT_EQ(m_began.size(), 1u);
        EXPECT_EQ(m_ended.size(), 1u);
        EXPECT_TRUE(m_began[0] == PairKey::Make(3, 4));
        EXPECT_TRUE(m_ended[0] == PairKey::Make(1, 2));
    }

    TEST_F(Collision2DTrackerTest, Clear_DropsActiveSoNextStepRebegins)
    {
        Step({ PairKey::Make(1, 2) });
        m_tracker.Clear();
        EXPECT_EQ(m_tracker.ActiveCount(), 0u);
        Step({ PairKey::Make(1, 2) });
        EXPECT_EQ(m_began.size(), 1u); // begins again after a clear
    }

    // ---- MinimumTranslation (pushbox resolution primitive) ----

    class Collision2DMTVTest : public ::testing::Test
    {
    protected:
        static constexpr float Tol = 1e-4f;
    };

    TEST_F(Collision2DMTVTest, NoOverlap_IsZero)
    {
        const AZ::Vector2 mtv = Collision2D::MinimumTranslation(MakeCircle(1, 0.0f, 0.0f, 0.5f), MakeCircle(2, 5.0f, 0.0f, 0.5f));
        EXPECT_NEAR(mtv.GetLength(), 0.0f, Tol);
    }

    TEST_F(Collision2DMTVTest, Circles_PushAlongCenterLine)
    {
        // radii 1 + 1 = 2, centers 1.5 apart on X -> penetration 0.5, push 'a' to -X.
        const AZ::Vector2 mtv = Collision2D::MinimumTranslation(MakeCircle(1, 0.0f, 0.0f, 1.0f), MakeCircle(2, 1.5f, 0.0f, 1.0f));
        EXPECT_NEAR(mtv.GetX(), -0.5f, Tol);
        EXPECT_NEAR(mtv.GetY(), 0.0f, Tol);
    }

    TEST_F(Collision2DMTVTest, Boxes_ResolveAlongLeastPenetrationAxis)
    {
        // Two unit half-extent boxes; centers (0,0) and (1.5, 0.2). X overlap = 0.5,
        // Y overlap = 1.8 -> resolve along X (least), pushing 'a' to -X by 0.5.
        const AZ::Vector2 mtv = Collision2D::MinimumTranslation(MakeBox(1, 0.0f, 0.0f, 1.0f, 1.0f), MakeBox(2, 1.5f, 0.2f, 1.0f, 1.0f));
        EXPECT_NEAR(mtv.GetX(), -0.5f, Tol);
        EXPECT_NEAR(mtv.GetY(), 0.0f, Tol);
    }

    TEST_F(Collision2DMTVTest, CircleVsBox_PushesCircleOut)
    {
        // Circle (r 0.5) at (1.2, 0) vs box half-extent 1 at origin: closest point on
        // the box is (1, 0), distance 0.2 < 0.5 -> push the circle +X by 0.3.
        const AZ::Vector2 mtv = Collision2D::MinimumTranslation(MakeCircle(1, 1.2f, 0.0f, 0.5f), MakeBox(2, 0.0f, 0.0f, 1.0f, 1.0f));
        EXPECT_NEAR(mtv.GetX(), 0.3f, Tol);
        EXPECT_NEAR(mtv.GetY(), 0.0f, Tol);
    }
} // namespace Diorama
