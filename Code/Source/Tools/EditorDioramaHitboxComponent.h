/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaHitboxComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime DioramaHitboxComponent. Authors the box
    //! library (hitboxes/hurtboxes with frame windows) and builds the runtime component
    //! via BuildGameEntity. No live preview: activation is frame-driven at game time, so
    //! the boxes are exercised in game mode (or by an agent reading GetHitboxInfo).
    class EditorDioramaHitboxComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaHitboxComponent, EditorDioramaHitboxComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        EditorDioramaHitboxComponent() = default;
        ~EditorDioramaHitboxComponent() override = default;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        DioramaHitboxConfig m_config;
    };
} // namespace Diorama
