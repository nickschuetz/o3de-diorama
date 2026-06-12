/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaAnimStateMachineBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectAnimStateMachineBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaAnimStateMachineRequestBus>("DioramaAnimStateMachineRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Animation")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetBool",
                &DioramaAnimStateMachineRequestBus::Events::SetBool,
                { { { "name", "Parameter name." }, { "value", "New bool value." } } })
            ->Event(
                "SetFloat",
                &DioramaAnimStateMachineRequestBus::Events::SetFloat,
                { { { "name", "Parameter name." }, { "value", "New float value." } } })
            ->Event(
                "SetTrigger",
                &DioramaAnimStateMachineRequestBus::Events::SetTrigger,
                { { { "name", "Trigger parameter to pulse; fires a matching transition once." } } })
            ->Event(
                "ResetTrigger",
                &DioramaAnimStateMachineRequestBus::Events::ResetTrigger,
                { { { "name", "Trigger parameter to clear before it fires." } } })
            ->Event(
                "Play",
                &DioramaAnimStateMachineRequestBus::Events::Play,
                { { { "stateName", "State to enter immediately, bypassing transitions." } } })
            ->Event(
                "SetUseSimClock",
                &DioramaAnimStateMachineRequestBus::Events::SetUseSimClock,
                { { { "enabled",
                      "Evaluate the graph on the 2D Simulation Clock's fixed steps instead of the render tick; "
                      "with no clock in the level the render tick still evaluates." } } })
            ->Event("GetCurrentState", &DioramaAnimStateMachineRequestBus::Events::GetCurrentState)
            ->Event("GetBool", &DioramaAnimStateMachineRequestBus::Events::GetBool, { { { "name", "Bool/Trigger parameter name." } } })
            ->Event("GetFloat", &DioramaAnimStateMachineRequestBus::Events::GetFloat, { { { "name", "Float parameter name." } } });

        behaviorContext->EBus<DioramaAnimStateMachineNotificationBus>("DioramaAnimStateMachineNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Animation")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaAnimStateMachineNotificationHandler>();
    }
} // namespace Diorama
