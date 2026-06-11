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
                ->Version(1)
                ->Field("frame", &DioramaHitboxInfo::m_frame)
                ->Field("facing", &DioramaHitboxInfo::m_facing)
                ->Field("activeHitboxes", &DioramaHitboxInfo::m_activeHitboxes)
                ->Field("activeHurtboxes", &DioramaHitboxInfo::m_activeHurtboxes);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<DioramaHitboxInfo>("DioramaHitboxInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Hitbox")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("frame", BehaviorValueGetter(&DioramaHitboxInfo::m_frame), nullptr)
                ->Property("facing", BehaviorValueGetter(&DioramaHitboxInfo::m_facing), nullptr)
                ->Property("activeHitboxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeHitboxes), nullptr)
                ->Property("activeHurtboxes", BehaviorValueGetter(&DioramaHitboxInfo::m_activeHurtboxes), nullptr);
        }
    }

    void ReflectHitboxBuses(AZ::ReflectContext* context)
    {
        DioramaHitboxInfo::Reflect(context);

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
            ->Event("GetHitboxInfo", &DioramaHitboxRequestBus::Events::GetHitboxInfo);

        behaviorContext->EBus<DioramaHitboxNotificationBus>("DioramaHitboxNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Hitbox")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaHitboxNotificationHandler>();
    }
} // namespace Diorama
