/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaAsepriteBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Diorama
{
    void ReflectAsepriteBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaAsepriteRequestBus>("DioramaAsepriteRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Aseprite")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "PlayTag",
                &DioramaAsepriteRequestBus::Events::PlayTag,
                { { { "tagName", "Name of the Aseprite tag to play from its start." } } })
            ->Event("Play", &DioramaAsepriteRequestBus::Events::Play)
            ->Event("Stop", &DioramaAsepriteRequestBus::Events::Stop)
            ->Event(
                "SetFrame",
                &DioramaAsepriteRequestBus::Events::SetFrame,
                { { { "frameIndex", "Frame index to show; clamped to the sheet." } } })
            ->Event(
                "SetSpeed",
                &DioramaAsepriteRequestBus::Events::SetSpeed,
                { { { "speed", "Playback rate multiplier; negative reverses." } } })
            ->Event(
                "SetLooping",
                &DioramaAsepriteRequestBus::Events::SetLooping,
                { { { "looping", "Wrap the current tag (true) or hold the last frame." } } })
            ->Event("IsPlaying", &DioramaAsepriteRequestBus::Events::IsPlaying)
            ->Event("GetCurrentFrame", &DioramaAsepriteRequestBus::Events::GetCurrentFrame);
    }
} // namespace Diorama
