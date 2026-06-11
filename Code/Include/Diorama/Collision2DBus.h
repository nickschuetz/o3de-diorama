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
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a 2D collider, returned by GetColliderInfo. The
    //! verify-loop payload: it reports what the collider actually is and how many
    //! contacts it currently has, so an agent can confirm setup without a viewport.
    struct Collider2DInfo
    {
        AZ_TYPE_INFO(Diorama::Collider2DInfo, Collider2DInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        bool m_isCircle = true; //!< true: circle (m_radius); false: box (m_halfWidth/Height).
        float m_radius = 0.5f;
        float m_halfWidth = 0.5f;
        float m_halfHeight = 0.5f;
        AZ::u32 m_layer = 1; //!< Category bit(s) this collider belongs to.
        AZ::u32 m_collidesWith = 0xFFFFFFFFu; //!< Mask of categories it reports against.
        bool m_isTrigger = false;
        bool m_enabled = true;
        int m_contactCount = 0; //!< Number of colliders it currently overlaps.
    };

    //! Result of a 2D ray cast. The point is in the collision plane (X, Z).
    struct Raycast2DResult
    {
        AZ_TYPE_INFO(Diorama::Raycast2DResult, Raycast2DResultTypeId);
        static void Reflect(AZ::ReflectContext* context);

        bool m_hit = false;
        AZ::EntityId m_entityId; //!< The collider that was hit (invalid if m_hit is false).
        float m_distance = 0.0f; //!< Distance along the ray to the hit.
        float m_pointX = 0.0f;
        float m_pointZ = 0.0f;
    };

    //! Result of a ground probe (ProbeGroundY): whether a walkable surface (flat tile
    //! top or ramp) was found at the queried column within reach, and its height.
    struct GroundProbe2DResult
    {
        AZ_TYPE_INFO(Diorama::GroundProbe2DResult, GroundProbe2DResultTypeId);
        static void Reflect(AZ::ReflectContext* context);

        bool m_onGround = false; //!< A surface was within step-up / max-drop reach.
        float m_groundY = 0.0f; //!< Surface height to place the feet at (valid if m_onGround).
    };

    //! Per-collider control, addressed by the collider entity's id. Scalar,
    //! clamped, agent-friendly verbs in the same style as DioramaSpriteRequestBus.
    class Diorama2DColliderRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(Diorama2DColliderRequests, Diorama2DColliderRequestsTypeId);
        virtual ~Diorama2DColliderRequests() = default;

        //! Make this collider a circle of the given radius (negative clamps to 0).
        virtual void SetCircle(float radius) = 0;
        //! Make this collider an axis-aligned box with the given half extents.
        virtual void SetBox(float halfWidth, float halfHeight) = 0;
        //! In-plane offset from the entity origin (collision plane is X, Z).
        virtual void SetOffset(float x, float z) = 0;
        //! Category bit(s) this collider belongs to.
        virtual void SetLayer(AZ::u32 layer) = 0;
        //! Mask of categories this collider reports against.
        virtual void SetCollidesWith(AZ::u32 mask) = 0;
        //! A trigger reports overlaps (OnTrigger*) but is never a solid contact.
        virtual void SetTrigger(bool isTrigger) = 0;
        //! Disable to keep the collider registered but excluded from detection.
        virtual void SetEnabled(bool enabled) = 0;
        //! One-way platform: a box lands on the top but passes through from below and
        //! the sides (push-out resolves it upward only).
        virtual void SetOneWay(bool oneWay) = 0;
        //! Resolved collider state. Safe to poll.
        virtual Collider2DInfo GetColliderInfo() = 0;
    };

    using Diorama2DColliderRequestBus = AZ::EBus<Diorama2DColliderRequests>;

    //! Contact and trigger notifications, addressed by the collider entity's id.
    //! Both entities in a pair are notified, each told the other's id. Contacts
    //! (solid pairs) get begin/stay/end; triggers (either collider is a trigger)
    //! get enter/exit. This is the launcher-reachable path the PhysX collision bus
    //! is not (see Docs/design/2d-collision.md).
    class Diorama2DCollisionNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(Diorama2DCollisionNotifications, Diorama2DCollisionNotificationsTypeId);
        virtual ~Diorama2DCollisionNotifications() = default;

        virtual void OnContactBegin(AZ::EntityId other)
        {
            AZ_UNUSED(other);
        }
        virtual void OnContactStay(AZ::EntityId other)
        {
            AZ_UNUSED(other);
        }
        virtual void OnContactEnd(AZ::EntityId other)
        {
            AZ_UNUSED(other);
        }
        virtual void OnTriggerEnter(AZ::EntityId other)
        {
            AZ_UNUSED(other);
        }
        virtual void OnTriggerExit(AZ::EntityId other)
        {
            AZ_UNUSED(other);
        }
    };

    using Diorama2DCollisionNotificationBus = AZ::EBus<Diorama2DCollisionNotifications>;

    //! BehaviorContext handler so scripts/agents can subscribe to contact/trigger
    //! events (the whole point: gameplay Lua receives these in the launcher).
    class Diorama2DCollisionNotificationHandler
        : public Diorama2DCollisionNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            Diorama2DCollisionNotificationHandler,
            "{DBCD2533-6DBF-4D88-87BF-C4FFF9EF1C92}",
            AZ::SystemAllocator,
            OnContactBegin,
            OnContactStay,
            OnContactEnd,
            OnTriggerEnter,
            OnTriggerExit);

        void OnContactBegin(AZ::EntityId other) override
        {
            Call(FN_OnContactBegin, other);
        }
        void OnContactStay(AZ::EntityId other) override
        {
            Call(FN_OnContactStay, other);
        }
        void OnContactEnd(AZ::EntityId other) override
        {
            Call(FN_OnContactEnd, other);
        }
        void OnTriggerEnter(AZ::EntityId other) override
        {
            Call(FN_OnTriggerEnter, other);
        }
        void OnTriggerExit(AZ::EntityId other) override
        {
            Call(FN_OnTriggerExit, other);
        }
    };

    //! Global 2D spatial queries, answered by the collision system. All positions
    //! and directions are in the collision plane (X, Z). A layerMask of 0 means
    //! "any layer"; otherwise a collider matches when (collider.layer & layerMask)
    //! is non-zero.
    class Diorama2DCollisionRequests : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~Diorama2DCollisionRequests() = default;

        //! Entities whose collider overlaps the given circle.
        virtual AZStd::vector<AZ::EntityId> OverlapCircle(float x, float z, float radius, AZ::u32 layerMask) = 0;
        //! Entities whose collider overlaps the given axis-aligned box.
        virtual AZStd::vector<AZ::EntityId> OverlapBox(float x, float z, float halfWidth, float halfHeight, AZ::u32 layerMask) = 0;
        //! Nearest collider hit by a ray from (x,z) along (dirX,dirZ), up to maxDistance.
        virtual Raycast2DResult Raycast2D(float x, float z, float dirX, float dirZ, float maxDistance, AZ::u32 layerMask) = 0;
        //! Net push-out vector to separate a box at (x, z) from every overlapping
        //! collider on layerMask, excluding the entity 'exclude' (pass the caller's
        //! own id). Add the result to the entity's position to resolve pushbox
        //! overlap; when both characters do this each frame, they part naturally.
        //! Returns (0, 0) when nothing overlaps.
        virtual AZ::Vector2 ComputeBoxPushOut(
            float x, float z, float halfWidth, float halfHeight, AZ::u32 layerMask, AZ::EntityId exclude) = 0;
        //! Ground-follow probe for a side-scroller: at horizontal column `x`, find the
        //! highest walkable surface (flat tile top or ramp) whose height is within
        //! `stepUp` above and `maxDrop` below the feet at `footY`. Surfaces are supplied
        //! by SetGroundSegments (the tilemap projects its solid/slope tile tops). Returns
        //! whether a surface was found and its height, so the body snaps to ground and
        //! ramps and falls off ledges. Slopes are ground-probe geometry, not solid boxes.
        virtual GroundProbe2DResult ProbeGroundY(float x, float footY, float maxDrop, float stepUp) = 0;
        //! Append a walkable ground segment for ProbeGroundY: a span on world X in
        //! [x0, x1] with surface height y0 at x0 and y1 at x1 (linear between). A flat
        //! ledge has y0 == y1; a ramp has them differ. The script/agent authoring path
        //! for floors and ramps (the C++/tilemap path is SetGroundSegments).
        virtual void AddGroundSegment(float x0, float x1, float y0, float y1) = 0;
        //! Clear all script-authored ground segments (added via AddGroundSegment).
        virtual void ClearScriptGroundSegments() = 0;
    };

    using Diorama2DCollisionRequestBus = AZ::EBus<Diorama2DCollisionRequests>;

    //! Reflect the 2D collision structs and buses to the BehaviorContext (Common
    //! scope) so they are callable/subscribable from launcher Lua, Python, and
    //! Script Canvas. Called from the collision system component's Reflect.
    void ReflectCollision2DBuses(AZ::ReflectContext* context);
} // namespace Diorama
