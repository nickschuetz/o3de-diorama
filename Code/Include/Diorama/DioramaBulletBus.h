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
    //! Resolved runtime state of a bullet-pattern emitter, returned by GetBulletInfo.
    //! The verify-loop payload: an agent confirms the emitter is firing and how many
    //! bullets are live without a viewport.
    struct DioramaBulletInfo
    {
        AZ_TYPE_INFO(Diorama::DioramaBulletInfo, DioramaBulletInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        int m_aliveCount = 0; //!< Bullets currently in flight.
        int m_maxBullets = 0; //!< Hard pool capacity.
        bool m_firing = false; //!< Continuous fire is on.
        int m_pattern = 0; //!< 0 ring, 1 fan, 2 spiral.
        float m_fireRate = 0.0f; //!< Shots per second.
    };

    //! Per-entity control of a bullet-pattern (danmaku) emitter, addressed by the
    //! entity id. Scalar, agent-friendly verbs in the same style as the other Diorama
    //! buses. A human authors the pattern in the Inspector; a script/agent retargets
    //! and fires it at run time.
    class DioramaBulletRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaBulletRequests, DioramaBulletRequestsTypeId);
        virtual ~DioramaBulletRequests() = default;

        //! Fire one shot of the current pattern immediately (a spiral also advances).
        virtual void Fire() = 0;
        //! Start firing continuously at the configured fire rate.
        virtual void Play() = 0;
        //! Stop continuous firing (bullets already in flight keep moving).
        virtual void Stop() = 0;
        //! Select the pattern: 0 ring, 1 fan, 2 spiral. Out-of-range clamps to ring.
        virtual void SetPattern(int pattern) = 0;
        //! Shots per second for continuous fire (clamped >= 0; 0 = manual Fire only).
        virtual void SetFireRate(float shotsPerSecond) = 0;
        //! Bullets per shot (clamped >= 0).
        virtual void SetCount(int count) = 0;
        //! Bullet speed in world units/sec (clamped >= 0).
        virtual void SetSpeed(float speed) = 0;
        //! Aim/base angle in degrees (0 = +X, 90 = +Y). Ring's first bullet, fan's
        //! center, spiral's starting angle.
        virtual void SetAim(float degrees) = 0;
        //! Fan arc width in degrees (ignored by ring; clamped to [0, 360]).
        virtual void SetSpread(float degrees) = 0;
        //! Spiral rotation added per shot, in degrees (ignored by ring/fan).
        virtual void SetSpin(float degreesPerShot) = 0;
        //! Muzzle offset from the entity origin (world XY) where bullets spawn, e.g. a
        //! ship's nose, so the gun does not fire from the body center.
        virtual void SetMuzzleOffset(float x, float y) = 0;
        //! Resolved emitter state. Safe to poll.
        virtual DioramaBulletInfo GetBulletInfo() = 0;
    };

    using DioramaBulletRequestBus = AZ::EBus<DioramaBulletRequests>;

    //! Bullet-hit notifications, addressed by the emitter entity's id. When a live
    //! bullet overlaps a collider on the emitter's target layer, the emitter is told
    //! OnBulletHit (the struck entity) and the bullet is consumed. Applying damage is
    //! a gameplay decision; the emitter only reports the contact and removes the bullet.
    class DioramaBulletNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaBulletNotifications, DioramaBulletNotificationsTypeId);
        virtual ~DioramaBulletNotifications() = default;

        //! A bullet from this emitter struck `target`.
        virtual void OnBulletHit(AZ::EntityId target)
        {
            AZ_UNUSED(target);
        }
    };

    using DioramaBulletNotificationBus = AZ::EBus<DioramaBulletNotifications>;

    //! BehaviorContext handler so gameplay Lua/Script Canvas/agents subscribe to hits
    //! (apply damage / score on OnBulletHit) rather than polling.
    class DioramaBulletNotificationHandler
        : public DioramaBulletNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            DioramaBulletNotificationHandler, "{0A1B2C3D-4E5F-4061-9273-8495A6B7C8DA}", AZ::SystemAllocator, OnBulletHit);

        void OnBulletHit(AZ::EntityId target) override
        {
            Call(FN_OnBulletHit, target);
        }
    };

    //! Reflect the bullet structs and buses to the BehaviorContext (Common scope) so
    //! they are callable/subscribable from launcher Lua, Python, and Script Canvas.
    //! Called from the bullet emitter component's Reflect.
    void ReflectBulletBuses(AZ::ReflectContext* context);
} // namespace Diorama
