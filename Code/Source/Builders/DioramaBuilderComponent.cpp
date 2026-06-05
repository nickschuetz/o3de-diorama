/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Builders/DioramaBuilderComponent.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaBuilderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaBuilderComponent, AZ::Component>()->Version(1);
        }
    }

    void DioramaBuilderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaBuilderService"));
    }

    void DioramaBuilderComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaBuilderService"));
    }

    void DioramaBuilderComponent::Activate()
    {
        m_asepriteBuilder.RegisterBuilder();
    }

    void DioramaBuilderComponent::Deactivate()
    {
    }
} // namespace Diorama
