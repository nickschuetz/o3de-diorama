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
                // Schedule a one-shot dirty so the new config is persisted.
                SchedulePersist();
            });
    }

    void EditorTilemapComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        m_persistPending = false;
        m_requestHandler.Disconnect();
        m_presenter.Disconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void EditorTilemapComponent::SchedulePersist()
    {
        m_persistPending = true;
        // Coalesce a burst of edits (e.g. a script writing every tile) into a
        // single persist on the next tick rather than one undo step per edit.
        if (!AZ::TickBus::Handler::BusIsConnected())
        {
            AZ::TickBus::Handler::BusConnect();
        }
    }

    void EditorTilemapComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        AZ::TickBus::Handler::BusDisconnect();
        if (!m_persistPending)
        {
            return;
        }
        m_persistPending = false;

        // MarkEntityDirty inside an undo batch records the component's new state
        // into the prefab DOM, so save (and undo/redo) capture the edited tilemap.
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
