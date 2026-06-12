/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>

#include <Clients/DioramaAnimStateMachineComponent.h>
#include <Clients/DioramaAsepriteComponent.h>
#include <Clients/DioramaBulletEmitterComponent.h>
#include <Clients/DioramaSimClockComponent.h>
#include <Clients/SimStateBus.h>
#include <Clients/SpriteComponent.h>

// Sim-clock migration acceptance: components flagged "Use Simulation Clock"
// advance on the clock's fixed steps (StepOnce, no render tick needed), the
// SetUseSimClock bus verbs toggle that live, and the new playback-position
// chunks ('SPRA' / 'ASEP' / 'ANSM') round trip byte-identically.
namespace Diorama
{
    namespace
    {
        //! Provides TransformService so components requiring it activate on a bare
        //! entity (identity transform; positions are irrelevant here).
        class MigrationTransformStub : public AZ::Component
        {
        public:
            AZ_COMPONENT(MigrationTransformStub, "{B6E0BBF4-32A1-4D5B-9C87-D31FA60C2E19}", AZ::Component);
            static void Reflect(AZ::ReflectContext* context)
            {
                if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
                {
                    sc->Class<MigrationTransformStub, AZ::Component>()->Version(1);
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

        //! Capture one entity's participant chunks into a buffer.
        AZStd::vector<AZ::u8> SaveMigrationChunks(AZ::EntityId id)
        {
            AZStd::vector<AZ::u8> buf;
            SimState::Writer writer(buf);
            DioramaSimStateParticipantBus::Event(id, &DioramaSimStateParticipants::SaveSimState, writer);
            return buf;
        }

        //! Offer every chunk in a captured buffer back to the entity's participants.
        bool RestoreMigrationChunks(AZ::EntityId id, const AZStd::vector<AZ::u8>& buf)
        {
            SimState::Reader reader(buf.data(), buf.size());
            while (reader.Ok() && reader.Remaining() > 0)
            {
                AZ::u32 tag = 0;
                AZ::u32 size = 0;
                if (!reader.ChunkHeader(tag, size))
                {
                    return false;
                }
                SimState::Reader payload = reader.SubReader(size);
                bool consumed = false;
                DioramaSimStateParticipantBus::EnumerateHandlersId(
                    id,
                    [&consumed, tag, &payload](DioramaSimStateParticipants* handler)
                    {
                        consumed = handler->TryRestoreChunk(tag, payload);
                        return !consumed;
                    });
                if (!consumed)
                {
                    return false;
                }
            }
            return reader.Ok();
        }
    } // namespace

    class SimClockMigrationTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            m_app.RegisterComponentDescriptor(MigrationTransformStub::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaSimClockComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(SpriteComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaAsepriteComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaAnimStateMachineComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaBulletEmitterComponent::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();

            DioramaSimClockConfig clockConfig;
            clockConfig.m_startPaused = true; // every step is an explicit StepOnce
            m_clockEntity = aznew AZ::Entity("SimClock");
            m_clockEntity->CreateComponent<DioramaSimClockComponent>(clockConfig);
            m_clockEntity->Init();
            m_clockEntity->Activate();
        }

        void TearDown() override
        {
            for (AZ::Entity* e : m_entities)
            {
                e->Deactivate();
                delete e;
            }
            m_entities.clear();
            m_clockEntity->Deactivate();
            delete m_clockEntity;
            m_app.Destroy();
        }

        void StepClock(int steps)
        {
            for (int i = 0; i < steps; ++i)
            {
                DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
            }
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_clockEntity = nullptr;
        AZStd::vector<AZ::Entity*> m_entities;
    };

    TEST_F(SimClockMigrationTest, SimClockedSpriteAdvancesOnStepOnce)
    {
        // At 60 fps animation and the clock's 60 steps/sec, one StepOnce = one frame.
        SpriteComponentConfig config;
        config.m_animEnabled = true;
        config.m_frameColumns = 2;
        config.m_frameRows = 2;
        config.m_frameCount = 4;
        config.m_framesPerSecond = 60.0f;
        config.m_useSimClock = true;
        AZ::Entity* e = aznew AZ::Entity("Sprite");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<SpriteComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        StepClock(2);

        SpriteInfo info;
        DioramaSpriteRequestBus::EventResult(info, id, &DioramaSpriteRequests::GetSpriteInfo);
        EXPECT_EQ(info.m_currentFrame, 2);
        EXPECT_TRUE(info.m_useSimClock);
    }

    TEST_F(SimClockMigrationTest, SimClockedEmitterExpiresBulletsOnStepOnce)
    {
        // No manual StepBullets: the clock's fixed steps age the pool until the
        // short lifetime expires every bullet.
        DioramaBulletConfig config;
        config.m_count = 4;
        config.m_fireOnActivate = false;
        config.m_fireRate = 0.0f;
        config.m_bulletLifetime = 0.05f; // 3 steps at 60 steps/sec
        config.m_targetMask = 0;
        config.m_useSimClock = true;
        AZ::Entity* e = aznew AZ::Entity("Emitter");
        e->CreateComponent<MigrationTransformStub>();
        auto* emitter = e->CreateComponent<DioramaBulletEmitterComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        emitter->EmitShot();
        DioramaBulletInfo before;
        DioramaBulletRequestBus::EventResult(before, id, &DioramaBulletRequests::GetBulletInfo);
        ASSERT_EQ(before.m_aliveCount, 4);

        StepClock(6);

        DioramaBulletInfo after;
        DioramaBulletRequestBus::EventResult(after, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(after.m_aliveCount, 0);
    }

    TEST_F(SimClockMigrationTest, SetUseSimClockVerbTogglesLive)
    {
        // Activated off the clock: steps do nothing until the bus verb opts in.
        DioramaBulletConfig config;
        config.m_count = 4;
        config.m_fireOnActivate = false;
        config.m_fireRate = 0.0f;
        config.m_bulletLifetime = 0.05f;
        config.m_targetMask = 0;
        config.m_useSimClock = false;
        AZ::Entity* e = aznew AZ::Entity("Emitter");
        e->CreateComponent<MigrationTransformStub>();
        auto* emitter = e->CreateComponent<DioramaBulletEmitterComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        emitter->EmitShot();
        StepClock(6);
        DioramaBulletInfo info;
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 4); // untouched: not on the clock

        DioramaBulletRequestBus::Event(id, &DioramaBulletRequests::SetUseSimClock, true);
        StepClock(6);
        DioramaBulletRequestBus::EventResult(info, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(info.m_aliveCount, 0); // now aged out by the fixed steps
    }

    TEST_F(SimClockMigrationTest, SpriteChunkRoundTripsAndIsCanonical)
    {
        SpriteComponentConfig config;
        config.m_animEnabled = true;
        config.m_frameColumns = 2;
        config.m_frameRows = 2;
        config.m_frameCount = 4;
        config.m_framesPerSecond = 10.0f;
        AZ::Entity* e = aznew AZ::Entity("Sprite");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<SpriteComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        // Hand-build a 'SPRA' image (frame 2, mid-frame timer, not finished) and
        // offer it: the component applies it and re-saves it byte-identically.
        AZStd::vector<AZ::u8> image;
        SimState::Writer writer(image);
        const size_t sizePos = writer.BeginChunk(SpriteComponent::SpriteChunkTag);
        writer.S64(2);
        writer.F32(0.025f);
        writer.U8(0);
        writer.EndChunk(sizePos);

        ASSERT_TRUE(RestoreMigrationChunks(id, image));
        SpriteInfo info;
        DioramaSpriteRequestBus::EventResult(info, id, &DioramaSpriteRequests::GetSpriteInfo);
        EXPECT_EQ(info.m_currentFrame, 2);
        EXPECT_TRUE(SaveMigrationChunks(id) == image);
    }

    TEST_F(SimClockMigrationTest, AsepriteChunkRoundTripsAndIsCanonical)
    {
        DioramaAsepriteConfig config;
        config.m_atlasWidth = 96;
        config.m_atlasHeight = 32;
        config.m_frames.resize(3);
        for (int i = 0; i < 3; ++i)
        {
            config.m_frames[i].m_x = i * 32;
            config.m_frames[i].m_w = 32;
            config.m_frames[i].m_h = 32;
        }
        config.m_autoPlay = false;
        AZ::Entity* e = aznew AZ::Entity("Aseprite");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaAsepriteComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        DioramaAsepriteRequestBus::Event(id, &DioramaAsepriteRequests::SetFrame, 2);
        const AZStd::vector<AZ::u8> image = SaveMigrationChunks(id);
        ASSERT_FALSE(image.empty());

        // Diverge, then restore: the shown frame returns to the saved one.
        DioramaAsepriteRequestBus::Event(id, &DioramaAsepriteRequests::SetFrame, 0);
        ASSERT_TRUE(RestoreMigrationChunks(id, image));

        int frame = -1;
        DioramaAsepriteRequestBus::EventResult(frame, id, &DioramaAsepriteRequests::GetCurrentFrame);
        EXPECT_EQ(frame, 2);
        EXPECT_TRUE(SaveMigrationChunks(id) == image);
    }

    TEST_F(SimClockMigrationTest, AnimStateMachineChunkRoundTripsAndIsCanonical)
    {
        DioramaAnimStateMachineConfig config;
        config.m_parameters.resize(1);
        config.m_parameters[0].m_name = "speed";
        config.m_parameters[0].m_kind = AnimSM::ParamKind::Float;
        config.m_states.resize(2);
        config.m_states[0].m_name = "idle";
        config.m_states[1].m_name = "run";
        config.m_defaultState = "idle";
        AZ::Entity* e = aznew AZ::Entity("Graph");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaAnimStateMachineComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        DioramaAnimStateMachineRequestBus::Event(id, &DioramaAnimStateMachineRequests::Play, AZStd::string("run"));
        DioramaAnimStateMachineRequestBus::Event(id, &DioramaAnimStateMachineRequests::SetFloat, AZStd::string("speed"), 3.5f);
        const AZStd::vector<AZ::u8> image = SaveMigrationChunks(id);
        ASSERT_FALSE(image.empty());

        // Diverge, then restore: state and parameter values return to the image.
        DioramaAnimStateMachineRequestBus::Event(id, &DioramaAnimStateMachineRequests::Play, AZStd::string("idle"));
        DioramaAnimStateMachineRequestBus::Event(id, &DioramaAnimStateMachineRequests::SetFloat, AZStd::string("speed"), 1.0f);
        ASSERT_TRUE(RestoreMigrationChunks(id, image));

        AZStd::string state;
        DioramaAnimStateMachineRequestBus::EventResult(state, id, &DioramaAnimStateMachineRequests::GetCurrentState);
        EXPECT_EQ(state, "run");
        float speed = 0.0f;
        DioramaAnimStateMachineRequestBus::EventResult(speed, id, &DioramaAnimStateMachineRequests::GetFloat, AZStd::string("speed"));
        EXPECT_FLOAT_EQ(speed, 3.5f);
        EXPECT_TRUE(SaveMigrationChunks(id) == image);
    }
} // namespace Diorama
