/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaAudioBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectAudioBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext
            ->EBus<DioramaAudioRequestBus>("DioramaAudioRequestBus")
            // Common scope so it is callable from editor Python and runtime script.
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Audio")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "PlayOneShot",
                &DioramaAudioRequestBus::Events::PlayOneShot,
                { { { "productPath", "Sound product path, e.g. 'diorama/audio/blip.wav'." }, { "volume", "Volume 0..1 (clamped)." } } })
            ->Event(
                "SetMasterVolume",
                &DioramaAudioRequestBus::Events::SetMasterVolume,
                { { { "volume", "Master volume for all audio, 0..1 (clamped)." } } });
    }
} // namespace Diorama
