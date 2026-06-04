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
    //! Typed, agent-facing API for the 2D post-processing "look", addressed by its
    //! entity. Reflected Common, so a script, Script Canvas, or an agent tunes the
    //! scene's bloom and vignette the same way it drives sprites, the HUD, and audio.
    //! The component drives Atom's stock PostProcessFeatureProcessor, so values match
    //! the engine's Bloom/Vignette components exactly; this is the one-component,
    //! 2D-friendly front door to them.
    class DioramaLookRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaLookRequests, DioramaLookRequestsTypeId);
        virtual ~DioramaLookRequests() = default;

        //! Turn the glow (HDR bloom) on or off.
        virtual void SetBloomEnabled(bool enabled) = 0;
        //! HDR brightness above which a pixel blooms; clamped >= 0. Sprites written
        //! brighter than this (e.g. via the emissive channel) glow.
        virtual void SetBloomThreshold(float threshold) = 0;
        //! Softness of the bloom threshold edge, 0..1 (clamped).
        virtual void SetBloomKnee(float knee) = 0;
        //! Bloom strength; clamped >= 0.
        virtual void SetBloomIntensity(float intensity) = 0;
        //! Turn the edge darkening (vignette) on or off.
        virtual void SetVignetteEnabled(bool enabled) = 0;
        //! Vignette strength, 0..1 (clamped).
        virtual void SetVignetteIntensity(float intensity) = 0;
    };

    using DioramaLookRequestBus = AZ::EBus<DioramaLookRequests>;

    //! Reflect the look bus to the BehaviorContext (Common scope). Called from the
    //! look component's Reflect.
    void ReflectLookBuses(AZ::ReflectContext* context);
} // namespace Diorama
