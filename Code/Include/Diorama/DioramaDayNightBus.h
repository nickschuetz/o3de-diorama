/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a day/night cycle, returned by GetDayNightInfo. The
    //! verify-loop payload: an agent confirms the time of day and the resolved sun color
    //! / intensity / elevation without a viewport.
    struct DioramaDayNightInfo
    {
        AZ_TYPE_INFO(Diorama::DioramaDayNightInfo, DioramaDayNightInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        float m_timeOfDay = 0.5f; //!< [0,1): 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
        float m_sunR = 1.0f;
        float m_sunG = 1.0f;
        float m_sunB = 1.0f;
        float m_sunIntensity = 1.0f;
        float m_elevationDegrees = 0.0f; //!< Sun height (+90 overhead, 0 horizon, negative = night).
        bool m_isDaytime = true; //!< Sun above the horizon (elevation > 0).
        bool m_paused = false;
    };

    //! Per-entity control of a day/night cycle, addressed by the entity id. The cycle
    //! advances a normalized time-of-day clock and drives a target Diorama light's color,
    //! intensity, and direction over the day (a complementary layer over the existing
    //! lighting, not a new light). Every verb takes plain scalars and is forgiving
    //! (values are validated and wrapped/clamped, never crash). Mirrors the other
    //! Diorama request buses.
    class DioramaDayNightRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaDayNightRequests, DioramaDayNightRequestsTypeId);
        virtual ~DioramaDayNightRequests() = default;

        //! Jump to a normalized time of day; wrapped into [0,1). The target light
        //! re-lights immediately. 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
        virtual void SetTimeOfDay(float timeOfDay) = 0;
        //! Real seconds for a full 24-hour cycle; clamped to be non-negative. 0 freezes
        //! the clock (set the time of day manually).
        virtual void SetCycleSeconds(float cycleSeconds) = 0;
        //! Pause or resume the automatic advance.
        virtual void SetPaused(bool paused) = 0;
        //! Advance the clock by a number of in-game hours (24 = a full cycle); may be
        //! negative to run time backward. Wraps. Re-lights immediately.
        virtual void StepHours(float hours) = 0;
        //! Current normalized time of day.
        virtual float GetTimeOfDay() = 0;
        //! Resolved cycle state. Safe to poll.
        virtual DioramaDayNightInfo GetDayNightInfo() = 0;
    };

    using DioramaDayNightRequestBus = AZ::EBus<DioramaDayNightRequests>;

    //! Reflect the day/night struct and bus to the BehaviorContext (Common scope) so they
    //! are callable from launcher Lua, Python, and Script Canvas. Called from the day/night
    //! component's Reflect.
    void ReflectDayNightBuses(AZ::ReflectContext* context);
} // namespace Diorama
