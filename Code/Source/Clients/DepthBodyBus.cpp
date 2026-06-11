/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaDepthBodyBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaDepthBodyInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaDepthBodyInfo>()
                ->Version(1)
                ->Field("depth", &DioramaDepthBodyInfo::m_depth)
                ->Field("screenLift", &DioramaDepthBodyInfo::m_screenLift)
                ->Field("sortBias", &DioramaDepthBodyInfo::m_sortBias);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<DioramaDepthBodyInfo>("DioramaDepthBodyInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/DepthBody")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("depth", BehaviorValueGetter(&DioramaDepthBodyInfo::m_depth), nullptr)
                ->Property("screenLift", BehaviorValueGetter(&DioramaDepthBodyInfo::m_screenLift), nullptr)
                ->Property("sortBias", BehaviorValueGetter(&DioramaDepthBodyInfo::m_sortBias), nullptr);
        }
    }

    void ReflectDepthBodyBuses(AZ::ReflectContext* context)
    {
        DioramaDepthBodyInfo::Reflect(context);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaDepthBodyRequestBus>("DioramaDepthBodyRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/DepthBody")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("SetDepth", &DioramaDepthBodyRequestBus::Events::SetDepth, { { { "depth", "Depth lane (toward/away from camera)." } } })
            ->Event(
                "MoveDepthToward",
                &DioramaDepthBodyRequestBus::Events::MoveDepthToward,
                { { { "targetDepth", "Lane to approach." }, { "maxDelta", "Max change this call (>= 0)." } } })
            ->Event("GetDepth", &DioramaDepthBodyRequestBus::Events::GetDepth)
            ->Event("GetDepthInfo", &DioramaDepthBodyRequestBus::Events::GetDepthInfo);
    }
} // namespace Diorama
