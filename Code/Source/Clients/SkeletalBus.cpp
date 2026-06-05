/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaSkeletalBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectSkeletalBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaSkeletalRequestBus>("DioramaSkeletalRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Skeletal")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("Play", &DioramaSkeletalRequestBus::Events::Play)
            ->Event("Stop", &DioramaSkeletalRequestBus::Events::Stop)
            ->Event(
                "SetNormalizedTime",
                &DioramaSkeletalRequestBus::Events::SetNormalizedTime,
                { { { "normalizedTime", "Point in the clip as 0..1 of the duration; clamped." } } })
            ->Event(
                "SetSpeed",
                &DioramaSkeletalRequestBus::Events::SetSpeed,
                { { { "speed", "Playback rate multiplier; negative plays in reverse." } } })
            ->Event(
                "SetLooping",
                &DioramaSkeletalRequestBus::Events::SetLooping,
                { { { "looping", "Wrap at the end (true) or hold the last frame (false)." } } })
            ->Event(
                "SetDuration",
                &DioramaSkeletalRequestBus::Events::SetDuration,
                { { { "seconds", "Clip length in seconds; clamped > 0." } } })
            ->Event("IsPlaying", &DioramaSkeletalRequestBus::Events::IsPlaying);
    }
} // namespace Diorama
