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
#include <Clients/DioramaBulletEmitterComponent.h>
#include <Diorama/Collision2DBus.h>
#include <Diorama/DioramaBulletBus.h>

// Integration test for the bullet emitter's engine-coupled path that the pure
// BulletPattern core cannot reach: emission into a pooled set, lifetime expiry, and
// hit-testing live bullets against the 2D collision world (OnBulletHit + consume).
// Rendering is skipped (StepBullets(dt, false)), so no feature processor is needed.
// Fixture mirrors Collision2DSystemTest / DioramaHitboxComponentTest.
namespace Diorama
{
    class BulletTransformStub : public AZ::Component
    {
    public:
        AZ_COMPONENT(BulletTransformStub, "{C7F3B8D5-2E4A-4B01-8D3F-9A6C0E2B4D31}", AZ::Component);
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
            {
                sc->Class<BulletTransformStub, AZ::Component>()->Version(1);
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

    class TestBulletHitListener : public DioramaBulletNotificationBus::Handler
    {
    public:
        void Listen(AZ::EntityId id)
        {
            DioramaBulletNotificationBus::Handler::BusConnect(id);
        }
        void Stop()
        {
            DioramaBulletNotificationBus::Handler::BusDisconnect();
        }
        void OnBulletHit(AZ::EntityId target) override
        {
            ++m_hitCount;
            m_lastTarget = target;
        }
        int m_hitCount = 0;
        AZ::EntityId m_lastTarget;
    };

    class DioramaBulletEmitterComponentTest : public ::testing::Test
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
            m_app.RegisterComponentDescriptor(DioramaBulletEmitterComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(BulletTransformStub::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();

            m_worldEntity = aznew AZ::Entity("Collision2DWorld");
            m_worldEntity->CreateComponent<Collision2DSystemComponent>();
            m_worldEntity->Init();
            m_worldEntity->Activate();
        }

        void TearDown() override
        {
            for (AZ::Entity* e : m_entities)
            {
                e->Deactivate();
                delete e;
            }
            m_entities.clear();
            m_worldEntity->Deactivate();
            delete m_worldEntity;
            m_app.Destroy();
        }

        //! A target collider: circle of `radius` on layer 1, positioned via SetOffset
        //! (the stub provides only an identity transform).
        AZ::EntityId MakeTarget(float x, float z, float radius)
        {
            AZ::Entity* e = aznew AZ::Entity("Target");
            e->CreateComponent<BulletTransformStub>();
            e->CreateComponent<Collider2DComponent>();
            e->Init();
            e->Activate();
            m_entities.push_back(e);
            const AZ::EntityId id = e->GetId();
            Diorama2DColliderRequestBus::Event(id, &Diorama2DColliderRequests::SetCircle, radius);
            Diorama2DColliderRequestBus::Event(id, &Diorama2DColliderRequests::SetOffset, x, z);
            return id;
        }

        DioramaBulletEmitterComponent* MakeEmitter(const DioramaBulletConfig& config, AZ::EntityId& outId)
        {
            AZ::Entity* e = aznew AZ::Entity("Emitter");
            e->CreateComponent<BulletTransformStub>();
            auto* emitter = e->CreateComponent<DioramaBulletEmitterComponent>(config);
            e->Init();
            e->Activate();
            m_entities.push_back(e);
            outId = e->GetId();
            return emitter;
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_worldEntity = nullptr;
        AZStd::vector<AZ::Entity*> m_entities;
    };

    TEST_F(DioramaBulletEmitterComponentTest, RingEmitsCountBulletsAtOrigin)
    {
        DioramaBulletConfig cfg;
        cfg.m_pattern = BulletPattern::Kind::Ring;
        cfg.m_count = 12;
        cfg.m_speed = 5.0f;
        cfg.m_fireOnActivate = false;
        cfg.m_targetMask = 0; // visual only: no hit-test, bullets persist
        cfg.m_bulletLifetime = 100.0f;

        AZ::EntityId id;
        DioramaBulletEmitterComponent* emitter = MakeEmitter(cfg, id);
        ASSERT_NE(emitter, nullptr);

        emitter->EmitShot();
        DioramaBulletInfo info;
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 12);
        EXPECT_EQ(info.m_pattern, static_cast<int>(BulletPattern::Kind::Ring));
    }

    TEST_F(DioramaBulletEmitterComponentTest, BulletHitsTargetFiresOnBulletHitAndConsumes)
    {
        // A single bullet aimed at +X toward a target at (5, 0).
        DioramaBulletConfig cfg;
        cfg.m_pattern = BulletPattern::Kind::Fan;
        cfg.m_count = 1;
        cfg.m_speed = 5.0f;
        cfg.m_aimDegrees = 0.0f; // +X
        cfg.m_fireOnActivate = false;
        cfg.m_bulletLifetime = 100.0f;
        cfg.m_bulletRadius = 0.25f;
        cfg.m_targetMask = 1; // hit layer-1 colliders

        const AZ::EntityId target = MakeTarget(5.0f, 0.0f, 1.0f);
        AZ::EntityId emitterId;
        DioramaBulletEmitterComponent* emitter = MakeEmitter(cfg, emitterId);

        TestBulletHitListener listener;
        listener.Listen(emitterId);

        emitter->EmitShot(); // one bullet at origin moving +X

        // Advance until it reaches the target (~1s at speed 5). Stop as soon as it hits.
        for (int i = 0; i < 30 && listener.m_hitCount == 0; ++i)
        {
            emitter->StepBullets(0.1f, false);
        }

        EXPECT_EQ(listener.m_hitCount, 1);
        EXPECT_EQ(listener.m_lastTarget, target);

        // The bullet was consumed on the hit.
        DioramaBulletInfo info;
        DioramaBulletRequestBus::EventResult(info, emitterId, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 0);

        listener.Stop();
    }

    TEST_F(DioramaBulletEmitterComponentTest, MuzzleOffsetMovesTheSpawnPoint)
    {
        // With a muzzle offset, bullets spawn at origin + offset. A target placed at the
        // muzzle is hit on the first hit-test step (the bullet spawns inside it), which it
        // would not be if the bullet spawned at the origin 5 units away.
        DioramaBulletConfig cfg;
        cfg.m_pattern = BulletPattern::Kind::Fan;
        cfg.m_count = 1;
        cfg.m_speed = 1.0f;
        cfg.m_aimDegrees = 90.0f; // +Y
        cfg.m_fireOnActivate = false;
        cfg.m_bulletLifetime = 100.0f;
        cfg.m_bulletRadius = 0.25f;
        cfg.m_targetMask = 1;
        cfg.m_muzzleOffset = AZ::Vector2(0.0f, 5.0f);

        const AZ::EntityId target = MakeTarget(0.0f, 5.0f, 0.5f);
        AZ::EntityId emitterId;
        DioramaBulletEmitterComponent* emitter = MakeEmitter(cfg, emitterId);

        TestBulletHitListener listener;
        listener.Listen(emitterId);

        emitter->EmitShot(); // bullet spawns at (0, 5), inside the target
        emitter->StepBullets(0.01f, false); // one tiny step: an origin spawn would be far off

        EXPECT_EQ(listener.m_hitCount, 1);
        EXPECT_EQ(listener.m_lastTarget, target);

        listener.Stop();
    }

    TEST_F(DioramaBulletEmitterComponentTest, BulletAwayFromTargetDoesNotHit)
    {
        // Same target at (5,0) but the bullet is aimed at -X, so it never overlaps.
        DioramaBulletConfig cfg;
        cfg.m_pattern = BulletPattern::Kind::Fan;
        cfg.m_count = 1;
        cfg.m_speed = 5.0f;
        cfg.m_aimDegrees = 180.0f; // -X
        cfg.m_fireOnActivate = false;
        cfg.m_bulletLifetime = 100.0f;
        cfg.m_bulletRadius = 0.25f;
        cfg.m_targetMask = 1;

        MakeTarget(5.0f, 0.0f, 1.0f);
        AZ::EntityId emitterId;
        DioramaBulletEmitterComponent* emitter = MakeEmitter(cfg, emitterId);

        TestBulletHitListener listener;
        listener.Listen(emitterId);

        emitter->EmitShot();
        for (int i = 0; i < 20; ++i)
        {
            emitter->StepBullets(0.1f, false);
        }

        EXPECT_EQ(listener.m_hitCount, 0);
        listener.Stop();
    }

    TEST_F(DioramaBulletEmitterComponentTest, BulletsExpireByLifetime)
    {
        DioramaBulletConfig cfg;
        cfg.m_pattern = BulletPattern::Kind::Ring;
        cfg.m_count = 8;
        cfg.m_speed = 5.0f;
        cfg.m_fireOnActivate = false;
        cfg.m_targetMask = 0;
        cfg.m_bulletLifetime = 0.5f;

        AZ::EntityId id;
        DioramaBulletEmitterComponent* emitter = MakeEmitter(cfg, id);
        emitter->EmitShot();

        DioramaBulletInfo info;
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 8);

        // Step past the lifetime: every bullet expires.
        for (int i = 0; i < 10; ++i)
        {
            emitter->StepBullets(0.1f, false);
        }
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 0);
    }

    TEST_F(DioramaBulletEmitterComponentTest, PoolCapBoundsTheBulletCount)
    {
        DioramaBulletConfig cfg;
        cfg.m_pattern = BulletPattern::Kind::Ring;
        cfg.m_count = 100;
        cfg.m_speed = 5.0f;
        cfg.m_maxBullets = 16; // smaller than one shot
        cfg.m_fireOnActivate = false;
        cfg.m_targetMask = 0;
        cfg.m_bulletLifetime = 100.0f;

        AZ::EntityId id;
        DioramaBulletEmitterComponent* emitter = MakeEmitter(cfg, id);
        emitter->EmitShot();
        emitter->EmitShot(); // a second shot cannot exceed the cap either

        DioramaBulletInfo info;
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 16);
        EXPECT_EQ(info.m_maxBullets, 16);
    }

    TEST_F(DioramaBulletEmitterComponentTest, BusRetargetsThePattern)
    {
        DioramaBulletConfig cfg;
        cfg.m_fireOnActivate = false;
        cfg.m_targetMask = 0;
        cfg.m_bulletLifetime = 100.0f;

        AZ::EntityId id;
        MakeEmitter(cfg, id);

        DioramaBulletRequestBus::Event(id, &DioramaBulletRequests::SetPattern, 2); // spiral
        DioramaBulletRequestBus::Event(id, &DioramaBulletRequests::SetCount, 3);
        DioramaBulletRequestBus::Event(id, &DioramaBulletRequests::SetSpeed, 7.0f);
        DioramaBulletRequestBus::Event(id, &DioramaBulletRequests::Fire);

        DioramaBulletInfo info;
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_pattern, static_cast<int>(BulletPattern::Kind::Spiral));
        EXPECT_EQ(info.m_aliveCount, 3);
    }
} // namespace Diorama
