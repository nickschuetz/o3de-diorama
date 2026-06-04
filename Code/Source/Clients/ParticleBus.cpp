/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaParticleBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void ParticleInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ParticleInfo>()
                ->Version(1)
                ->Field("aliveCount", &ParticleInfo::m_aliveCount)
                ->Field("maxParticles", &ParticleInfo::m_maxParticles)
                ->Field("playing", &ParticleInfo::m_playing)
                ->Field("rate", &ParticleInfo::m_rate)
                ->Field("textureLoaded", &ParticleInfo::m_textureLoaded);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<ParticleInfo>("DioramaParticleInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("aliveCount", BehaviorValueProperty(&ParticleInfo::m_aliveCount))
                ->Property("maxParticles", BehaviorValueProperty(&ParticleInfo::m_maxParticles))
                ->Property("playing", BehaviorValueProperty(&ParticleInfo::m_playing))
                ->Property("rate", BehaviorValueProperty(&ParticleInfo::m_rate))
                ->Property("textureLoaded", BehaviorValueProperty(&ParticleInfo::m_textureLoaded));
        }
    }

    void ReflectParticleBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext
            ->EBus<DioramaParticleRequestBus>("DioramaParticleRequestBus")
            // Common scope so it is callable from editor Python and runtime script.
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "Emit",
                &DioramaParticleRequestBus::Events::Emit,
                { { { "count", "Number of particles to spawn now (clamped to remaining pool capacity)." } } })
            ->Event("Burst", &DioramaParticleRequestBus::Events::Burst)
            ->Event("Play", &DioramaParticleRequestBus::Events::Play)
            ->Event("Stop", &DioramaParticleRequestBus::Events::Stop)
            ->Event(
                "SetRate",
                &DioramaParticleRequestBus::Events::SetRate,
                { { { "particlesPerSecond", "Continuous emission rate; clamped non-negative." } } })
            ->Event(
                "SetGravity",
                &DioramaParticleRequestBus::Events::SetGravity,
                { { { "x", "Gravity X (world units/sec^2)." }, { "y", "Gravity Y (world units/sec^2)." } } })
            ->Event(
                "SetLifetime",
                &DioramaParticleRequestBus::Events::SetLifetime,
                { { { "minSeconds", "Shortest particle life; clamped non-negative." },
                    { "maxSeconds", "Longest particle life; clamped to >= min." } } })
            ->Event(
                "SetSpeed",
                &DioramaParticleRequestBus::Events::SetSpeed,
                { { { "minSpeed", "Slowest initial speed; clamped non-negative." },
                    { "maxSpeed", "Fastest initial speed; clamped to >= min." } } })
            ->Event(
                "SetSpin",
                &DioramaParticleRequestBus::Events::SetSpin,
                { { { "minRadiansPerSecond", "Slowest per-particle spin (signed; negative = clockwise)." },
                    { "maxRadiansPerSecond", "Fastest per-particle spin (signed)." } } })
            ->Event(
                "SetDirection",
                &DioramaParticleRequestBus::Events::SetDirection,
                { { { "degrees", "Emission direction in degrees (0 = +X, 90 = +Y)." },
                    { "spreadDegrees", "Cone spread in degrees (360 = radial); clamped 0..360." } } })
            ->Event(
                "SetStartColor",
                &DioramaParticleRequestBus::Events::SetStartColor,
                { { { "r", "Red 0..1." }, { "g", "Green 0..1." }, { "b", "Blue 0..1." }, { "a", "Alpha 0..1." } } })
            ->Event(
                "SetEndColor",
                &DioramaParticleRequestBus::Events::SetEndColor,
                { { { "r", "Red 0..1." }, { "g", "Green 0..1." }, { "b", "Blue 0..1." }, { "a", "Alpha 0..1 (0 fades out)." } } })
            ->Event(
                "SetStartSize",
                &DioramaParticleRequestBus::Events::SetStartSize,
                { { { "size", "Size at birth (world units); clamped non-negative." } } })
            ->Event(
                "SetEndSize",
                &DioramaParticleRequestBus::Events::SetEndSize,
                { { { "size", "Size at death (world units); clamped non-negative." } } })
            ->Event(
                "SetTextureByPath",
                &DioramaParticleRequestBus::Events::SetTextureByPath,
                { { { "productPath", "Particle texture product path, e.g. 'diorama/textures/spark.png'." } } })
            ->Event("GetParticleInfo", &DioramaParticleRequestBus::Events::GetParticleInfo);
    }
} // namespace Diorama
