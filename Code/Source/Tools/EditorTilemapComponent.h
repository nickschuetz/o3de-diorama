/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/TilemapPresenter.h>
#include <Clients/TilemapRequestHandler.h>
#include <Diorama/TilemapComponentConfig.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime TilemapComponent. It authors the
    //! shared TilemapComponentConfig in the entity inspector and renders a live
    //! preview in the editor viewport through the shared TilemapPresenter, so the
    //! tilemap is visible while editing without entering game mode. On export or
    //! play it builds the lightweight runtime TilemapComponent through
    //! BuildGameEntity. All Qt and AzToolsFramework dependencies stay in this
    //! editor module so the runtime client stays lean.
    class EditorTilemapComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorTilemapComponent, EditorTilemapComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        EditorTilemapComponent() = default;
        ~EditorTilemapComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        // Called by the edit context when a property changes; refreshes the preview.
        AZ::u32 OnConfigChanged();

        // Mark the entity dirty so the (possibly script-driven) request-bus edits to
        // m_config are captured into the prefab on save. Done synchronously per edit
        // so a scripted edit-then-save reliably bakes (see the .cpp for why this
        // cannot defer); one SetTile maps to one undo step, matching how painting a
        // tile in the inspector records one undo step.
        void PersistConfig();

        TilemapComponentConfig m_config;
        TilemapPresenter m_presenter;
        TilemapRequestHandler m_requestHandler;
    };
} // namespace Diorama
