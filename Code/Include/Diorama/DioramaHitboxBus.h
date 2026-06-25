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
        int m_activePushboxes = 0; //!< Active body-volume boxes this frame.
        int m_activeThrowboxes = 0; //!< Active grabbing boxes this frame.
        int m_activeThrowableBoxes = 0; //!< Active grabbable boxes this frame.
        int m_activeArmorBoxes = 0; //!< Active hit-absorbing boxes this frame.
        int m_activeProximityBoxes = 0; //!< Active presence-signal boxes this frame.
        float m_pushOutX = 0.0f; //!< In-plane push-out to separate this rig's pushboxes.
        float m_pushOutY = 0.0f; //!< (full vector; halved per step when auto-separate is on).
    };

    //! One resolved box interaction, delivered on OnBoxEvent to both the attacker and
    //! the defender entity. Flat scalars throughout (no math types), like every
    //! Diorama event payload: readable from Lua / Script Canvas / Python as-is. The
    //! gem fills the attack payload from the attacking box and never interprets it;
    //! applying damage, stun, or pushback is the receiving combat script's decision.
    struct DioramaBoxEvent
    {
        AZ_TYPE_INFO(Diorama::DioramaBoxEvent, DioramaBoxEventTypeId);
        static void Reflect(AZ::ReflectContext* context);

        int m_result = 0; //!< 1 Hit, 2 Clash, 3 Beaten (attacker's side), 4 Absorbed, 5 Throw, 6 Proximity.
        AZ::EntityId m_attacker; //!< Rig whose attacking box drove the interaction.
        AZ::EntityId m_defender; //!< Rig whose receiving box was contacted.
        int m_attackerBoxIndex = 0; //!< Index into the attacker rig's authored boxes.
        int m_defenderBoxIndex = 0; //!< Index into the defender rig's authored boxes.
        float m_contactX = 0.0f; //!< Approximate world contact point in the rig plane
        float m_contactY = 0.0f; //!< (center of the boxes' overlap; spark placement).

        // The attacking box's authored attack payload (HitProperties), flattened.
        float m_damage = 0.0f;
        int m_hitstunFrames = 0;
        int m_blockstunFrames = 0;
        int m_hitstopFrames = 0;
        float m_pushbackX = 0.0f;
        float m_pushbackY = 0.0f;
        int m_guardHeight = 0; //!< 0 any, 1 high, 2 low, 3 unblockable.
        float m_launchX = 0.0f;
        float m_launchY = 0.0f;
        int m_priority = 0;
        AZ::u32 m_customId = 0; //!< Opaque game key authored on the box (move id, sound key).
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
        //! When true, the rig applies half its computed pushbox push-out to its own
        //! translation each evaluation (overlapping pairs split the separation evenly
        //! and converge). When false (default) the push-out is only reported in
        //! GetHitboxInfo for the game to apply.
        virtual void SetAutoSeparate(bool enabled) = 0;
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
        //! A typed box interaction this entity took part in (as attacker or defender):
        //! Hit / Clash / Beaten / Absorbed / Throw / Proximity, with the attacking
        //! box's payload and an approximate contact point. Hits also fire the simpler
        //! OnHit / OnHurt above, which keep their v1 behavior.
        virtual void OnBoxEvent(const DioramaBoxEvent& boxEvent)
        {
            AZ_UNUSED(boxEvent);
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
            DioramaHitboxNotificationHandler, "{7A1B2C3D-4E5F-4061-9273-8495A6B7C8D9}", AZ::SystemAllocator, OnHit, OnHurt, OnBoxEvent);

        void OnHit(AZ::EntityId target) override
        {
            Call(FN_OnHit, target);
        }
        void OnHurt(AZ::EntityId attacker) override
        {
            Call(FN_OnHurt, attacker);
        }
        void OnBoxEvent(const DioramaBoxEvent& boxEvent) override
        {
            Call(FN_OnBoxEvent, boxEvent);
        }
    };

    //! Reflect the hitbox structs and buses to the BehaviorContext (Common scope) so
    //! they are callable/subscribable from launcher Lua, Python, and Script Canvas.
    //! Called from the hitbox component's Reflect.
    void ReflectHitboxBuses(AZ::ReflectContext* context);
} // namespace Diorama
