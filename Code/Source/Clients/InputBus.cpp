/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaInputBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectInputBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaInputRequestBus>("DioramaInputRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Input")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("IsPressed", &DioramaInputRequestBus::Events::IsPressed, { { { "action", "Action name." } } })
            ->Event("WasPressedThisFrame", &DioramaInputRequestBus::Events::WasPressedThisFrame, { { { "action", "Action name." } } })
            ->Event("WasReleasedThisFrame", &DioramaInputRequestBus::Events::WasReleasedThisFrame, { { { "action", "Action name." } } })
            ->Event("GetValue", &DioramaInputRequestBus::Events::GetValue, { { { "action", "Action name." } } })
            ->Event("GetValueY", &DioramaInputRequestBus::Events::GetValueY, { { { "action", "Action name." } } })
            ->Event(
                "WasMotionPerformed",
                &DioramaInputRequestBus::Events::WasMotionPerformed,
                { { { "motion", "Motion name authored on the component." } } })
            ->Event(
                "WasPressedAtFrame",
                &DioramaInputRequestBus::Events::WasPressedAtFrame,
                { { { "action", "Action name." }, { "frame", "Simulation frame (sim-clock mode)." } } })
            ->Event(
                "GetValueAtFrame",
                &DioramaInputRequestBus::Events::GetValueAtFrame,
                { { { "action", "Action name." }, { "frame", "Simulation frame (sim-clock mode)." } } })
            ->Event(
                "GetValueYAtFrame",
                &DioramaInputRequestBus::Events::GetValueYAtFrame,
                { { { "action", "Action name." }, { "frame", "Simulation frame (sim-clock mode)." } } })
            ->Event(
                "InjectActionState",
                &DioramaInputRequestBus::Events::InjectActionState,
                { { { "frame", "Simulation frame to overwrite (current or future)." },
                    { "action", "Action name." },
                    { "x", "Primary value (Button 0..1, axis X)." },
                    { "y", "Y value (Axis2D)." },
                    { "pressed", "Held state; edges derive against the prior frame." } } });

        behaviorContext->EBus<DioramaInputNotificationBus>("DioramaInputNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Input")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaInputNotificationHandler>();
    }
} // namespace Diorama
