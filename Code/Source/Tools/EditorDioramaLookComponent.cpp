/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorDioramaLookComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void EditorDioramaLookComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorDioramaLookComponent, AzToolsFramework::Components::EditorComponentBase>()->Version(1)->Field(
                "Config", &EditorDioramaLookComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<EditorDioramaLookComponent>(
                        "2D Look", "One-component 2D post-processing: tuned bloom + vignette (drivable via DioramaLookRequestBus)")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorDioramaLookComponent::m_config, "Config", "2D look configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorDioramaLookComponent::OnConfigChanged);
            }
        }
    }

    void EditorDioramaLookComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        // Preview in the editor viewport. The viewport scene/feature processor may not
        // be ready yet on activate; if so, retry on tick until it applies.
        ApplyPreview();
        if (!m_previewApplied)
        {
            AZ::TickBus::Handler::BusConnect();
        }
    }

    void EditorDioramaLookComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        RemovePreview();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void EditorDioramaLookComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        ApplyPreview();
        if (m_previewApplied)
        {
            AZ::TickBus::Handler::BusDisconnect();
        }
    }

    AZ::u32 EditorDioramaLookComponent::OnConfigChanged()
    {
        ApplyPreview();
        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
    }

    void EditorDioramaLookComponent::ApplyPreview()
    {
        m_previewApplied = ApplyLookToScene(GetEntityId(), m_config);
    }

    void EditorDioramaLookComponent::RemovePreview()
    {
        if (!m_previewApplied)
        {
            return;
        }
        RemoveLookFromScene(GetEntityId());
        m_previewApplied = false;
    }

    void EditorDioramaLookComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<DioramaLookComponent>(m_config);
    }
} // namespace Diorama
