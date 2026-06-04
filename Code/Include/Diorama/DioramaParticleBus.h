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
    //! Resolved runtime state of a particle emitter, returned by GetParticleInfo.
    //! The verify-loop payload: an agent can confirm a burst happened and the pool
    //! is bounded without a viewport.
    struct ParticleInfo
    {
        AZ_TYPE_INFO(Diorama::ParticleInfo, ParticleInfoTypeId);

        static void Reflect(AZ::ReflectContext* context);

        int m_aliveCount = 0; //!< Particles currently alive.
        int m_maxParticles = 0; //!< Hard pool capacity.
        bool m_playing = false; //!< Whether continuous emission is running.
        float m_rate = 0.0f; //!< Continuous emission rate (particles/sec).
        bool m_textureLoaded = false; //!< Whether the particle texture has streamed in.
    };

    //! Stable, typed, agent-facing API for a 2D particle emitter, addressed by the
    //! emitter entity's id. Plain scalars, forgiving and clamped, in the same style
    //! as the sprite/light/camera buses. Emit/Burst are the gameplay hooks.
    class DioramaParticleRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaParticleRequests, DioramaParticleRequestsTypeId);
        virtual ~DioramaParticleRequests() = default;

        //! Spawn count particles now (clamped to remaining pool capacity).
        virtual void Emit(int count) = 0;
        //! Spawn the configured burst count now.
        virtual void Burst() = 0;
        //! Start continuous emission at the configured rate.
        virtual void Play() = 0;
        //! Stop continuous emission (live particles finish their life).
        virtual void Stop() = 0;
        //! Continuous emission rate in particles/sec; clamped non-negative.
        virtual void SetRate(float particlesPerSecond) = 0;
        //! Constant acceleration applied to every particle (world units/sec^2).
        virtual void SetGravity(float x, float y) = 0;
        //! Per-particle life range in seconds (clamped non-negative, min<=max).
        virtual void SetLifetime(float minSeconds, float maxSeconds) = 0;
        //! Initial speed range in world units/sec (clamped non-negative, min<=max).
        virtual void SetSpeed(float minSpeed, float maxSpeed) = 0;
        //! Per-particle angular velocity range in radians/sec (signed; min/max as given).
        virtual void SetSpin(float minRadiansPerSecond, float maxRadiansPerSecond) = 0;
        //! Emission direction (degrees) and cone spread (degrees; 360 = radial).
        virtual void SetDirection(float degrees, float spreadDegrees) = 0;
        //! Color at birth (channels clamped 0..1).
        virtual void SetStartColor(float r, float g, float b, float a) = 0;
        //! Color at death (channels clamped 0..1).
        virtual void SetEndColor(float r, float g, float b, float a) = 0;
        //! Size at birth / death in world units (clamped non-negative).
        virtual void SetStartSize(float size) = 0;
        virtual void SetEndSize(float size) = 0;
        //! Assign the particle texture by product path; false if it does not resolve.
        virtual bool SetTextureByPath(AZStd::string_view productPath) = 0;
        //! Resolved emitter state. Safe to poll.
        virtual ParticleInfo GetParticleInfo() = 0;
    };

    using DioramaParticleRequestBus = AZ::EBus<DioramaParticleRequests>;

    //! Reflect the agent-facing particle bus and ParticleInfo to the BehaviorContext
    //! (Common scope) so they are callable from launcher Lua, Python, and Script
    //! Canvas. Called from the emitter component's Reflect.
    void ReflectParticleBuses(AZ::ReflectContext* context);
} // namespace Diorama
