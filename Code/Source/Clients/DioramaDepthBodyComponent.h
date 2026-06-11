/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DepthLane.h>
#include <Diorama/DioramaDepthBodyBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>

namespace Diorama
{
    //! Configuration for a 2.5D brawler depth body. The character's logical position is
    //! its transform X (left/right) plus a DEPTH lane (toward/away from the camera); the
    //! body renders that depth by lifting the sprite up-screen and biasing its draw order
    //! (the orthographic 2.5D look). A tilted/perspective camera does this for free, so
    //! set the lift to 0 and use the body only for the depth value + lane gating there.
    class DioramaDepthBodyConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaDepthBodyConfig, DioramaDepthBodyConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaDepthBodyConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaDepthBodyConfig() override = default;

        float m_depth = 0.0f; //!< Starting depth lane.
        float m_liftPerUnit = 0.25f; //!< Up-screen Y lift per unit of depth (0 with a tilted camera).
        float m_sortPerUnit = 1.0f; //!< Sprite sort-offset bias per unit of depth (nearer on top).
        float m_minDepth = 0.0f; //!< Arena near edge (clamps SetDepth).
        float m_maxDepth = 4.0f; //!< Arena far edge.
    };

    //! Runtime 2.5D brawler depth body. Owns the character's depth lane: each frame it
    //! sets the entity's Y from `baseY + DepthLane::ScreenLift(depth)` and the sprite's
    //! sort offset from `DepthLane::SortBias(depth)`, so a flat orthographic scene reads
    //! as 2.5D, and exposes the depth so combat gates hits to the same lane
    //! (`DepthLane::SameLane`). The game drives X with the transform and the lane with
    //! SetDepth / MoveDepthToward. Backed by the pure tested `DepthLane` core.
    class DioramaDepthBodyComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaDepthBodyRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaDepthBodyComponent, DioramaDepthBodyComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        DioramaDepthBodyComponent() = default;
        explicit DioramaDepthBodyComponent(const DioramaDepthBodyConfig& config);
        ~DioramaDepthBodyComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaDepthBodyRequests
        void SetDepth(float depth) override;
        float MoveDepthToward(float targetDepth, float maxDelta) override;
        float GetDepth() override;
        DioramaDepthBodyInfo GetDepthInfo() override;

    private:
        //! Apply the current depth to the transform Y (lift) and the sprite sort offset.
        void ApplyDepth();

        DioramaDepthBodyConfig m_config;
        float m_baseY = 0.0f; //!< Entity Y at depth 0 (captured on activate).
        bool m_haveBaseY = false;
    };
} // namespace Diorama
