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
#include <Clients/DioramaSimStateComponent.h>
#include <Clients/DioramaSkeletalClipComponent.h>
#include <Clients/DioramaSkinnedSpriteComponent.h>
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
            m_app.RegisterComponentDescriptor(DioramaSkeletalClipComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaSkinnedSpriteComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaSimStateComponent::CreateDescriptor());

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

    TEST_F(SimClockMigrationTest, SkeletalClockedAdvancesOnStepOnce)
    {
        // A short non-looping clip on the sim clock: the fixed steps advance it past its end,
        // so IsPlaying goes false with no render tick (no bone entities needed to advance time).
        DioramaSkeletalClipConfig config;
        config.m_duration = 0.05f; // 3 steps at 60 steps/sec
        config.m_looping = false;
        config.m_autoPlay = true;
        config.m_useSimClock = true;
        AZ::Entity* e = aznew AZ::Entity("Skeletal");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaSkeletalClipComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        bool playing = false;
        DioramaSkeletalRequestBus::EventResult(playing, id, &DioramaSkeletalRequests::IsPlaying);
        EXPECT_TRUE(playing);

        StepClock(6); // 0.1s > 0.05s duration

        DioramaSkeletalRequestBus::EventResult(playing, id, &DioramaSkeletalRequests::IsPlaying);
        EXPECT_FALSE(playing);
        bool useSim = false;
        DioramaSkeletalRequestBus::EventResult(useSim, id, &DioramaSkeletalRequests::GetUseSimClock);
        EXPECT_TRUE(useSim);
    }

    TEST_F(SimClockMigrationTest, SkeletalChunkRoundTripsAndIsCanonical)
    {
        DioramaSkeletalClipConfig config;
        config.m_duration = 1.0f;
        config.m_looping = true;
        config.m_autoPlay = true;
        config.m_clips.resize(2);
        config.m_clips[0].m_name = "walk";
        config.m_clips[0].m_duration = 0.5f;
        config.m_clips[1].m_name = "run";
        config.m_clips[1].m_duration = 0.3f;
        AZ::Entity* e = aznew AZ::Entity("Skeletal");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaSkeletalClipComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        // Drive to a distinctive state: current clip = walk (0), a blend param, a speed, and a
        // looping override (which only sticks because the current clip is tracked by index).
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::CrossFadeTo, AZStd::string("walk"), 0.0f);
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::SetBlendParam, 0.7f);
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::SetSpeed, 2.5f);
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::SetLooping, false);
        const AZStd::vector<AZ::u8> image = SaveMigrationChunks(id);
        ASSERT_FALSE(image.empty());

        // Diverge: switch to run (clip 1) and a different blend param.
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::CrossFadeTo, AZStd::string("run"), 0.0f);
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::SetBlendParam, 0.1f);
        ASSERT_TRUE(RestoreMigrationChunks(id, image));

        float blend = 0.0f;
        DioramaSkeletalRequestBus::EventResult(blend, id, &DioramaSkeletalRequests::GetBlendParam);
        EXPECT_FLOAT_EQ(blend, 0.7f);
        // Canonical: re-saving after restore is byte-identical, so the current clip index, the
        // speed, and the loop / duration overrides all round-trip, not just the blend param.
        EXPECT_TRUE(SaveMigrationChunks(id) == image);
    }

    TEST_F(SimClockMigrationTest, SkinnedSpriteChunkRoundTripsAndIsCanonical)
    {
        // No rig is loaded here (a real DragonBones armature needs an asset), so the play state
        // is minimal; but the 'SKIN' chunk mechanism and the runtime scalars (incl. the speed set
        // via the bus) must round-trip byte-identically. A rig-driven determinism check is a
        // monitor verify (Phase C).
        DioramaSkinnedSpriteConfig config;
        AZ::Entity* e = aznew AZ::Entity("Skinned");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaSkinnedSpriteComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        DioramaSkinnedSpriteRequestBus::Event(id, &DioramaSkinnedSpriteRequests::SetAnimationSpeed, 2.5f);
        const AZStd::vector<AZ::u8> image = SaveMigrationChunks(id);
        ASSERT_FALSE(image.empty());

        DioramaSkinnedSpriteRequestBus::Event(id, &DioramaSkinnedSpriteRequests::SetAnimationSpeed, 1.0f);
        ASSERT_TRUE(RestoreMigrationChunks(id, image));
        EXPECT_TRUE(SaveMigrationChunks(id) == image);
    }

    TEST_F(SimClockMigrationTest, CharacterStateRoundTripsThroughClockSlotCapture)
    {
        // The REAL rollback path, not a direct SaveSimState: a character enrolled via the
        // Simulation State marker is captured by the clock's SaveToSlot (CaptureFrame walks the
        // registry) and restored by RestoreFromSlot. This is what a rollback game actually runs,
        // and what the direct-chunk round-trips above do not exercise.
        DioramaSkeletalClipConfig config;
        config.m_duration = 1.0f;
        config.m_looping = true;
        config.m_useSimClock = true;
        AZ::Entity* e = aznew AZ::Entity("Fighter");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaSkeletalClipComponent>(config);
        e->CreateComponent<DioramaSimStateComponent>(); // the marker: enrolls this entity in capture
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        // Distinctive state, then capture the whole frame through the clock.
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::SetBlendParam, 0.7f);
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::SaveToSlot, 0);

        // Diverge, then restore through the clock.
        DioramaSkeletalRequestBus::Event(id, &DioramaSkeletalRequests::SetBlendParam, 0.1f);
        bool restored = false;
        DioramaSimClockRequestBus::BroadcastResult(restored, &DioramaSimClockRequests::RestoreFromSlot, 0);
        EXPECT_TRUE(restored);

        // If the clock's capture reached the component (via the marker's enrollment), the blend
        // parameter is back to the saved value; if enrollment is broken, it stays diverged.
        float blend = 0.0f;
        DioramaSkeletalRequestBus::EventResult(blend, id, &DioramaSkeletalRequests::GetBlendParam);
        EXPECT_FLOAT_EQ(blend, 0.7f);
    }

    TEST_F(SimClockMigrationTest, SkinnedSpriteUseSimClockVerbToggles)
    {
        DioramaSkinnedSpriteConfig config;
        config.m_useSimClock = false;
        AZ::Entity* e = aznew AZ::Entity("Skinned");
        e->CreateComponent<MigrationTransformStub>();
        e->CreateComponent<DioramaSkinnedSpriteComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        bool useSim = true;
        DioramaSkinnedSpriteRequestBus::EventResult(useSim, id, &DioramaSkinnedSpriteRequests::GetUseSimClock);
        EXPECT_FALSE(useSim);
        DioramaSkinnedSpriteRequestBus::Event(id, &DioramaSkinnedSpriteRequests::SetUseSimClock, true);
        DioramaSkinnedSpriteRequestBus::EventResult(useSim, id, &DioramaSkinnedSpriteRequests::GetUseSimClock);
        EXPECT_TRUE(useSim);
    }
} // namespace Diorama
