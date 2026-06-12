/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaDayNightBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaDayNightInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaDayNightInfo>()
                ->Version(1)
                ->Field("timeOfDay", &DioramaDayNightInfo::m_timeOfDay)
                ->Field("sunR", &DioramaDayNightInfo::m_sunR)
                ->Field("sunG", &DioramaDayNightInfo::m_sunG)
                ->Field("sunB", &DioramaDayNightInfo::m_sunB)
                ->Field("sunIntensity", &DioramaDayNightInfo::m_sunIntensity)
                ->Field("elevationDegrees", &DioramaDayNightInfo::m_elevationDegrees)
                ->Field("isDaytime", &DioramaDayNightInfo::m_isDaytime)
                ->Field("paused", &DioramaDayNightInfo::m_paused);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<DioramaDayNightInfo>("DioramaDayNightInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/DayNight")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("timeOfDay", BehaviorValueGetter(&DioramaDayNightInfo::m_timeOfDay), nullptr)
                ->Property("sunR", BehaviorValueGetter(&DioramaDayNightInfo::m_sunR), nullptr)
                ->Property("sunG", BehaviorValueGetter(&DioramaDayNightInfo::m_sunG), nullptr)
                ->Property("sunB", BehaviorValueGetter(&DioramaDayNightInfo::m_sunB), nullptr)
                ->Property("sunIntensity", BehaviorValueGetter(&DioramaDayNightInfo::m_sunIntensity), nullptr)
                ->Property("elevationDegrees", BehaviorValueGetter(&DioramaDayNightInfo::m_elevationDegrees), nullptr)
                ->Property("isDaytime", BehaviorValueGetter(&DioramaDayNightInfo::m_isDaytime), nullptr)
                ->Property("paused", BehaviorValueGetter(&DioramaDayNightInfo::m_paused), nullptr);
        }
    }

    void ReflectDayNightBuses(AZ::ReflectContext* context)
    {
        DioramaDayNightInfo::Reflect(context);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaDayNightRequestBus>("DioramaDayNightRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/DayNight")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetTimeOfDay",
                &DioramaDayNightRequestBus::Events::SetTimeOfDay,
                { { { "timeOfDay", "Normalized time of day (0 midnight, 0.5 noon); wrapped." } } })
            ->Event(
                "SetCycleSeconds",
                &DioramaDayNightRequestBus::Events::SetCycleSeconds,
                { { { "cycleSeconds", "Real seconds for a full 24h cycle (0 freezes)." } } })
            ->Event("SetPaused", &DioramaDayNightRequestBus::Events::SetPaused, { { { "paused", "Pause the automatic advance." } } })
            ->Event(
                "StepHours",
                &DioramaDayNightRequestBus::Events::StepHours,
                { { { "hours", "In-game hours to advance (24 = full cycle; may be negative)." } } })
            ->Event("GetTimeOfDay", &DioramaDayNightRequestBus::Events::GetTimeOfDay)
            ->Event("GetDayNightInfo", &DioramaDayNightRequestBus::Events::GetDayNightInfo);
    }
} // namespace Diorama
