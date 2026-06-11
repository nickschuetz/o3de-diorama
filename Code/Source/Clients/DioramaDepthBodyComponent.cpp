/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaDepthBodyComponent.h>

#include <Diorama/SpriteBus.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaDepthBodyConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaDepthBodyConfig>()
                ->Version(1)
                ->Field("depth", &DioramaDepthBodyConfig::m_depth)
                ->Field("liftPerUnit", &DioramaDepthBodyConfig::m_liftPerUnit)
                ->Field("sortPerUnit", &DioramaDepthBodyConfig::m_sortPerUnit)
                ->Field("minDepth", &DioramaDepthBodyConfig::m_minDepth)
                ->Field("maxDepth", &DioramaDepthBodyConfig::m_maxDepth);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaDepthBodyConfig>("Depth Body Config", "2.5D depth lane: sprite lift + draw-order from depth")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDepthBodyConfig::m_depth,
                        "Depth",
                        "Starting depth lane (toward/away from camera)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDepthBodyConfig::m_liftPerUnit,
                        "Lift Per Unit",
                        "Up-screen Y lift per unit of depth (set 0 with a tilted/perspective camera)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDepthBodyConfig::m_sortPerUnit,
                        "Sort Per Unit",
                        "Sprite sort-offset bias per unit of depth (nearer draws on top)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaDepthBodyConfig::m_minDepth, "Min Depth", "Arena near edge")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaDepthBodyConfig::m_maxDepth, "Max Depth", "Arena far edge");
            }
        }
    }

    DioramaDepthBodyComponent::DioramaDepthBodyComponent(const DioramaDepthBodyConfig& config)
        : m_config(config)
    {
    }

    void DioramaDepthBodyComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaDepthBodyConfig::Reflect(context);
        ReflectDepthBodyBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaDepthBodyComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaDepthBodyComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<DioramaDepthBodyComponent>("2.5D Depth Body", "Depth-lane lift + draw-order for a 2.5D brawler character")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaDepthBodyComponent::m_config, "Config", "Depth body configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void DioramaDepthBodyComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaDepthBodyService"));
    }

    void DioramaDepthBodyComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        // One depth body per entity: two would both own the entity's Y and sort offset
        // and fight over them each tick.
        incompatible.push_back(AZ_CRC_CE("DioramaDepthBodyService"));
    }

    void DioramaDepthBodyComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void DioramaDepthBodyComponent::Activate()
    {
        if (m_config.m_maxDepth < m_config.m_minDepth)
        {
            m_config.m_maxDepth = m_config.m_minDepth;
        }
        m_config.m_depth = AZ::GetClamp(m_config.m_depth, m_config.m_minDepth, m_config.m_maxDepth);

        // The authored Y corresponds to the starting depth, so the base (depth-0) Y is
        // the authored Y minus the starting lift. ApplyDepth then re-derives Y from any
        // depth without drifting away from where the character was placed.
        AZ::Vector3 world = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        m_baseY = world.GetY() - DepthLane::ScreenLift(m_config.m_depth, m_config.m_liftPerUnit);
        m_haveBaseY = true;

        DioramaDepthBodyRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        ApplyDepth();
    }

    void DioramaDepthBodyComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaDepthBodyRequestBus::Handler::BusDisconnect();
    }

    void DioramaDepthBodyComponent::ApplyDepth()
    {
        if (!m_haveBaseY)
        {
            return;
        }
        // Keep X/Z (the game owns horizontal movement), set Y from the depth lift.
        AZ::Vector3 world = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        const float liftedY = m_baseY + DepthLane::ScreenLift(m_config.m_depth, m_config.m_liftPerUnit);
        world.SetY(liftedY);
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, world);

        // Draw-order bias so nearer characters render on top (no-op if no sprite).
        DioramaSpriteRequestBus::Event(
            GetEntityId(), &DioramaSpriteRequests::SetSortOffset, DepthLane::SortBias(m_config.m_depth, m_config.m_sortPerUnit));
    }

    void DioramaDepthBodyComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        ApplyDepth();
    }

    void DioramaDepthBodyComponent::SetDepth(float depth)
    {
        m_config.m_depth = AZ::GetClamp(depth, m_config.m_minDepth, m_config.m_maxDepth);
        ApplyDepth();
    }

    float DioramaDepthBodyComponent::MoveDepthToward(float targetDepth, float maxDelta)
    {
        const float clampedTarget = AZ::GetClamp(targetDepth, m_config.m_minDepth, m_config.m_maxDepth);
        m_config.m_depth = DepthLane::MoveToward(m_config.m_depth, clampedTarget, maxDelta);
        ApplyDepth();
        return m_config.m_depth;
    }

    float DioramaDepthBodyComponent::GetDepth()
    {
        return m_config.m_depth;
    }

    DioramaDepthBodyInfo DioramaDepthBodyComponent::GetDepthInfo()
    {
        DioramaDepthBodyInfo info;
        info.m_depth = m_config.m_depth;
        info.m_screenLift = DepthLane::ScreenLift(m_config.m_depth, m_config.m_liftPerUnit);
        info.m_sortBias = DepthLane::SortBias(m_config.m_depth, m_config.m_sortPerUnit);
        return info;
    }
} // namespace Diorama
