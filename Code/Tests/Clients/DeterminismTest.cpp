/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>

#include <Clients/DioramaBulletEmitterComponent.h>
#include <Clients/DioramaSimClockComponent.h>
#include <Clients/DioramaSimStateComponent.h>

// The deterministic-sim acceptance test (design phase D): the same seed and the
// same per-frame actions produce bit-identical state hashes, and a mid-run
// restore re-advances to the same hash the original run reached. This runs in the
// normal suite, so every PR proves the simulation is still deterministic.
namespace Diorama
{
    namespace
    {
        class DeterminismTransformStub : public AZ::Component
        {
        public:
            AZ_COMPONENT(DeterminismTransformStub, "{8E47A12B-6F3D-4C90-B2E5-1A7D4F8C3B62}", AZ::Component);
            static void Reflect(AZ::ReflectContext* context)
            {
                if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
                {
                    sc->Class<DeterminismTransformStub, AZ::Component>()->Version(1);
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
    } // namespace

    class DeterminismTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            m_app.RegisterComponentDescriptor(DeterminismTransformStub::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaSimClockComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaSimStateComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaBulletEmitterComponent::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();

            DioramaSimClockConfig clockConfig;
            clockConfig.m_startPaused = true; // every step is an explicit StepOnce
            clockConfig.m_randomSeed = 7;
            m_clockEntity = aznew AZ::Entity("SimClock");
            m_clockEntity->CreateComponent<DioramaSimClockComponent>(clockConfig);
            m_clockEntity->Init();
            m_clockEntity->Activate();

            DioramaBulletConfig bulletConfig;
            bulletConfig.m_count = 6;
            bulletConfig.m_fireOnActivate = false;
            bulletConfig.m_fireRate = 0.0f;
            m_emitterEntity = aznew AZ::Entity("Emitter");
            m_emitterEntity->CreateComponent<DeterminismTransformStub>();
            m_emitter = m_emitterEntity->CreateComponent<DioramaBulletEmitterComponent>(bulletConfig);
            m_emitterEntity->CreateComponent<DioramaSimStateComponent>(); // enroll in capture
            m_emitterEntity->Init();
            m_emitterEntity->Activate();
        }

        void TearDown() override
        {
            m_emitterEntity->Deactivate();
            delete m_emitterEntity;
            m_clockEntity->Deactivate();
            delete m_clockEntity;
            m_app.Destroy();
        }

        //! One scripted simulation step: a deterministic mix of emission, bullet
        //! integration, and RNG draws, keyed only off the frame number.
        void ScriptedStep(AZ::s64 frame)
        {
            if (frame % 7 == 0)
            {
                m_emitter->EmitShot();
            }
            m_emitter->StepBullets(1.0f / 60.0f, false);
            if (frame % 3 == 0)
            {
                float unused = 0.0f;
                DioramaRandomRequestBus::BroadcastResult(unused, &DioramaRandomRequests::RandFloat);
            }
            DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        }

        AZ::u64 Hash()
        {
            AZ::u64 h = 0;
            DioramaSimClockRequestBus::BroadcastResult(h, &DioramaSimClockRequests::GetStateHash);
            return h;
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_clockEntity = nullptr;
        AZ::Entity* m_emitterEntity = nullptr;
        DioramaBulletEmitterComponent* m_emitter = nullptr;
    };

    TEST_F(DeterminismTest, SameRunReplaysToIdenticalHashesEveryFrame)
    {
        constexpr int FrameCount = 30;

        // Capture the initial state, run the scripted sequence, and record the hash
        // after every frame.
        AZStd::vector<AZ::u8> initial;
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::CaptureFrame, initial);
        AZStd::vector<AZ::u64> firstRun;
        for (AZ::s64 f = 0; f < FrameCount; ++f)
        {
            ScriptedStep(f);
            firstRun.push_back(Hash());
        }

        // Restore to the start and replay the identical sequence: every per-frame
        // hash must match bit-for-bit.
        bool restored = false;
        DioramaSimClockRequestBus::BroadcastResult(restored, &DioramaSimClockRequests::RestoreFrame, initial);
        ASSERT_TRUE(restored);
        for (AZ::s64 f = 0; f < FrameCount; ++f)
        {
            ScriptedStep(f);
            EXPECT_EQ(Hash(), firstRun[static_cast<size_t>(f)]) << "diverged at frame " << f;
        }
    }

    TEST_F(DeterminismTest, MidRunRestoreReadvancesToTheSameHash)
    {
        constexpr int Midpoint = 15;
        constexpr int FrameCount = 30;

        for (AZ::s64 f = 0; f < Midpoint; ++f)
        {
            ScriptedStep(f);
        }
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::SaveToSlot, 0);
        for (AZ::s64 f = Midpoint; f < FrameCount; ++f)
        {
            ScriptedStep(f);
        }
        const AZ::u64 endHash = Hash();

        // Roll back to the midpoint and re-advance with the same scripted inputs:
        // the rollback-and-resimulate loop must land on the identical end state.
        bool restored = false;
        DioramaSimClockRequestBus::BroadcastResult(restored, &DioramaSimClockRequests::RestoreFromSlot, 0);
        ASSERT_TRUE(restored);
        for (AZ::s64 f = Midpoint; f < FrameCount; ++f)
        {
            ScriptedStep(f);
        }
        EXPECT_EQ(Hash(), endHash);
    }
} // namespace Diorama
