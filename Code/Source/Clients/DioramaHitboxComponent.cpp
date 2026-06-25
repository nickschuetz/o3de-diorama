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
    void ReflectHitProperties(AZ::ReflectContext* context)
    {
        using HitboxFrames::GuardHeight;
        using HitboxFrames::HitProperties;

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HitProperties>()
                ->Version(1)
                ->Field("damage", &HitProperties::m_damage)
                ->Field("hitstunFrames", &HitProperties::m_hitstunFrames)
                ->Field("blockstunFrames", &HitProperties::m_blockstunFrames)
                ->Field("hitstopFrames", &HitProperties::m_hitstopFrames)
                ->Field("pushback", &HitProperties::m_pushback)
                ->Field("guardHeight", &HitProperties::m_guardHeight)
                ->Field("launch", &HitProperties::m_launch)
                ->Field("priority", &HitProperties::m_priority)
                ->Field("customId", &HitProperties::m_customId);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<HitProperties>("Attack Payload", "Numbers the box delivers with its event; the gem never applies them itself")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HitProperties::m_damage, "Damage", "Delivered with the event")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HitProperties::m_hitstunFrames, "Hitstun Frames", "Sim frames of hitstun")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &HitProperties::m_blockstunFrames, "Blockstun Frames", "Sim frames when blocked")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HitProperties::m_hitstopFrames,
                        "Hitstop Frames",
                        "Sim frames both sides freeze on contact")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &HitProperties::m_pushback, "Pushback", "Applied to the defender (by its script)")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &HitProperties::m_guardHeight, "Guard Height", "Which guards stop it")
                    ->EnumAttribute(GuardHeight::Any, "Any")
                    ->EnumAttribute(GuardHeight::High, "High (block standing)")
                    ->EnumAttribute(GuardHeight::Low, "Low (block crouching)")
                    ->EnumAttribute(GuardHeight::Unblockable, "Unblockable")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HitProperties::m_launch, "Launch", "Zero means a grounded reaction")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HitProperties::m_priority, "Priority", "Hitbox-vs-hitbox clash rank")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &HitProperties::m_customId, "Custom Id", "Opaque game key (move id, sound key)");
            }
        }
    }

    void DioramaHitboxData::Reflect(AZ::ReflectContext* context)
    {
        ReflectHitProperties(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaHitboxData>()
                ->Version(2)
                ->Field("kind", &DioramaHitboxData::m_kind)
                ->Field("offset", &DioramaHitboxData::m_offset)
                ->Field("halfExtents", &DioramaHitboxData::m_halfExtents)
                ->Field("startFrame", &DioramaHitboxData::m_startFrame)
                ->Field("endFrame", &DioramaHitboxData::m_endFrame)
                ->Field("hit", &DioramaHitboxData::m_hit);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaHitboxData>("Box", "A typed interaction box live on a window of frames")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &DioramaHitboxData::m_kind, "Kind", "What the box does while active")
                    ->EnumAttribute(HitboxFrames::BoxKind::Hurtbox, "Hurtbox")
                    ->EnumAttribute(HitboxFrames::BoxKind::Hitbox, "Hitbox")
                    ->EnumAttribute(HitboxFrames::BoxKind::Pushbox, "Pushbox")
                    ->EnumAttribute(HitboxFrames::BoxKind::Throwbox, "Throwbox")
                    ->EnumAttribute(HitboxFrames::BoxKind::ThrowableBox, "Throwable")
                    ->EnumAttribute(HitboxFrames::BoxKind::ArmorBox, "Armor")
                    ->EnumAttribute(HitboxFrames::BoxKind::ProximityBox, "Proximity")
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
                        "Last active animation frame (inclusive)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaHitboxData::m_hit,
                        "Attack Payload",
                        "Delivered with the box's event (meaningful on Hitbox / Throwbox)");
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
                ->Field("pushLayer", &DioramaHitboxConfig::m_pushLayer)
                ->Field("facing", &DioramaHitboxConfig::m_facing)
                ->Field("boxes", &DioramaHitboxConfig::m_boxes)
                ->Field("useSimClock", &DioramaHitboxConfig::m_useSimClock)
                ->Field("autoSeparate", &DioramaHitboxConfig::m_autoSeparate);

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
                        AZ::Edit::UIHandlers::Default,
                        &DioramaHitboxConfig::m_pushLayer,
                        "Push Layer",
                        "Layer bit this rig's pushboxes register on and separate against")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxConfig::m_facing, "Facing", "+1 faces +X, -1 mirrors box offsets")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaHitboxConfig::m_boxes, "Boxes", "The authored hitboxes and hurtboxes")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaHitboxConfig::m_useSimClock,
                        "Use Simulation Clock",
                        "Evaluate overlaps on the 2D Simulation Clock's fixed steps instead of the render tick. "
                        "With no clock in the level, falls back to the render tick.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaHitboxConfig::m_autoSeparate,
                        "Auto Separate Pushboxes",
                        "Apply half the computed pushbox push-out to this entity each evaluation "
                        "(overlapping pairs split the separation and converge). Off: only report it.");
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
        DioramaHitboxRigBus::Handler::BusConnect();
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
        DioramaHitboxRigBus::Handler::BusDisconnect();
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
        using HitboxFrames::BoxKind;
        using HitboxFrames::BoxResult;

        const AZ::Vector2 origin = PlaneCenter();
        const AZ::EntityId self = GetEntityId();

        // ---- Pass 1: activation. Build this rig's world-resolved active-box
        // snapshot (published on the rig bus), per-kind counts, and the static
        // geometry other systems query (hurt/armor/throwable on the hurt layer for
        // bullets and plain colliders; pushboxes on the push layer for push-out).
        m_staticScratch.clear();
        m_activeBoxScratch.clear();
        m_activeHitboxes = 0;
        m_activeHurtboxes = 0;
        m_activePushboxes = 0;
        m_activeThrowboxes = 0;
        m_activeThrowableBoxes = 0;
        m_activeArmorBoxes = 0;
        m_activeProximityBoxes = 0;

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
                state.m_alreadyContested.clear();
                continue;
            }
            if (!state.m_wasActive)
            {
                // Fresh activation: this box may interact with each target again.
                state.m_alreadyHit.clear();
                state.m_alreadyContested.clear();
                state.m_wasActive = true;
            }

            ActiveRigBox snapshot;
            snapshot.m_index = static_cast<int>(i);
            snapshot.m_kind = data.m_kind;
            snapshot.m_center = BoxCenter(data, origin);
            snapshot.m_halfExtents = data.m_halfExtents;
            snapshot.m_hit = data.m_hit;
            m_activeBoxScratch.push_back(snapshot);

            const auto publishStatic = [this, &snapshot](AZ::u32 layer)
            {
                Collision2D::Collider c;
                c.m_center = snapshot.m_center;
                c.m_shape.m_type = Collision2D::ShapeType::Box;
                c.m_shape.m_halfExtents = snapshot.m_halfExtents;
                c.m_layer = layer;
                m_staticScratch.push_back(c);
            };

            switch (data.m_kind)
            {
            case BoxKind::Hurtbox:
                ++m_activeHurtboxes;
                publishStatic(m_config.m_hurtLayer);
                break;
            case BoxKind::Hitbox:
                ++m_activeHitboxes;
                break;
            case BoxKind::Pushbox:
                ++m_activePushboxes;
                publishStatic(m_config.m_pushLayer);
                break;
            case BoxKind::Throwbox:
                ++m_activeThrowboxes;
                break;
            case BoxKind::ThrowableBox:
                ++m_activeThrowableBoxes;
                break;
            case BoxKind::ArmorBox:
                ++m_activeArmorBoxes;
                break;
            case BoxKind::ProximityBox:
                ++m_activeProximityBoxes;
                break;
            }
        }

        if (auto* world = Collision2DWorld::Get())
        {
            world->SetStaticColliders(self, m_staticScratch);
        }

        // ---- Pass 2: collect the other rigs, sorted by entity id so resolution
        // order is deterministic, and remember which entities are rigs (the legacy
        // collider path below must not double-report them).
        m_rigScratch.clear();
        DioramaHitboxRigBus::EnumerateHandlers(
            [this, self](DioramaHitboxRigs* rig)
            {
                if (rig->GetRigEntity() != self)
                {
                    m_rigScratch.emplace_back(rig->GetRigEntity(), rig);
                }
                return true;
            });
        AZStd::sort(
            m_rigScratch.begin(),
            m_rigScratch.end(),
            [](const auto& a, const auto& b)
            {
                return a.first < b.first;
            });
        const auto isRig = [this](AZ::EntityId id)
        {
            for (const auto& entry : m_rigScratch)
            {
                if (entry.first == id)
                {
                    return true;
                }
            }
            return false;
        };

        // ---- Pass 3: legacy collider path. Active hitboxes still strike non-rig
        // colliders on the target mask (a destructible, a bullet's body) exactly as
        // v1 did; rig entities resolve at box level in pass 4 instead.
        AZStd::vector<AZ::EntityId> hits;
        for (const ActiveRigBox& mine : m_activeBoxScratch)
        {
            if (mine.m_kind != BoxKind::Hitbox)
            {
                continue;
            }
            BoxState& state = m_boxState[mine.m_index];
            hits.clear();
            Diorama2DCollisionRequestBus::BroadcastResult(
                hits,
                &Diorama2DCollisionRequests::OverlapBox,
                mine.m_center.GetX(),
                mine.m_center.GetY(),
                mine.m_halfExtents.GetX(),
                mine.m_halfExtents.GetY(),
                m_config.m_targetMask);
            for (const AZ::EntityId& target : hits)
            {
                if (target == self || isRig(target) || state.m_alreadyHit.find(target) != state.m_alreadyHit.end())
                {
                    continue;
                }
                state.m_alreadyHit.insert(target);
                DioramaHitboxNotificationBus::Event(self, &DioramaHitboxNotifications::OnHit, target);
                DioramaHitboxNotificationBus::Event(target, &DioramaHitboxNotifications::OnHurt, self);
            }
        }

        // ---- Pass 4: rig-pair matrix. My attacking boxes against each rig's
        // receiving boxes (and opposing hitboxes), in sorted rig order, first
        // overlapping box in authoring order, once per (box, target, window).
        for (const auto& [otherId, rig] : m_rigScratch)
        {
            if ((rig->GetRigReceiveLayer() & m_config.m_targetMask) == 0)
            {
                continue;
            }
            const AZStd::vector<ActiveRigBox>& theirs = rig->GetRigActiveBoxes();

            for (const ActiveRigBox& mine : m_activeBoxScratch)
            {
                BoxState& state = m_boxState[mine.m_index];

                if (mine.m_kind == BoxKind::Hitbox)
                {
                    // Strike: armor shadows the hurtbox (Absorbed instead of Hit).
                    // OnHit / OnHurt fire only here, keeping their exact v1 meaning
                    // ("my hitbox struck a hurtbox"), so existing combat scripts
                    // cannot double-apply damage from a contest result.
                    if (state.m_alreadyHit.find(otherId) == state.m_alreadyHit.end())
                    {
                        const ActiveRigBox* armor = FindOverlapping(mine, theirs, BoxKind::ArmorBox);
                        const ActiveRigBox* hurt = (armor == nullptr) ? FindOverlapping(mine, theirs, BoxKind::Hurtbox) : nullptr;
                        if (armor != nullptr)
                        {
                            state.m_alreadyHit.insert(otherId);
                            FireBoxEvent(BoxResult::Absorbed, self, mine, otherId, *armor, mine.m_hit);
                        }
                        else if (hurt != nullptr)
                        {
                            state.m_alreadyHit.insert(otherId);
                            FireBoxEvent(BoxResult::Hit, self, mine, otherId, *hurt, mine.m_hit);
                            DioramaHitboxNotificationBus::Event(self, &DioramaHitboxNotifications::OnHit, otherId);
                            DioramaHitboxNotificationBus::Event(otherId, &DioramaHitboxNotifications::OnHurt, self);
                        }
                    }

                    // Hit-vs-hit contest: resolved once per pair by the lower entity
                    // id, independent of the strike above so trades keep working.
                    // One event per side, each carrying that side's own payload and
                    // its own box result (Clash both, or Hit for the priority winner
                    // and Beaten for the loser).
                    if (self < otherId && state.m_alreadyContested.find(otherId) == state.m_alreadyContested.end())
                    {
                        if (const ActiveRigBox* opposing = FindOverlapping(mine, theirs, BoxKind::Hitbox))
                        {
                            state.m_alreadyContested.insert(otherId);
                            const HitboxFrames::Resolution outcome =
                                HitboxFrames::Resolve(BoxKind::Hitbox, mine.m_hit.m_priority, BoxKind::Hitbox, opposing->m_hit.m_priority);
                            FireBoxEvent(outcome.m_forA, self, mine, otherId, *opposing, mine.m_hit);
                            FireBoxEvent(outcome.m_forB, otherId, *opposing, self, mine, opposing->m_hit);
                        }
                    }
                }
                else if (mine.m_kind == BoxKind::Throwbox)
                {
                    if (state.m_alreadyHit.find(otherId) == state.m_alreadyHit.end())
                    {
                        if (const ActiveRigBox* throwable = FindOverlapping(mine, theirs, BoxKind::ThrowableBox))
                        {
                            state.m_alreadyHit.insert(otherId);
                            FireBoxEvent(BoxResult::Throw, self, mine, otherId, *throwable, mine.m_hit);
                        }
                    }
                }
                else if (mine.m_kind == BoxKind::ProximityBox)
                {
                    if (state.m_alreadyHit.find(otherId) == state.m_alreadyHit.end())
                    {
                        if (const ActiveRigBox* hurt = FindOverlapping(mine, theirs, BoxKind::Hurtbox))
                        {
                            state.m_alreadyHit.insert(otherId);
                            FireBoxEvent(BoxResult::Proximity, self, mine, otherId, *hurt, mine.m_hit);
                        }
                    }
                }
            }
        }

        // ---- Pass 5: pushbox separation. Full push-out reported; auto-separate
        // applies half so an overlapping pair splits it evenly and converges.
        m_pushOut = AZ::Vector2(0.0f, 0.0f);
        for (const ActiveRigBox& mine : m_activeBoxScratch)
        {
            if (mine.m_kind != BoxKind::Pushbox)
            {
                continue;
            }
            AZ::Vector2 out = AZ::Vector2(0.0f, 0.0f);
            Diorama2DCollisionRequestBus::BroadcastResult(
                out,
                &Diorama2DCollisionRequests::ComputeBoxPushOut,
                mine.m_center.GetX(),
                mine.m_center.GetY(),
                mine.m_halfExtents.GetX(),
                mine.m_halfExtents.GetY(),
                m_config.m_pushLayer,
                self);
            m_pushOut += out;
        }
        if (m_config.m_autoSeparate && !m_pushOut.IsZero())
        {
            ApplyPlaneTranslation(m_pushOut * 0.5f);
        }
    }

    const ActiveRigBox* DioramaHitboxComponent::FindOverlapping(
        const ActiveRigBox& mine, const AZStd::vector<ActiveRigBox>& theirs, HitboxFrames::BoxKind kind) const
    {
        for (const ActiveRigBox& candidate : theirs)
        {
            if (candidate.m_kind == kind &&
                HitboxFrames::BoxesOverlap(mine.m_center, mine.m_halfExtents, candidate.m_center, candidate.m_halfExtents))
            {
                return &candidate;
            }
        }
        return nullptr;
    }

    void DioramaHitboxComponent::FireBoxEvent(
        HitboxFrames::BoxResult result,
        AZ::EntityId attacker,
        const ActiveRigBox& attackerBox,
        AZ::EntityId defender,
        const ActiveRigBox& defenderBox,
        const HitboxFrames::HitProperties& payload)
    {
        const AZ::Vector2 contact =
            HitboxFrames::OverlapCenter(attackerBox.m_center, attackerBox.m_halfExtents, defenderBox.m_center, defenderBox.m_halfExtents);

        DioramaBoxEvent boxEvent;
        boxEvent.m_result = static_cast<int>(result);
        boxEvent.m_attacker = attacker;
        boxEvent.m_defender = defender;
        boxEvent.m_attackerBoxIndex = attackerBox.m_index;
        boxEvent.m_defenderBoxIndex = defenderBox.m_index;
        boxEvent.m_contactX = contact.GetX();
        boxEvent.m_contactY = contact.GetY();
        boxEvent.m_damage = payload.m_damage;
        boxEvent.m_hitstunFrames = payload.m_hitstunFrames;
        boxEvent.m_blockstunFrames = payload.m_blockstunFrames;
        boxEvent.m_hitstopFrames = payload.m_hitstopFrames;
        boxEvent.m_pushbackX = payload.m_pushback.GetX();
        boxEvent.m_pushbackY = payload.m_pushback.GetY();
        boxEvent.m_guardHeight = static_cast<int>(payload.m_guardHeight);
        boxEvent.m_launchX = payload.m_launch.GetX();
        boxEvent.m_launchY = payload.m_launch.GetY();
        boxEvent.m_priority = payload.m_priority;
        boxEvent.m_customId = payload.m_customId;

        DioramaHitboxNotificationBus::Event(attacker, &DioramaHitboxNotifications::OnBoxEvent, boxEvent);
        DioramaHitboxNotificationBus::Event(defender, &DioramaHitboxNotifications::OnBoxEvent, boxEvent);
    }

    void DioramaHitboxComponent::ApplyPlaneTranslation(const AZ::Vector2& delta)
    {
        AZ::Vector3 translation = m_worldTranslation;
        switch (m_config.m_plane)
        {
        case CollisionPlane::XZ:
            translation.SetX(translation.GetX() + delta.GetX());
            translation.SetZ(translation.GetZ() + delta.GetY());
            break;
        case CollisionPlane::YZ:
            translation.SetY(translation.GetY() + delta.GetX());
            translation.SetZ(translation.GetZ() + delta.GetY());
            break;
        case CollisionPlane::XY:
        default:
            translation.SetX(translation.GetX() + delta.GetX());
            translation.SetY(translation.GetY() + delta.GetY());
            break;
        }
        m_worldTranslation = translation;
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, translation);
    }

    AZ::EntityId DioramaHitboxComponent::GetRigEntity() const
    {
        return GetEntityId();
    }

    AZ::u32 DioramaHitboxComponent::GetRigReceiveLayer() const
    {
        return m_config.m_hurtLayer;
    }

    const AZStd::vector<ActiveRigBox>& DioramaHitboxComponent::GetRigActiveBoxes() const
    {
        return m_activeBoxScratch;
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

    void DioramaHitboxComponent::SetAutoSeparate(bool enabled)
    {
        m_config.m_autoSeparate = enabled;
    }

    DioramaHitboxInfo DioramaHitboxComponent::GetHitboxInfo()
    {
        DioramaHitboxInfo info;
        info.m_frame = m_frame;
        info.m_facing = m_config.m_facing;
        info.m_activeHitboxes = m_activeHitboxes;
        info.m_activeHurtboxes = m_activeHurtboxes;
        info.m_activePushboxes = m_activePushboxes;
        info.m_activeThrowboxes = m_activeThrowboxes;
        info.m_activeThrowableBoxes = m_activeThrowableBoxes;
        info.m_activeArmorBoxes = m_activeArmorBoxes;
        info.m_activeProximityBoxes = m_activeProximityBoxes;
        info.m_pushOutX = m_pushOut.GetX();
        info.m_pushOutY = m_pushOut.GetY();
        return info;
    }

    // ---- Snapshot / restore (design phase B) -----------------------------------------
    // Chunk payload: frame (s64), facing (u8: 1 = +X), box count (u32), then per box
    // wasActive (u8) + already-hit count (u32) + that many entity ids (u64) + already-
    // contested count (u32) + that many entity ids (u64). Both sets are written in
    // ascending id order so the image is canonical (the live sets are unordered).

    namespace
    {
        void WriteSortedIdSet(SimState::Writer& writer, const AZStd::unordered_set<AZ::EntityId>& set)
        {
            AZStd::vector<AZ::u64> ids;
            ids.reserve(set.size());
            for (const AZ::EntityId& id : set)
            {
                ids.push_back(static_cast<AZ::u64>(id));
            }
            AZStd::sort(ids.begin(), ids.end());
            writer.U32(static_cast<AZ::u32>(ids.size()));
            for (AZ::u64 id : ids)
            {
                writer.U64(id);
            }
        }

        bool ReadIdSet(SimState::Reader& payload, AZStd::unordered_set<AZ::EntityId>& out)
        {
            AZ::u32 count = 0;
            if (!payload.U32(count) || count > payload.Remaining() / 8)
            {
                return false;
            }
            for (AZ::u32 i = 0; i < count; ++i)
            {
                AZ::u64 raw = 0;
                if (!payload.U64(raw))
                {
                    return false;
                }
                out.insert(AZ::EntityId(raw));
            }
            return true;
        }
    } // namespace

    void DioramaHitboxComponent::SaveSimState(SimState::Writer& writer)
    {
        const size_t sizePos = writer.BeginChunk(HitboxChunkTag);
        writer.S64(m_frame);
        writer.U8(m_config.m_facing < 0 ? 0 : 1);
        writer.U32(static_cast<AZ::u32>(m_boxState.size()));
        for (const BoxState& state : m_boxState)
        {
            writer.U8(state.m_wasActive ? 1 : 0);
            WriteSortedIdSet(writer, state.m_alreadyHit);
            WriteSortedIdSet(writer, state.m_alreadyContested);
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
            if (!payload.U8(wasActive) || !ReadIdSet(payload, restored[i].m_alreadyHit) ||
                !ReadIdSet(payload, restored[i].m_alreadyContested))
            {
                return false;
            }
            restored[i].m_wasActive = wasActive != 0;
        }
        m_frame = static_cast<int>(frame);
        m_config.m_facing = (facing != 0) ? 1 : -1;
        m_boxState = AZStd::move(restored);
        return true;
    }
} // namespace Diorama
