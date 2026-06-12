/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>

#include <Clients/DioramaInputComponent.h>
#include <Clients/DioramaSimClockComponent.h>

// Integration test for the per-sim-frame input ring (deterministic sim phase C):
// the input component in sim-clock mode records one action sample per fixed step,
// serves frame queries, accepts injected states (the rollback / replay / bot path),
// and REPLAYS recorded frames on re-simulation instead of re-sampling live devices.
// No input devices exist in this harness; states are driven entirely by injection,
// which is exactly the re-simulation code path.
namespace Diorama
{
    class SimInputRingTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            m_app.RegisterComponentDescriptor(DioramaSimClockComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaInputComponent::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();

            // Paused clock: every step below is an explicit StepOnce, so frame
            // numbers in the assertions are exact.
            DioramaSimClockConfig clockConfig;
            clockConfig.m_startPaused = true;
            m_clockEntity = aznew AZ::Entity("SimClock");
            m_clockEntity->CreateComponent<DioramaSimClockComponent>(clockConfig);
            m_clockEntity->Init();
            m_clockEntity->Activate();

            DioramaInputConfig inputConfig;
            DioramaInputActionData fire;
            fire.m_name = "fire";
            fire.m_kind = InputMap::ActionKind::Button;
            inputConfig.m_actions.push_back(fire);
            inputConfig.m_useSimClock = true;
            inputConfig.m_historyFrames = 8; // small ring so wrap behavior is exercised
            m_inputEntity = aznew AZ::Entity("Input");
            m_inputEntity->CreateComponent<DioramaInputComponent>(inputConfig);
            m_inputEntity->Init();
            m_inputEntity->Activate();
            m_inputId = m_inputEntity->GetId();
        }

        void TearDown() override
        {
            m_inputEntity->Deactivate();
            delete m_inputEntity;
            m_clockEntity->Deactivate();
            delete m_clockEntity;
            m_app.Destroy();
        }

        void Step()
        {
            DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        }
        void Inject(AZ::s64 frame, float x, bool pressed)
        {
            DioramaInputRequestBus::Event(
                m_inputId, &DioramaInputRequests::InjectActionState, frame, AZStd::string("fire"), x, 0.0f, pressed);
        }
        bool PressedAt(AZ::s64 frame)
        {
            bool v = false;
            DioramaInputRequestBus::EventResult(v, m_inputId, &DioramaInputRequests::WasPressedAtFrame, AZStd::string("fire"), frame);
            return v;
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_clockEntity = nullptr;
        AZ::Entity* m_inputEntity = nullptr;
        AZ::EntityId m_inputId;
    };

    TEST_F(SimInputRingTest, InjectedFramesRecordAndDeriveEdges)
    {
        Inject(0, 1.0f, true);
        Step(); // frame 0: injected press; edge derives against absent frame -1

        bool pressed = false;
        DioramaInputRequestBus::EventResult(pressed, m_inputId, &DioramaInputRequests::IsPressed, AZStd::string("fire"));
        EXPECT_TRUE(pressed);
        bool edge = false;
        DioramaInputRequestBus::EventResult(edge, m_inputId, &DioramaInputRequests::WasPressedThisFrame, AZStd::string("fire"));
        EXPECT_TRUE(edge);

        Step(); // frame 1: nothing injected, no devices -> released; release edge fires
        bool released = false;
        DioramaInputRequestBus::EventResult(released, m_inputId, &DioramaInputRequests::WasReleasedThisFrame, AZStd::string("fire"));
        EXPECT_TRUE(released);

        EXPECT_TRUE(PressedAt(0));
        EXPECT_FALSE(PressedAt(1));
        float value = 0.0f;
        DioramaInputRequestBus::EventResult(value, m_inputId, &DioramaInputRequests::GetValueAtFrame, AZStd::string("fire"), AZ::s64(0));
        EXPECT_FLOAT_EQ(value, 1.0f);
    }

    TEST_F(SimInputRingTest, OutOfWindowFramesReadZero)
    {
        Inject(0, 1.0f, true);
        Step();
        EXPECT_FALSE(PressedAt(-1)); // never simulated
        EXPECT_FALSE(PressedAt(99)); // future
        // Step past the 8-frame ring: frame 0's slot is reused, so it reads zero.
        for (int i = 0; i < 9; ++i)
        {
            Step();
        }
        EXPECT_FALSE(PressedAt(0));
    }

    TEST_F(SimInputRingTest, RestoreThenResimulateReplaysTheRing)
    {
        // Frame 0: press (injected). Save the post-frame-0 state to a slot.
        Inject(0, 1.0f, true);
        Step();
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::SaveToSlot, 0);

        // Frame 1: release. The ring now holds press@0, release@1.
        Step();
        ASSERT_TRUE(PressedAt(0));
        ASSERT_FALSE(PressedAt(1));

        // Rollback: restore (clock rewinds to just after frame 0) and re-simulate
        // frame 1. The ring survives the restore (the input log lives OUTSIDE the
        // snapshot) and frame 1 REPLAYS its recorded release rather than re-sampling.
        bool restored = false;
        DioramaSimClockRequestBus::BroadcastResult(restored, &DioramaSimClockRequests::RestoreFromSlot, 0);
        ASSERT_TRUE(restored);
        AZ::s64 frame = -1;
        DioramaSimClockRequestBus::BroadcastResult(frame, &DioramaSimClockRequests::GetSimFrame);
        EXPECT_EQ(frame, 1); // rewound: frame 1 is next to simulate again

        Step(); // re-simulate frame 1
        bool released = false;
        DioramaInputRequestBus::EventResult(released, m_inputId, &DioramaInputRequests::WasReleasedThisFrame, AZStd::string("fire"));
        EXPECT_TRUE(released); // identical edge on re-simulation
        EXPECT_TRUE(PressedAt(0)); // history intact through the rollback

        // Inject a CORRECTION for frame 1 (the rollback case: a remote input arrived
        // late), restore again, re-simulate: now frame 1 replays the corrected press.
        DioramaSimClockRequestBus::BroadcastResult(restored, &DioramaSimClockRequests::RestoreFromSlot, 0);
        ASSERT_TRUE(restored);
        Inject(1, 1.0f, true);
        Step();
        EXPECT_TRUE(PressedAt(1));
        bool pressedEdge = false;
        DioramaInputRequestBus::EventResult(pressedEdge, m_inputId, &DioramaInputRequests::WasPressedThisFrame, AZStd::string("fire"));
        EXPECT_FALSE(pressedEdge); // held since frame 0: no fresh press edge
    }
} // namespace Diorama
