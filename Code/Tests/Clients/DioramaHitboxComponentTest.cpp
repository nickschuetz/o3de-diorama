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
        void OnBoxEvent(const DioramaBoxEvent& boxEvent) override
        {
            m_boxEvents.push_back(boxEvent);
        }

        //! Events with the given result received at this listener.
        int CountResult(HitboxFrames::BoxResult result) const
        {
            int count = 0;
            for (const DioramaBoxEvent& e : m_boxEvents)
            {
                if (e.m_result == static_cast<int>(result))
                {
                    ++count;
                }
            }
            return count;
        }
        const DioramaBoxEvent* FirstResult(HitboxFrames::BoxResult result) const
        {
            for (const DioramaBoxEvent& e : m_boxEvents)
            {
                if (e.m_result == static_cast<int>(result))
                {
                    return &e;
                }
            }
            return nullptr;
        }

        int m_hitCount = 0;
        int m_hurtCount = 0;
        AZ::EntityId m_lastTarget;
        AZ::EntityId m_lastAttacker;
        AZStd::vector<DioramaBoxEvent> m_boxEvents;
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
            Box(HitboxFrames::BoxKind::Pushbox, 0.0f, 0, 99),       Box(HitboxFrames::BoxKind::ArmorBox, 0.0f, 5, 8),
            Box(HitboxFrames::BoxKind::ProximityBox, 1.0f, 50, 60), // inactive on frame 6
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
        EXPECT_EQ(info.m_activePushboxes, 1);
        EXPECT_EQ(info.m_activeArmorBoxes, 1);
        EXPECT_EQ(info.m_activeProximityBoxes, 0);
    }

    TEST_F(DioramaHitboxComponentTest, HitEventCarriesPayloadContactAndBoxIndices)
    {
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        DioramaHitboxData jab = Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9);
        jab.m_hit.m_damage = 45.0f;
        jab.m_hit.m_hitstunFrames = 14;
        jab.m_hit.m_hitstopFrames = 6;
        jab.m_hit.m_pushback = AZ::Vector2(1.5f, 0.0f);
        jab.m_hit.m_guardHeight = HitboxFrames::GuardHeight::Low;
        jab.m_hit.m_customId = 7001;
        attackerCfg.m_boxes = { jab };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener tgt;
        tgt.Listen(target);
        Tick();

        ASSERT_EQ(tgt.CountResult(HitboxFrames::BoxResult::Hit), 1); // once per window
        const DioramaBoxEvent* hit = tgt.FirstResult(HitboxFrames::BoxResult::Hit);
        EXPECT_EQ(hit->m_attacker, attacker);
        EXPECT_EQ(hit->m_defender, target);
        EXPECT_EQ(hit->m_attackerBoxIndex, 0);
        EXPECT_EQ(hit->m_defenderBoxIndex, 0);
        EXPECT_FLOAT_EQ(hit->m_damage, 45.0f);
        EXPECT_EQ(hit->m_hitstunFrames, 14);
        EXPECT_EQ(hit->m_hitstopFrames, 6);
        EXPECT_FLOAT_EQ(hit->m_pushbackX, 1.5f);
        EXPECT_EQ(hit->m_guardHeight, 2); // Low
        EXPECT_EQ(hit->m_customId, 7001u);
        // Contact: boxes span [0.0, 1.0] and [0.2, 1.2] in X -> overlap [0.2, 1.0],
        // center 0.6; both centered on Y 0.
        EXPECT_NEAR(hit->m_contactX, 0.6f, 0.001f);
        EXPECT_NEAR(hit->m_contactY, 0.0f, 0.001f);

        tgt.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, ArmorAbsorbsInsteadOfHit)
    {
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        // Defender has BOTH an active armor box and an overlapping hurtbox: the
        // armor shadows the hurtbox, so the result is Absorbed, never Hit.
        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = {
            Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99),
            Box(HitboxFrames::BoxKind::ArmorBox, 0.7f, 0, 99),
        };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener atk;
        TestHitListener tgt;
        atk.Listen(attacker);
        tgt.Listen(target);
        Tick();

        EXPECT_EQ(atk.CountResult(HitboxFrames::BoxResult::Absorbed), 1);
        EXPECT_EQ(tgt.CountResult(HitboxFrames::BoxResult::Absorbed), 1);
        EXPECT_EQ(atk.CountResult(HitboxFrames::BoxResult::Hit), 0);
        EXPECT_EQ(atk.m_hitCount, 0); // legacy OnHit stays strike-only
        EXPECT_EQ(tgt.m_hurtCount, 0);
        const DioramaBoxEvent* absorbed = tgt.FirstResult(HitboxFrames::BoxResult::Absorbed);
        EXPECT_EQ(absorbed->m_defenderBoxIndex, 1); // the armor box, not the hurtbox

        atk.Stop();
        tgt.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, ThrowboxGrabsThrowableOncePerWindow)
    {
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        DioramaHitboxData grab = Box(HitboxFrames::BoxKind::Throwbox, 0.5f, 0, 9);
        grab.m_hit.m_customId = 9002;
        attackerCfg.m_boxes = { grab };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::ThrowableBox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);
        Tick();

        EXPECT_EQ(atk.CountResult(HitboxFrames::BoxResult::Throw), 1);
        const DioramaBoxEvent* grabEvent = atk.FirstResult(HitboxFrames::BoxResult::Throw);
        EXPECT_EQ(grabEvent->m_attacker, attacker);
        EXPECT_EQ(grabEvent->m_defender, target);
        EXPECT_EQ(grabEvent->m_customId, 9002u);
        EXPECT_EQ(atk.m_hitCount, 0); // a throw is not a hit

        atk.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, ProximitySignalsOverHurtbox)
    {
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::ProximityBox, 0.5f, 0, 9) };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener tgt;
        tgt.Listen(target);
        Tick();

        EXPECT_EQ(tgt.CountResult(HitboxFrames::BoxResult::Proximity), 1);
        EXPECT_EQ(tgt.m_hurtCount, 0); // a signal, not a hit

        tgt.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, EqualPriorityHitboxesClashOnEachSide)
    {
        DioramaHitboxConfig cfgA;
        cfgA.m_hurtLayer = HurtLayer;
        cfgA.m_targetMask = HurtLayer;
        cfgA.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        DioramaHitboxConfig cfgB = cfgA; // same priority (0), overlapping
        cfgB.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.7f, 0, 9) };

        const AZ::EntityId a = MakeRig("A", cfgA);
        const AZ::EntityId b = MakeRig("B", cfgB);

        TestHitListener la;
        TestHitListener lb;
        la.Listen(a);
        lb.Listen(b);
        Tick();

        // One contest, one event per side, each delivered to both entities.
        EXPECT_EQ(la.CountResult(HitboxFrames::BoxResult::Clash), 2);
        EXPECT_EQ(lb.CountResult(HitboxFrames::BoxResult::Clash), 2);
        EXPECT_EQ(la.m_hitCount + la.m_hurtCount, 0); // contests never fire OnHit/OnHurt
        EXPECT_EQ(lb.m_hitCount + lb.m_hurtCount, 0);

        la.Stop();
        lb.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, PriorityWinnerScoresHitLoserIsBeaten)
    {
        DioramaHitboxConfig cfgA;
        cfgA.m_hurtLayer = HurtLayer;
        cfgA.m_targetMask = HurtLayer;
        DioramaHitboxData heavy = Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9);
        heavy.m_hit.m_priority = 5;
        cfgA.m_boxes = { heavy };

        DioramaHitboxConfig cfgB;
        cfgB.m_hurtLayer = HurtLayer;
        cfgB.m_targetMask = HurtLayer;
        DioramaHitboxData light = Box(HitboxFrames::BoxKind::Hitbox, 0.7f, 0, 9);
        light.m_hit.m_priority = 2;
        cfgB.m_boxes = { light };

        const AZ::EntityId a = MakeRig("Heavy", cfgA);
        const AZ::EntityId b = MakeRig("Light", cfgB);

        TestHitListener la;
        la.Listen(a);
        Tick();

        // The winner's side: result Hit with the winner as attacker; the loser's
        // side: result Beaten with the loser as attacker. Both reach entity A.
        const DioramaBoxEvent* win = la.FirstResult(HitboxFrames::BoxResult::Hit);
        ASSERT_NE(win, nullptr);
        EXPECT_EQ(win->m_attacker, a);
        EXPECT_EQ(win->m_priority, 5);
        const DioramaBoxEvent* beaten = la.FirstResult(HitboxFrames::BoxResult::Beaten);
        ASSERT_NE(beaten, nullptr);
        EXPECT_EQ(beaten->m_attacker, b);
        EXPECT_EQ(beaten->m_priority, 2);

        la.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, PushboxOverlapReportsOppositePushOuts)
    {
        DioramaHitboxConfig cfgA;
        cfgA.m_hurtLayer = HurtLayer;
        cfgA.m_targetMask = HurtLayer;
        cfgA.m_boxes = { Box(HitboxFrames::BoxKind::Pushbox, 0.0f, 0, 99) };

        DioramaHitboxConfig cfgB = cfgA;
        cfgB.m_boxes = { Box(HitboxFrames::BoxKind::Pushbox, 0.6f, 0, 99) }; // overlaps by 0.4 in X

        const AZ::EntityId a = MakeRig("A", cfgA);
        const AZ::EntityId b = MakeRig("B", cfgB);
        Tick();

        DioramaHitboxInfo infoA;
        DioramaHitboxInfo infoB;
        DioramaHitboxRequestBus::EventResult(infoA, a, &DioramaHitboxRequests::GetHitboxInfo);
        DioramaHitboxRequestBus::EventResult(infoB, b, &DioramaHitboxRequests::GetHitboxInfo);

        // Each rig is pushed away from the other along X, by the same magnitude.
        EXPECT_LT(infoA.m_pushOutX, 0.0f);
        EXPECT_GT(infoB.m_pushOutX, 0.0f);
        EXPECT_NEAR(infoA.m_pushOutX, -infoB.m_pushOutX, 0.001f);
        EXPECT_NEAR(infoA.m_pushOutY, 0.0f, 0.001f);
    }

    TEST_F(DioramaHitboxComponentTest, ShowOverlayTogglesSafelyWithoutAScene)
    {
        // The box overlay draws through the sprite feature processor, which has no
        // scene in this headless fixture, so RefreshOverlay must no-op rather than
        // crash and must not disturb hit resolution. This guards the scene == nullptr
        // early-out and the toggle path; the visual is verified at a monitor.
        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_showOverlay = true; // on from activation
        attackerCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9) };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);
        Tick(); // overlay refresh runs (and no-ops); the hit still lands
        EXPECT_EQ(atk.m_hitCount, 1);

        // Toggle off and back on at runtime: still no crash, still resolves.
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetShowOverlay, false);
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetShowOverlay, true);
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetFrame, 50); // leave the window
        Tick();
        DioramaHitboxRequestBus::Event(attacker, &DioramaHitboxRequests::SetFrame, 3); // re-enter -> re-arm
        Tick();
        EXPECT_EQ(atk.m_hitCount, 2);

        atk.Stop();
    }

    TEST_F(DioramaHitboxComponentTest, UnresolvedBoneNameFallsBackToTheStaticOffset)
    {
        // A box that names a 2D-skeletal bone resolves the bone entity by name from the
        // rig's transform hierarchy. This headless fixture has no such hierarchy, so the
        // name does not resolve and the box must fall back to its static offset (the v1
        // behavior) - adding a bone name never breaks a box whose bone is missing. Live
        // bone-following is verified at a monitor. A static box at 0.5 and a
        // bone-named box at 0.5 (bone "fist" absent) must land the same hit.
        DioramaHitboxData jab = Box(HitboxFrames::BoxKind::Hitbox, 0.5f, 0, 9);
        jab.m_boneName = "fist"; // no descendant entity named "fist" in this fixture

        DioramaHitboxConfig attackerCfg;
        attackerCfg.m_hurtLayer = HurtLayer;
        attackerCfg.m_targetMask = HurtLayer;
        attackerCfg.m_boxes = { jab };

        DioramaHitboxConfig targetCfg;
        targetCfg.m_hurtLayer = HurtLayer;
        targetCfg.m_targetMask = HurtLayer;
        targetCfg.m_boxes = { Box(HitboxFrames::BoxKind::Hurtbox, 0.7f, 0, 99) };

        const AZ::EntityId attacker = MakeRig("Attacker", attackerCfg);
        const AZ::EntityId target = MakeRig("Target", targetCfg);

        TestHitListener atk;
        atk.Listen(attacker);
        Tick();

        // Same overlap as the plain static-hitbox test: the fallback landed the hit.
        EXPECT_EQ(atk.m_hitCount, 1);
        EXPECT_EQ(atk.m_lastTarget, target);

        atk.Stop();
    }
} // namespace Diorama
