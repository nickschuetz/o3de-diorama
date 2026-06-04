/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaLookComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor twin for the runtime DioramaLookComponent: authors the 2D look config
    //! and exports the runtime component via BuildGameEntity. Put it on any entity
    //! (e.g. the camera) to give the scene a tuned bloom + vignette at game time,
    //! drivable via DioramaLookRequestBus.
    class EditorDioramaLookComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaLookComponent, EditorDioramaLookComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        EditorDioramaLookComponent() = default;
        ~EditorDioramaLookComponent() override = default;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        DioramaLookConfig m_config;
    };
} // namespace Diorama
