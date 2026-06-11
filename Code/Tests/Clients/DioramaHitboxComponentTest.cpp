/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>

#include <Clients/Collision2DSystemComponent.h>
#include <Clients/DioramaHitboxComponent.h>
#include <Diorama/DioramaHitboxBus.h>

// Integration test for the engine-coupled hit path that the pure HitboxFrames core
// cannot reach: two frame-data hitbox rigs in a live 2D collision world. It verifies
// that an active hitbox overlapping an active hurtbox fires OnHit on the attacker and
// OnHurt on the target, exactly once per active window (dedup), that leaving and
// re-entering the active window re-arms the hit, and that facing mirrors the box so a
// turned-away attacker misses. Fixture pattern mirrors Collision2DSystemTest.
namespace Diorama
{
    // Minimal TransformService provider so the hitbox component (which requires it)
    // activates on a bare entity. It does not implement TransformBus, so the component
    // reads an identity world transform and boxes are positioned purely by their
    // configured offsets, which is all these tests need.
    class HitboxTransformStub : public AZ::Component
    {
    public:
        AZ_COMPONENT(HitboxTransformStub, "{B6E2A7C4-1D3F-4A90-9C2E-7F5B8D1A3C20}", AZ::Component);
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
            {
                sc->Class<HitboxTransformStub, AZ::Component>()->Version(1);
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

    // Counts OnHit / OnHurt at one entity and records the reported other party.
    class TestHitListener : public DioramaHitboxNotificationBus::Handler
    {
    public:
        void Listen(AZ::EntityId id)
        {
            DioramaHitboxNotificationBus::Handler::BusConnect(id);
        }
        void Stop()
        {
            DioramaHitboxNotificationBus::Handler::BusDisconnect();
        }

        void OnHit(AZ::EntityId target) override
        {
            ++m_hitCount;
            m_lastTarget = target;
        }
        void OnHurt(AZ::EntityId attacker) override
        {
            ++m_hurtCount;
            m_lastAttacker = attacker;
        }

        int m_hitCount = 0;
        int m_hurtCount = 0;
        AZ::EntityId m_lastTarget;
        AZ::EntityId m_lastAttacker;
    };

    class DioramaHitboxComponentTest : public ::testing::Test
    {
    protected:
        static constexpr AZ::u32 HurtLayer = 0x0100;

        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            m_app.RegisterComponentDescriptor(Collision2DSystemComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaHitboxComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(HitboxTransformStub::CreateDescriptor());

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

        //! One box at offset (offX, 0) with half extents (0.5, 0.5), active on [from, to].
        static DioramaHitboxData Box(HitboxFrames::BoxKind kind, float offX, int from, int to)
        {
            DioramaHitboxData b;
            b.m_kind = kind;
            b.m_offset = AZ::Vector2(offX, 0.0f);
            b.m_halfExtents = AZ::Vector2(0.5f, 0.5f);
            b.m_startFrame = from;
            b.m_endFrame = to;
            return b;
        }

        AZ::EntityId MakeRig(const char* name, const DioramaHitboxConfig& config)
        {
            AZ::Entity* e = aznew AZ::Entity(name);
            e->CreateComponent<HitboxTransformStub>();
            e->CreateComponent<DioramaHitboxComponent>(config);
            e->Init();
            e->Activate();
            m_entities.push_back(e);
            return e->GetId();
        }

        // Tick the whole bus a few times. Hurtbox registration and the hitbox query run
        // in (unspecified) handler order, so a single tick can race; two ticks guarantee
        // every rig has published its hurtboxes before the queries read them. Extra ticks
        // also prove the dedup: a held hitbox still lands exactly once.
        void Tick(int times = 3)
        {
            for (int i = 0; i < times; ++i)
            {
                AZ::TickBus::Broadcast(&AZ::TickBus::Events::OnTick, 0.016f, AZ::ScriptTimePoint());
            }
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_worldEntity = nullptr;
        AZStd::vector<AZ::Entity*> m_entities;
    };

    TEST_F(DioramaHitboxComponentTest, ActiveHitboxOverlappingHurtbox_FiresHitAndHurtOnce)
    {
        // Attacker: a hitbox at x=0.5 active on frames 0..9. Target: a hurtbox at x=0.7
        // (always active). Their boxes overlap (centers 0.2 apart, half extents 0.5).
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener atk;
        TestHitListener tgt;
        atk.Listen(attacker);
        tgt.Listen(target);

        Tick(); // frame defaults to 0, inside the hitbox window

        EXPECT_EQ(atk.m_hitCount, 1); // one swing lands once, not once per tick
        EXPECT_EQ(atk.m_lastTarget, target);
        EXPECT_EQ(tgt.m_hurtCount, 1);
        EXPECT_EQ(tgt.m_lastAttacker, attacker);

        atk.Stop();
        tgt.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, HitboxInactiveOnFrame_FiresNothing)
    {
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 5, 8) }; // active 5..8 only

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);

        Tick(); // frame 0 is outside [5,8]
        EXPECT_EQ(atk.m_hitCount, 0);

        atk.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, LeavingAndReenteringTheWindow_ReArmsTheHit)
    {
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);

        Tick(); // active window: lands once
        EXPECT_EQ(atk.m_hitCount, 1);

        // Move the frame out of the active window: the box deactivates, clearing its
        // already-hit set so the next activation can land again.
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetFrame, 50);
        Tick();
        EXPECT_EQ(atk.m_hitCount, 1); // still just the first

        // Re-enter the window: a fresh swing lands a second hit on the same target.
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetFrame, 3);
        Tick();
        EXPECT_EQ(atk.m_hitCount, 2);
        EXPECT_EQ(atk.m_lastTarget, target);

        atk.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, FacingMirrorsTheHitbox_TurnedAwayMisses)
    {
        // Hitbox authored in front at +x. With facing -1 it mirrors to -x, away from the
        // target at +0.7, so it no longer overlaps.
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_facing = -1;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);

        Tick();
        EXPECT_EQ(atk.m_hitCount, 0); // mirrored hitbox at x=-0.5 misses the +0.7 hurtbox

        // Turn back to face the target: now it connects.
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetFacing, 1);
        Tick();
        EXPECT_EQ(atk.m_hitCount, 1);
        EXPECT_EQ(atk.m_lastTarget, target);

        atk.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, DisjointTargetMask_DoesNotHit)
    {
        // Attacker strikes layer 0x200, but the target's hurtbox is on 0x100.
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = 0x0200;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer; // 0x100
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);

        Tick();
        EXPECT_EQ(atk.m_hitCount, 0);

        atk.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, GetHitboxInfo_ReportsActiveCountsAndFacing)
    {
        DioramaHitboxConfig cfg;
        cfg.m_hurtLayer = HurtLayer;
        cfg.m_targetMask = HurtLayer;
        cfg.m_facing = -1;
        cfg.m_boxes = {
            Box(HitboxFrames::BoxKind::Hurtbox, 0.0f, 0, 99), // always active
            Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 5, 8), // active 5..8
        };

        const AZ::EntityId rig = MakeRig("Rig", cfg);

        DioramaHitboxRequestBus::Event(rig, &DioramaHitboxRequests::SetFrame, 6); // inside the hitbox window
        Tick();

        DioramaHitboxInfo info;
        DioramaHitboxRequestBus::EventResult(info, rig, &DioramaHitboxRequests::GetHitboxInfo);
        EXPECT_EQ(info.m_frame, 6);
        EXPECT_EQ(info.m_facing, -1);
        EXPECT_EQ(info.m_activeHurtboxes, 1);
        EXPECT_EQ(info.m_activeHitboxes, 1);
    }
} // namespace Diorama
