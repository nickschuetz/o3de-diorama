/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>

#include <Clients/Collider2DComponent.h>
#include <Clients/Collision2DSystemComponent.h>
#include <Diorama/Collision2DBus.h>

namespace Diorama
{
    // Minimal component that only provides TransformService, so a Collider2DComponent
    // (which requires it) can activate on a bare entity. It does not implement
    // TransformBus, so colliders read an identity world transform and are positioned
    // in these tests via SetOffset through the request bus.
    class Collider2DTransformStub : public AZ::Component
    {
    public:
        AZ_COMPONENT(Collider2DTransformStub, "{CC156F8C-D730-4712-AEE0-0C836F186A41}", AZ::Component);
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
            {
                sc->Class<Collider2DTransformStub, AZ::Component>()->Version(1);
            }
        }
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC_CE("TransformService"));
        }
        void Activate() override
        {
        }
        void Deactivate() override
        {
        }
    };

    // Counting handler for collision notifications at one entity.
    class TestContactListener : public Diorama2DCollisionNotificationBus::Handler
    {
    public:
        void Listen(AZ::EntityId id)
        {
            Diorama2DCollisionNotificationBus::Handler::BusConnect(id);
        }
        void Stop()
        {
            Diorama2DCollisionNotificationBus::Handler::BusDisconnect();
        }

        void OnContactBegin(AZ::EntityId other) override
        {
            ++m_beginCount;
            m_lastOther = other;
        }
        void OnContactStay(AZ::EntityId) override
        {
            ++m_stayCount;
        }
        void OnContactEnd(AZ::EntityId other) override
        {
            ++m_endCount;
            m_lastOther = other;
        }
        void OnTriggerEnter(AZ::EntityId other) override
        {
            ++m_triggerEnterCount;
            m_lastOther = other;
        }
        void OnTriggerExit(AZ::EntityId) override
        {
            ++m_triggerExitCount;
        }

        int m_beginCount = 0;
        int m_stayCount = 0;
        int m_endCount = 0;
        int m_triggerEnterCount = 0;
        int m_triggerExitCount = 0;
        AZ::EntityId m_lastOther;
    };

    class Collision2DSystemTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            m_app.RegisterComponentDescriptor(Collision2DSystemComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(Collider2DComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(Collider2DTransformStub::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();

            // The collision world lives on its own entity, activated before any
            // colliders so Collision2DWorld::Get() is valid when they register.
            m_worldEntity = aznew AZ::Entity("Collision2DWorld");
            m_system = m_worldEntity->CreateComponent<Collision2DSystemComponent>();
            m_worldEntity->Init();
            m_worldEntity->Activate();
        }

        void TearDown() override
        {
            for (AZ::Entity* e : m_colliderEntities)
            {
                e->Deactivate();
                delete e;
            }
            m_colliderEntities.clear();

            m_worldEntity->Deactivate();
            delete m_worldEntity;
            m_app.Destroy();
        }

        //! Create and activate an entity with a default-circle collider (radius 0.5
        //! at the origin). Position it afterwards with SetOffset.
        AZ::EntityId MakeCollider(const char* name)
        {
            AZ::Entity* e = aznew AZ::Entity(name);
            e->CreateComponent<Collider2DTransformStub>();
            e->CreateComponent<Collider2DComponent>();
            e->Init();
            e->Activate();
            m_colliderEntities.push_back(e);
            return e->GetId();
        }

        void SetPos(AZ::EntityId id, float x, float z)
        {
            Diorama2DColliderRequestBus::Event(id, &Diorama2DColliderRequests::SetOffset, x, z);
        }
        void Step()
        {
            m_system->StepSimulation();
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_worldEntity = nullptr;
        Collision2DSystemComponent* m_system = nullptr;
        AZStd::vector<AZ::Entity*> m_colliderEntities;
    };

    TEST_F(Collision2DSystemTest, Overlap_FiresContactBeginOnBothEntities)
    {
        const AZ::EntityId a = MakeCollider("A");
        const AZ::EntityId b = MakeCollider("B");
        SetPos(a, 0.0f, 0.0f);
        SetPos(b, 0.5f, 0.0f); // centers 0.5 apart, radii 0.5+0.5=1.0 -> overlap

        TestContactListener la;
        TestContactListener lb;
        la.Listen(a);
        lb.Listen(b);

        Step();

        EXPECT_EQ(la.m_beginCount, 1);
        EXPECT_EQ(lb.m_beginCount, 1);
        EXPECT_EQ(la.m_lastOther, b);
        EXPECT_EQ(lb.m_lastOther, a);

        la.Stop();
        lb.Stop();
    }

    TEST_F(Collision2DSystemTest, Separated_FiresNothing)
    {
        const AZ::EntityId a = MakeCollider("A");
        const AZ::EntityId b = MakeCollider("B");
        SetPos(a, 0.0f, 0.0f);
        SetPos(b, 10.0f, 0.0f);

        TestContactListener la;
        la.Listen(a);
        Step();
        EXPECT_EQ(la.m_beginCount, 0);
        EXPECT_EQ(la.m_stayCount, 0);
        la.Stop();
    }

    TEST_F(Collision2DSystemTest, PersistThenSeparate_BeginThenEnd)
    {
        const AZ::EntityId a = MakeCollider("A");
        const AZ::EntityId b = MakeCollider("B");
        SetPos(a, 0.0f, 0.0f);
        SetPos(b, 0.5f, 0.0f);

        TestContactListener la;
        la.Listen(a);

        Step(); // begin
        EXPECT_EQ(la.m_beginCount, 1);
        Step(); // stay
        EXPECT_EQ(la.m_stayCount, 1);
        EXPECT_EQ(la.m_endCount, 0);

        SetPos(b, 20.0f, 0.0f); // move apart
        Step(); // end
        EXPECT_EQ(la.m_endCount, 1);
        EXPECT_EQ(la.m_lastOther, b);

        la.Stop();
    }

    TEST_F(Collision2DSystemTest, Trigger_FiresTriggerEnterNotContact)
    {
        const AZ::EntityId a = MakeCollider("A");
        const AZ::EntityId b = MakeCollider("B");
        SetPos(a, 0.0f, 0.0f);
        SetPos(b, 0.5f, 0.0f);
        Diorama2DColliderRequestBus::Event(b, &Diorama2DColliderRequests::SetTrigger, true);

        TestContactListener la;
        la.Listen(a);
        Step();

        EXPECT_EQ(la.m_triggerEnterCount, 1);
        EXPECT_EQ(la.m_beginCount, 0);
        la.Stop();
    }

    TEST_F(Collision2DSystemTest, DisjointLayers_DoNotContact)
    {
        const AZ::EntityId a = MakeCollider("A");
        const AZ::EntityId b = MakeCollider("B");
        SetPos(a, 0.0f, 0.0f);
        SetPos(b, 0.5f, 0.0f);
        // a: category 1, only collides with 1; b: category 2, only collides with 2.
        Diorama2DColliderRequestBus::Event(a, &Diorama2DColliderRequests::SetLayer, AZ::u32(0x1));
        Diorama2DColliderRequestBus::Event(a, &Diorama2DColliderRequests::SetCollidesWith, AZ::u32(0x1));
        Diorama2DColliderRequestBus::Event(b, &Diorama2DColliderRequests::SetLayer, AZ::u32(0x2));
        Diorama2DColliderRequestBus::Event(b, &Diorama2DColliderRequests::SetCollidesWith, AZ::u32(0x2));

        TestContactListener la;
        la.Listen(a);
        Step();
        EXPECT_EQ(la.m_beginCount, 0);
        la.Stop();
    }

    TEST_F(Collision2DSystemTest, OverlapCircleQuery_ReturnsOverlappingEntity)
    {
        const AZ::EntityId a = MakeCollider("A");
        SetPos(a, 3.0f, 4.0f);

        AZStd::vector<AZ::EntityId> hits;
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapCircle, 3.0f, 4.0f, 1.0f, AZ::u32(0));

        ASSERT_EQ(hits.size(), 1u);
        EXPECT_EQ(hits[0], a);

        // A query far away finds nothing.
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapCircle, 100.0f, 0.0f, 1.0f, AZ::u32(0));
        EXPECT_TRUE(hits.empty());
    }

    TEST_F(Collision2DSystemTest, OverlapQuery_ResultsSortedByEntityId)
    {
        // The collider store is an unordered_map, so without an explicit sort the
        // result ORDER would depend on hash-map history (nondeterministic across
        // runs). Scripts take "the first hit", so the order is contract: ascending
        // entity id, regardless of registration order.
        AZStd::vector<AZ::EntityId> made;
        for (int i = 0; i < 8; ++i)
        {
            const AZ::EntityId id = MakeCollider("Cluster");
            SetPos(id, 0.0f, 0.0f); // all overlapping the same spot
            made.push_back(id);
        }

        AZStd::vector<AZ::EntityId> hits;
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapBox, 0.0f, 0.0f, 1.0f, 1.0f, AZ::u32(0));

        ASSERT_EQ(hits.size(), made.size());
        for (size_t i = 1; i < hits.size(); ++i)
        {
            EXPECT_LT(hits[i - 1], hits[i]); // strictly ascending: sorted and unique
        }

        // And stable: a second identical query returns the identical sequence.
        AZStd::vector<AZ::EntityId> again;
        Diorama2DCollisionRequestBus::BroadcastResult(again, &Diorama2DCollisionRequests::OverlapBox, 0.0f, 0.0f, 1.0f, 1.0f, AZ::u32(0));
        EXPECT_TRUE(hits == again);
    }

    TEST_F(Collision2DSystemTest, Raycast2DQuery_HitsCollider)
    {
        const AZ::EntityId a = MakeCollider("A");
        SetPos(a, 5.0f, 0.0f); // circle radius 0.5 at (5,0)

        Raycast2DResult result;
        Diorama2DCollisionRequestBus::BroadcastResult(
            result, &Diorama2DCollisionRequests::Raycast2D, 0.0f, 0.0f, 1.0f, 0.0f, 100.0f, AZ::u32(0));

        EXPECT_TRUE(result.m_hit);
        EXPECT_EQ(result.m_entityId, a);
        EXPECT_NEAR(result.m_distance, 4.5f, 1e-3f); // enters at x = 4.5
    }

    TEST_F(Collision2DSystemTest, ColliderRequestBus_SetCircleAndInfoReflectsContacts)
    {
        const AZ::EntityId a = MakeCollider("A");
        const AZ::EntityId b = MakeCollider("B");
        SetPos(a, 0.0f, 0.0f);
        SetPos(b, 0.5f, 0.0f);

        Diorama2DColliderRequestBus::Event(a, &Diorama2DColliderRequests::SetCircle, 2.0f);

        Step(); // populates contact counts

        Collider2DInfo info;
        Diorama2DColliderRequestBus::EventResult(info, a, &Diorama2DColliderRequests::GetColliderInfo);
        EXPECT_TRUE(info.m_isCircle);
        EXPECT_NEAR(info.m_radius, 2.0f, 1e-5f);
        EXPECT_EQ(info.m_contactCount, 1);
    }

    // Static collider sets (tilemap solid tiles): query-only geometry that joins
    // overlap / push-out / raycast but not contact dispatch. Verifies the new
    // SetStaticColliders path end to end, the integration the per-tile collision
    // feature relies on.
    TEST_F(Collision2DSystemTest, StaticColliders_JoinQueriesLayerFilterAndClear)
    {
        Collision2D::Collider box;
        box.m_center = AZ::Vector2(10.0f, 0.0f);
        box.m_shape.m_type = Collision2D::ShapeType::Box;
        box.m_shape.m_halfExtents = AZ::Vector2(1.0f, 1.0f);
        box.m_layer = 1;
        const AZ::EntityId owner(0x7654u);
        Collision2DWorld::Get()->SetStaticColliders(owner, { box });

        // Overlapping the static box returns its owner entity.
        AZStd::vector<AZ::EntityId> hits;
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapBox, 10.0f, 0.0f, 0.5f, 0.5f, 1u);
        ASSERT_EQ(hits.size(), 1u);
        EXPECT_EQ(hits[0], owner);

        // Far from the box: no hit.
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapBox, 100.0f, 100.0f, 0.5f, 0.5f, 1u);
        EXPECT_TRUE(hits.empty());

        // A layer mask that excludes the box's layer filters it out.
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapBox, 10.0f, 0.0f, 0.5f, 0.5f, 2u);
        EXPECT_TRUE(hits.empty());

        // Push-out of a query box overlapping the static box is non-zero (a moving
        // collider would be ejected from the wall).
        AZ::Vector2 push(0.0f, 0.0f);
        Diorama2DCollisionRequestBus::BroadcastResult(
            push, &Diorama2DCollisionRequests::ComputeBoxPushOut, 10.6f, 0.0f, 0.5f, 0.5f, 1u, AZ::EntityId());
        EXPECT_GT(push.GetLength(), 0.0f);

        // Clearing removes the static set from queries.
        Collision2DWorld::Get()->ClearStaticColliders(owner);
        Diorama2DCollisionRequestBus::BroadcastResult(hits, &Diorama2DCollisionRequests::OverlapBox, 10.0f, 0.0f, 0.5f, 0.5f, 1u);
        EXPECT_TRUE(hits.empty());
    }

    // One-way platform push-out: a box landing from above is pushed up onto the
    // platform top; a box rising from below passes through (zero push).
    TEST_F(Collision2DSystemTest, OneWayPlatform_PushesUpFromAboveOnly)
    {
        Collision2D::Collider platform;
        platform.m_center = AZ::Vector2(0.0f, 0.0f);
        platform.m_shape.m_type = Collision2D::ShapeType::Box;
        platform.m_shape.m_halfExtents = AZ::Vector2(2.0f, 0.25f); // a thin wide ledge
        platform.m_layer = 1;
        platform.m_oneWay = true;
        Collision2DWorld::Get()->SetStaticColliders(AZ::EntityId(0x1111u), { platform });

        // A box whose center is above the platform center and overlaps: pushed straight up.
        AZ::Vector2 push(0.0f, 0.0f);
        Diorama2DCollisionRequestBus::BroadcastResult(
            push, &Diorama2DCollisionRequests::ComputeBoxPushOut, 0.0f, 0.3f, 0.5f, 0.5f, 1u, AZ::EntityId());
        EXPECT_GT(push.GetY(), 0.0f);
        EXPECT_NEAR(push.GetX(), 0.0f, 1e-4f); // one-way only resolves vertically

        // A box whose center is below the platform center (jumping up through it): no push.
        Diorama2DCollisionRequestBus::BroadcastResult(
            push, &Diorama2DCollisionRequests::ComputeBoxPushOut, 0.0f, -0.3f, 0.5f, 0.5f, 1u, AZ::EntityId());
        EXPECT_NEAR(push.GetLength(), 0.0f, 1e-4f);
    }

    // Ground segments + ProbeGroundY: flat ground and a ramp, with step-up / max-drop.
    TEST_F(Collision2DSystemTest, GroundProbe_FollowsFlatAndRampWithinReach)
    {
        // A flat floor at y=0 over x[0,10] and a ramp rising 0->2 over x[10,14].
        AZStd::vector<SlopeCollision::FloorSegment> segments = {
            { 0.0f, 10.0f, 0.0f, 0.0f },
            { 10.0f, 14.0f, 0.0f, 2.0f },
        };
        const AZ::EntityId owner(0x2222u);
        Collision2DWorld::Get()->SetGroundSegments(owner, segments);

        // On the flat: ground at 0.
        GroundProbe2DResult r;
        Diorama2DCollisionRequestBus::BroadcastResult(r, &Diorama2DCollisionRequests::ProbeGroundY, 5.0f, 0.1f, 4.0f, 0.5f);
        EXPECT_TRUE(r.m_onGround);
        EXPECT_NEAR(r.m_groundY, 0.0f, 1e-4f);

        // Halfway up the ramp (x=12): surface at 1.0, reachable from footY 0.6 within step-up.
        Diorama2DCollisionRequestBus::BroadcastResult(r, &Diorama2DCollisionRequests::ProbeGroundY, 12.0f, 0.6f, 4.0f, 0.5f);
        EXPECT_TRUE(r.m_onGround);
        EXPECT_NEAR(r.m_groundY, 1.0f, 1e-4f);

        // Off the end of all segments (x=20): no ground -> the body falls.
        Diorama2DCollisionRequestBus::BroadcastResult(r, &Diorama2DCollisionRequests::ProbeGroundY, 20.0f, 0.1f, 4.0f, 0.5f);
        EXPECT_FALSE(r.m_onGround);

        // Clearing removes the segments.
        Collision2DWorld::Get()->ClearGroundSegments(owner);
        Diorama2DCollisionRequestBus::BroadcastResult(r, &Diorama2DCollisionRequests::ProbeGroundY, 5.0f, 0.1f, 4.0f, 0.5f);
        EXPECT_FALSE(r.m_onGround);
    }
} // namespace Diorama
