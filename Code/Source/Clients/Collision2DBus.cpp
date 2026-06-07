/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/Collision2DBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void Collider2DInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Collider2DInfo>()
                ->Version(1)
                ->Field("isCircle", &Collider2DInfo::m_isCircle)
                ->Field("radius", &Collider2DInfo::m_radius)
                ->Field("halfWidth", &Collider2DInfo::m_halfWidth)
                ->Field("halfHeight", &Collider2DInfo::m_halfHeight)
                ->Field("layer", &Collider2DInfo::m_layer)
                ->Field("collidesWith", &Collider2DInfo::m_collidesWith)
                ->Field("isTrigger", &Collider2DInfo::m_isTrigger)
                ->Field("enabled", &Collider2DInfo::m_enabled)
                ->Field("contactCount", &Collider2DInfo::m_contactCount);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<Collider2DInfo>("DioramaCollider2DInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Collision")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("isCircle", BehaviorValueGetter(&Collider2DInfo::m_isCircle), nullptr)
                ->Property("radius", BehaviorValueGetter(&Collider2DInfo::m_radius), nullptr)
                ->Property("halfWidth", BehaviorValueGetter(&Collider2DInfo::m_halfWidth), nullptr)
                ->Property("halfHeight", BehaviorValueGetter(&Collider2DInfo::m_halfHeight), nullptr)
                ->Property("layer", BehaviorValueGetter(&Collider2DInfo::m_layer), nullptr)
                ->Property("collidesWith", BehaviorValueGetter(&Collider2DInfo::m_collidesWith), nullptr)
                ->Property("isTrigger", BehaviorValueGetter(&Collider2DInfo::m_isTrigger), nullptr)
                ->Property("enabled", BehaviorValueGetter(&Collider2DInfo::m_enabled), nullptr)
                ->Property("contactCount", BehaviorValueGetter(&Collider2DInfo::m_contactCount), nullptr);
        }
    }

    void Raycast2DResult::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Raycast2DResult>()
                ->Version(1)
                ->Field("hit", &Raycast2DResult::m_hit)
                ->Field("entityId", &Raycast2DResult::m_entityId)
                ->Field("distance", &Raycast2DResult::m_distance)
                ->Field("pointX", &Raycast2DResult::m_pointX)
                ->Field("pointZ", &Raycast2DResult::m_pointZ);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<Raycast2DResult>("DioramaRaycast2DResult")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Collision")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("hit", BehaviorValueGetter(&Raycast2DResult::m_hit), nullptr)
                ->Property("entityId", BehaviorValueGetter(&Raycast2DResult::m_entityId), nullptr)
                ->Property("distance", BehaviorValueGetter(&Raycast2DResult::m_distance), nullptr)
                ->Property("pointX", BehaviorValueGetter(&Raycast2DResult::m_pointX), nullptr)
                ->Property("pointZ", BehaviorValueGetter(&Raycast2DResult::m_pointZ), nullptr);
        }
    }

    void ReflectCollision2DBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<Diorama2DColliderRequestBus>("Diorama2DColliderRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Collision")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetCircle",
                &Diorama2DColliderRequestBus::Events::SetCircle,
                { { { "radius", "Circle radius in world units; negative is clamped to zero." } } })
            ->Event(
                "SetBox",
                &Diorama2DColliderRequestBus::Events::SetBox,
                { { { "halfWidth", "Half width of the axis-aligned box; negative is clamped to zero." },
                    { "halfHeight", "Half height of the axis-aligned box; negative is clamped to zero." } } })
            ->Event(
                "SetOffset",
                &Diorama2DColliderRequestBus::Events::SetOffset,
                { { { "x", "Offset from the entity origin along the plane X axis." },
                    { "z", "Offset from the entity origin along the plane Z axis." } } })
            ->Event(
                "SetLayer",
                &Diorama2DColliderRequestBus::Events::SetLayer,
                { { { "layer", "Category bit(s) this collider belongs to." } } })
            ->Event(
                "SetCollidesWith",
                &Diorama2DColliderRequestBus::Events::SetCollidesWith,
                { { { "mask", "Bit mask of categories this collider reports against." } } })
            ->Event(
                "SetTrigger",
                &Diorama2DColliderRequestBus::Events::SetTrigger,
                { { { "isTrigger", "When true the collider reports overlaps (OnTrigger*) but is never a solid contact." } } })
            ->Event(
                "SetEnabled",
                &Diorama2DColliderRequestBus::Events::SetEnabled,
                { { { "enabled", "When false the collider is kept registered but excluded from detection." } } })
            ->Event("GetColliderInfo", &Diorama2DColliderRequestBus::Events::GetColliderInfo);

        behaviorContext->EBus<Diorama2DCollisionRequestBus>("Diorama2DCollisionRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Collision")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "OverlapCircle",
                &Diorama2DCollisionRequestBus::Events::OverlapCircle,
                { { { "x", "Circle center, plane X." },
                    { "z", "Circle center, plane Z." },
                    { "radius", "Circle radius." },
                    { "layerMask", "Category mask to test against; 0 means any layer." } } })
            ->Event(
                "OverlapBox",
                &Diorama2DCollisionRequestBus::Events::OverlapBox,
                { { { "x", "Box center, plane X." },
                    { "z", "Box center, plane Z." },
                    { "halfWidth", "Half width of the box." },
                    { "halfHeight", "Half height of the box." },
                    { "layerMask", "Category mask to test against; 0 means any layer." } } })
            ->Event(
                "Raycast2D",
                &Diorama2DCollisionRequestBus::Events::Raycast2D,
                { { { "x", "Ray origin, plane X." },
                    { "z", "Ray origin, plane Z." },
                    { "dirX", "Ray direction X (need not be normalized)." },
                    { "dirZ", "Ray direction Z (need not be normalized)." },
                    { "maxDistance", "Maximum distance to search along the ray." },
                    { "layerMask", "Category mask to test against; 0 means any layer." } } })
            ->Event(
                "ComputeBoxPushOut",
                &Diorama2DCollisionRequestBus::Events::ComputeBoxPushOut,
                { { { "x", "Box center, plane X." },
                    { "z", "Box center, plane Z." },
                    { "halfWidth", "Box half-width (plane X)." },
                    { "halfHeight", "Box half-height (plane Z)." },
                    { "layerMask", "Category mask of colliders to push out of; 0 means any layer." },
                    { "exclude", "Entity to ignore (pass the caller's own id so it never pushes out of itself)." } } });

        behaviorContext->EBus<Diorama2DCollisionNotificationBus>("Diorama2DCollisionNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Collision")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<Diorama2DCollisionNotificationHandler>();
    }
} // namespace Diorama
