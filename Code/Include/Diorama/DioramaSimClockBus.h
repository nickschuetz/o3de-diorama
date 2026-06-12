/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of the 2D simulation clock, returned by GetSimClockInfo.
    //! The verify-loop payload: an agent or test confirms the sim frame, rate, and
    //! pause state without a viewport.
    struct DioramaSimClockInfo
    {
        AZ_TYPE_INFO(Diorama::DioramaSimClockInfo, DioramaSimClockInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZ::s64 m_frame = 0; //!< Simulation frames run since activation.
        float m_stepsPerSecond = 60.0f;
        bool m_paused = false;
        AZ::s64 m_randomDraws = 0; //!< Values drawn from the seeded RNG since its last seed.
    };

    //! Global (broadcast) control of the level's 2D simulation clock; handled by the
    //! single 2D Simulation Clock component (one per level). The clock turns variable
    //! render time into fixed simulation steps and emits OnSimTick once per step, which
    //! is the deterministic heartbeat gameplay scripts and (in later phases) the
    //! sim-stateful Diorama components advance on. See
    //! Docs/design/2d-deterministic-sim.md.
    class DioramaSimClockRequests : public AZ::EBusTraits
    {
    public:
        AZ_RTTI(DioramaSimClockRequests, DioramaSimClockRequestsTypeId);
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~DioramaSimClockRequests() = default;

        //! Current simulation frame (monotonic, counts every step run). -1 is never
        //! returned; before the first step it is 0.
        virtual AZ::s64 GetSimFrame() = 0;
        //! Pause or resume the automatic stepping. While paused the clock holds and
        //! StepOnce is the only way to advance (training-mode freeze / frame advance).
        virtual void SetPaused(bool paused) = 0;
        //! Run exactly one simulation step now (works paused or running). This is the
        //! frame-advance primitive: training-mode single step, and re-simulation after
        //! a state restore (rollback).
        virtual void StepOnce() = 0;
        //! Fixed steps per second; clamped to [1, 1000]. Changing the rate mid-game
        //! changes what the same wall-clock time simulates to, so deterministic games
        //! set it once at startup.
        virtual void SetStepsPerSecond(float stepsPerSecond) = 0;
        //! Resolved clock state. Safe to poll.
        virtual DioramaSimClockInfo GetSimClockInfo() = 0;
    };

    using DioramaSimClockRequestBus = AZ::EBus<DioramaSimClockRequests>;

    //! Broadcast once per fixed simulation step. Handlers advance their simulation by
    //! exactly one fixed step per call; `frame` is the step's number (monotonic from 0).
    //! Handler invocation order follows connect order, which is scene-deterministic for
    //! the same build and level; handlers must not depend on running before or after
    //! another handler.
    class DioramaSimTickNotifications : public AZ::EBusTraits
    {
    public:
        AZ_RTTI(DioramaSimTickNotifications, DioramaSimTickNotificationsTypeId);
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~DioramaSimTickNotifications() = default;

        //! One fixed simulation step. stepSeconds is the fixed delta (1 / steps per
        //! second), the same every call until the rate is changed.
        virtual void OnSimTick(AZ::s64 frame, float stepSeconds)
        {
            AZ_UNUSED(frame);
            AZ_UNUSED(stepSeconds);
        }
    };

    using DioramaSimTickNotificationBus = AZ::EBus<DioramaSimTickNotifications>;

    //! BehaviorContext handler so gameplay Lua / Script Canvas / agents can run their
    //! per-step logic on the deterministic heartbeat instead of the render tick.
    class DioramaSimTickNotificationHandler
        : public DioramaSimTickNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            DioramaSimTickNotificationHandler, "{98D8FE9F-C8A1-4858-9073-DEF04AC18B0A}", AZ::SystemAllocator, OnSimTick);

        void OnSimTick(AZ::s64 frame, float stepSeconds) override
        {
            Call(FN_OnSimTick, frame, stepSeconds);
        }
    };

    //! Reflect the sim clock structs and buses to the BehaviorContext (Common scope).
    //! Called from the sim clock component's Reflect.
    void ReflectSimClockBuses(AZ::ReflectContext* context);
} // namespace Diorama
