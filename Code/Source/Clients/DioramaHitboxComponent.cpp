/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/Collision2DSystemComponent.h> // Collision2DWorld (SetStaticColliders)
#include <Clients/DioramaHitboxComponent.h>

#include <Diorama/Collision2DBus.h> // Diorama2DCollisionRequestBus (OverlapBox)

#include <AzCore/Component/TransformBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaHitboxData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaHitboxData>()
                ->Version(1)
                ->Field("kind", &DioramaHitboxData::m_kind)
                ->Field("offset", &DioramaHitboxData::m_offset)
                ->Field("halfExtents", &DioramaHitboxData::m_halfExtents)
                ->Field("startFrame", &DioramaHitboxData::m_startFrame)
                ->Field("endFrame", &DioramaHitboxData::m_endFrame);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaHitboxData>("Box", "A hitbox or hurtbox live on a window of frames")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::ComboBox, &DioramaHitboxData::m_kind, "Kind", "Hurtbox (vulnerable) or hitbox (attacking)")
                    ->EnumAttribute(HitboxFrames::BoxKind::Hurtbox, "Hurtbox")
                    ->EnumAttribute(HitboxFrames::BoxKind::Hitbox, "Hitbox")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaHitboxData::m_offset,
                        "Offset",
                        "In-plane center offset (X mirrors with facing)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxData::m_halfExtents, "Half Extents", "Box half width and height")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxData::m_startFrame, "Start Frame", "First active animation frame")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaHitboxData::m_endFrame,
                        "End Frame",
                        "Last active animation frame (inclusive)");
            }
        }
    }

    void DioramaHitboxConfig::Reflect(AZ::ReflectContext* context)
    {
        DioramaHitboxData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaHitboxConfig>()
                ->Version(1)
                ->Field("plane", &DioramaHitboxConfig::m_plane)
                ->Field("hurtLayer", &DioramaHitboxConfig::m_hurtLayer)
                ->Field("targetMask", &DioramaHitboxConfig::m_targetMask)
                ->Field("facing", &DioramaHitboxConfig::m_facing)
                ->Field("boxes", &DioramaHitboxConfig::m_boxes);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaHitboxConfig>("Hitbox Config", "Frame-data hitboxes and hurtboxes")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &DioramaHitboxConfig::m_plane, "Plane", "World axes the boxes live in")
                    ->EnumAttribute(CollisionPlane::XY, "XY (screen)")
                    ->EnumAttribute(CollisionPlane::XZ, "XZ (ground)")
                    ->EnumAttribute(CollisionPlane::YZ, "YZ")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaHitboxConfig::m_hurtLayer,
                        "Hurt Layer",
                        "Layer bit this rig's hurtboxes register on")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaHitboxConfig::m_targetMask,
                        "Target Mask",
                        "Layer mask this rig's hitboxes strike")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxConfig::m_facing, "Facing", "+1 faces +X, -1 mirrors box offsets")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxConfig::m_boxes, "Boxes", "The authored hitboxes and hurtboxes");
            }
        }
    }

    DioramaHitboxComponent::DioramaHitboxComponent(const DioramaHitboxConfig& config)
        : m_config(config)
    {
    }

    void DioramaHitboxComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaHitboxConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaHitboxComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaHitboxComponent::m_config);
        }

        ReflectHitboxBuses(context);
    }

    void DioramaHitboxComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaHitboxService"));
    }

    void DioramaHitboxComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        // One rig per entity: two would both own the entity's static-collider set and
        // clobber each other every tick (the world keys static sets by owner entity).
        incompatible.push_back(AZ_CRC_CE("DioramaHitboxService"));
    }

    void DioramaHitboxComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void DioramaHitboxComponent::Activate()
    {
        m_frame = 0;
        m_boxState.assign(m_config.m_boxes.size(), BoxState());

        AZ::Transform worldTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTM, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
        m_worldTranslation = worldTM.GetTranslation();

        DioramaHitboxRequestBus::Handler::BusConnect(GetEntityId());
        DioramaSpriteNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
    }

    void DioramaHitboxComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSpriteNotificationBus::Handler::BusDisconnect();
        DioramaHitboxRequestBus::Handler::BusDisconnect();
        if (auto* world = Collision2DWorld::Get())
        {
            world->ClearStaticColliders(GetEntityId());
        }
    }

    void DioramaHitboxComponent::OnAnimationFrame(int frameIndex)
    {
        m_frame = frameIndex;
    }

    void DioramaHitboxComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        // Refresh world position (the fighter moves) and re-evaluate boxes for the
        // current frame. Cheap: a handful of boxes per rig.
        AZ::Transform worldTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTM, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
        m_worldTranslation = worldTM.GetTranslation();
        Evaluate();
    }

    AZ::Vector2 DioramaHitboxComponent::PlaneCenter() const
    {
        switch (m_config.m_plane)
        {
        case CollisionPlane::XZ:
            return AZ::Vector2(m_worldTranslation.GetX(), m_worldTranslation.GetZ());
        case CollisionPlane::YZ:
            return AZ::Vector2(m_worldTranslation.GetY(), m_worldTranslation.GetZ());
        case CollisionPlane::XY:
        default:
            return AZ::Vector2(m_worldTranslation.GetX(), m_worldTranslation.GetY());
        }
    }

    AZ::Vector2 DioramaHitboxComponent::BoxCenter(const DioramaHitboxData& box, const AZ::Vector2& origin) const
    {
        const float facing = m_config.m_facing < 0 ? -1.0f : 1.0f;
        return origin + AZ::Vector2(box.m_offset.GetX() * facing, box.m_offset.GetY());
    }

    void DioramaHitboxComponent::Evaluate()
    {
        const AZ::Vector2 origin = PlaneCenter();
        const AZ::EntityId self = GetEntityId();

        m_hurtScratch.clear();
        m_activeHitboxes = 0;
        m_activeHurtboxes = 0;

        for (size_t i = 0; i < m_config.m_boxes.size(); ++i)
        {
            const DioramaHitboxData& data = m_config.m_boxes[i];
            const HitboxFrames::Box coreBox{ data.m_kind, data.m_offset, data.m_halfExtents, data.m_startFrame, data.m_endFrame };
            const bool active = HitboxFrames::IsActiveAt(coreBox, m_frame);
            BoxState& state = m_boxState[i];

            if (!active)
            {
                state.m_wasActive = false;
                state.m_alreadyHit.clear();
                continue;
            }

            const AZ::Vector2 center = BoxCenter(data, origin);

            if (data.m_kind == HitboxFrames::BoxKind::Hurtbox)
            {
                ++m_activeHurtboxes;
                Collision2D::Collider c;
                c.m_center = center;
                c.m_shape.m_type = Collision2D::ShapeType::Box;
                c.m_shape.m_halfExtents = data.m_halfExtents;
                c.m_layer = m_config.m_hurtLayer;
                m_hurtScratch.push_back(c);
            }
            else // Hitbox
            {
                ++m_activeHitboxes;
                if (!state.m_wasActive)
                {
                    state.m_alreadyHit.clear(); // fresh swing: this box may hit each target again
                }

                AZStd::vector<AZ::EntityId> hits;
                Diorama2DCollisionRequestBus::BroadcastResult(
                    hits,
                    &Diorama2DCollisionRequests::OverlapBox,
                    center.GetX(),
                    center.GetY(),
                    data.m_halfExtents.GetX(),
                    data.m_halfExtents.GetY(),
                    m_config.m_targetMask);

                for (const AZ::EntityId& target : hits)
                {
                    if (target == self || state.m_alreadyHit.find(target) != state.m_alreadyHit.end())
                    {
                        continue;
                    }
                    state.m_alreadyHit.insert(target);
                    DioramaHitboxNotificationBus::Event(self, &DioramaHitboxNotifications::OnHit, target);
                    DioramaHitboxNotificationBus::Event(target, &DioramaHitboxNotifications::OnHurt, self);
                }
            }
            state.m_wasActive = true;
        }

        // Publish this rig's live hurtboxes as static, query-only geometry so other
        // rigs' hitbox queries find them. An empty set clears our registration.
        if (auto* world = Collision2DWorld::Get())
        {
            world->SetStaticColliders(self, m_hurtScratch);
        }
    }

    void DioramaHitboxComponent::SetFacing(int facing)
    {
        m_config.m_facing = facing < 0 ? -1 : 1;
    }

    int DioramaHitboxComponent::GetFacing()
    {
        return m_config.m_facing;
    }

    void DioramaHitboxComponent::SetFrame(int frame)
    {
        m_frame = frame;
    }

    DioramaHitboxInfo DioramaHitboxComponent::GetHitboxInfo()
    {
        DioramaHitboxInfo info;
        info.m_frame = m_frame;
        info.m_facing = m_config.m_facing;
        info.m_activeHitboxes = m_activeHitboxes;
        info.m_activeHurtboxes = m_activeHurtboxes;
        return info;
    }
} // namespace Diorama
