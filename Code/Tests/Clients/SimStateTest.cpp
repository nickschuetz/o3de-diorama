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
#include <Clients/DioramaHitboxComponent.h>
#include <Clients/DioramaSimClockComponent.h>
#include <Clients/DioramaSimStateComponent.h>
#include <Clients/SimState.h>
#include <Clients/SimStateBus.h>

namespace Diorama
{
    // ---- Pure writer/reader ----------------------------------------------------------

    TEST(SimStateTest, RoundTripAllTypes)
    {
        AZStd::vector<AZ::u8> buf;
        SimState::Writer w(buf);
        w.U8(0xAB);
        w.U32(0xDEADBEEF);
        w.U64(0x0123456789ABCDEFull);
        w.S64(-42);
        w.F32(3.5f);
        w.F64(-0.125);

        SimState::Reader r(buf.data(), buf.size());
        AZ::u8 u8v = 0;
        AZ::u32 u32v = 0;
        AZ::u64 u64v = 0;
        AZ::s64 s64v = 0;
        float f32v = 0.0f;
        double f64v = 0.0;
        EXPECT_TRUE(r.U8(u8v));
        EXPECT_TRUE(r.U32(u32v));
        EXPECT_TRUE(r.U64(u64v));
        EXPECT_TRUE(r.S64(s64v));
        EXPECT_TRUE(r.F32(f32v));
        EXPECT_TRUE(r.F64(f64v));
        EXPECT_EQ(u8v, 0xAB);
        EXPECT_EQ(u32v, 0xDEADBEEFu);
        EXPECT_EQ(u64v, 0x0123456789ABCDEFull);
        EXPECT_EQ(s64v, -42);
        EXPECT_FLOAT_EQ(f32v, 3.5f);
        EXPECT_DOUBLE_EQ(f64v, -0.125);
        EXPECT_TRUE(r.Ok());
        EXPECT_EQ(r.Remaining(), 0u);
    }

    TEST(SimStateTest, LittleEndianOnTheWire)
    {
        AZStd::vector<AZ::u8> buf;
        SimState::Writer w(buf);
        w.U32(0x11223344);
        ASSERT_EQ(buf.size(), 4u);
        EXPECT_EQ(buf[0], 0x44); // least-significant byte first, every platform
        EXPECT_EQ(buf[1], 0x33);
        EXPECT_EQ(buf[2], 0x22);
        EXPECT_EQ(buf[3], 0x11);
    }

    TEST(SimStateTest, TruncatedReadFailsAndLatches)
    {
        AZStd::vector<AZ::u8> buf;
        SimState::Writer w(buf);
        w.U32(7);

        SimState::Reader r(buf.data(), buf.size());
        AZ::u64 v = 0;
        EXPECT_FALSE(r.U64(v)); // only 4 bytes present
        EXPECT_FALSE(r.Ok());
        AZ::u8 b = 0;
        EXPECT_FALSE(r.U8(b)); // failure latches: nothing reads after
    }

    TEST(SimStateTest, ChunkHeaderRejectsHostileSize)
    {
        AZStd::vector<AZ::u8> buf;
        SimState::Writer w(buf);
        w.U32(0x534E5254); // tag
        w.U32(1000); // claims 1000 payload bytes; none follow

        SimState::Reader r(buf.data(), buf.size());
        AZ::u32 tag = 0;
        AZ::u32 size = 0;
        EXPECT_FALSE(r.ChunkHeader(tag, size));
        EXPECT_FALSE(r.Ok());
    }

    TEST(SimStateTest, ChunkRoundTripAndSkip)
    {
        AZStd::vector<AZ::u8> buf;
        SimState::Writer w(buf);
        const size_t a = w.BeginChunk(111);
        w.F32(1.0f);
        w.EndChunk(a);
        const size_t b = w.BeginChunk(222);
        w.U64(9);
        w.EndChunk(b);

        SimState::Reader r(buf.data(), buf.size());
        AZ::u32 tag = 0;
        AZ::u32 size = 0;
        ASSERT_TRUE(r.ChunkHeader(tag, size));
        EXPECT_EQ(tag, 111u);
        EXPECT_EQ(size, 4u);
        EXPECT_TRUE(r.Skip(size)); // skip an unrecognized chunk wholesale
        ASSERT_TRUE(r.ChunkHeader(tag, size));
        EXPECT_EQ(tag, 222u);
        SimState::Reader payload = r.SubReader(size);
        AZ::u64 v = 0;
        EXPECT_TRUE(payload.U64(v));
        EXPECT_EQ(v, 9u);
        EXPECT_EQ(r.Remaining(), 0u);
    }

    TEST(SimStateTest, HashIsStableAndInputSensitive)
    {
        const AZ::u8 data[] = { 1, 2, 3, 4, 5 };
        const AZ::u64 h1 = SimState::Fnv1a64(data, sizeof(data));
        const AZ::u64 h2 = SimState::Fnv1a64(data, sizeof(data));
        EXPECT_EQ(h1, h2);
        const AZ::u8 tweaked[] = { 1, 2, 3, 4, 6 };
        EXPECT_NE(h1, SimState::Fnv1a64(tweaked, sizeof(tweaked)));
        // Known FNV-1a 64 vector: empty input hashes to the offset basis.
        EXPECT_EQ(SimState::Fnv1a64(nullptr, 0), 0xCBF29CE484222325ull);
    }

    // ---- Clock component capture / restore --------------------------------------------

    class SimClockSnapshotTest : public ::testing::Test
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
            m_app.RegisterComponentDescriptor(DioramaSimStateComponent::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();

            DioramaSimClockConfig config;
            config.m_randomSeed = 1234;
            m_clockEntity = aznew AZ::Entity("SimClock");
            m_clockEntity->CreateComponent<DioramaSimClockComponent>(config);
            m_clockEntity->Init();
            m_clockEntity->Activate();

            m_participant = aznew AZ::Entity("Participant");
            m_participant->CreateComponent<DioramaSimStateComponent>();
            m_participant->Init();
            m_participant->Activate();
        }

        void TearDown() override
        {
            m_participant->Deactivate();
            delete m_participant;
            m_clockEntity->Deactivate();
            delete m_clockEntity;
            m_app.Destroy();
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_clockEntity = nullptr;
        AZ::Entity* m_participant = nullptr;
    };

    TEST_F(SimClockSnapshotTest, CaptureRestoreRoundTripsClockAndRng)
    {
        // Advance into a distinctive state: steps + draws.
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        float r1 = 0.0f;
        DioramaRandomRequestBus::BroadcastResult(r1, &DioramaRandomRequests::RandFloat);

        AZ::u64 savedHash = 0;
        DioramaSimClockRequestBus::BroadcastResult(savedHash, &DioramaSimClockRequests::GetStateHash);
        AZStd::vector<AZ::u8> image;
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::CaptureFrame, image);
        ASSERT_FALSE(image.empty());

        // Diverge: more steps, more draws. The hash must change.
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        float r2 = 0.0f;
        DioramaRandomRequestBus::BroadcastResult(r2, &DioramaRandomRequests::RandFloat);
        AZ::u64 divergedHash = 0;
        DioramaSimClockRequestBus::BroadcastResult(divergedHash, &DioramaSimClockRequests::GetStateHash);
        EXPECT_NE(divergedHash, savedHash);

        // Restore: frame, draws, and the RNG stream position all return.
        bool restored = false;
        DioramaSimClockRequestBus::BroadcastResult(restored, &DioramaSimClockRequests::RestoreFrame, image);
        ASSERT_TRUE(restored);
        AZ::u64 restoredHash = 0;
        DioramaSimClockRequestBus::BroadcastResult(restoredHash, &DioramaSimClockRequests::GetStateHash);
        EXPECT_EQ(restoredHash, savedHash);

        DioramaSimClockInfo info;
        DioramaSimClockRequestBus::BroadcastResult(info, &DioramaSimClockRequests::GetSimClockInfo);
        EXPECT_EQ(info.m_frame, 2);
        EXPECT_EQ(info.m_randomDraws, 1);

        // The next draw replays the diverged run's draw exactly (same stream position).
        float r2again = 0.0f;
        DioramaRandomRequestBus::BroadcastResult(r2again, &DioramaRandomRequests::RandFloat);
        EXPECT_FLOAT_EQ(r2again, r2);
    }

    TEST_F(SimClockSnapshotTest, SlotsSaveAndRestore)
    {
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        AZ::u64 atFrame1 = 0;
        DioramaSimClockRequestBus::BroadcastResult(atFrame1, &DioramaSimClockRequests::GetStateHash);
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::SaveToSlot, 0);

        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        bool ok = false;
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFromSlot, 0);
        EXPECT_TRUE(ok);
        AZ::u64 back = 0;
        DioramaSimClockRequestBus::BroadcastResult(back, &DioramaSimClockRequests::GetStateHash);
        EXPECT_EQ(back, atFrame1);

        // Empty and out-of-range slots refuse cleanly.
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFromSlot, 5);
        EXPECT_FALSE(ok);
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFromSlot, 99);
        EXPECT_FALSE(ok);
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFromSlot, -1);
        EXPECT_FALSE(ok);
    }

    TEST_F(SimClockSnapshotTest, MalformedBuffersAreRejected)
    {
        AZStd::vector<AZ::u8> image;
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::CaptureFrame, image);
        ASSERT_FALSE(image.empty());

        bool ok = true;

        // Wrong magic.
        AZStd::vector<AZ::u8> badMagic = image;
        badMagic[0] ^= 0xFF;
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFrame, badMagic);
        EXPECT_FALSE(ok);

        // Truncated header.
        AZStd::vector<AZ::u8> truncated(image.begin(), image.begin() + 8);
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFrame, truncated);
        EXPECT_FALSE(ok);

        // Empty.
        AZStd::vector<AZ::u8> empty;
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFrame, empty);
        EXPECT_FALSE(ok);

        // A clean image still restores after the rejected attempts.
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFrame, image);
        EXPECT_TRUE(ok);
    }

    TEST_F(SimClockSnapshotTest, ParticipantBlockIsCanonicalAndSkippable)
    {
        // The participant entity contributes a translation chunk; the image parses
        // even though no TransformBus handler exists in this harness (restore offers
        // the chunk and the marker consumes it; SetWorldTranslation is a no-op here).
        AZStd::vector<AZ::u8> image;
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::CaptureFrame, image);

        // Two captures in the same state are byte-identical (canonical image).
        AZStd::vector<AZ::u8> again;
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::CaptureFrame, again);
        EXPECT_TRUE(image == again);

        bool ok = false;
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFrame, image);
        EXPECT_TRUE(ok);

        // Deactivate the participant: its block in an old image must be skipped
        // gracefully, not fail the restore.
        m_participant->Deactivate();
        DioramaSimClockRequestBus::BroadcastResult(ok, &DioramaSimClockRequests::RestoreFrame, image);
        EXPECT_TRUE(ok);
        m_participant->Activate(); // TearDown deactivates again
    }

    // ---- Component participants (phase B2): chunk round trips through the bus --------

    namespace
    {
        //! Provides TransformService so components requiring it activate on a bare
        //! entity (identity transform; positions are irrelevant to these chunks).
        class SimStateTransformStub : public AZ::Component
        {
        public:
            AZ_COMPONENT(SimStateTransformStub, "{50EEB3BB-9B5F-4EF0-8698-F982DEFF6227}", AZ::Component);
            static void Reflect(AZ::ReflectContext* context)
            {
                if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
                {
                    sc->Class<SimStateTransformStub, AZ::Component>()->Version(1);
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
        AZStd::vector<AZ::u8> SaveEntityChunks(AZ::EntityId id)
        {
            AZStd::vector<AZ::u8> buf;
            SimState::Writer writer(buf);
            DioramaSimStateParticipantBus::Event(id, &DioramaSimStateParticipants::SaveSimState, writer);
            return buf;
        }

        //! Offer every chunk in a captured buffer back to the entity's participants.
        bool RestoreEntityChunks(AZ::EntityId id, const AZStd::vector<AZ::u8>& buf)
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

    class SimStateParticipantTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            m_app.RegisterComponentDescriptor(SimStateTransformStub::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaHitboxComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(DioramaBulletEmitterComponent::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();
        }

        void TearDown() override
        {
            for (AZ::Entity* e : m_entities)
            {
                e->Deactivate();
                delete e;
            }
            m_entities.clear();
            m_app.Destroy();
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZStd::vector<AZ::Entity*> m_entities;
    };

    TEST_F(SimStateParticipantTest, HitboxChunkRoundTripsAndIsCanonical)
    {
        DioramaHitboxConfig config;
        config.m_boxes.resize(2); // two default boxes: per-box state must round trip
        AZ::Entity* e = aznew AZ::Entity("Fighter");
        e->CreateComponent<SimStateTransformStub>();
        e->CreateComponent<DioramaHitboxComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        DioramaHitboxRequestBus::Event(id, &DioramaHitboxRequests::SetFrame, 7);
        DioramaHitboxRequestBus::Event(id, &DioramaHitboxRequests::SetFacing, -1);
        const AZStd::vector<AZ::u8> image = SaveEntityChunks(id);
        ASSERT_FALSE(image.empty());

        // Diverge, then restore: the readback returns to the saved state.
        DioramaHitboxRequestBus::Event(id, &DioramaHitboxRequests::SetFrame, 2);
        DioramaHitboxRequestBus::Event(id, &DioramaHitboxRequests::SetFacing, 1);
        ASSERT_TRUE(RestoreEntityChunks(id, image));

        DioramaHitboxInfo info;
        DioramaHitboxRequestBus::EventResult(info, id, &DioramaHitboxRequests::GetHitboxInfo);
        EXPECT_EQ(info.m_frame, 7);
        EXPECT_EQ(info.m_facing, -1);

        // Canonical: re-saving the restored state is byte-identical.
        EXPECT_TRUE(SaveEntityChunks(id) == image);
    }

    TEST_F(SimStateParticipantTest, BulletPoolChunkRoundTripsAndIsCanonical)
    {
        DioramaBulletConfig config;
        config.m_count = 4;
        config.m_fireOnActivate = false;
        config.m_fireRate = 0.0f;
        AZ::Entity* e = aznew AZ::Entity("Emitter");
        e->CreateComponent<SimStateTransformStub>();
        auto* emitter = e->CreateComponent<DioramaBulletEmitterComponent>(config);
        e->Init();
        e->Activate();
        m_entities.push_back(e);
        const AZ::EntityId id = e->GetId();

        emitter->EmitShot();
        emitter->StepBullets(0.1f, false);
        DioramaBulletInfo before;
        DioramaBulletRequestBus::EventResult(before, id, &DioramaBulletRequests::GetBulletInfo);
        ASSERT_EQ(before.m_aliveCount, 4);
        const AZStd::vector<AZ::u8> image = SaveEntityChunks(id);
        ASSERT_FALSE(image.empty());

        // Diverge: another volley plus aging, then restore the original field.
        emitter->EmitShot();
        emitter->StepBullets(0.5f, false);
        ASSERT_TRUE(RestoreEntityChunks(id, image));

        DioramaBulletInfo after;
        DioramaBulletRequestBus::EventResult(after, id, &DioramaBulletRequests::GetBulletInfo);
        EXPECT_EQ(after.m_aliveCount, 4);

        // Canonical: the restored pool re-saves byte-identical (positions, ages, and
        // fire state all round-tripped exactly).
        EXPECT_TRUE(SaveEntityChunks(id) == image);

        // A hostile bullet count is rejected (count field claims more than the pool).
        AZStd::vector<AZ::u8> hostile = image;
        // Chunk layout: tag(4) + size(4) + spiral(4) + accumulator(4) + firing(1) -> count at offset 17.
        hostile[17] = 0xFF;
        hostile[18] = 0xFF;
        EXPECT_FALSE(RestoreEntityChunks(id, hostile));
    }
} // namespace Diorama
