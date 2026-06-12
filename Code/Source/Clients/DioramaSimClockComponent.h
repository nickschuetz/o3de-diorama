/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SimClock.h>
#include <Clients/SimRandom.h>
#include <Diorama/DioramaRandomBus.h>
#include <Diorama/DioramaSimClockBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    //! Configuration for the 2D simulation clock. One per level: it turns variable
    //! render time into fixed simulation steps (the deterministic heartbeat), and hosts
    //! the seeded gameplay RNG. Adding the component is the opt-in: without it, nothing
    //! in the gem changes behavior. See Docs/design/2d-deterministic-sim.md.
    class DioramaSimClockConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaSimClockConfig, DioramaSimClockConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaSimClockConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaSimClockConfig() override = default;

        float m_stepsPerSecond = 60.0f; //!< Fixed simulation rate; clamped to [1, 1000].
        int m_maxCatchUpSteps = 5; //!< Max steps run per render frame; backlog beyond is dropped.
        bool m_startPaused = false; //!< Start frozen (advance via StepOnce / SetPaused(false)).
        AZ::u64 m_randomSeed = 0; //!< Initial seed for the gameplay RNG.
    };

    //! Runtime 2D simulation clock. Each render tick it feeds the elapsed time to the
    //! pure SimClock accumulator and broadcasts OnSimTick once per granted fixed step;
    //! gameplay that runs on OnSimTick advances in exact frames, the foundation for
    //! deterministic replays and rollback. Also handles the seeded RNG bus on the pure
    //! SimRandom core. Controlled over DioramaSimClockRequestBus (pause, single-step,
    //! frame readback) with every knob mirrored in the Inspector.
    class DioramaSimClockComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaSimClockRequestBus::Handler
        , protected DioramaRandomRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaSimClockComponent, DioramaSimClockComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        DioramaSimClockComponent() = default;
        explicit DioramaSimClockComponent(const DioramaSimClockConfig& config);
        ~DioramaSimClockComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSimClockRequests
        AZ::s64 GetSimFrame() override;
        void SetPaused(bool paused) override;
        void StepOnce() override;
        void SetStepsPerSecond(float stepsPerSecond) override;
        DioramaSimClockInfo GetSimClockInfo() override;
        void CaptureFrame(AZStd::vector<AZ::u8>& out) override;
        bool RestoreFrame(const AZStd::vector<AZ::u8>& buffer) override;
        AZ::u64 GetStateHash() override;
        void SaveToSlot(int slot) override;
        bool RestoreFromSlot(int slot) override;

        // DioramaRandomRequests
        void SetSeed(AZ::u64 seed) override;
        float RandFloat() override;
        float RandRange(float minValue, float maxValue) override;
        AZ::s64 RandInt(AZ::s64 minValue, AZ::s64 maxValue) override;
        AZ::s64 GetRandomDraws() override;

    private:
        //! Run one fixed step: broadcast OnSimTick with the step's frame number.
        void RunStep();
        //! The fixed step length in seconds (1 / steps per second).
        float StepSeconds() const;

        static constexpr int SlotCount = 8;

        DioramaSimClockConfig m_config;
        SimClock::State m_clock;
        SimRandom::State m_random;
        bool m_paused = false;
        //! Script-facing snapshot slots (SaveToSlot / RestoreFromSlot).
        AZStd::array<AZStd::vector<AZ::u8>, SlotCount> m_slots;
        //! Reused by GetStateHash so steady-state hashing allocates nothing.
        AZStd::vector<AZ::u8> m_hashScratch;
    };
} // namespace Diorama
