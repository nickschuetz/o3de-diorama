/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Typed, agent-facing API for the Aseprite animation player, addressed by its
    //! entity. Reflected Common, so a script, Script Canvas, or an agent plays a
    //! character's named animations (Aseprite tags) the same way it drives every other
    //! Diorama feature. The component drives a Sprite on the same entity, setting its
    //! texture to the imported atlas and its UV region to the current frame.
    class DioramaAsepriteRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaAsepriteRequests, DioramaAsepriteRequestsTypeId);
        virtual ~DioramaAsepriteRequests() = default;

        //! Play the named Aseprite tag from its start. Unknown name -> no change.
        virtual void PlayTag(const AZStd::string& tagName) = 0;
        //! Resume playback of the current tag.
        virtual void Play() = 0;
        //! Stop playback and hold the current frame.
        virtual void Stop() = 0;
        //! Show a specific frame index (clamped) and stop.
        virtual void SetFrame(int frameIndex) = 0;
        //! Playback rate multiplier (1 = the clip's authored timing). Negative reverses.
        virtual void SetSpeed(float speed) = 0;
        //! Whether the current tag wraps at its end or holds the last frame.
        virtual void SetLooping(bool looping) = 0;
        //! True while a tag is advancing.
        virtual bool IsPlaying() = 0;
        //! The frame index currently shown.
        virtual int GetCurrentFrame() = 0;
    };

    using DioramaAsepriteRequestBus = AZ::EBus<DioramaAsepriteRequests>;

    //! Reflect the Aseprite bus to the BehaviorContext (Common scope). Called from the
    //! Aseprite component's Reflect.
    void ReflectAsepriteBuses(AZ::ReflectContext* context);
} // namespace Diorama
