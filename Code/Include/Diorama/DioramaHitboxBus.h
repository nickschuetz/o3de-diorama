/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a frame-data hitbox component, returned by
    //! GetHitboxInfo. The verify-loop payload: an agent confirms which frame is live
    //! and how many boxes are active without a viewport.
    struct DioramaHitboxInfo
    {
        AZ_TYPE_INFO(Diorama::DioramaHitboxInfo, DioramaHitboxInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        int m_frame = 0; //!< Animation frame currently driving box activation.
        int m_facing = 1; //!< +1 faces +X, -1 mirrors hitbox/hurtbox X offsets.
        int m_activeHitboxes = 0; //!< Active attacking boxes this frame.
        int m_activeHurtboxes = 0; //!< Active vulnerable boxes this frame.
    };

    //! Per-entity control of a frame-data hitbox rig, addressed by the entity id.
    //! Scalar, agent-friendly verbs in the same style as the other Diorama buses. A
    //! human sets facing/frame in the Inspector or via gameplay; an agent reads back
    //! GetHitboxInfo to confirm.
    class DioramaHitboxRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaHitboxRequests, DioramaHitboxRequestsTypeId);
        virtual ~DioramaHitboxRequests() = default;

        //! Mirror box X offsets: +1 faces +X (right), -1 faces -X (left). A fighter
        //! sets this when it turns to face its opponent so its boxes follow.
        virtual void SetFacing(int facing) = 0;
        //! Current facing (+1 or -1).
        virtual int GetFacing() = 0;
        //! Override the animation frame that drives activation. Normally the frame is
        //! taken from OnAnimationFrame; this is for rigs not driven by the sprite
        //! animation player (or for deterministic testing/agents).
        virtual void SetFrame(int frame) = 0;
        //! Evaluate overlaps on the 2D Simulation Clock's fixed steps instead of the
        //! render tick (deterministic hit order, snapshot/rewind friendly). With no
        //! clock in the level the render tick still evaluates as before.
        virtual void SetUseSimClock(bool enabled) = 0;
        //! Resolved hitbox state. Safe to poll.
        virtual DioramaHitboxInfo GetHitboxInfo() = 0;
    };

    using DioramaHitboxRequestBus = AZ::EBus<DioramaHitboxRequests>;

    //! Hit notifications, addressed by a hitbox entity's id. When this entity's active
    //! hitbox overlaps another entity's active hurtbox, the attacker is told OnHit
    //! (the target it struck) and the target is told OnHurt (the attacker). Each
    //! ordered attacker->target pair fires once per the hitbox's active window, so a
    //! single swing lands one hit per opponent, not one per frame.
    class DioramaHitboxNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaHitboxNotifications, DioramaHitboxNotificationsTypeId);
        virtual ~DioramaHitboxNotifications() = default;

        //! This entity's hitbox struck `target`'s hurtbox.
        virtual void OnHit(AZ::EntityId target)
        {
            AZ_UNUSED(target);
        }
        //! This entity's hurtbox was struck by `attacker`'s hitbox.
        virtual void OnHurt(AZ::EntityId attacker)
        {
            AZ_UNUSED(attacker);
        }
    };

    using DioramaHitboxNotificationBus = AZ::EBus<DioramaHitboxNotifications>;

    //! BehaviorContext handler so gameplay Lua/Script Canvas/agents subscribe to hits
    //! (apply damage on OnHit, start hit-stun on OnHurt) rather than polling.
    class DioramaHitboxNotificationHandler
        : public DioramaHitboxNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            DioramaHitboxNotificationHandler, "{7A1B2C3D-4E5F-4061-9273-8495A6B7C8D9}", AZ::SystemAllocator, OnHit, OnHurt);

        void OnHit(AZ::EntityId target) override
        {
            Call(FN_OnHit, target);
        }
        void OnHurt(AZ::EntityId attacker) override
        {
            Call(FN_OnHurt, attacker);
        }
    };

    //! Reflect the hitbox structs and buses to the BehaviorContext (Common scope) so
    //! they are callable/subscribable from launcher Lua, Python, and Script Canvas.
    //! Called from the hitbox component's Reflect.
    void ReflectHitboxBuses(AZ::ReflectContext* context);
} // namespace Diorama
