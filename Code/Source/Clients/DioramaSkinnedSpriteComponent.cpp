/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaSkinnedSpriteComponent.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    DioramaSkinnedSpriteComponent::DioramaSkinnedSpriteComponent(const DioramaSkinnedSpriteConfig& config)
        : m_config(config)
    {
    }

    void DioramaSkinnedSpriteComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaSkinnedSpriteConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSkinnedSpriteComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaSkinnedSpriteComponent::m_config);
        }

        ReflectSkinnedSpriteBuses(context);
    }

    void DioramaSkinnedSpriteComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaSkinnedSpriteService"));
    }

    void DioramaSkinnedSpriteComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaSkinnedSpriteService"));
    }

    void DioramaSkinnedSpriteComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void DioramaSkinnedSpriteComponent::Activate()
    {
        m_presenter.SetConfig(m_config);
        m_presenter.Connect(GetEntityId());
        DioramaSkinnedSpriteRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
    }

    void DioramaSkinnedSpriteComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSkinnedSpriteRequestBus::Handler::BusDisconnect();
        m_presenter.Disconnect();
    }

    void DioramaSkinnedSpriteComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_presenter.Tick();
    }

    void DioramaSkinnedSpriteComponent::SetBoneRotation(const AZStd::string& boneName, float degrees)
    {
        m_presenter.SetBoneRotation(boneName, degrees);
    }

    void DioramaSkinnedSpriteComponent::SetBoneTranslation(const AZStd::string& boneName, const AZ::Vector2& offset)
    {
        m_presenter.SetBoneTranslation(boneName, offset);
    }

    void DioramaSkinnedSpriteComponent::ResetPose()
    {
        m_presenter.ResetPose();
    }

    SkinnedSpriteInfo DioramaSkinnedSpriteComponent::GetSkinnedSpriteInfo()
    {
        return m_presenter.GetInfo();
    }
} // namespace Diorama
