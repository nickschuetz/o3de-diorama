/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaSkinnedSpriteComponent.h>

#include <AzCore/Component/TransformBus.h>
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
        DioramaSimStateParticipantBus::Handler::BusConnect(GetEntityId());
        if (m_config.m_useSimClock)
        {
            DioramaSimTickNotificationBus::Handler::BusConnect();
        }
    }

    void DioramaSkinnedSpriteComponent::Deactivate()
    {
        DioramaSimStateParticipantBus::Handler::BusDisconnect();
        DioramaSimTickNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSkinnedSpriteRequestBus::Handler::BusDisconnect();
        m_presenter.Disconnect();
    }

    void DioramaSkinnedSpriteComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        // Use Simulation Clock mode: a running clock owns the advance (OnSimTick); with no clock
        // the render tick advances (editor preview + non-fighting scenes).
        if (m_config.m_useSimClock && DioramaSimClockRequestBus::HasHandlers())
        {
            return;
        }
        m_presenter.Tick(deltaTime);
    }

    void DioramaSkinnedSpriteComponent::OnSimTick([[maybe_unused]] AZ::s64 frame, float stepSeconds)
    {
        m_presenter.Tick(stepSeconds);
    }

    void DioramaSkinnedSpriteComponent::PlayAnimation(const AZStd::string& name, bool looping)
    {
        m_presenter.PlayAnimation(name, looping);
    }

    void DioramaSkinnedSpriteComponent::StopAnimation()
    {
        m_presenter.StopAnimation();
    }

    void DioramaSkinnedSpriteComponent::SetAnimationSpeed(float speed)
    {
        m_presenter.SetAnimationSpeed(speed);
    }

    void DioramaSkinnedSpriteComponent::SetBoneRotation(const AZStd::string& boneName, float degrees)
    {
        m_presenter.SetBoneRotation(boneName, degrees);
    }

    void DioramaSkinnedSpriteComponent::SetBoneTranslation(const AZStd::string& boneName, float x, float y)
    {
        m_presenter.SetBoneTranslation(boneName, x, y);
    }

    void DioramaSkinnedSpriteComponent::ResetPose()
    {
        m_presenter.ResetPose();
    }

    bool DioramaSkinnedSpriteComponent::HasBone(const AZStd::string& boneName)
    {
        return m_presenter.HasBone(boneName);
    }

    AZ::Vector3 DioramaSkinnedSpriteComponent::GetBoneWorldPosition(const AZStd::string& boneName)
    {
        AZ::Vector3 world = AZ::Vector3::CreateZero();
        if (m_presenter.GetBoneWorld(boneName, world))
        {
            return world;
        }
        // Unknown bone / no rig: degrade to the rig origin so a caller always gets a
        // sensible anchor (HasBone distinguishes the cases).
        AZ::TransformBus::EventResult(world, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
        return world;
    }

    SkinnedSpriteInfo DioramaSkinnedSpriteComponent::GetSkinnedSpriteInfo()
    {
        return m_presenter.GetInfo();
    }

    void DioramaSkinnedSpriteComponent::SetUseSimClock(bool enabled)
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

    bool DioramaSkinnedSpriteComponent::GetUseSimClock()
    {
        return m_config.m_useSimClock;
    }

    void DioramaSkinnedSpriteComponent::SaveSimState(SimState::Writer& writer)
    {
        const size_t sizePos = writer.BeginChunk(SkinnedChunkTag);
        m_presenter.SaveState(writer);
        writer.EndChunk(sizePos);
    }

    bool DioramaSkinnedSpriteComponent::TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload)
    {
        if (tag != SkinnedChunkTag)
        {
            return false;
        }
        return m_presenter.RestoreState(payload);
    }
} // namespace Diorama
