/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaSimClockComponent.h>
#include <Clients/SimState.h>
#include <Clients/SimStateBus.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/sort.h>

namespace Diorama
{
    static float ClampStepsPerSecond(float stepsPerSecond)
    {
        if (stepsPerSecond < 1.0f)
        {
            return 1.0f;
        }
        if (stepsPerSecond > 1000.0f)
        {
            return 1000.0f;
        }
        return stepsPerSecond;
    }

    void DioramaSimClockConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSimClockConfig, AZ::ComponentConfig>()
                ->Version(1)
                ->Field("StepsPerSecond", &DioramaSimClockConfig::m_stepsPerSecond)
                ->Field("MaxCatchUpSteps", &DioramaSimClockConfig::m_maxCatchUpSteps)
                ->Field("StartPaused", &DioramaSimClockConfig::m_startPaused)
                ->Field("RandomSeed", &DioramaSimClockConfig::m_randomSeed);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaSimClockConfig>("2D Simulation Clock Config", "Fixed-step simulation heartbeat")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_stepsPerSecond,
                        "Steps Per Second",
                        "Fixed simulation steps per second (60 is the genre standard); clamped to 1..1000")
                    ->Attribute(AZ::Edit::Attributes::Min, 1.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1000.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_maxCatchUpSteps,
                        "Max Catch-Up Steps",
                        "Most steps run in one render frame after a hitch; the rest of the backlog is dropped")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, 30)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_startPaused,
                        "Start Paused",
                        "Begin frozen; advance with StepOnce or SetPaused(false)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_randomSeed,
                        "Random Seed",
                        "Initial seed for the deterministic gameplay RNG (DioramaRandomRequestBus)");
            }
        }
    }

    void DioramaSimClockComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaSimClockConfig::Reflect(context);
        ReflectSimClockBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSimClockComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaSimClockComponent::m_config);
        }
    }

    void DioramaSimClockComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaSimClockService"));
    }

    void DioramaSimClockComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        // One heartbeat per level: a second clock would double-tick every handler.
        incompatible.push_back(AZ_CRC_CE("DioramaSimClockService"));
    }

    DioramaSimClockComponent::DioramaSimClockComponent(const DioramaSimClockConfig& config)
        : m_config(config)
    {
    }

    void DioramaSimClockComponent::Activate()
    {
        m_config.m_stepsPerSecond = ClampStepsPerSecond(m_config.m_stepsPerSecond);
        m_clock = SimClock::State{};
        m_paused = m_config.m_startPaused;
        SimRandom::Seed(m_random, m_config.m_randomSeed);

        DioramaSimClockRequestBus::Handler::BusConnect();
        DioramaRandomRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void DioramaSimClockComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaRandomRequestBus::Handler::BusDisconnect();
        DioramaSimClockRequestBus::Handler::BusDisconnect();
    }

    void DioramaSimClockComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        if (m_paused)
        {
            return;
        }
        const int steps =
            SimClock::Advance(m_clock, static_cast<double>(deltaTime), static_cast<double>(StepSeconds()), m_config.m_maxCatchUpSteps);
        for (int i = 0; i < steps; ++i)
        {
            // Super-freeze: a whole step's worth of real time still elapses, but the
            // step is not run, so the sim frame holds and no OnSimTick consumer
            // advances. After the countdown, automatic stepping resumes on its own.
            if (m_freezeFramesRemaining > 0)
            {
                --m_freezeFramesRemaining;
                continue;
            }
            RunStep();
        }
    }

    AZ::s64 DioramaSimClockComponent::GetSimFrame()
    {
        return m_clock.m_frame;
    }

    void DioramaSimClockComponent::SetPaused(bool paused)
    {
        m_paused = paused;
    }

    void DioramaSimClockComponent::StepOnce()
    {
        RunStep();
    }

    void DioramaSimClockComponent::SetStepsPerSecond(float stepsPerSecond)
    {
        m_config.m_stepsPerSecond = ClampStepsPerSecond(stepsPerSecond);
    }

    void DioramaSimClockComponent::FreezeFor(AZ::s64 frames)
    {
        m_freezeFramesRemaining = frames > 0 ? frames : 0;
    }

    DioramaSimClockInfo DioramaSimClockComponent::GetSimClockInfo()
    {
        DioramaSimClockInfo info;
        info.m_frame = m_clock.m_frame;
        info.m_stepsPerSecond = m_config.m_stepsPerSecond;
        info.m_paused = m_paused;
        info.m_randomDraws = static_cast<AZ::s64>(m_random.m_draws);
        info.m_freezeFramesRemaining = m_freezeFramesRemaining;
        info.m_frozen = m_freezeFramesRemaining > 0;
        return info;
    }

    void DioramaSimClockComponent::SetSeed(AZ::u64 seed)
    {
        SimRandom::Seed(m_random, seed);
    }

    float DioramaSimClockComponent::RandFloat()
    {
        return SimRandom::Float01(m_random);
    }

    float DioramaSimClockComponent::RandRange(float minValue, float maxValue)
    {
        return SimRandom::Range(m_random, minValue, maxValue);
    }

    AZ::s64 DioramaSimClockComponent::RandInt(AZ::s64 minValue, AZ::s64 maxValue)
    {
        return SimRandom::RangeInt(m_random, minValue, maxValue);
    }

    AZ::s64 DioramaSimClockComponent::GetRandomDraws()
    {
        return static_cast<AZ::s64>(m_random.m_draws);
    }

    void DioramaSimClockComponent::RunStep()
    {
        const AZ::s64 frame = SimClock::Step(m_clock);
        DioramaSimTickNotificationBus::Broadcast(&DioramaSimTickNotifications::OnSimTick, frame, StepSeconds());
    }

    float DioramaSimClockComponent::StepSeconds() const
    {
        return 1.0f / m_config.m_stepsPerSecond;
    }

    // ---- Snapshot / restore (design phase B) -----------------------------------------
    //
    // Frame image layout (all little-endian through SimState::Writer):
    //   u32  magic 'DSS1'
    //   u8   version (2)
    //   s64  clock frame
    //   f64  clock accumulator
    //   u8   paused
    //   u64  rng state
    //   u64  rng draws
    //   f32  steps per second
    //   s64  super-freeze frames remaining (version 2+)
    //   u32  participant count
    //   per participant (sorted by entity id for a canonical, hash-stable image):
    //     u64  entity id
    //     u32  block size (bytes of chunks that follow)
    //     ...  tagged chunks (each component's own payload)

    namespace
    {
        constexpr AZ::u32 FrameMagic = 0x31535344; // 'DSS1'
        constexpr AZ::u8 FrameVersion = 2;
    } // namespace

    void DioramaSimClockComponent::CaptureFrame(AZStd::vector<AZ::u8>& out)
    {
        out.clear();
        SimState::Writer writer(out);
        writer.U32(FrameMagic);
        writer.U8(FrameVersion);
        writer.S64(m_clock.m_frame);
        writer.F64(m_clock.m_accumulator);
        writer.U8(m_paused ? 1 : 0);
        writer.U64(m_random.m_state);
        writer.U64(m_random.m_draws);
        writer.F32(m_config.m_stepsPerSecond);
        writer.S64(m_freezeFramesRemaining);

        // Enumerate enrolled entities and capture in entity-id order, so the image
        // (and therefore the state hash) is canonical regardless of activation order.
        AZStd::vector<AZ::EntityId> participants;
        DioramaSimStateRegistryBus::EnumerateHandlers(
            [&participants](DioramaSimStateRegistry* handler)
            {
                participants.push_back(handler->GetSimStateEntity());
                return true;
            });
        AZStd::sort(participants.begin(), participants.end());

        writer.U32(static_cast<AZ::u32>(participants.size()));
        for (const AZ::EntityId& id : participants)
        {
            writer.U64(static_cast<AZ::u64>(id));
            // Length-prefix the entity's chunk block so restore can skip a missing
            // entity wholesale: write a size placeholder, append the chunks, patch.
            const size_t sizePos = out.size();
            writer.U32(0);
            const size_t blockStart = out.size();
            DioramaSimStateParticipantBus::Event(id, &DioramaSimStateParticipants::SaveSimState, writer);
            const AZ::u32 blockSize = static_cast<AZ::u32>(out.size() - blockStart);
            out[sizePos + 0] = static_cast<AZ::u8>(blockSize);
            out[sizePos + 1] = static_cast<AZ::u8>(blockSize >> 8);
            out[sizePos + 2] = static_cast<AZ::u8>(blockSize >> 16);
            out[sizePos + 3] = static_cast<AZ::u8>(blockSize >> 24);
        }
    }

    bool DioramaSimClockComponent::RestoreFrame(const AZStd::vector<AZ::u8>& buffer)
    {
        SimState::Reader reader(buffer.data(), buffer.size());

        AZ::u32 magic = 0;
        AZ::u8 version = 0;
        if (!reader.U32(magic) || magic != FrameMagic || !reader.U8(version) || version != FrameVersion)
        {
            return false;
        }

        // Parse the clock state into locals first: nothing is applied unless the
        // whole header parses (no partial clock state from a truncated buffer).
        AZ::s64 frame = 0;
        double accumulator = 0.0;
        AZ::u8 paused = 0;
        AZ::u64 rngState = 0;
        AZ::u64 rngDraws = 0;
        float stepsPerSecond = 0.0f;
        AZ::s64 freezeFramesRemaining = 0;
        AZ::u32 participantCount = 0;
        if (!reader.S64(frame) || !reader.F64(accumulator) || !reader.U8(paused) || !reader.U64(rngState) || !reader.U64(rngDraws) ||
            !reader.F32(stepsPerSecond) || !reader.S64(freezeFramesRemaining) || !reader.U32(participantCount))
        {
            return false;
        }
        if (rngState == 0 || !(stepsPerSecond >= 1.0f && stepsPerSecond <= 1000.0f) || freezeFramesRemaining < 0)
        {
            return false; // invalid by construction: never produced by CaptureFrame
        }

        m_clock.m_frame = frame;
        m_clock.m_accumulator = accumulator;
        m_paused = paused != 0;
        m_random.m_state = rngState;
        m_random.m_draws = rngDraws;
        m_config.m_stepsPerSecond = stepsPerSecond;
        m_freezeFramesRemaining = freezeFramesRemaining;

        // Apply participant blocks. An entity that no longer exists (no handler) has
        // its block skipped; an unrecognized chunk within a block is skipped too, so
        // newer images degrade gracefully on older component sets.
        for (AZ::u32 p = 0; p < participantCount; ++p)
        {
            AZ::u64 rawId = 0;
            AZ::u32 blockSize = 0;
            if (!reader.U64(rawId) || !reader.U32(blockSize) || blockSize > reader.Remaining())
            {
                return false;
            }
            const AZ::EntityId entityId(rawId);
            SimState::Reader block = reader.SubReader(blockSize);
            while (block.Ok() && block.Remaining() > 0)
            {
                AZ::u32 tag = 0;
                AZ::u32 payloadSize = 0;
                if (!block.ChunkHeader(tag, payloadSize))
                {
                    return false;
                }
                SimState::Reader payload = block.SubReader(payloadSize);
                bool consumed = false;
                DioramaSimStateParticipantBus::EnumerateHandlersId(
                    entityId,
                    [&consumed, tag, &payload](DioramaSimStateParticipants* handler)
                    {
                        // Offer until one component takes the chunk; a later handler
                        // never sees a consumed payload.
                        consumed = handler->TryRestoreChunk(tag, payload);
                        return !consumed;
                    });
            }
        }
        return reader.Ok();
    }

    AZ::u64 DioramaSimClockComponent::GetStateHash()
    {
        CaptureFrame(m_hashScratch);
        return SimState::Fnv1a64(m_hashScratch.data(), m_hashScratch.size());
    }

    void DioramaSimClockComponent::SaveToSlot(int slot)
    {
        if (slot < 0 || slot >= SlotCount)
        {
            return;
        }
        CaptureFrame(m_slots[slot]);
    }

    bool DioramaSimClockComponent::RestoreFromSlot(int slot)
    {
        if (slot < 0 || slot >= SlotCount || m_slots[slot].empty())
        {
            return false;
        }
        return RestoreFrame(m_slots[slot]);
    }
} // namespace Diorama
