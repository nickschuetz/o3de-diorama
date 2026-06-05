/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaParallaxBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void ParallaxInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ParallaxInfo>()
                ->Version(1)
                ->Field("hasCamera", &ParallaxInfo::m_hasCamera)
                ->Field("factor", &ParallaxInfo::m_factor)
                ->Field("enabled", &ParallaxInfo::m_enabled);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ParallaxInfo>("DioramaParallaxInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Parallax")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("hasCamera", BehaviorValueGetter(&ParallaxInfo::m_hasCamera), nullptr)
                ->Property("factor", BehaviorValueGetter(&ParallaxInfo::m_factor), nullptr)
                ->Property("enabled", BehaviorValueGetter(&ParallaxInfo::m_enabled), nullptr);
        }
    }

    void ReflectParallaxBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaParallaxRequestBus>("DioramaParallaxRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Parallax")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetCamera",
                &DioramaParallaxRequestBus::Events::SetCamera,
                { { { "camera", "Entity the parallax is measured relative to (usually the camera)." } } })
            ->Event(
                "SetFactor",
                &DioramaParallaxRequestBus::Events::SetFactor,
                { { { "factor", "0 = far background (follows camera), 1 = foreground (fixed in world); clamped 0..1." } } })
            ->Event(
                "SetEnabled",
                &DioramaParallaxRequestBus::Events::SetEnabled,
                { { { "enabled", "When false the layer stays at its authored position." } } })
            ->Event("GetParallaxInfo", &DioramaParallaxRequestBus::Events::GetParallaxInfo);
    }
} // namespace Diorama
