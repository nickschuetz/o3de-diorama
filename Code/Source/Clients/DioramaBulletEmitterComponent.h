/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/BulletPattern.h>
#include <Clients/Particles2D.h>
#include <Clients/SimStateBus.h>
#include <Diorama/DioramaBulletBus.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::BulletPattern::Kind, "{1B2C3D4E-5F60-4172-93A4-B5C6D7E8F1B5}");
} // namespace AZ

namespace Diorama
{
    class SpriteFeatureProcessor;

    //! Shared configuration for a bullet-pattern (danmaku) emitter. Authored by the
    //! editor twin and handed to the runtime DioramaBulletEmitterComponent. Bullets
    //! live and collide in the world XY plane (the screen plane), the danmaku default.
    class DioramaBulletConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaBulletConfig, DioramaBulletConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaBulletConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaBulletConfig() override = default;

        //! Bullet texture (a soft dot / orb). Each bullet is a billboarded quad of it,
        //! so a whole pattern is one draw call.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_texture;
        //! Hard pool capacity (clamped). Bullets past it are not spawned.
        int m_maxBullets = 512;

        BulletPattern::Kind m_pattern = BulletPattern::Kind::Ring;
        int m_count = 16; //!< Bullets per shot.
        float m_speed = 6.0f; //!< World units/sec.
        float m_fireRate = 4.0f; //!< Shots/sec for continuous fire; 0 = manual Fire only.
        float m_aimDegrees = 90.0f; //!< 0 = +X; ring start / fan center / spiral start.
        float m_spreadDegrees = 60.0f; //!< Fan arc width (ignored by ring).
        float m_spinPerShotDegrees = 11.0f; //!< Spiral rotation per shot (ignored by ring/fan).
        bool m_fireOnActivate = true;

        float m_bulletLifetime = 4.0f; //!< Seconds before a bullet expires.
        float m_bulletRadius = 0.25f; //!< Collision + render half size (world units).
        //! Spawn offset from the entity origin (world XY), e.g. a ship's nose or wings,
        //! so the gun does not fire from the body center.
        AZ::Vector2 m_muzzleOffset = AZ::Vector2(0.0f, 0.0f);
        AZ::Vector2 m_gravity = AZ::Vector2(0.0f, 0.0f); //!< Optional acceleration.
        float m_drag = 0.0f;
        AZ::Color m_color = AZ::Color(1.0f, 0.4f, 0.4f, 1.0f);

        //! Collision layer mask the bullets test against. A bullet that overlaps a
        //! collider on this mask fires OnBulletHit and is consumed. 0 = hit nothing
        //! (bullets are purely visual).
        AZ::u32 m_targetMask = 0x0001;
        //! Transparent draw-order bias for the bullets.
        float m_sortOffset = 1.0f;
    };

    //! Runtime bullet-pattern (danmaku) emitter. Fires shots of a Ring / Fan / Spiral
    //! pattern at a configurable rate (geometry from the pure tested BulletPattern
    //! core), simulates the bullets as a pooled CPU particle set (Particles2D core),
    //! and renders each as a billboarded sprite quad through the SpriteFeatureProcessor
    //! (one draw call per emitter). Each frame every live bullet is tested against the
    //! target collision layer; an overlap fires OnBulletHit and consumes the bullet.
    //! The pool is fixed-capacity, so no script or asset value can spawn unboundedly.
    class DioramaBulletEmitterComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaBulletRequestBus::Handler
        , protected AZ::Data::AssetBus::Handler
        , protected DioramaSimStateParticipantBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaBulletEmitterComponent, DioramaBulletEmitterComponentTypeId);

        //! Chunk tag for the live-bullet pool + fire state.
        static constexpr AZ::u32 BulletChunkTag = 0x544C4C42; // 'BLLT' little-endian

        static void Reflect(AZ::ReflectContext* context);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        DioramaBulletEmitterComponent() = default;
        explicit DioramaBulletEmitterComponent(const DioramaBulletConfig& config);
        ~DioramaBulletEmitterComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::Data::AssetBus
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // DioramaBulletRequests
        void Fire() override;
        void Play() override;
        void Stop() override;
        void SetPattern(int pattern) override;
        void SetFireRate(float shotsPerSecond) override;
        void SetCount(int count) override;
        void SetSpeed(float speed) override;
        void SetAim(float degrees) override;
        void SetSpread(float degrees) override;
        void SetSpin(float degreesPerShot) override;
        void SetMuzzleOffset(float x, float y) override;
        DioramaBulletInfo GetBulletInfo() override;

        // DioramaSimStateParticipants (rollback snapshot: the live bullet pool and
        // the fire state, so a restored frame replays the exact danmaku field)
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;

    public:
        //! Run one emission shot now: compute the pattern velocities and spawn a bullet
        //! per velocity at the emitter origin (bounded by the pool). Public so tests can
        //! drive emission deterministically without the tick bus.
        void EmitShot();
        //! Integrate, expire, hit-test, and (optionally) render the bullets for one
        //! step. Public so tests can advance the sim without the tick bus or a renderer.
        void StepBullets(float dt, bool render);

    private:
        bool TryAcquireFeatureProcessor();
        Particles2D::EmitterParams MakeParams() const;
        void PushToRenderer();
        void QueueTextureLoad();

        DioramaBulletConfig m_config;
        AZStd::vector<Particles2D::Particle> m_bullets;
        AZStd::vector<AZ::u32> m_handles; //!< One sprite handle per pool slot.
        SpriteFeatureProcessor* m_featureProcessor = nullptr;
        float m_spiralAngleDegrees = 0.0f; //!< Running spiral base angle.
        float m_fireAccumulator = 0.0f; //!< Fractional carry for continuous fire.
        bool m_firing = false;
        bool m_acquired = false;
    };
} // namespace Diorama
