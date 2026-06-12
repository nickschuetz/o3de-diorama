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
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Typed, agent-facing API for the 2D animation state machine, addressed by its
    //! entity. Reflected Common, so a script, Script Canvas, or an agent drives a
    //! character's animation graph the same way it drives sprites, the HUD, and audio:
    //! push parameters (speed, grounded, a "jump" trigger) and the graph picks the
    //! state, which plays the bound Sprite sheet or Aseprite tag on the target entity.
    //! Parameters are addressed by name (the same names authored in the editor); an
    //! unknown name is ignored rather than an error, so callers stay forgiving.
    class DioramaAnimStateMachineRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaAnimStateMachineRequests, DioramaAnimStateMachineRequestsTypeId);
        virtual ~DioramaAnimStateMachineRequests() = default;

        //! Set a Bool parameter (e.g. "grounded"). Unknown name -> ignored.
        virtual void SetBool(const AZStd::string& name, bool value) = 0;
        //! Set a Float parameter (e.g. "speed"). Unknown name -> ignored.
        virtual void SetFloat(const AZStd::string& name, float value) = 0;
        //! Pulse a Trigger parameter (e.g. "jump"): fires a matching transition once on
        //! the next evaluation, then auto-clears. Unknown name -> ignored.
        virtual void SetTrigger(const AZStd::string& name) = 0;
        //! Clear a pending Trigger before it fires. Unknown name -> ignored.
        virtual void ResetTrigger(const AZStd::string& name) = 0;

        //! Force the graph into a named state immediately (bypassing transitions) and
        //! play its animation. Unknown name -> no change.
        virtual void Play(const AZStd::string& stateName) = 0;

        //! Evaluate the graph on the 2D Simulation Clock's fixed steps instead of the
        //! render tick (deterministic stepping, snapshot/rewind friendly). With no
        //! clock in the level the render tick still evaluates as before.
        virtual void SetUseSimClock(bool enabled) = 0;

        //! Name of the current state (empty if not yet entered). Safe to poll.
        virtual AZStd::string GetCurrentState() = 0;
        //! Current value of a Bool/Trigger parameter (false if unknown).
        virtual bool GetBool(const AZStd::string& name) = 0;
        //! Current value of a Float parameter (0 if unknown).
        virtual float GetFloat(const AZStd::string& name) = 0;
    };

    using DioramaAnimStateMachineRequestBus = AZ::EBus<DioramaAnimStateMachineRequests>;

    //! Notifications for event-driven agents/games that prefer subscribing over polling
    //! GetCurrentState. Addressed by the state machine entity's id. These fire whenever
    //! the graph changes state (from a transition or an explicit Play), so logic can be
    //! tied to a state without rendering: spawn a hitbox on entering "attack", play a
    //! footstep on entering "run", etc.
    class DioramaAnimStateMachineNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaAnimStateMachineNotifications, DioramaAnimStateMachineNotificationsTypeId);
        virtual ~DioramaAnimStateMachineNotifications() = default;

        //! The graph entered a new state. fromState is empty for the initial state.
        virtual void OnStateChanged(const AZStd::string& fromState, const AZStd::string& toState)
        {
            AZ_UNUSED(fromState);
            AZ_UNUSED(toState);
        }
    };

    using DioramaAnimStateMachineNotificationBus = AZ::EBus<DioramaAnimStateMachineNotifications>;

    //! BehaviorContext handler so scripts/agents can subscribe to state changes rather
    //! than poll GetCurrentState.
    class DioramaAnimStateMachineNotificationHandler
        : public DioramaAnimStateMachineNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            DioramaAnimStateMachineNotificationHandler, "{5D6C7B8A-C9DA-45E6-F708-192A3B4C5D0C}", AZ::SystemAllocator, OnStateChanged);

        void OnStateChanged(const AZStd::string& fromState, const AZStd::string& toState) override
        {
            Call(FN_OnStateChanged, fromState, toState);
        }
    };

    //! Reflect the animation state machine buses (request + notification) to the
    //! BehaviorContext (Common scope). Called from the component's Reflect.
    void ReflectAnimStateMachineBuses(AZ::ReflectContext* context);
} // namespace Diorama
