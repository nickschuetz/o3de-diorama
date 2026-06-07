/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaCameraBus.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Random.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>

namespace Diorama
{
    //! Which pair of world axes the camera follows in. A 2.5D game picks the plane
    //! its gameplay moves on: XY is the conventional screen plane (the twin-stick
    //! sample), XZ is the horizontal ground plane (Y up). The out-of-plane axis is
    //! held at the follow offset so the framing (e.g. the pulled-back 2.5D tilt) is
    //! preserved while the camera tracks in the plane.
    enum class CameraPlane2D : AZ::u8
    {
        XY = 0,
        XZ = 1,
        YZ = 2
    };

    //! Shared configuration for the 2D camera controller. Authored by the editor
    //! twin and handed to the runtime DioramaCamera2DComponent.
    class DioramaCamera2DConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaCamera2DConfig, DioramaCamera2DConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaCamera2DConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaCamera2DConfig() override = default;

        //! Entity to follow (invalid = hold still).
        AZ::EntityId m_target;
        //! World plane the camera tracks in (default XY, the screen plane).
        CameraPlane2D m_plane = CameraPlane2D::XY;
        //! Offset added to the target to frame the shot; its out-of-plane component
        //! is the camera's distance/height (preserves the 2.5D framing).
        AZ::Vector3 m_followOffset = AZ::Vector3(0.0f, 0.0f, 0.0f);
        //! Follow smoothing time in seconds (0 snaps).
        float m_smoothTime = 0.2f;
        //! In-plane deadzone half-extents (target moves freely inside).
        AZ::Vector2 m_deadzoneHalf = AZ::Vector2(0.0f, 0.0f);
        //! Distance the view leads the target's motion.
        float m_lookahead = 0.0f;
        //! When true, clamp the camera center to [m_boundsMin, m_boundsMax].
        bool m_useBounds = false;
        AZ::Vector2 m_boundsMin = AZ::Vector2(-100.0f, -100.0f);
        AZ::Vector2 m_boundsMax = AZ::Vector2(100.0f, 100.0f);
        //! Max shake displacement at full trauma (world units).
        float m_maxShake = 1.0f;
        //! Trauma decay per second (how fast shake settles).
        float m_traumaDecay = 1.0f;
        //! Snap the camera position to a whole-texel grid (pixel-perfect feel);
        //! m_pixelsPerUnit is pixels per world unit (<=0 disables).
        bool m_pixelSnap = false;
        float m_pixelsPerUnit = 0.0f;
        //! Disabled cameras freeze where they are.
        bool m_enabled = true;

        //! Optional second target. When valid, the camera frames the midpoint of the
        //! two targets (a versus / fighting camera) instead of following m_target alone.
        AZ::EntityId m_secondaryTarget;
        //! Manual extra dolly: distance to pull the camera back from the play plane
        //! (along its out-of-plane axis), on top of the follow offset. Used when
        //! m_autoZoom is false.
        float m_zoom = 0.0f;
        //! When true the dolly is computed from the two targets' separation instead of
        //! using m_zoom: dolly = clamp(m_zoomBase + separation * m_zoomPerSeparation,
        //! m_zoomMin, m_zoomMax). Pulls the view out as the targets move apart.
        bool m_autoZoom = false;
        float m_zoomBase = 0.0f;
        float m_zoomPerSeparation = 0.0f;
        float m_zoomMin = 0.0f;
        float m_zoomMax = 100.0f;
    };

    //! Runtime 2D camera controller. Place it on a camera entity (one with an Atom
    //! Camera component); each tick it computes a desired position from the target,
    //! applies deadzone, smoothing, bounds, lookahead, and trauma-based shake (all
    //! from the pure Camera2D math core), and writes the camera entity's world
    //! translation. It never touches rotation, so the authored 2.5D tilt is kept.
    class DioramaCamera2DComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaCamera2DRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaCamera2DComponent, DioramaCamera2DComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        DioramaCamera2DComponent() = default;
        explicit DioramaCamera2DComponent(const DioramaCamera2DConfig& config);
        ~DioramaCamera2DComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaCamera2DRequests
        void SetTarget(AZ::EntityId target) override;
        void SetSecondaryTarget(AZ::EntityId target) override;
        void SetZoom(float dolly) override;
        void SetAutoZoom(float base, float perSeparation, float minDolly, float maxDolly) override;
        void SetFollowOffset(float x, float y, float z) override;
        void SetSmoothTime(float seconds) override;
        void SetDeadzone(float halfX, float halfY) override;
        void SetLookahead(float distance) override;
        void SetBounds(float minX, float minY, float maxX, float maxY) override;
        void ClearBounds() override;
        void AddTrauma(float amount) override;
        void SetEnabled(bool enabled) override;
        Camera2DInfo GetCameraInfo() override;

    private:
        //! Project a world position onto the working plane / read its out-of-plane
        //! axis / rebuild a world position from an in-plane point + out-of-plane.
        AZ::Vector2 ProjectToPlane(const AZ::Vector3& world) const;
        float OutOfPlane(const AZ::Vector3& world) const;
        AZ::Vector3 Compose(const AZ::Vector2& inPlane, float outOfPlane) const;

        //! Seed the camera center and previous-target cache from current transforms.
        void InitTracking();

        DioramaCamera2DConfig m_config;
        //! Current camera center in the working plane (the smoothed follow state).
        AZ::Vector2 m_center = AZ::Vector2(0.0f, 0.0f);
        //! Previous target in-plane position, for velocity-based lookahead.
        AZ::Vector2 m_prevTarget = AZ::Vector2(0.0f, 0.0f);
        bool m_hasPrevTarget = false;
        //! Current shake trauma (0..1).
        float m_trauma = 0.0f;
        //! Per-camera RNG for the shake angle (kept off the math core so the core
        //! stays deterministic and unit-testable).
        AZ::SimpleLcgRandom m_rng;
    };
} // namespace Diorama
