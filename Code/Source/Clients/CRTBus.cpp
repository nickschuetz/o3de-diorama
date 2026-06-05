/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaCRTBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectCRTBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaCRTRequestBus>("DioramaCRTRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/CRT")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("SetEnabled", &DioramaCRTRequestBus::Events::SetEnabled, { { { "enabled", "Show or hide the overlay." } } })
            ->Event(
                "SetScanlineDarkness",
                &DioramaCRTRequestBus::Events::SetScanlineDarkness,
                { { { "darkness", "Scanline darkness 0..1." } } })
            ->Event(
                "SetScanlineSpacing",
                &DioramaCRTRequestBus::Events::SetScanlineSpacing,
                { { { "pixels", "Scanline spacing in pixels (>= 1)." } } });
    }
} // namespace Diorama
