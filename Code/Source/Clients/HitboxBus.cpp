/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaHitboxBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaHitboxInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaHitboxInfo>()
                ->Version(2)
                ->Field("frame", &DioramaHitboxInfo::m_frame)
                ->Field("facing", &DioramaHitboxInfo::m_facing)
                ->Field("activeHitboxes", &DioramaHitboxInfo::m_activeHitboxes)
                ->Field("activeHurtboxes", &DioramaHitboxInfo::m_activeHurtboxes)
                ->Field("activePushboxes", &DioramaHitboxInfo::m_activePushboxes)
                ->Field("activeThrowboxes", &DioramaHitboxInfo::m_activeThrowboxes)
                ->Field("activeThrowableBoxes", &DioramaHitboxInfo::m_activeThrowableBoxes)
                ->Field("activeArmorBoxes", &DioramaHitboxInfo::m_activeArmorBoxes)
                ->Field("activeProximityBoxes", &DioramaHitboxInfo::m_activeProximityBoxes)
                ->Field("pushOutX", &DioramaHitboxInfo::m_pushOutX)
                ->Field("pushOutY", &DioramaHitboxInfo::m_pushOutY);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<DioramaHitboxInfo>("DioramaHitboxInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Hitbox")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("frame", BehaviorValueGetter(&DioramaHitboxInfo::m_frame), nullptr)
                ->Property("facing", BehaviorValueGetter(&DioramaHitboxInfo::m_facing), nullptr)
                ->Property("activeHitboxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeHitboxes), nullptr)
                ->Property("activeHurtboxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeHurtboxes), nullptr)
                ->Property("activePushboxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activePushboxes), nullptr)
                ->Property("activeThrowboxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeThrowboxes), nullptr)
                ->Property("activeThrowableBoxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeThrowableBoxes), nullptr)
                ->Property("activeArmorBoxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeArmorBoxes), nullptr)
                ->Property("activeProximityBoxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeProximityBoxes), nullptr)
                ->Property("pushOutX", BehaviorValueGetter(&DioramaHitboxInfo::m_pushOutX), nullptr)
                ->Property("pushOutY", BehaviorValueGetter(&DioramaHitboxInfo::m_pushOutY), nullptr);
        }
    }

    void DioramaBoxEvent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaBoxEvent>()
                ->Version(1)
                ->Field("result", &DioramaBoxEvent::m_result)
                ->Field("attacker", &DioramaBoxEvent::m_attacker)
                ->Field("defender", &DioramaBoxEvent::m_defender)
                ->Field("attackerBoxIndex", &DioramaBoxEvent::m_attackerBoxIndex)
                ->Field("defenderBoxIndex", &DioramaBoxEvent::m_defenderBoxIndex)
                ->Field("contactX", &DioramaBoxEvent::m_contactX)
                ->Field("contactY", &DioramaBoxEvent::m_contactY)
                ->Field("damage", &DioramaBoxEvent::m_damage)
                ->Field("hitstunFrames", &DioramaBoxEvent::m_hitstunFrames)
                ->Field("blockstunFrames", &DioramaBoxEvent::m_blockstunFrames)
                ->Field("hitstopFrames", &DioramaBoxEvent::m_hitstopFrames)
                ->Field("pushbackX", &DioramaBoxEvent::m_pushbackX)
                ->Field("pushbackY", &DioramaBoxEvent::m_pushbackY)
                ->Field("guardHeight", &DioramaBoxEvent::m_guardHeight)
                ->Field("launchX", &DioramaBoxEvent::m_launchX)
                ->Field("launchY", &DioramaBoxEvent::m_launchY)
                ->Field("priority", &DioramaBoxEvent::m_priority)
                ->Field("customId", &DioramaBoxEvent::m_customId);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            // Read-only, like every Diorama event/Info payload: each field is a getter
            // with no setter, so scripts read it and it lists once in node palettes.
            behaviorContext->Class<DioramaBoxEvent>("DioramaBoxEvent")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Hitbox")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("result", BehaviorValueGetter(&DioramaBoxEvent::m_result), nullptr)
                ->Property("attacker", BehaviorValueGetter(&DioramaBoxEvent::m_attacker), nullptr)
                ->Property("defender", BehaviorValueGetter(&DioramaBoxEvent::m_defender), nullptr)
                ->Property("attackerBoxIndex", BehaviorValueGetter(&DioramaBoxEvent::m_attackerBoxIndex), nullptr)
                ->Property("defenderBoxIndex", BehaviorValueGetter(&DioramaBoxEvent::m_defenderBoxIndex), nullptr)
                ->Property("contactX", BehaviorValueGetter(&DioramaBoxEvent::m_contactX), nullptr)
                ->Property("contactY", BehaviorValueGetter(&DioramaBoxEvent::m_contactY), nullptr)
                ->Property("damage", BehaviorValueGetter(&DioramaBoxEvent::m_damage), nullptr)
                ->Property("hitstunFrames", BehaviorValueGetter(&DioramaBoxEvent::m_hitstunFrames), nullptr)
                ->Property("blockstunFrames", BehaviorValueGetter(&DioramaBoxEvent::m_blockstunFrames), nullptr)
                ->Property("hitstopFrames", BehaviorValueGetter(&DioramaBoxEvent::m_hitstopFrames), nullptr)
                ->Property("pushbackX", BehaviorValueGetter(&DioramaBoxEvent::m_pushbackX), nullptr)
                ->Property("pushbackY", BehaviorValueGetter(&DioramaBoxEvent::m_pushbackY), nullptr)
                ->Property("guardHeight", BehaviorValueGetter(&DioramaBoxEvent::m_guardHeight), nullptr)
                ->Property("launchX", BehaviorValueGetter(&DioramaBoxEvent::m_launchX), nullptr)
                ->Property("launchY", BehaviorValueGetter(&DioramaBoxEvent::m_launchY), nullptr)
                ->Property("priority", BehaviorValueGetter(&DioramaBoxEvent::m_priority), nullptr)
                ->Property("customId", BehaviorValueGetter(&DioramaBoxEvent::m_customId), nullptr);
        }
    }

    void ReflectHitboxBuses(AZ::ReflectContext* context)
    {
        DioramaHitboxInfo::Reflect(context);
        DioramaBoxEvent::Reflect(context);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaHitboxRequestBus>("DioramaHitboxRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Hitbox")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetFacing",
                &DioramaHitboxRequestBus::Events::SetFacing,
                { { { "facing", "+1 faces +X (right), -1 faces -X (left); mirrors box offsets." } } })
            ->Event("GetFacing", &DioramaHitboxRequestBus::Events::GetFacing)
            ->Event(
                "SetFrame",
                &DioramaHitboxRequestBus::Events::SetFrame,
                { { { "frame", "Animation frame driving box activation (for rigs not driven by the sprite player)." } } })
            ->Event(
                "SetUseSimClock",
                &DioramaHitboxRequestBus::Events::SetUseSimClock,
                { { { "enabled",
                      "Evaluate overlaps on the 2D Simulation Clock's fixed steps instead of the render tick; "
                      "with no clock in the level the render tick still evaluates." } } })
            ->Event(
                "SetAutoSeparate",
                &DioramaHitboxRequestBus::Events::SetAutoSeparate,
                { { { "enabled",
                      "Apply half the computed pushbox push-out to this entity each evaluation (pairs split the "
                      "separation and converge); off (default) only reports it in GetHitboxInfo." } } })
            ->Event("GetHitboxInfo", &DioramaHitboxRequestBus::Events::GetHitboxInfo);

        behaviorContext->EBus<DioramaHitboxNotificationBus>("DioramaHitboxNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Hitbox")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaHitboxNotificationHandler>();
    }
} // namespace Diorama
