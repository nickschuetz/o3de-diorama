/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/Camera2D.h>
#include <Clients/DioramaCamera2DComponent.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    namespace
    {
        // Two pi, for mapping a 0..1 random into a full-circle shake angle.
        constexpr float CameraTwoPi = 6.28318530718f;

        float CameraNonNegative(float v)
        {
            return v < 0.0f ? 0.0f : v;
        }
    } // namespace

    void DioramaCamera2DConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaCamera2DConfig>()
                ->Version(2) // 2: secondary target + zoom/dolly fields
                ->Field("target", &DioramaCamera2DConfig::m_target)
                ->Field("plane", &DioramaCamera2DConfig::m_plane)
                ->Field("followOffset", &DioramaCamera2DConfig::m_followOffset)
                ->Field("smoothTime", &DioramaCamera2DConfig::m_smoothTime)
                ->Field("deadzoneHalf", &DioramaCamera2DConfig::m_deadzoneHalf)
                ->Field("lookahead", &DioramaCamera2DConfig::m_lookahead)
                ->Field("useBounds", &DioramaCamera2DConfig::m_useBounds)
                ->Field("boundsMin", &DioramaCamera2DConfig::m_boundsMin)
                ->Field("boundsMax", &DioramaCamera2DConfig::m_boundsMax)
                ->Field("maxShake", &DioramaCamera2DConfig::m_maxShake)
                ->Field("traumaDecay", &DioramaCamera2DConfig::m_traumaDecay)
                ->Field("pixelSnap", &DioramaCamera2DConfig::m_pixelSnap)
                ->Field("pixelsPerUnit", &DioramaCamera2DConfig::m_pixelsPerUnit)
                ->Field("enabled", &DioramaCamera2DConfig::m_enabled)
                ->Field("secondaryTarget", &DioramaCamera2DConfig::m_secondaryTarget)
                ->Field("zoom", &DioramaCamera2DConfig::m_zoom)
                ->Field("autoZoom", &DioramaCamera2DConfig::m_autoZoom)
                ->Field("zoomBase", &DioramaCamera2DConfig::m_zoomBase)
                ->Field("zoomPerSeparation", &DioramaCamera2DConfig::m_zoomPerSeparation)
                ->Field("zoomMin", &DioramaCamera2DConfig::m_zoomMin)
                ->Field("zoomMax", &DioramaCamera2DConfig::m_zoomMax);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaCamera2DConfig>("2D Camera Config", "Follow, deadzone, bounds, lookahead, and shake")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaCamera2DConfig::m_target, "Target", "Entity to follow")
                    ->DataElement(
                        AZ::Edit::UIHandlers::ComboBox,
                        &DioramaCamera2DConfig::m_plane,
                        "Plane",
                        "World axes the camera tracks in (XY screen, XZ ground)")
                    ->EnumAttribute(CameraPlane2D::XY, "XY (screen)")
                    ->EnumAttribute(CameraPlane2D::XZ, "XZ (ground)")
                    ->EnumAttribute(CameraPlane2D::YZ, "YZ")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_followOffset,
                        "Follow Offset",
                        "Offset added to the target (its out-of-plane part is the camera distance/height)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_smoothTime,
                        "Smooth Time",
                        "Follow smoothing time in seconds (0 snaps)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_deadzoneHalf,
                        "Deadzone Half",
                        "In-plane half-extents the target moves within without moving the camera")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_lookahead,
                        "Lookahead",
                        "Distance the view leads the target's motion")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaCamera2DConfig::m_useBounds,
                        "Use Bounds",
                        "Clamp the camera to a world rectangle")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_boundsMin,
                        "Bounds Min",
                        "Minimum camera center (when Use Bounds is on)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_boundsMax,
                        "Bounds Max",
                        "Maximum camera center (when Use Bounds is on)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_maxShake,
                        "Max Shake",
                        "Max shake displacement at full trauma (world units)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_traumaDecay,
                        "Trauma Decay",
                        "How fast shake settles (per second)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaCamera2DConfig::m_pixelSnap,
                        "Pixel Snap",
                        "Snap the camera to a whole-texel grid")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_pixelsPerUnit,
                        "Pixels Per Unit",
                        "Texels per world unit for pixel snap (<=0 disables)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox, &DioramaCamera2DConfig::m_enabled, "Enabled", "Disabled cameras freeze in place")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Versus / Zoom")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_secondaryTarget,
                        "Secondary Target",
                        "Optional second entity; when set, the camera frames the midpoint of the two")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_zoom,
                        "Zoom (manual dolly)",
                        "Distance to pull back from the play plane when Auto Zoom is off")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaCamera2DConfig::m_autoZoom,
                        "Auto Zoom",
                        "Compute the dolly from the two targets' separation instead of the manual value")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_zoomBase,
                        "Zoom Base",
                        "Auto-zoom dolly at zero separation")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaCamera2DConfig::m_zoomPerSeparation,
                        "Zoom Per Separation",
                        "Extra dolly per world unit of separation (auto-zoom)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaCamera2DConfig::m_zoomMin, "Zoom Min", "Minimum auto-zoom dolly")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaCamera2DConfig::m_zoomMax, "Zoom Max", "Maximum auto-zoom dolly")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
            }
        }
    }

    DioramaCamera2DComponent::DioramaCamera2DComponent(const DioramaCamera2DConfig& config)
        : m_config(config)
    {
    }

    void DioramaCamera2DComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaCamera2DConfig::Reflect(context);
        Camera2DInfo::Reflect(context);
        ReflectCameraBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaCamera2DComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaCamera2DComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                // No AppearsInAddComponentMenu: built from the editor twin via
                // BuildGameEntity, never added directly (same as the other runtime
                // Diorama components).
                editContext
                    ->Class<DioramaCamera2DComponent>(
                        "2D Camera Controller", "Follows a target with deadzone, bounds, lookahead, and shake")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaCamera2DComponent::m_config, "Config", "Camera configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void DioramaCamera2DComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void DioramaCamera2DComponent::Activate()
    {
        InitTracking();
        AZ::TickBus::Handler::BusConnect();
        DioramaCamera2DRequestBus::Handler::BusConnect(GetEntityId());
    }

    void DioramaCamera2DComponent::Deactivate()
    {
        DioramaCamera2DRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    AZ::Vector2 DioramaCamera2DComponent::ProjectToPlane(const AZ::Vector3& world) const
    {
        switch (m_config.m_plane)
        {
        case CameraPlane2D::XZ:
            return AZ::Vector2(world.GetX(), world.GetZ());
        case CameraPlane2D::YZ:
            return AZ::Vector2(world.GetY(), world.GetZ());
        case CameraPlane2D::XY:
        default:
            return AZ::Vector2(world.GetX(), world.GetY());
        }
    }

    float DioramaCamera2DComponent::OutOfPlane(const AZ::Vector3& world) const
    {
        switch (m_config.m_plane)
        {
        case CameraPlane2D::XZ:
            return world.GetY();
        case CameraPlane2D::YZ:
            return world.GetX();
        case CameraPlane2D::XY:
        default:
            return world.GetZ();
        }
    }

    AZ::Vector3 DioramaCamera2DComponent::Compose(const AZ::Vector2& inPlane, float outOfPlane) const
    {
        switch (m_config.m_plane)
        {
        case CameraPlane2D::XZ:
            return AZ::Vector3(inPlane.GetX(), outOfPlane, inPlane.GetY());
        case CameraPlane2D::YZ:
            return AZ::Vector3(outOfPlane, inPlane.GetX(), inPlane.GetY());
        case CameraPlane2D::XY:
        default:
            return AZ::Vector3(inPlane.GetX(), inPlane.GetY(), outOfPlane);
        }
    }

    void DioramaCamera2DComponent::InitTracking()
    {
        AZ::Vector3 camWorld = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(camWorld, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        m_center = ProjectToPlane(camWorld);

        m_hasPrevTarget = false;
        if (m_config.m_target.IsValid())
        {
            AZ::Vector3 targetWorld = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(targetWorld, m_config.m_target, &AZ::TransformBus::Events::GetWorldTranslation);
            m_prevTarget = ProjectToPlane(targetWorld);
            m_hasPrevTarget = true;
        }
    }

    void DioramaCamera2DComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_config.m_enabled)
        {
            return;
        }

        // Default the out-of-plane coordinate to wherever the camera currently is,
        // so a camera with no target still shakes around its own position.
        AZ::Vector3 camWorld = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(camWorld, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        float outOfPlane = OutOfPlane(camWorld);

        if (m_config.m_target.IsValid())
        {
            AZ::Vector3 targetWorld = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(targetWorld, m_config.m_target, &AZ::TransformBus::Events::GetWorldTranslation);

            // Frame point: the single target, or the midpoint of two targets (a versus
            // / fighting camera), with their separation for auto-zoom.
            AZ::Vector2 followInPlane = ProjectToPlane(targetWorld);
            float followDepth = OutOfPlane(targetWorld);
            float separation = 0.0f;
            if (m_config.m_secondaryTarget.IsValid())
            {
                AZ::Vector3 secondWorld = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(secondWorld, m_config.m_secondaryTarget, &AZ::TransformBus::Events::GetWorldTranslation);
                const AZ::Vector2 secondInPlane = ProjectToPlane(secondWorld);
                separation = (followInPlane - secondInPlane).GetLength();
                followInPlane = Camera2D::Midpoint(followInPlane, secondInPlane);
                followDepth = (followDepth + OutOfPlane(secondWorld)) * 0.5f;
            }

            // Decompose the follow offset (linear projection), so the single-target,
            // no-zoom path is byte-identical to before.
            const AZ::Vector2 offsetInPlane = ProjectToPlane(m_config.m_followOffset);
            const float offsetDepth = OutOfPlane(m_config.m_followOffset);
            const float baseDepth = followDepth + offsetDepth;

            // Dolly (zoom): pull further from the play plane, manual or from separation.
            const float dolly = m_config.m_autoZoom
                ? Camera2D::SeparationDolly(
                      separation, m_config.m_zoomBase, m_config.m_zoomPerSeparation, m_config.m_zoomMin, m_config.m_zoomMax)
                : (m_config.m_zoom < 0.0f ? 0.0f : m_config.m_zoom);
            const float backSign = (offsetDepth >= 0.0f) ? 1.0f : -1.0f;
            outOfPlane = baseDepth + backSign * dolly;

            AZ::Vector2 velocity = AZ::Vector2(0.0f, 0.0f);
            if (m_hasPrevTarget && deltaTime > 0.0f)
            {
                velocity = (followInPlane - m_prevTarget) / deltaTime;
            }
            m_prevTarget = followInPlane;
            m_hasPrevTarget = true;

            const AZ::Vector2 lookahead = Camera2D::Lookahead(velocity, m_config.m_lookahead);
            const AZ::Vector2 goal = followInPlane + offsetInPlane + lookahead;
            const AZ::Vector2 afterDeadzone = Camera2D::ApplyDeadzone(m_center, goal, m_config.m_deadzoneHalf);
            m_center = Camera2D::SmoothFollow(m_center, afterDeadzone, m_config.m_smoothTime, deltaTime);
            if (m_config.m_useBounds)
            {
                m_center = Camera2D::ClampToBounds(m_center, m_config.m_boundsMin, m_config.m_boundsMax);
            }
        }

        // Trauma-based shake, applied after follow so it never fights tracking.
        const float magnitude = Camera2D::ShakeMagnitude(m_trauma, m_config.m_maxShake);
        const float angle = m_rng.GetRandomFloat() * CameraTwoPi;
        const AZ::Vector2 shake = Camera2D::ShakeOffset(magnitude, angle);
        m_trauma = Camera2D::DecayTrauma(m_trauma, m_config.m_traumaDecay, deltaTime);

        AZ::Vector2 finalInPlane = m_center;
        if (m_config.m_pixelSnap)
        {
            finalInPlane = Camera2D::PixelSnap(finalInPlane, m_config.m_pixelsPerUnit);
        }
        finalInPlane += shake;

        const AZ::Vector3 worldPos = Compose(finalInPlane, outOfPlane);
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, worldPos);
    }

    void DioramaCamera2DComponent::SetTarget(AZ::EntityId target)
    {
        m_config.m_target = target;
        m_hasPrevTarget = false; // restart velocity tracking for the new target
    }

    void DioramaCamera2DComponent::SetSecondaryTarget(AZ::EntityId target)
    {
        m_config.m_secondaryTarget = target;
        m_hasPrevTarget = false; // the follow point changes (midpoint vs single)
    }

    void DioramaCamera2DComponent::SetZoom(float dolly)
    {
        m_config.m_zoom = dolly < 0.0f ? 0.0f : dolly;
        m_config.m_autoZoom = false;
    }

    void DioramaCamera2DComponent::SetAutoZoom(float base, float perSeparation, float minDolly, float maxDolly)
    {
        m_config.m_zoomBase = base < 0.0f ? 0.0f : base;
        m_config.m_zoomPerSeparation = perSeparation < 0.0f ? 0.0f : perSeparation;
        m_config.m_zoomMin = minDolly < 0.0f ? 0.0f : minDolly;
        m_config.m_zoomMax = maxDolly < m_config.m_zoomMin ? m_config.m_zoomMin : maxDolly;
        m_config.m_autoZoom = true;
    }

    void DioramaCamera2DComponent::SetFollowOffset(float x, float y, float z)
    {
        m_config.m_followOffset = AZ::Vector3(x, y, z);
    }

    void DioramaCamera2DComponent::SetSmoothTime(float seconds)
    {
        m_config.m_smoothTime = CameraNonNegative(seconds);
    }

    void DioramaCamera2DComponent::SetDeadzone(float halfX, float halfY)
    {
        m_config.m_deadzoneHalf = AZ::Vector2(CameraNonNegative(halfX), CameraNonNegative(halfY));
    }

    void DioramaCamera2DComponent::SetLookahead(float distance)
    {
        m_config.m_lookahead = CameraNonNegative(distance);
    }

    void DioramaCamera2DComponent::SetBounds(float minX, float minY, float maxX, float maxY)
    {
        m_config.m_boundsMin = AZ::Vector2(minX, minY);
        m_config.m_boundsMax = AZ::Vector2(maxX, maxY);
        m_config.m_useBounds = true;
    }

    void DioramaCamera2DComponent::ClearBounds()
    {
        m_config.m_useBounds = false;
    }

    void DioramaCamera2DComponent::AddTrauma(float amount)
    {
        m_trauma = Camera2D::AddTrauma(m_trauma, amount);
    }

    void DioramaCamera2DComponent::SetEnabled(bool enabled)
    {
        m_config.m_enabled = enabled;
    }

    Camera2DInfo DioramaCamera2DComponent::GetCameraInfo()
    {
        Camera2DInfo info;
        info.m_hasTarget = m_config.m_target.IsValid();
        info.m_centerX = m_center.GetX();
        info.m_centerY = m_center.GetY();
        info.m_trauma = m_trauma;
        info.m_smoothTime = m_config.m_smoothTime;
        info.m_lookahead = m_config.m_lookahead;
        info.m_useBounds = m_config.m_useBounds;
        info.m_enabled = m_config.m_enabled;
        return info;
    }
} // namespace Diorama
