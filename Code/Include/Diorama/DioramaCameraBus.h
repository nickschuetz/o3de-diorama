/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a 2D camera controller, returned by GetCameraInfo.
    //! The verify-loop payload: it reports where the camera is tracking and its
    //! current trauma so an agent can confirm follow/shake without a viewport.
    struct Camera2DInfo
    {
        AZ_TYPE_INFO(Diorama::Camera2DInfo, Camera2DInfoTypeId);

        static void Reflect(AZ::ReflectContext* context);

        bool m_hasTarget = false; //!< True when a valid follow target is set.
        float m_centerX = 0.0f; //!< Current camera center in the working plane.
        float m_centerY = 0.0f;
        float m_trauma = 0.0f; //!< Current shake trauma (0..1).
        float m_smoothTime = 0.0f;
        float m_lookahead = 0.0f;
        bool m_useBounds = false;
        bool m_enabled = true;
    };

    //! Stable, typed, agent-facing API for a 2D camera controller, addressed by the
    //! camera entity's id. The AI-native front door to the camera: plain scalars,
    //! forgiving and clamped. AddTrauma is the gameplay "juice" hook (call it on a
    //! hit). Mirrors DioramaSpriteRequestBus / DioramaLightRequestBus.
    class DioramaCamera2DRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaCamera2DRequests, DioramaCamera2DRequestsTypeId);
        virtual ~DioramaCamera2DRequests() = default;

        //! Entity to follow. An invalid id clears the target (camera holds still).
        virtual void SetTarget(AZ::EntityId target) = 0;
        //! Optional second entity to frame. When valid, the camera centers on the
        //! midpoint of the two targets (a versus / fighting camera); invalid clears it.
        virtual void SetSecondaryTarget(AZ::EntityId target) = 0;
        //! Manually pull the camera back from the play plane by this distance (extra
        //! dolly on top of the follow offset). Turns auto-zoom off; clamped non-negative.
        virtual void SetZoom(float dolly) = 0;
        //! Auto-zoom from the two targets' separation: dolly = clamp(base +
        //! separation * perSeparation, minDolly, maxDolly). perSeparation > 0 enables
        //! it (the view pulls out as the pair separates and back in as they close).
        virtual void SetAutoZoom(float base, float perSeparation, float minDolly, float maxDolly) = 0;
        //! Offset added to the target to frame the shot (keeps the 2.5D framing).
        virtual void SetFollowOffset(float x, float y, float z) = 0;
        //! Follow smoothing time in seconds; 0 snaps. Clamped non-negative.
        virtual void SetSmoothTime(float seconds) = 0;
        //! In-plane deadzone half-extents; the target moves freely inside without
        //! moving the camera. Clamped non-negative.
        virtual void SetDeadzone(float halfX, float halfY) = 0;
        //! Distance the view leads the target's motion. Clamped non-negative.
        virtual void SetLookahead(float distance) = 0;
        //! Clamp the camera center to this in-plane rectangle (enables bounds).
        virtual void SetBounds(float minX, float minY, float maxX, float maxY) = 0;
        //! Stop clamping to bounds (free follow).
        virtual void ClearBounds() = 0;
        //! Add shake trauma from an event (e.g. a hit); clamped to 0..1. Shake is
        //! maxShake * trauma^2 and decays over time.
        virtual void AddTrauma(float amount) = 0;
        //! Disable to freeze the camera where it is (stops following and shaking).
        virtual void SetEnabled(bool enabled) = 0;
        //! Resolved camera state. Safe to poll.
        virtual Camera2DInfo GetCameraInfo() = 0;
    };

    using DioramaCamera2DRequestBus = AZ::EBus<DioramaCamera2DRequests>;

    //! Reflect the agent-facing camera bus and Camera2DInfo to the BehaviorContext
    //! (Common scope) so they are callable from launcher Lua, Python, and Script
    //! Canvas. Called from the camera component's Reflect.
    void ReflectCameraBuses(AZ::ReflectContext* context);
} // namespace Diorama
