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

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a depth body, returned by GetDepthInfo. The verify-loop
    //! payload: an agent confirms the lane and the screen lift without a viewport.
    struct DioramaDepthBodyInfo
    {
        AZ_TYPE_INFO(Diorama::DioramaDepthBodyInfo, DioramaDepthBodyInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        float m_depth = 0.0f; //!< Current depth lane (toward/away from the camera).
        float m_screenLift = 0.0f; //!< Up-screen Y lift applied for the current depth.
        float m_sortBias = 0.0f; //!< Draw-order bias applied for the current depth.
    };

    //! Per-entity control of a 2.5D brawler depth body, addressed by the entity id.
    //! The body owns the character's DEPTH lane (toward/away from the camera): it lifts
    //! the sprite up-screen and biases its draw order so a flat orthographic scene reads
    //! as 2.5D, and exposes the depth so combat can gate hits to the same lane. The game
    //! moves the character in X with the transform and drives the lane with SetDepth.
    class DioramaDepthBodyRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaDepthBodyRequests, DioramaDepthBodyRequestsTypeId);
        virtual ~DioramaDepthBodyRequests() = default;

        //! Set the depth lane (toward/away from the camera); the sprite re-lifts and
        //! re-sorts. Clamped to the configured arena depth range.
        virtual void SetDepth(float depth) = 0;
        //! Move the depth lane toward `targetDepth` by at most `maxDelta` (for smooth
        //! lane changes / AI approach). Returns the new depth.
        virtual float MoveDepthToward(float targetDepth, float maxDelta) = 0;
        //! Current depth lane.
        virtual float GetDepth() = 0;
        //! Resolved depth state. Safe to poll.
        virtual DioramaDepthBodyInfo GetDepthInfo() = 0;
    };

    using DioramaDepthBodyRequestBus = AZ::EBus<DioramaDepthBodyRequests>;

    //! Reflect the depth-body struct and bus to the BehaviorContext (Common scope) so
    //! they are callable from launcher Lua, Python, and Script Canvas. Called from the
    //! depth body component's Reflect.
    void ReflectDepthBodyBuses(AZ::ReflectContext* context);
} // namespace Diorama
