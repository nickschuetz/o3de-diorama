/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/Collider2DComponent.h>
#include <Clients/Collision2DSystemComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    namespace
    {
        float NonNegative(float v)
        {
            return v < 0.0f ? 0.0f : v;
        }
    } // namespace

    void Collider2DConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Collider2DConfig>()
                ->Version(3)
                ->Field("plane", &Collider2DConfig::m_plane)
                ->Field("isCircle", &Collider2DConfig::m_isCircle)
                ->Field("radius", &Collider2DConfig::m_radius)
                ->Field("halfExtents", &Collider2DConfig::m_halfExtents)
                ->Field("offset", &Collider2DConfig::m_offset)
                ->Field("layer", &Collider2DConfig::m_layer)
                ->Field("collidesWith", &Collider2DConfig::m_collidesWith)
                ->Field("isTrigger", &Collider2DConfig::m_isTrigger)
                ->Field("enabled", &Collider2DConfig::m_enabled)
                ->Field("oneWay", &Collider2DConfig::m_oneWay);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<Collider2DConfig>("2D Collider Config", "Shape and filtering for a 2D collider")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::ComboBox,
                        &Collider2DConfig::m_plane,
                        "Plane",
                        "World axes the collider lives in (XY screen, XZ ground)")
                    ->EnumAttribute(CollisionPlane::XY, "XY (screen)")
                    ->EnumAttribute(CollisionPlane::XZ, "XZ (ground)")
                    ->EnumAttribute(CollisionPlane::YZ, "YZ")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &Collider2DConfig::m_isCircle,
                        "Circle",
                        "Circle when on, axis-aligned box when off")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &Collider2DConfig::m_radius, "Radius", "Circle radius (used when Circle is on)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &Collider2DConfig::m_halfExtents,
                        "Half Extents",
                        "Box half width and height (used when Circle is off)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &Collider2DConfig::m_offset,
                        "Offset",
                        "In-plane offset from the entity origin (plane is X, Z)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &Collider2DConfig::m_layer, "Layer", "Category bit(s) this collider belongs to")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &Collider2DConfig::m_collidesWith,
                        "Collides With",
                        "Bit mask of categories this collider reports against")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &Collider2DConfig::m_isTrigger,
                        "Trigger",
                        "Reports overlaps (OnTrigger*) but is never a solid contact")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &Collider2DConfig::m_enabled, "Enabled", "Excluded from detection when off")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &Collider2DConfig::m_oneWay,
                        "One Way",
                        "One-way platform: a box lands on the top but passes through from below and the sides (push-out only)");
            }
        }
    }

    Collider2DComponent::Collider2DComponent(const Collider2DConfig& config)
        : m_config(config)
    {
    }

    void Collider2DComponent::Reflect(AZ::ReflectContext* context)
    {
        Collider2DConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Collider2DComponent, AZ::Component>()->Version(1)->Field("Config", &Collider2DComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<Collider2DComponent>("2D Collider", "Gem-native 2D collider for contact and trigger events")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Collider2DComponent::m_config, "Config", "Collider configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void Collider2DComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Diorama2DColliderService"));
    }

    void Collider2DComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        // One collider per entity in v1 (the world keys colliders by entity id).
        incompatible.push_back(AZ_CRC_CE("Diorama2DColliderService"));
    }

    void Collider2DComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void Collider2DComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    void Collider2DComponent::Activate()
    {
        AZ::Transform worldTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTM, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
        m_worldTranslation = worldTM.GetTranslation();

        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        Diorama2DColliderRequestBus::Handler::BusConnect(GetEntityId());
        PushToWorld();
    }

    void Collider2DComponent::Deactivate()
    {
        Diorama2DColliderRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        if (auto* world = Collision2DWorld::Get())
        {
            world->RemoveCollider(GetEntityId());
        }
    }

    Collision2D::Collider Collider2DComponent::BuildCollider() const
    {
        Collision2D::Collider c;
        c.m_id = static_cast<AZ::u64>(GetEntityId());
        AZ::Vector2 planeCenter;
        switch (m_config.m_plane)
        {
        case CollisionPlane::XZ:
            planeCenter = AZ::Vector2(m_worldTranslation.GetX(), m_worldTranslation.GetZ());
            break;
        case CollisionPlane::YZ:
            planeCenter = AZ::Vector2(m_worldTranslation.GetY(), m_worldTranslation.GetZ());
            break;
        case CollisionPlane::XY:
        default:
            planeCenter = AZ::Vector2(m_worldTranslation.GetX(), m_worldTranslation.GetY());
            break;
        }
        c.m_center = planeCenter + m_config.m_offset;
        c.m_shape.m_type = m_config.m_isCircle ? Collision2D::ShapeType::Circle : Collision2D::ShapeType::Box;
        c.m_shape.m_radius = m_config.m_radius;
        c.m_shape.m_halfExtents = m_config.m_halfExtents;
        c.m_layer = m_config.m_layer;
        c.m_collidesWith = m_config.m_collidesWith;
        c.m_isTrigger = m_config.m_isTrigger;
        c.m_enabled = m_config.m_enabled;
        c.m_oneWay = m_config.m_oneWay;
        return c;
    }

    void Collider2DComponent::PushToWorld()
    {
        if (auto* world = Collision2DWorld::Get())
        {
            world->UpsertCollider(GetEntityId(), BuildCollider());
        }
    }

    void Collider2DComponent::OnTransformChanged([[maybe_unused]] const AZ::Transform& local, const AZ::Transform& world)
    {
        m_worldTranslation = world.GetTranslation();
        PushToWorld();
    }

    void Collider2DComponent::SetCircle(float radius)
    {
        m_config.m_isCircle = true;
        m_config.m_radius = NonNegative(radius);
        PushToWorld();
    }

    void Collider2DComponent::SetBox(float halfWidth, float halfHeight)
    {
        m_config.m_isCircle = false;
        m_config.m_halfExtents = AZ::Vector2(NonNegative(halfWidth), NonNegative(halfHeight));
        PushToWorld();
    }

    void Collider2DComponent::SetOffset(float x, float z)
    {
        m_config.m_offset = AZ::Vector2(x, z);
        PushToWorld();
    }

    void Collider2DComponent::SetLayer(AZ::u32 layer)
    {
        m_config.m_layer = layer;
        PushToWorld();
    }

    void Collider2DComponent::SetCollidesWith(AZ::u32 mask)
    {
        m_config.m_collidesWith = mask;
        PushToWorld();
    }

    void Collider2DComponent::SetTrigger(bool isTrigger)
    {
        m_config.m_isTrigger = isTrigger;
        PushToWorld();
    }

    void Collider2DComponent::SetEnabled(bool enabled)
    {
        m_config.m_enabled = enabled;
        PushToWorld();
    }

    void Collider2DComponent::SetOneWay(bool oneWay)
    {
        m_config.m_oneWay = oneWay;
        PushToWorld();
    }

    Collider2DInfo Collider2DComponent::GetColliderInfo()
    {
        Collider2DInfo info;
        info.m_isCircle = m_config.m_isCircle;
        info.m_radius = m_config.m_radius;
        info.m_halfWidth = m_config.m_halfExtents.GetX();
        info.m_halfHeight = m_config.m_halfExtents.GetY();
        info.m_layer = m_config.m_layer;
        info.m_collidesWith = m_config.m_collidesWith;
        info.m_isTrigger = m_config.m_isTrigger;
        info.m_enabled = m_config.m_enabled;
        if (auto* world = Collision2DWorld::Get())
        {
            info.m_contactCount = world->GetContactCount(GetEntityId());
        }
        return info;
    }
} // namespace Diorama
