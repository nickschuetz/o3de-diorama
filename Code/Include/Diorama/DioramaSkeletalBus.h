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

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Typed, agent-facing API for the cutout skeletal clip player, addressed by its
    //! entity. Reflected Common, so a script, Script Canvas, or an agent drives a
    //! character's animation the same way it drives sprites, the HUD, and audio. The
    //! component animates a transform hierarchy of sprite "bones" (descendant entities)
    //! from a keyframed clip; these verbs control playback of that clip at runtime.
    class DioramaSkeletalRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaSkeletalRequests, DioramaSkeletalRequestsTypeId);
        virtual ~DioramaSkeletalRequests() = default;

        //! Start (or resume) playback from the current time.
        virtual void Play() = 0;
        //! Stop playback and hold the current pose.
        virtual void Stop() = 0;
        //! Jump to a point in the clip as a 0..1 fraction of the duration (clamped).
        //! Applies the pose immediately whether or not it is playing.
        virtual void SetNormalizedTime(float normalizedTime) = 0;
        //! Playback rate multiplier (1 = real time). Negative plays in reverse.
        virtual void SetSpeed(float speed) = 0;
        //! Whether the clip wraps at the end (true) or stops on the last frame (false).
        virtual void SetLooping(bool looping) = 0;
        //! Clip length in seconds; the time base SetNormalizedTime maps onto. Clamped > 0.
        virtual void SetDuration(float seconds) = 0;
        //! True while the clip is advancing.
        virtual bool IsPlaying() = 0;
    };

    using DioramaSkeletalRequestBus = AZ::EBus<DioramaSkeletalRequests>;

    //! Reflect the skeletal bus to the BehaviorContext (Common scope). Called from the
    //! skeletal component's Reflect.
    void ReflectSkeletalBuses(AZ::ReflectContext* context);
} // namespace Diorama
