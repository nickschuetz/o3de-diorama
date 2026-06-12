/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaBulletEmitterComponent.h>
#include <Clients/SpriteFeatureProcessor.h>

#include <Diorama/Collision2DBus.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <cmath>

namespace Diorama
{
    namespace
    {
        constexpr float BulletDegToRad = 0.01745329252f;
        constexpr float BulletRadToDeg = 57.2957795131f;
        constexpr int BulletMaxCap = 4096; // hard ceiling on the pool
        constexpr int BulletMaxShotsPerTick = 8; // never fire more than this in one tick (hitch guard)
        constexpr float BulletMaxTimeStep = 0.1f;

        float BulletNonNeg(float v)
        {
            return v < 0.0f ? 0.0f : v;
        }
    } // namespace

    void DioramaBulletConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaBulletConfig>()
                ->Version(1)
                ->Field("texture", &DioramaBulletConfig::m_texture)
                ->Field("maxBullets", &DioramaBulletConfig::m_maxBullets)
                ->Field("pattern", &DioramaBulletConfig::m_pattern)
                ->Field("count", &DioramaBulletConfig::m_count)
                ->Field("speed", &DioramaBulletConfig::m_speed)
                ->Field("fireRate", &DioramaBulletConfig::m_fireRate)
                ->Field("aimDegrees", &DioramaBulletConfig::m_aimDegrees)
                ->Field("spreadDegrees", &DioramaBulletConfig::m_spreadDegrees)
                ->Field("spinPerShotDegrees", &DioramaBulletConfig::m_spinPerShotDegrees)
                ->Field("fireOnActivate", &DioramaBulletConfig::m_fireOnActivate)
                ->Field("bulletLifetime", &DioramaBulletConfig::m_bulletLifetime)
                ->Field("bulletRadius", &DioramaBulletConfig::m_bulletRadius)
                ->Field("muzzleOffset", &DioramaBulletConfig::m_muzzleOffset)
                ->Field("gravity", &DioramaBulletConfig::m_gravity)
                ->Field("drag", &DioramaBulletConfig::m_drag)
                ->Field("color", &DioramaBulletConfig::m_color)
                ->Field("targetMask", &DioramaBulletConfig::m_targetMask)
                ->Field("sortOffset", &DioramaBulletConfig::m_sortOffset)
                ->Field("useSimClock", &DioramaBulletConfig::m_useSimClock);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaBulletConfig>("Bullet Emitter Config", "Danmaku pattern emission, motion, and collision")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_texture, "Texture", "Bullet texture (a soft dot / orb)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_maxBullets, "Max Bullets", "Hard pool capacity")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, BulletMaxCap)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &DioramaBulletConfig::m_pattern, "Pattern", "Emission shape")
                    ->EnumAttribute(BulletPattern::Kind::Ring, "Ring")
                    ->EnumAttribute(BulletPattern::Kind::Fan, "Fan")
                    ->EnumAttribute(BulletPattern::Kind::Spiral, "Spiral")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_count, "Count", "Bullets per shot")
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_speed, "Speed", "Bullet speed (world units/sec)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_fireRate, "Fire Rate", "Shots/sec (0 = manual Fire only)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_aimDegrees, "Aim", "Base angle (0 = +X, 90 = +Y)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_spreadDegrees, "Spread", "Fan arc width in degrees")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 360.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaBulletConfig::m_spinPerShotDegrees,
                        "Spin Per Shot",
                        "Spiral rotation per shot (degrees)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaBulletConfig::m_fireOnActivate,
                        "Fire On Activate",
                        "Start firing when the game starts")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaBulletConfig::m_bulletLifetime,
                        "Bullet Lifetime",
                        "Seconds before a bullet expires")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaBulletConfig::m_bulletRadius,
                        "Bullet Radius",
                        "Collision + render half size")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaBulletConfig::m_muzzleOffset,
                        "Muzzle Offset",
                        "Spawn offset from the entity origin (e.g. a ship's nose)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_gravity, "Gravity", "Constant acceleration on every bullet")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_drag, "Drag", "Fraction of velocity shed per second")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaBulletConfig::m_color, "Color", "Bullet tint")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaBulletConfig::m_targetMask,
                        "Target Mask",
                        "Collision layer mask the bullets hit (0 = visual only)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletConfig::m_sortOffset, "Sort Offset", "Transparent draw-order bias")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaBulletConfig::m_useSimClock,
                        "Use Simulation Clock",
                        "Step bullets and fire accumulation on the 2D Simulation Clock's fixed steps instead of the "
                        "render tick. With no clock in the level, falls back to the render tick.");
            }
        }
    }

    DioramaBulletEmitterComponent::DioramaBulletEmitterComponent(const DioramaBulletConfig& config)
        : m_config(config)
    {
    }

    void DioramaBulletEmitterComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaBulletConfig::Reflect(context);
        ReflectBulletBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaBulletEmitterComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaBulletEmitterComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<DioramaBulletEmitterComponent>(
                        "2D Bullet Emitter", "Pooled danmaku pattern emitter rendered through the sprite batch")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaBulletEmitterComponent::m_config, "Config", "Emitter configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void DioramaBulletEmitterComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void DioramaBulletEmitterComponent::Activate()
    {
        m_config.m_maxBullets = AZ::GetClamp(m_config.m_maxBullets, 1, BulletMaxCap);
        m_bullets.clear();
        m_bullets.reserve(static_cast<size_t>(m_config.m_maxBullets));
        m_spiralAngleDegrees = m_config.m_aimDegrees;
        m_fireAccumulator = 0.0f;
        m_acquired = false;
        m_firing = m_config.m_fireOnActivate && m_config.m_fireRate > 0.0f;

        QueueTextureLoad();
        AZ::TickBus::Handler::BusConnect();
        if (m_config.m_useSimClock)
        {
            DioramaSimTickNotificationBus::Handler::BusConnect();
        }
        DioramaBulletRequestBus::Handler::BusConnect(GetEntityId());
        DioramaSimStateParticipantBus::Handler::BusConnect(GetEntityId());
        TryAcquireFeatureProcessor();
    }

    void DioramaBulletEmitterComponent::Deactivate()
    {
        DioramaSimTickNotificationBus::Handler::BusDisconnect();
        DioramaSimStateParticipantBus::Handler::BusDisconnect();
        DioramaBulletRequestBus::Handler::BusDisconnect();
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
        m_bullets.clear();
        m_featureProcessor = nullptr;
        m_acquired = false;
    }

    void DioramaBulletEmitterComponent::QueueTextureLoad()
    {
        if (m_config.m_texture.GetId().IsValid())
        {
            m_config.m_texture.QueueLoad();
            AZ::Data::AssetBus::Handler::BusConnect(m_config.m_texture.GetId());
        }
    }

    void DioramaBulletEmitterComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() == m_config.m_texture.GetId())
        {
            m_config.m_texture = asset;
        }
    }

    bool DioramaBulletEmitterComponent::TryAcquireFeatureProcessor()
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

        m_handles.clear();
        m_handles.reserve(static_cast<size_t>(m_config.m_maxBullets));
        for (int i = 0; i < m_config.m_maxBullets; ++i)
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

    Particles2D::EmitterParams DioramaBulletEmitterComponent::MakeParams() const
    {
        // Bullets do not shrink or recolor over life (size and color are constant), so
        // start == end. Only gravity and drag come from the config.
        Particles2D::EmitterParams params;
        params.m_gravity = m_config.m_gravity;
        params.m_drag = BulletNonNeg(m_config.m_drag);
        const float size = BulletNonNeg(m_config.m_bulletRadius) * 2.0f;
        params.m_startSize = size;
        params.m_endSize = size;
        params.m_startColor = m_config.m_color;
        params.m_endColor = m_config.m_color;
        return params;
    }

    void DioramaBulletEmitterComponent::EmitShot()
    {
        const int count = m_config.m_count;
        const float speed = BulletNonNeg(m_config.m_speed);
        if (count <= 0 || speed <= 0.0f)
        {
            return;
        }

        AZStd::vector<AZ::Vector2> velocities;
        switch (m_config.m_pattern)
        {
        case BulletPattern::Kind::Fan:
            BulletPattern::Fan(count, m_config.m_aimDegrees * BulletDegToRad, m_config.m_spreadDegrees * BulletDegToRad, speed, velocities);
            break;
        case BulletPattern::Kind::Spiral:
            {
                const float baseRad = m_spiralAngleDegrees * BulletDegToRad;
                BulletPattern::Ring(count, baseRad, speed, velocities);
                // Advance the running spiral angle by the per-shot spin (tested core).
                m_spiralAngleDegrees =
                    BulletPattern::AdvanceSpiral(baseRad, m_config.m_spinPerShotDegrees * BulletDegToRad) * BulletRadToDeg;
                break;
            }
        case BulletPattern::Kind::Ring:
        default:
            BulletPattern::Ring(count, m_config.m_aimDegrees * BulletDegToRad, speed, velocities);
            break;
        }

        AZ::Vector3 world = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        // Spawn from the muzzle, an offset from the entity origin (e.g. a ship's nose),
        // so the gun does not fire from the body center.
        const AZ::Vector2 origin(world.GetX() + m_config.m_muzzleOffset.GetX(), world.GetY() + m_config.m_muzzleOffset.GetY());
        const float life = BulletNonNeg(m_config.m_bulletLifetime);

        for (const AZ::Vector2& v : velocities)
        {
            if (static_cast<int>(m_bullets.size()) >= m_config.m_maxBullets)
            {
                break; // pool full: drop the rest of this shot
            }
            m_bullets.push_back(Particles2D::Spawn(origin, v, life, 0.0f, 0.0f));
        }
    }

    void DioramaBulletEmitterComponent::StepBullets(float dt, bool render)
    {
        // Integrate + expire by lifetime (swap-and-pop keeps the pool packed).
        Particles2D::StepPool(m_bullets, MakeParams(), dt);

        // Hit-test each live bullet against the target layer; an overlap fires
        // OnBulletHit and consumes the bullet. Skipped entirely when target mask is 0.
        if (m_config.m_targetMask != 0u && m_config.m_bulletRadius > 0.0f)
        {
            const AZ::EntityId self = GetEntityId();
            AZStd::vector<AZ::EntityId> hits;
            for (size_t i = 0; i < m_bullets.size();)
            {
                const AZ::Vector2& p = m_bullets[i].m_position;
                hits.clear();
                Diorama2DCollisionRequestBus::BroadcastResult(
                    hits, &Diorama2DCollisionRequests::OverlapCircle, p.GetX(), p.GetY(), m_config.m_bulletRadius, m_config.m_targetMask);

                AZ::EntityId struck;
                for (const AZ::EntityId& h : hits)
                {
                    if (h != self)
                    {
                        struck = h;
                        break;
                    }
                }
                if (struck.IsValid())
                {
                    DioramaBulletNotificationBus::Event(self, &DioramaBulletNotifications::OnBulletHit, struck);
                    m_bullets[i] = m_bullets.back(); // consume: swap-and-pop
                    m_bullets.pop_back();
                }
                else
                {
                    ++i;
                }
            }
        }

        if (render)
        {
            PushToRenderer();
        }
    }

    void DioramaBulletEmitterComponent::PushToRenderer()
    {
        if (!m_acquired || m_featureProcessor == nullptr)
        {
            return;
        }

        AZ::Vector3 world = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        const float planeZ = world.GetZ();
        const float size = BulletNonNeg(m_config.m_bulletRadius) * 2.0f;
        const size_t live = m_bullets.size();

        for (size_t i = 0; i < m_handles.size(); ++i)
        {
            SpriteComponentConfig config;
            config.m_texture = m_config.m_texture;
            config.m_billboard = true;
            config.m_sortOffset = m_config.m_sortOffset;

            if (i < live)
            {
                const Particles2D::Particle& b = m_bullets[i];
                config.m_size = AZ::Vector2(size, size);
                config.m_tint = m_config.m_color;
                const AZ::Transform tm = AZ::Transform::CreateTranslation(AZ::Vector3(b.m_position.GetX(), b.m_position.GetY(), planeZ));
                m_featureProcessor->UpdateSprite(m_handles[i], tm, config);
            }
            else
            {
                config.m_size = AZ::Vector2(0.0f, 0.0f);
                m_featureProcessor->UpdateSprite(m_handles[i], AZ::Transform::CreateIdentity(), config);
            }
        }
    }

    void DioramaBulletEmitterComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_acquired && !TryAcquireFeatureProcessor())
        {
            return;
        }

        // Use Simulation Clock mode: a running clock owns firing, integration, and
        // hit-testing (OnSimTick); the render tick only re-pushes the current pool
        // to the renderer. Never run a zero-dt StepBullets here: its hit-test would
        // consume bullets on the render tick, off the deterministic timeline.
        if (m_config.m_useSimClock && DioramaSimClockRequestBus::HasHandlers())
        {
            PushToRenderer();
            return;
        }

        AdvanceFire(Particles2D::ClampTimeStep(deltaTime, BulletMaxTimeStep));
        StepBullets(Particles2D::ClampTimeStep(deltaTime, BulletMaxTimeStep), true);
    }

    void DioramaBulletEmitterComponent::OnSimTick([[maybe_unused]] AZ::s64 frame, float stepSeconds)
    {
        // No feature-processor guard here: the deterministic advance must not depend
        // on renderer availability (headless runs). Acquisition stays on the render tick.
        const float dt = Particles2D::ClampTimeStep(stepSeconds, BulletMaxTimeStep);
        AdvanceFire(dt);
        StepBullets(dt, false); // render push happens on the render tick
    }

    void DioramaBulletEmitterComponent::AdvanceFire(float dt)
    {
        if (m_firing && m_config.m_fireRate > 0.0f)
        {
            m_fireAccumulator += m_config.m_fireRate * dt;
            // Consume every whole shot from the accumulator (so no backlog builds up),
            // but emit at most a few per tick: a huge rate or a hitch drops the excess
            // rather than spawning an unbounded burst in one frame.
            int shots = static_cast<int>(m_fireAccumulator);
            m_fireAccumulator -= static_cast<float>(shots);
            if (shots > BulletMaxShotsPerTick)
            {
                shots = BulletMaxShotsPerTick;
            }
            for (int i = 0; i < shots; ++i)
            {
                EmitShot();
            }
        }
    }

    void DioramaBulletEmitterComponent::Fire()
    {
        EmitShot();
    }

    void DioramaBulletEmitterComponent::Play()
    {
        m_firing = true;
    }

    void DioramaBulletEmitterComponent::Stop()
    {
        m_firing = false;
    }

    void DioramaBulletEmitterComponent::SetPattern(int pattern)
    {
        switch (pattern)
        {
        case 1:
            m_config.m_pattern = BulletPattern::Kind::Fan;
            break;
        case 2:
            m_config.m_pattern = BulletPattern::Kind::Spiral;
            break;
        default:
            m_config.m_pattern = BulletPattern::Kind::Ring;
            break;
        }
    }

    void DioramaBulletEmitterComponent::SetFireRate(float shotsPerSecond)
    {
        m_config.m_fireRate = BulletNonNeg(shotsPerSecond);
    }

    void DioramaBulletEmitterComponent::SetCount(int count)
    {
        m_config.m_count = count < 0 ? 0 : count;
    }

    void DioramaBulletEmitterComponent::SetSpeed(float speed)
    {
        m_config.m_speed = BulletNonNeg(speed);
    }

    void DioramaBulletEmitterComponent::SetAim(float degrees)
    {
        m_config.m_aimDegrees = degrees;
    }

    void DioramaBulletEmitterComponent::SetSpread(float degrees)
    {
        m_config.m_spreadDegrees = AZ::GetClamp(degrees, 0.0f, 360.0f);
    }

    void DioramaBulletEmitterComponent::SetSpin(float degreesPerShot)
    {
        m_config.m_spinPerShotDegrees = degreesPerShot;
    }

    void DioramaBulletEmitterComponent::SetMuzzleOffset(float x, float y)
    {
        m_config.m_muzzleOffset = AZ::Vector2(x, y);
    }

    void DioramaBulletEmitterComponent::SetUseSimClock(bool enabled)
    {
        m_config.m_useSimClock = enabled;
        if (enabled)
        {
            if (!DioramaSimTickNotificationBus::Handler::BusIsConnected())
            {
                DioramaSimTickNotificationBus::Handler::BusConnect();
            }
        }
        else
        {
            DioramaSimTickNotificationBus::Handler::BusDisconnect();
        }
    }

    DioramaBulletInfo DioramaBulletEmitterComponent::GetBulletInfo()
    {
        DioramaBulletInfo info;
        info.m_aliveCount = static_cast<int>(m_bullets.size());
        info.m_maxBullets = m_config.m_maxBullets;
        info.m_firing = m_firing;
        info.m_pattern = static_cast<int>(m_config.m_pattern);
        info.m_fireRate = m_config.m_fireRate;
        return info;
    }

    // ---- Snapshot / restore (design phase B) -----------------------------------------
    // Chunk payload: spiral angle (f32), fire accumulator (f32), firing (u8), bullet
    // count (u32), then per live bullet the full particle: position, velocity (2x f32
    // each), rotation, angular velocity, age, lifetime (f32 each). The pool is dense
    // (live bullets only), so the image is canonical as captured.

    void DioramaBulletEmitterComponent::SaveSimState(SimState::Writer& writer)
    {
        const size_t sizePos = writer.BeginChunk(BulletChunkTag);
        writer.F32(m_spiralAngleDegrees);
        writer.F32(m_fireAccumulator);
        writer.U8(m_firing ? 1 : 0);
        writer.U32(static_cast<AZ::u32>(m_bullets.size()));
        for (const Particles2D::Particle& b : m_bullets)
        {
            writer.F32(b.m_position.GetX());
            writer.F32(b.m_position.GetY());
            writer.F32(b.m_velocity.GetX());
            writer.F32(b.m_velocity.GetY());
            writer.F32(b.m_rotation);
            writer.F32(b.m_angularVelocity);
            writer.F32(b.m_age);
            writer.F32(b.m_lifetime);
        }
        writer.EndChunk(sizePos);
    }

    bool DioramaBulletEmitterComponent::TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload)
    {
        if (tag != BulletChunkTag)
        {
            return false;
        }
        float spiral = 0.0f;
        float accumulator = 0.0f;
        AZ::u8 firing = 0;
        AZ::u32 count = 0;
        if (!payload.F32(spiral) || !payload.F32(accumulator) || !payload.U8(firing) || !payload.U32(count))
        {
            return false;
        }
        // Each bullet is 8 floats; cap by the configured pool so a hostile count
        // cannot grow the pool past its fixed capacity.
        if (count > payload.Remaining() / 32 || count > static_cast<AZ::u32>(m_config.m_maxBullets))
        {
            return false;
        }
        AZStd::vector<Particles2D::Particle> restored;
        restored.reserve(count);
        for (AZ::u32 i = 0; i < count; ++i)
        {
            float px = 0.0f;
            float py = 0.0f;
            float vx = 0.0f;
            float vy = 0.0f;
            Particles2D::Particle b;
            if (!payload.F32(px) || !payload.F32(py) || !payload.F32(vx) || !payload.F32(vy) || !payload.F32(b.m_rotation) ||
                !payload.F32(b.m_angularVelocity) || !payload.F32(b.m_age) || !payload.F32(b.m_lifetime))
            {
                return false;
            }
            b.m_position = AZ::Vector2(px, py);
            b.m_velocity = AZ::Vector2(vx, vy);
            restored.push_back(b);
        }
        m_spiralAngleDegrees = spiral;
        m_fireAccumulator = accumulator;
        m_firing = firing != 0;
        m_bullets = AZStd::move(restored);
        return true;
    }
} // namespace Diorama
