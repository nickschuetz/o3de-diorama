/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SpritePresenter.h>
#include <Clients/SpriteRequestHandler.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Component/TickBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime SpriteComponent. It authors the
    //! shared SpriteComponentConfig in the entity inspector and renders a live
    //! preview in the editor viewport through the shared SpritePresenter, so the
    //! sprite is visible while editing without entering game mode. On export or
    //! play it builds the lightweight runtime SpriteComponent through
    //! BuildGameEntity. All Qt and AzToolsFramework dependencies stay in this
    //! editor module so the runtime client stays lean.
    class EditorSpriteComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private AZ::TickBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(Diorama::EditorSpriteComponent, EditorSpriteComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        EditorSpriteComponent() = default;
        ~EditorSpriteComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        // Called by the edit context when a property changes; refreshes the preview.
        AZ::u32 OnConfigChanged();

        // AZ::TickBus::Handler. Used only to coalesce request-bus edits: a single
        // tick after one or more edits, persist the change to the prefab once.
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // Mark the entity dirty so request-bus edits to m_config (e.g. an agent
        // setting the texture/size) are captured into the prefab on save, matching
        // how an inspector edit persists. Coalesced via OnTick.
        void SchedulePersist();

        SpriteComponentConfig m_config;
        SpritePresenter m_presenter;
        SpriteRequestHandler m_requestHandler;
        bool m_persistPending = false;
    };
} // namespace Diorama
