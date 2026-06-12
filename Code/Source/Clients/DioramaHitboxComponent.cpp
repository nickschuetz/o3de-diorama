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
#include <AzCore/std/sort.h>

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
                ->Field("boxes", &DioramaHitboxConfig::m_boxes)
                ->Field("useSimClock", &DioramaHitboxConfig::m_useSimClock);

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
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxConfig::m_boxes, "Boxes", "The authored hitboxes and hurtboxes")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaHitboxConfig::m_useSimClock,
                        "Use Simulation Clock",
                        "Evaluate overlaps on the 2D Simulation Clock's fixed steps instead of the render tick. "
                        "With no clock in the level, falls back to the render tick.");
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
        DioramaSimStateParticipantBus::Handler::BusConnect(GetEntityId());
        if (m_config.m_useSimClock)
        {
            DioramaSimTickNotificationBus::Handler::BusConnect();
        }
        AZ::TickBus::Handler::BusConnect();
    }

    void DioramaHitboxComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSimTickNotificationBus::Handler::BusDisconnect();
        DioramaSimStateParticipantBus::Handler::BusDisconnect();
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
        // Use Simulation Clock mode: a running clock owns evaluation (OnSimTick),
        // so hits resolve once per fixed step, in deterministic order.
        if (m_config.m_useSimClock && DioramaSimClockRequestBus::HasHandlers())
        {
            return;
        }
        // Refresh world position (the fighter moves) and re-evaluate boxes for the
        // current frame. Cheap: a handful of boxes per rig.
        AZ::Transform worldTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTM, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
        m_worldTranslation = worldTM.GetTranslation();
        Evaluate();
    }

    void DioramaHitboxComponent::OnSimTick([[maybe_unused]] AZ::s64 frame, [[maybe_unused]] float stepSeconds)
    {
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

    void DioramaHitboxComponent::SetUseSimClock(bool enabled)
    {
        m_config.m_useSimClock = enabled;
        if (enabled)
        {
            if (!DioramaSimTickNotificationBus::Handler::BusIsConnected())
            {
                DioramaSimTickNotificationBus::Handler::BusConnect();
            }
        }
        else
        {
            DioramaSimTickNotificationBus::Handler::BusDisconnect();
        }
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

    // ---- Snapshot / restore (design phase B) -----------------------------------------
    // Chunk payload: frame (s64), facing (u8: 1 = +X), box count (u32), then per box
    // wasActive (u8) + already-hit count (u32) + that many entity ids (u64), written in
    // ascending id order so the image is canonical (the live set is unordered).

    void DioramaHitboxComponent::SaveSimState(SimState::Writer& writer)
    {
        const size_t sizePos = writer.BeginChunk(HitboxChunkTag);
        writer.S64(m_frame);
        writer.U8(m_config.m_facing < 0 ? 0 : 1);
        writer.U32(static_cast<AZ::u32>(m_boxState.size()));
        AZStd::vector<AZ::u64> ids;
        for (const BoxState& state : m_boxState)
        {
            writer.U8(state.m_wasActive ? 1 : 0);
            ids.clear();
            ids.reserve(state.m_alreadyHit.size());
            for (const AZ::EntityId& hit : state.m_alreadyHit)
            {
                ids.push_back(static_cast<AZ::u64>(hit));
            }
            AZStd::sort(ids.begin(), ids.end());
            writer.U32(static_cast<AZ::u32>(ids.size()));
            for (AZ::u64 id : ids)
            {
                writer.U64(id);
            }
        }
        writer.EndChunk(sizePos);
    }

    bool DioramaHitboxComponent::TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload)
    {
        if (tag != HitboxChunkTag)
        {
            return false;
        }
        AZ::s64 frame = 0;
        AZ::u8 facing = 1;
        AZ::u32 boxCount = 0;
        if (!payload.S64(frame) || !payload.U8(facing) || !payload.U32(boxCount))
        {
            return false;
        }
        if (boxCount != m_boxState.size())
        {
            // The rig was re-authored since this image was taken: apply the scalar
            // state and reset the per-box bookkeeping to a known-fresh state rather
            // than misalign windows across a different box set.
            m_frame = static_cast<int>(frame);
            m_config.m_facing = (facing != 0) ? 1 : -1;
            m_boxState.assign(m_boxState.size(), BoxState());
            return true;
        }
        // Parse into scratch first so a truncated payload cannot half-apply.
        AZStd::vector<BoxState> restored(boxCount);
        for (AZ::u32 i = 0; i < boxCount; ++i)
        {
            AZ::u8 wasActive = 0;
            AZ::u32 hitCount = 0;
            if (!payload.U8(wasActive) || !payload.U32(hitCount) || hitCount > payload.Remaining() / 8)
            {
                return false;
            }
            restored[i].m_wasActive = wasActive != 0;
            for (AZ::u32 h = 0; h < hitCount; ++h)
            {
                AZ::u64 raw = 0;
                if (!payload.U64(raw))
                {
                    return false;
                }
                restored[i].m_alreadyHit.insert(AZ::EntityId(raw));
            }
        }
        m_frame = static_cast<int>(frame);
        m_config.m_facing = (facing != 0) ? 1 : -1;
        m_boxState = AZStd::move(restored);
        return true;
    }
} // namespace Diorama
