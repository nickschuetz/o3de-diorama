/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaCameraBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void Camera2DInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Camera2DInfo>()
                ->Version(1)
                ->Field("hasTarget", &Camera2DInfo::m_hasTarget)
                ->Field("centerX", &Camera2DInfo::m_centerX)
                ->Field("centerY", &Camera2DInfo::m_centerY)
                ->Field("trauma", &Camera2DInfo::m_trauma)
                ->Field("smoothTime", &Camera2DInfo::m_smoothTime)
                ->Field("lookahead", &Camera2DInfo::m_lookahead)
                ->Field("useBounds", &Camera2DInfo::m_useBounds)
                ->Field("enabled", &Camera2DInfo::m_enabled);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<Camera2DInfo>("DioramaCamera2DInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Camera")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("hasTarget", BehaviorValueGetter(&Camera2DInfo::m_hasTarget), nullptr)
                ->Property("centerX", BehaviorValueGetter(&Camera2DInfo::m_centerX), nullptr)
                ->Property("centerY", BehaviorValueGetter(&Camera2DInfo::m_centerY), nullptr)
                ->Property("trauma", BehaviorValueGetter(&Camera2DInfo::m_trauma), nullptr)
                ->Property("smoothTime", BehaviorValueGetter(&Camera2DInfo::m_smoothTime), nullptr)
                ->Property("lookahead", BehaviorValueGetter(&Camera2DInfo::m_lookahead), nullptr)
                ->Property("useBounds", BehaviorValueGetter(&Camera2DInfo::m_useBounds), nullptr)
                ->Property("enabled", BehaviorValueGetter(&Camera2DInfo::m_enabled), nullptr);
        }
    }

    void ReflectCameraBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext
            ->EBus<DioramaCamera2DRequestBus>("DioramaCamera2DRequestBus")
            // Common scope so it is callable from editor Python and runtime script.
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Camera")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetTarget",
                &DioramaCamera2DRequestBus::Events::SetTarget,
                { { { "target", "Entity to follow; an invalid id clears the target." } } })
            ->Event(
                "SetSecondaryTarget",
                &DioramaCamera2DRequestBus::Events::SetSecondaryTarget,
                { { { "target", "Second entity to frame; when valid the camera centers on the midpoint of the two (versus camera)." } } })
            ->Event(
                "SetZoom",
                &DioramaCamera2DRequestBus::Events::SetZoom,
                { { { "dolly", "Distance to pull the camera back from the play plane (extra dolly); turns auto-zoom off." } } })
            ->Event(
                "SetAutoZoom",
                &DioramaCamera2DRequestBus::Events::SetAutoZoom,
                { { { "base", "Dolly distance at zero separation." },
                    { "perSeparation", "Extra dolly per world unit of separation between the two targets (>0 enables auto-zoom)." },
                    { "minDolly", "Minimum dolly distance (clamp)." },
                    { "maxDolly", "Maximum dolly distance (clamp)." } } })
            ->Event(
                "SetFollowOffset",
                &DioramaCamera2DRequestBus::Events::SetFollowOffset,
                { { { "x", "Follow offset X (world units)." },
                    { "y", "Follow offset Y (world units)." },
                    { "z", "Follow offset Z (world units); the out-of-plane part is the camera distance/height." } } })
            ->Event(
                "SetSmoothTime",
                &DioramaCamera2DRequestBus::Events::SetSmoothTime,
                { { { "seconds", "Follow smoothing time in seconds (0 snaps); clamped non-negative." } } })
            ->Event(
                "SetDeadzone",
                &DioramaCamera2DRequestBus::Events::SetDeadzone,
                { { { "halfX", "Deadzone half-width; clamped non-negative." },
                    { "halfY", "Deadzone half-height; clamped non-negative." } } })
            ->Event(
                "SetLookahead",
                &DioramaCamera2DRequestBus::Events::SetLookahead,
                { { { "distance", "Distance the view leads the target's motion; clamped non-negative." } } })
            ->Event(
                "SetBounds",
                &DioramaCamera2DRequestBus::Events::SetBounds,
                { { { "minX", "Minimum camera center X." },
                    { "minY", "Minimum camera center Y." },
                    { "maxX", "Maximum camera center X." },
                    { "maxY", "Maximum camera center Y." } } })
            ->Event("ClearBounds", &DioramaCamera2DRequestBus::Events::ClearBounds)
            ->Event(
                "AddTrauma",
                &DioramaCamera2DRequestBus::Events::AddTrauma,
                { { { "amount", "Shake trauma to add (e.g. on a hit); total clamped to 0..1." } } })
            ->Event(
                "SetEnabled",
                &DioramaCamera2DRequestBus::Events::SetEnabled,
                { { { "enabled", "When false the camera freezes in place." } } })
            ->Event("GetCameraInfo", &DioramaCamera2DRequestBus::Events::GetCameraInfo);
    }
} // namespace Diorama
