/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Builders/DioramaBuilderComponent.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>

#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaBuilderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            // The AssetBuilder application activates system components carrying the
            // AssetBuilder tag (it does not consult the module's required-components
            // list), so this tag is what makes the builder register in the AssetProcessor.
            serializeContext->Class<DioramaBuilderComponent, AZ::Component>()->Version(1)->Attribute(
                AZ::Edit::Attributes::SystemComponentTags, AZStd::vector<AZ::Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }));
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
