/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaRandomBus.h>
#include <Diorama/DioramaSimClockBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaSimClockInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSimClockInfo>()
                ->Version(2)
                ->Field("frame", &DioramaSimClockInfo::m_frame)
                ->Field("stepsPerSecond", &DioramaSimClockInfo::m_stepsPerSecond)
                ->Field("paused", &DioramaSimClockInfo::m_paused)
                ->Field("randomDraws", &DioramaSimClockInfo::m_randomDraws)
                ->Field("frozen", &DioramaSimClockInfo::m_frozen)
                ->Field("freezeFramesRemaining", &DioramaSimClockInfo::m_freezeFramesRemaining);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<DioramaSimClockInfo>("DioramaSimClockInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Simulation")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("frame", BehaviorValueGetter(&DioramaSimClockInfo::m_frame), nullptr)
                ->Property("stepsPerSecond", BehaviorValueGetter(&DioramaSimClockInfo::m_stepsPerSecond), nullptr)
                ->Property("paused", BehaviorValueGetter(&DioramaSimClockInfo::m_paused), nullptr)
                ->Property("randomDraws", BehaviorValueGetter(&DioramaSimClockInfo::m_randomDraws), nullptr)
                ->Property("frozen", BehaviorValueGetter(&DioramaSimClockInfo::m_frozen), nullptr)
                ->Property("freezeFramesRemaining", BehaviorValueGetter(&DioramaSimClockInfo::m_freezeFramesRemaining), nullptr);
        }
    }

    void ReflectSimClockBuses(AZ::ReflectContext* context)
    {
        DioramaSimClockInfo::Reflect(context);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaSimClockRequestBus>("DioramaSimClockRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Simulation")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("GetSimFrame", &DioramaSimClockRequestBus::Events::GetSimFrame)
            ->Event("SetPaused", &DioramaSimClockRequestBus::Events::SetPaused, { { { "paused", "Freeze the fixed stepping." } } })
            ->Event("StepOnce", &DioramaSimClockRequestBus::Events::StepOnce)
            ->Event(
                "SetStepsPerSecond",
                &DioramaSimClockRequestBus::Events::SetStepsPerSecond,
                { { { "stepsPerSecond", "Fixed steps per second; clamped to 1..1000." } } })
            ->Event(
                "FreezeFor",
                &DioramaSimClockRequestBus::Events::FreezeFor,
                { { { "frames",
                      "Suppress automatic stepping for this many fixed steps, then resume (the super-flash pause); "
                      "0 or less clears an active freeze. Drive the attacker's animation on the render tick to keep "
                      "it moving through the freeze." } } })
            ->Event("GetSimClockInfo", &DioramaSimClockRequestBus::Events::GetSimClockInfo)
            // Snapshot surface for scripts/agents: slots + the determinism hash. The
            // raw CaptureFrame/RestoreFrame buffer verbs are C++-facing (a rollback
            // layer owns its own frame history) and are intentionally not reflected.
            ->Event("GetStateHash", &DioramaSimClockRequestBus::Events::GetStateHash)
            ->Event("SaveToSlot", &DioramaSimClockRequestBus::Events::SaveToSlot, { { { "slot", "Snapshot slot index (0..7)." } } })
            ->Event(
                "RestoreFromSlot",
                &DioramaSimClockRequestBus::Events::RestoreFromSlot,
                { { { "slot", "Snapshot slot index (0..7); false if empty." } } });

        behaviorContext->EBus<DioramaSimTickNotificationBus>("DioramaSimTickNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Simulation")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaSimTickNotificationHandler>();

        behaviorContext->EBus<DioramaRandomRequestBus>("DioramaRandomRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Simulation")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("SetSeed", &DioramaRandomRequestBus::Events::SetSeed, { { { "seed", "Any value; resets the sequence." } } })
            ->Event("RandFloat", &DioramaRandomRequestBus::Events::RandFloat)
            ->Event(
                "RandRange",
                &DioramaRandomRequestBus::Events::RandRange,
                { { { "minValue", "Inclusive lower bound." }, { "maxValue", "Exclusive upper bound." } } })
            ->Event(
                "RandInt",
                &DioramaRandomRequestBus::Events::RandInt,
                { { { "minValue", "Inclusive lower bound." }, { "maxValue", "Inclusive upper bound." } } })
            ->Event("GetRandomDraws", &DioramaRandomRequestBus::Events::GetRandomDraws);
    }
} // namespace Diorama
