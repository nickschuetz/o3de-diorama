/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaLookBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectLookBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaLookRequestBus>("DioramaLookRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("SetBloomEnabled", &DioramaLookRequestBus::Events::SetBloomEnabled, { { { "enabled", "Turn HDR bloom on or off." } } })
            ->Event(
                "SetBloomThreshold",
                &DioramaLookRequestBus::Events::SetBloomThreshold,
                { { { "threshold", "HDR brightness above which a pixel blooms; clamped >= 0." } } })
            ->Event(
                "SetBloomIntensity",
                &DioramaLookRequestBus::Events::SetBloomIntensity,
                { { { "intensity", "Bloom strength; clamped >= 0." } } })
            ->Event(
                "SetVignetteEnabled",
                &DioramaLookRequestBus::Events::SetVignetteEnabled,
                { { { "enabled", "Turn the edge darkening on or off." } } })
            ->Event(
                "SetVignetteIntensity",
                &DioramaLookRequestBus::Events::SetVignetteIntensity,
                { { { "intensity", "Vignette strength 0..1." } } });
    }
} // namespace Diorama
