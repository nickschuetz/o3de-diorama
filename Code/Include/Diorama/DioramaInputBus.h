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
    //! Typed, agent-facing API for the input action-mapping surface, addressed by its
    //! entity. Reflected Common, so a script, Script Canvas, or an agent reads named
    //! gameplay actions ("move", "jump") the same way it drives every other Diorama
    //! feature, instead of wiring raw input channels. Actions are addressed by the names
    //! authored in the editor; an unknown name reads as zero / not pressed.
    class DioramaInputRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaInputRequests, DioramaInputRequestsTypeId);
        virtual ~DioramaInputRequests() = default;

        //! True while the action is held (a Button past its threshold, or an axis past it).
        virtual bool IsPressed(const AZStd::string& action) = 0;
        //! True only on the frame the action went from released to pressed.
        virtual bool WasPressedThisFrame(const AZStd::string& action) = 0;
        //! True only on the frame the action went from pressed to released.
        virtual bool WasReleasedThisFrame(const AZStd::string& action) = 0;
        //! The action's primary value: a Button's 0..1, an Axis1D's [-1,1], or an
        //! Axis2D's X. 0 for an unknown action.
        virtual float GetValue(const AZStd::string& action) = 0;
        //! The Y component of an Axis2D action (0 otherwise).
        virtual float GetValueY(const AZStd::string& action) = 0;
        //! True while a named motion (quarter-circle, dragon-punch) is recognized in the
        //! recent direction history. Stays true through the motion's buffer window, so
        //! gameplay gates it with a button edge, e.g. WasMotionPerformed("qcf") &&
        //! WasPressedThisFrame("punch"). Unknown motion names read false.
        virtual bool WasMotionPerformed(const AZStd::string& motion) = 0;

        // Per-sim-frame input history (deterministic sim phase C). Available when the
        // component's Use Simulation Clock mode is on: input is sampled once per fixed
        // simulation step into a ring of recent frames, so the same frames replay to
        // the same actions. Out-of-window frames (older than the ring, or not yet
        // simulated) read as zero / not pressed.

        //! Was the action pressed on a specific simulation frame.
        virtual bool WasPressedAtFrame(const AZStd::string& action, AZ::s64 frame) = 0;
        //! The action's primary value (X) on a specific simulation frame.
        virtual float GetValueAtFrame(const AZStd::string& action, AZ::s64 frame) = 0;
        //! The action's Y value on a specific simulation frame.
        virtual float GetValueYAtFrame(const AZStd::string& action, AZ::s64 frame) = 0;
        //! Overwrite one action's state for a simulation frame (current or future).
        //! When that frame is simulated (or re-simulated after a rollback restore),
        //! the injected state is used instead of sampling the live devices; press and
        //! release edges are derived against the prior frame's record. This is how a
        //! rollback layer feeds corrected remote inputs, how a replay is played back,
        //! and how a bot drives a fighter through the exact pipeline a human does.
        virtual void InjectActionState(AZ::s64 frame, const AZStd::string& action, float x, float y, bool pressed) = 0;
    };

    using DioramaInputRequestBus = AZ::EBus<DioramaInputRequests>;

    //! Notifications for event-driven agents/games that prefer subscribing over polling.
    //! Addressed by the input component's entity id. Fired on the press/release edge of
    //! every mapped action, so logic can be tied to an action without polling each tick.
    class DioramaInputNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaInputNotifications, DioramaInputNotificationsTypeId);
        virtual ~DioramaInputNotifications() = default;

        //! The action became pressed this frame.
        virtual void OnActionPressed(const AZStd::string& action)
        {
            AZ_UNUSED(action);
        }
        //! The action became released this frame.
        virtual void OnActionReleased(const AZStd::string& action)
        {
            AZ_UNUSED(action);
        }
        //! A named motion was just recognized (fired once on the frame the motion
        //! completes, not every frame it stays buffered).
        virtual void OnMotionPerformed(const AZStd::string& motion)
        {
            AZ_UNUSED(motion);
        }
    };

    using DioramaInputNotificationBus = AZ::EBus<DioramaInputNotifications>;

    //! BehaviorContext handler so scripts/agents can subscribe to action edges rather
    //! than poll WasPressedThisFrame each tick.
    class DioramaInputNotificationHandler
        : public DioramaInputNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            DioramaInputNotificationHandler,
            "{6E7F8091-DAEB-46F7-8091-2A3B4C5D6E1D}",
            AZ::SystemAllocator,
            OnActionPressed,
            OnActionReleased,
            OnMotionPerformed);

        void OnActionPressed(const AZStd::string& action) override
        {
            Call(FN_OnActionPressed, action);
        }
        void OnActionReleased(const AZStd::string& action) override
        {
            Call(FN_OnActionReleased, action);
        }
        void OnMotionPerformed(const AZStd::string& motion) override
        {
            Call(FN_OnMotionPerformed, motion);
        }
    };

    //! Reflect the input action-mapping buses (request + notification) to the
    //! BehaviorContext (Common scope). Called from the component's Reflect.
    void ReflectInputBuses(AZ::ReflectContext* context);
} // namespace Diorama
