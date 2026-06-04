/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/Particles2D.h>
#include <Diorama/DioramaParticleBus.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Random.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/std/containers/vector.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace Diorama
{
    class SpriteFeatureProcessor;

    //! Shared configuration for a 2D particle emitter. Authored by the editor twin
    //! and handed to the runtime ParticleEmitterComponent.
    class DioramaParticleConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaParticleConfig, DioramaParticleConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaParticleConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaParticleConfig() override = default;

        //! Particle texture (a soft dot, spark, heart, ...). Each particle is a
        //! billboarded quad of this texture, so a whole burst is one draw call.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_texture;
        //! Hard pool capacity (clamped). The pool never grows past this.
        int m_maxParticles = 256;
        //! Continuous emission rate (particles/sec). 0 = burst-only.
        float m_rate = 0.0f;
        //! Count emitted by Burst() / on activate when m_emitOnActivate.
        int m_burstCount = 24;
        //! Start continuous emission and fire one burst on activate.
        bool m_emitOnActivate = true;

        // Per-particle randomized ranges.
        float m_lifetimeMin = 0.6f;
        float m_lifetimeMax = 1.2f;
        float m_speedMin = 2.0f;
        float m_speedMax = 5.0f;
        //! Emission direction (degrees, 0 = +X) and cone spread (degrees; 360 = radial).
        float m_directionDegrees = 90.0f;
        float m_spreadDegrees = 360.0f;
        float m_spinMin = 0.0f; //!< angular velocity range (radians/sec)
        float m_spinMax = 0.0f;

        // Forces + over-life ramps (fed straight into Particles2D::EmitterParams).
        AZ::Vector2 m_gravity = AZ::Vector2(0.0f, -4.0f);
        float m_drag = 0.0f;
        float m_startSize = 0.6f;
        float m_endSize = 0.0f;
        AZ::Color m_startColor = AZ::Color(1.0f, 0.9f, 0.4f, 1.0f);
        AZ::Color m_endColor = AZ::Color(1.0f, 0.2f, 0.0f, 0.0f);

        //! Transparent draw-order bias for the emitter's particles.
        float m_sortOffset = 1.0f;
    };

    //! Runtime 2D particle emitter. Simulates a pooled set of particles on the CPU
    //! with the unit-tested Particles2D core and renders each as a billboarded sprite
    //! quad through the SpriteFeatureProcessor (same texture + sort layer = one draw
    //! call). No new render path. The pool is fixed-capacity, so no script or asset
    //! value can spawn unboundedly or size a buffer.
    class ParticleEmitterComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaParticleRequestBus::Handler
        , protected AZ::Data::AssetBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::ParticleEmitterComponent, DioramaParticleEmitterComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        ParticleEmitterComponent() = default;
        explicit ParticleEmitterComponent(const DioramaParticleConfig& config);
        ~ParticleEmitterComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::Data::AssetBus
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // DioramaParticleRequests
        void Emit(int count) override;
        void Burst() override;
        void Play() override;
        void Stop() override;
        void SetRate(float particlesPerSecond) override;
        void SetGravity(float x, float y) override;
        void SetLifetime(float minSeconds, float maxSeconds) override;
        void SetSpeed(float minSpeed, float maxSpeed) override;
        void SetSpin(float minRadiansPerSecond, float maxRadiansPerSecond) override;
        void SetDirection(float degrees, float spreadDegrees) override;
        void SetStartColor(float r, float g, float b, float a) override;
        void SetEndColor(float r, float g, float b, float a) override;
        void SetStartSize(float size) override;
        void SetEndSize(float size) override;
        bool SetTextureByPath(AZStd::string_view productPath) override;
        ParticleInfo GetParticleInfo() override;

    private:
        //! Resolve the scene feature processor and pre-acquire the handle pool.
        bool TryAcquireFeatureProcessor();
        //! Build EmitterParams from the config (forces + over-life ramps).
        Particles2D::EmitterParams MakeParams() const;
        //! Spawn one particle at the emitter's world position with randomized motion.
        void SpawnOne();
        //! Push current particle quads to the feature processor (visible for live
        //! slots, zero-size for the rest).
        void PushToRenderer();
        void QueueTextureLoad();

        DioramaParticleConfig m_config;
        AZStd::vector<Particles2D::Particle> m_particles;
        //! One sprite handle per pool slot (index-aligned with m_particles while it
        //! is packed by StepPool's swap-and-pop). Pre-acquired in Activate.
        AZStd::vector<AZ::u32> m_handles;
        SpriteFeatureProcessor* m_featureProcessor = nullptr;
        AZ::SimpleLcgRandom m_rng;
        float m_emitAccumulator = 0.0f; //!< fractional carry for continuous emission
        bool m_playing = false;
        bool m_acquired = false;
    };
} // namespace Diorama
