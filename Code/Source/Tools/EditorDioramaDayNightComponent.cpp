/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorDioramaDayNightComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void EditorDioramaDayNightComponent::Reflect(AZ::ReflectContext* context)
    {
        // DioramaDayNightConfig is reflected by the runtime component; do not reflect it
        // again here or the type registers twice.
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorDioramaDayNightComponent, AzToolsFramework::Components::EditorComponentBase>()->Version(1)->Field(
                "Config", &EditorDioramaDayNightComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<EditorDioramaDayNightComponent>("Day/Night Cycle", "Drives a Diorama light's color and direction over the day")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &EditorDioramaDayNightComponent::m_config, "Config", "Day/night configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void EditorDioramaDayNightComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaDayNightService"));
    }

    void EditorDioramaDayNightComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaDayNightService"));
    }

    void EditorDioramaDayNightComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<DioramaDayNightComponent>(m_config);
    }
} // namespace Diorama
