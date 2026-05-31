/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/TilemapComponent.h>
#include <Tools/EditorTilemapComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace Diorama
{
    void EditorTilemapComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorTilemapComponent, AzToolsFramework::Components::EditorComponentBase>()->Version(1)->Field(
                "Config", &EditorTilemapComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorTilemapComponent>("Tilemap", "Draws a world-space grid of atlas tiles through Atom")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorTilemapComponent::m_config, "Config", "Tilemap configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorTilemapComponent::OnConfigChanged);
            }
        }
    }

    void EditorTilemapComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaTilemapService"));
    }

    void EditorTilemapComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaTilemapService"));
    }

    void EditorTilemapComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void EditorTilemapComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        AZ_UNUSED(dependent);
    }

    void EditorTilemapComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        m_presenter.Connect(GetEntityId(), m_config);

        // Expose the same AI request API in the editor. A verb edits m_config,
        // pushes it to the live preview, and refreshes the inspector so an
        // agent-driven change shows up in the human UI too. Both faces drive the
        // one config.
        m_requestHandler.Connect(
            GetEntityId(),
            m_config,
            m_presenter,
            [this]()
            {
                m_presenter.SetConfig(m_config);
                AzToolsFramework::ToolsApplicationEvents::Bus::Broadcast(
                    &AzToolsFramework::ToolsApplicationEvents::InvalidatePropertyDisplay, AzToolsFramework::Refresh_Values);
                // A request-bus edit changes m_config in place but, unlike an
                // inspector edit, does not go through the property editor, so the
                // prefab never learns about it and the change is lost on save.
                // Persist it now so the new config is baked.
                PersistConfig();
            });
    }

    void EditorTilemapComponent::Deactivate()
    {
        m_requestHandler.Disconnect();
        m_presenter.Disconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void EditorTilemapComponent::PersistConfig()
    {
        // Only an actualized entity may touch the undo/prefab system. During prefab
        // construction or propagation the entity can be inactive and a request-bus
        // edit must not dirty it (mirrors EditorComponentBase::SetDirty's guard).
        AZ::Entity* entity = GetEntity();
        if (!entity || entity->GetState() != AZ::Entity::State::Active)
        {
            return;
        }

        // Record the new config into the prefab DOM synchronously. This cannot be
        // deferred to a later tick to coalesce a burst: there is no pre-save hook
        // (OnSaveLevel fires AFTER the level is written), so a script that writes
        // tiles and immediately saves would outrun a deferred dirty and bake the
        // old config. MarkEntityDirty is a no-op outside an undo batch, and the
        // batch commits the entity's DOM delta when it closes here, so the edit is
        // captured before control returns to the caller (and before any save).
        AzToolsFramework::ScopedUndoBatch undoBatch("Edit Diorama Tilemap");
        undoBatch.MarkEntityDirty(GetEntityId());
    }

    void EditorTilemapComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        // Hand the authored configuration to the runtime component so the exported
        // game runs only the lightweight client code path.
        gameEntity->CreateComponent<TilemapComponent>(m_config);
    }

    AZ::u32 EditorTilemapComponent::OnConfigChanged()
    {
        // Push the edited values to the live preview immediately.
        m_presenter.SetConfig(m_config);
        return AZ::Edit::PropertyRefreshLevels::None;
    }
} // namespace Diorama
