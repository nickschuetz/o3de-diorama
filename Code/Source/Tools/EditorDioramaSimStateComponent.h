/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaSimStateComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime DioramaSimStateComponent (the
    //! Simulation State marker). No configuration: adding it enrolls the entity in
    //! the 2D Simulation Clock's frame capture.
    class EditorDioramaSimStateComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaSimStateComponent,
            EditorDioramaSimStateComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        EditorDioramaSimStateComponent() = default;
        ~EditorDioramaSimStateComponent() override = default;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;
    };
} // namespace Diorama
