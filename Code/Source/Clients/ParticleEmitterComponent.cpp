/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/ParticleEmitterComponent.h>
#include <Clients/SpriteFeatureProcessor.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

namespace Diorama
{
    namespace
    {
        constexpr float ParticleDegToRad = 0.01745329252f;
        constexpr int ParticleMaxCap = 4096; // hard ceiling on the pool, whatever the config asks
        constexpr float MaxTimeStep = 0.1f; // clamp the per-tick delta (hitch / non-finite guard)

        float ParticleNonNeg(float v)
        {
            return v < 0.0f ? 0.0f : v;
        }
        float ParticleClamp01(float v)
        {
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }
    } // namespace

    void DioramaParticleConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaParticleConfig>()
                ->Version(1)
                ->Field("texture", &DioramaParticleConfig::m_texture)
                ->Field("maxParticles", &DioramaParticleConfig::m_maxParticles)
                ->Field("rate", &DioramaParticleConfig::m_rate)
                ->Field("burstCount", &DioramaParticleConfig::m_burstCount)
                ->Field("emitOnActivate", &DioramaParticleConfig::m_emitOnActivate)
                ->Field("lifetimeMin", &DioramaParticleConfig::m_lifetimeMin)
                ->Field("lifetimeMax", &DioramaParticleConfig::m_lifetimeMax)
                ->Field("speedMin", &DioramaParticleConfig::m_speedMin)
                ->Field("speedMax", &DioramaParticleConfig::m_speedMax)
                ->Field("directionDegrees", &DioramaParticleConfig::m_directionDegrees)
                ->Field("spreadDegrees", &DioramaParticleConfig::m_spreadDegrees)
                ->Field("spinMin", &DioramaParticleConfig::m_spinMin)
                ->Field("spinMax", &DioramaParticleConfig::m_spinMax)
                ->Field("gravity", &DioramaParticleConfig::m_gravity)
                ->Field("drag", &DioramaParticleConfig::m_drag)
                ->Field("startSize", &DioramaParticleConfig::m_startSize)
                ->Field("endSize", &DioramaParticleConfig::m_endSize)
                ->Field("startColor", &DioramaParticleConfig::m_startColor)
                ->Field("endColor", &DioramaParticleConfig::m_endColor)
                ->Field("sortOffset", &DioramaParticleConfig::m_sortOffset);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<DioramaParticleConfig>("2D Particle Config", "Emission, motion, and over-life ramps for a particle emitter")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_texture,
                        "Texture",
                        "Particle texture (a soft dot, spark, heart, ...)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaParticleConfig::m_maxParticles, "Max Particles", "Hard pool capacity")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, ParticleMaxCap)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_rate,
                        "Rate",
                        "Continuous emission (particles/sec); 0 = burst-only")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_burstCount,
                        "Burst Count",
                        "Particles emitted by Burst() / on activate")
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaParticleConfig::m_emitOnActivate,
                        "Emit On Activate",
                        "Start emitting / fire a burst when the game starts")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_lifetimeMin,
                        "Lifetime Min",
                        "Shortest particle life (seconds)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_lifetimeMax,
                        "Lifetime Max",
                        "Longest particle life (seconds)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_speedMin,
                        "Speed Min",
                        "Slowest initial speed (world units/sec)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_speedMax,
                        "Speed Max",
                        "Fastest initial speed (world units/sec)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_spinMin,
                        "Spin Min",
                        "Slowest per-particle spin (radians/sec; signed, negative = clockwise)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_spinMax,
                        "Spin Max",
                        "Fastest per-particle spin (radians/sec; signed)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_directionDegrees,
                        "Direction",
                        "Emission direction in degrees (0 = +X, 90 = +Y)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_spreadDegrees,
                        "Spread",
                        "Cone spread in degrees (360 = radial)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 360.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_gravity,
                        "Gravity",
                        "Constant acceleration on every particle (world units/sec^2)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_drag,
                        "Drag",
                        "Fraction of velocity shed per second (0 = none)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaParticleConfig::m_startSize, "Start Size", "Size at birth (world units)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaParticleConfig::m_endSize, "End Size", "Size at death (world units)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaParticleConfig::m_startColor, "Start Color", "Color at birth")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color,
                        &DioramaParticleConfig::m_endColor,
                        "End Color",
                        "Color at death (drop alpha to fade out)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaParticleConfig::m_sortOffset,
                        "Sort Offset",
                        "Transparent draw-order bias; larger draws on top");
            }
        }
    }

    ParticleEmitterComponent::ParticleEmitterComponent(const DioramaParticleConfig& config)
        : m_config(config)
    {
    }

    void ParticleEmitterComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaParticleConfig::Reflect(context);
        ParticleInfo::Reflect(context);
        ReflectParticleBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ParticleEmitterComponent, AZ::Component>()->Version(1)->Field(
                "Config", &ParticleEmitterComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                // No AppearsInAddComponentMenu: built from the editor twin via
                // BuildGameEntity, like the other runtime Diorama components.
                editContext
                    ->Class<ParticleEmitterComponent>("2D Particle Emitter", "Pooled 2D particle emitter rendered through the sprite batch")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ParticleEmitterComponent::m_config, "Config", "Emitter configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void ParticleEmitterComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void ParticleEmitterComponent::Activate()
    {
        // Clamp the pool capacity before anything sizes a buffer with it.
        if (m_config.m_maxParticles < 1)
        {
            m_config.m_maxParticles = 1;
        }
        if (m_config.m_maxParticles > ParticleMaxCap)
        {
            m_config.m_maxParticles = ParticleMaxCap;
        }
        m_particles.clear();
        m_particles.reserve(static_cast<size_t>(m_config.m_maxParticles));
        m_emitAccumulator = 0.0f;
        m_acquired = false;

        QueueTextureLoad();

        // Continuous emission + a one-shot burst start when the game does, but only
        // once the feature processor is acquired (deferred to the first tick if the
        // scene is not ready yet, mirroring SpritePresenter).
        m_playing = m_config.m_emitOnActivate && m_config.m_rate > 0.0f;

        AZ::TickBus::Handler::BusConnect();
        DioramaParticleRequestBus::Handler::BusConnect(GetEntityId());

        TryAcquireFeatureProcessor();
        if (m_acquired && m_config.m_emitOnActivate && m_config.m_burstCount > 0)
        {
            Emit(m_config.m_burstCount);
        }
    }

    void ParticleEmitterComponent::Deactivate()
    {
        DioramaParticleRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        AZ::Data::AssetBus::Handler::BusDisconnect();

        if (m_featureProcessor != nullptr)
        {
            for (AZ::u32 handle : m_handles)
            {
                m_featureProcessor->ReleaseSprite(handle);
            }
        }
        m_handles.clear();
        m_particles.clear();
        m_featureProcessor = nullptr;
        m_acquired = false;
    }

    void ParticleEmitterComponent::QueueTextureLoad()
    {
        if (m_config.m_texture.GetId().IsValid())
        {
            m_config.m_texture.QueueLoad();
            AZ::Data::AssetBus::Handler::BusConnect(m_config.m_texture.GetId());
        }
    }

    void ParticleEmitterComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() == m_config.m_texture.GetId())
        {
            m_config.m_texture = asset;
        }
    }

    bool ParticleEmitterComponent::TryAcquireFeatureProcessor()
    {
        if (m_acquired)
        {
            return true;
        }
        AZ::RPI::Scene* scene = AZ::RPI::Scene::GetSceneForEntityId(GetEntityId());
        if (scene == nullptr)
        {
            return false;
        }
        m_featureProcessor = scene->GetFeatureProcessor<SpriteFeatureProcessor>();
        if (m_featureProcessor == nullptr)
        {
            m_featureProcessor = scene->EnableFeatureProcessor<SpriteFeatureProcessor>();
        }
        if (m_featureProcessor == nullptr)
        {
            return false;
        }

        // Pre-acquire one handle per pool slot (fixed cost, no per-frame churn).
        m_handles.clear();
        m_handles.reserve(static_cast<size_t>(m_config.m_maxParticles));
        for (int i = 0; i < m_config.m_maxParticles; ++i)
        {
            const AZ::u32 handle = m_featureProcessor->AcquireSprite();
            if (handle != 0)
            {
                m_handles.push_back(handle);
            }
        }
        m_acquired = !m_handles.empty();
        return m_acquired;
    }

    Particles2D::EmitterParams ParticleEmitterComponent::MakeParams() const
    {
        Particles2D::EmitterParams params;
        params.m_gravity = m_config.m_gravity;
        params.m_drag = ParticleNonNeg(m_config.m_drag);
        params.m_startSize = ParticleNonNeg(m_config.m_startSize);
        params.m_endSize = ParticleNonNeg(m_config.m_endSize);
        params.m_startColor = m_config.m_startColor;
        params.m_endColor = m_config.m_endColor;
        return params;
    }

    void ParticleEmitterComponent::SpawnOne()
    {
        if (static_cast<int>(m_particles.size()) >= m_config.m_maxParticles)
        {
            return;
        }

        AZ::Vector3 world = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);

        const float speedMin = ParticleNonNeg(m_config.m_speedMin);
        const float speedMax = AZ::GetMax(speedMin, ParticleNonNeg(m_config.m_speedMax));
        const float lifeMin = ParticleNonNeg(m_config.m_lifetimeMin);
        const float lifeMax = AZ::GetMax(lifeMin, ParticleNonNeg(m_config.m_lifetimeMax));

        const float speed = speedMin + (speedMax - speedMin) * m_rng.GetRandomFloat();
        const float life = lifeMin + (lifeMax - lifeMin) * m_rng.GetRandomFloat();
        const float half = m_config.m_spreadDegrees * 0.5f;
        const float angleDeg = m_config.m_directionDegrees + (m_rng.GetRandomFloat() * 2.0f - 1.0f) * half;
        const float angleRad = angleDeg * ParticleDegToRad;
        const AZ::Vector2 velocity(cosf(angleRad) * speed, sinf(angleRad) * speed);
        const float spin = m_config.m_spinMin + (m_config.m_spinMax - m_config.m_spinMin) * m_rng.GetRandomFloat();

        m_particles.push_back(Particles2D::Spawn(AZ::Vector2(world.GetX(), world.GetY()), velocity, life, 0.0f, spin));
    }

    void ParticleEmitterComponent::PushToRenderer()
    {
        if (!m_acquired || m_featureProcessor == nullptr)
        {
            return;
        }

        AZ::Vector3 world = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        const float planeZ = world.GetZ();

        const Particles2D::EmitterParams params = MakeParams();
        const size_t live = m_particles.size();

        for (size_t i = 0; i < m_handles.size(); ++i)
        {
            SpriteComponentConfig config;
            config.m_texture = m_config.m_texture;
            config.m_billboard = true;
            config.m_sortOffset = m_config.m_sortOffset;

            if (i < live)
            {
                const Particles2D::Particle& p = m_particles[i];
                const float lifeFraction = Particles2D::LifeFraction(p);
                const float size = Particles2D::SizeAt(params, lifeFraction);
                config.m_size = AZ::Vector2(size, size);
                config.m_tint = Particles2D::ColorAt(params, lifeFraction);
                const AZ::Transform tm = AZ::Transform::CreateTranslation(AZ::Vector3(p.m_position.GetX(), p.m_position.GetY(), planeZ));
                m_featureProcessor->UpdateSprite(m_handles[i], tm, config);
            }
            else
            {
                // Unused slot: a zero-size quad draws nothing.
                config.m_size = AZ::Vector2(0.0f, 0.0f);
                m_featureProcessor->UpdateSprite(m_handles[i], AZ::Transform::CreateIdentity(), config);
            }
        }
    }

    void ParticleEmitterComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_acquired)
        {
            if (!TryAcquireFeatureProcessor())
            {
                return;
            }
            // First acquire: fire the deferred activate burst.
            if (m_config.m_emitOnActivate && m_config.m_burstCount > 0 && m_particles.empty())
            {
                Emit(m_config.m_burstCount);
            }
        }

        // Clamp the timestep before it drives emission or integration. The first
        // tick after a level load can hand us a huge or non-finite delta; left
        // unchecked, continuous emission accumulates an unbounded spawn count and
        // the drain loop spins forever, hanging the game thread (observed as a
        // frozen launcher: FPS 0, nothing renders).
        const float dt = Particles2D::ClampTimeStep(deltaTime, MaxTimeStep);

        // Continuous emission: accumulate fractional particles so a low rate still
        // emits at the right average cadence (bounded to the pool capacity per tick).
        if (m_playing)
        {
            const int toSpawn = Particles2D::EmitCountForTick(m_emitAccumulator, m_config.m_rate, dt, m_config.m_maxParticles);
            for (int i = 0; i < toSpawn; ++i)
            {
                SpawnOne();
            }
        }

        Particles2D::StepPool(m_particles, MakeParams(), dt);
        PushToRenderer();
    }

    void ParticleEmitterComponent::Emit(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            if (static_cast<int>(m_particles.size()) >= m_config.m_maxParticles)
            {
                break;
            }
            SpawnOne();
        }
    }

    void ParticleEmitterComponent::Burst()
    {
        Emit(m_config.m_burstCount);
    }

    void ParticleEmitterComponent::Play()
    {
        m_playing = true;
    }

    void ParticleEmitterComponent::Stop()
    {
        m_playing = false;
    }

    void ParticleEmitterComponent::SetRate(float particlesPerSecond)
    {
        m_config.m_rate = ParticleNonNeg(particlesPerSecond);
    }

    void ParticleEmitterComponent::SetGravity(float x, float y)
    {
        m_config.m_gravity = AZ::Vector2(x, y);
    }

    void ParticleEmitterComponent::SetLifetime(float minSeconds, float maxSeconds)
    {
        m_config.m_lifetimeMin = ParticleNonNeg(minSeconds);
        m_config.m_lifetimeMax = AZ::GetMax(m_config.m_lifetimeMin, ParticleNonNeg(maxSeconds));
    }

    void ParticleEmitterComponent::SetSpeed(float minSpeed, float maxSpeed)
    {
        m_config.m_speedMin = ParticleNonNeg(minSpeed);
        m_config.m_speedMax = AZ::GetMax(m_config.m_speedMin, ParticleNonNeg(maxSpeed));
    }

    void ParticleEmitterComponent::SetSpin(float minRadiansPerSecond, float maxRadiansPerSecond)
    {
        // Spin is signed (negative = clockwise), so no non-negative clamp; the spawn
        // path samples uniformly across [min, max] regardless of their order.
        m_config.m_spinMin = minRadiansPerSecond;
        m_config.m_spinMax = maxRadiansPerSecond;
    }

    void ParticleEmitterComponent::SetDirection(float degrees, float spreadDegrees)
    {
        m_config.m_directionDegrees = degrees;
        m_config.m_spreadDegrees = spreadDegrees < 0.0f ? 0.0f : (spreadDegrees > 360.0f ? 360.0f : spreadDegrees);
    }

    void ParticleEmitterComponent::SetStartColor(float r, float g, float b, float a)
    {
        m_config.m_startColor = AZ::Color(ParticleClamp01(r), ParticleClamp01(g), ParticleClamp01(b), ParticleClamp01(a));
    }

    void ParticleEmitterComponent::SetEndColor(float r, float g, float b, float a)
    {
        m_config.m_endColor = AZ::Color(ParticleClamp01(r), ParticleClamp01(g), ParticleClamp01(b), ParticleClamp01(a));
    }

    void ParticleEmitterComponent::SetStartSize(float size)
    {
        m_config.m_startSize = ParticleNonNeg(size);
    }

    void ParticleEmitterComponent::SetEndSize(float size)
    {
        m_config.m_endSize = ParticleNonNeg(size);
    }

    bool ParticleEmitterComponent::SetTextureByPath(AZStd::string_view productPath)
    {
        const AZ::Data::AssetId assetId =
            AZ::RPI::AssetUtils::GetAssetIdForProductPath(productPath.data(), AZ::RPI::AssetUtils::TraceLevel::Warning);
        if (!assetId.IsValid())
        {
            return false;
        }
        AZ::Data::AssetBus::Handler::BusDisconnect();
        m_config.m_texture =
            AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(assetId, AZ::Data::AssetLoadBehavior::PreLoad);
        QueueTextureLoad();
        return true;
    }

    ParticleInfo ParticleEmitterComponent::GetParticleInfo()
    {
        ParticleInfo info;
        info.m_aliveCount = static_cast<int>(m_particles.size());
        info.m_maxParticles = m_config.m_maxParticles;
        info.m_playing = m_playing;
        info.m_rate = m_config.m_rate;
        info.m_textureLoaded = m_config.m_texture.IsReady();
        return info;
    }
} // namespace Diorama
