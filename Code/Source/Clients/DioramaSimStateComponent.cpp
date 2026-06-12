/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaSimStateComponent.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaSimStateComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSimStateComponent, AZ::Component>()->Version(1);
        }
    }

    void DioramaSimStateComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaSimStateService"));
    }

    void DioramaSimStateComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaSimStateService"));
    }

    void DioramaSimStateComponent::Activate()
    {
        DioramaSimStateRegistryBus::Handler::BusConnect();
        DioramaSimStateParticipantBus::Handler::BusConnect(GetEntityId());
    }

    void DioramaSimStateComponent::Deactivate()
    {
        DioramaSimStateParticipantBus::Handler::BusDisconnect();
        DioramaSimStateRegistryBus::Handler::BusDisconnect();
    }

    AZ::EntityId DioramaSimStateComponent::GetSimStateEntity()
    {
        return GetEntityId();
    }

    void DioramaSimStateComponent::SaveSimState(SimState::Writer& writer)
    {
        AZ::Vector3 translation = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(translation, GetEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);

        const size_t sizePos = writer.BeginChunk(TranslationChunkTag);
        writer.F32(translation.GetX());
        writer.F32(translation.GetY());
        writer.F32(translation.GetZ());
        writer.EndChunk(sizePos);
    }

    bool DioramaSimStateComponent::TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload)
    {
        if (tag != TranslationChunkTag)
        {
            return false;
        }
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!payload.F32(x) || !payload.F32(y) || !payload.F32(z))
        {
            return false; // truncated payload: reject rather than apply garbage
        }
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(x, y, z));
        return true;
    }
} // namespace Diorama
