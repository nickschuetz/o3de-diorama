/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorDioramaSimStateComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void EditorDioramaSimStateComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorDioramaSimStateComponent, AzToolsFramework::Components::EditorComponentBase>()->Version(1);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<EditorDioramaSimStateComponent>(
                        "Simulation State", "Enrolls this entity (its world translation) in the 2D Simulation Clock's frame capture")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
            }
        }
    }

    void EditorDioramaSimStateComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaSimStateService"));
    }

    void EditorDioramaSimStateComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaSimStateService"));
    }

    void EditorDioramaSimStateComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<DioramaSimStateComponent>();
    }
} // namespace Diorama
