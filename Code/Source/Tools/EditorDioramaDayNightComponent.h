/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaDayNightComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime DioramaDayNightComponent. Authors the cycle
    //! timing and the four phase colors, and builds the runtime component via
    //! BuildGameEntity. The cycle drives the light at game time, so it is exercised in game
    //! mode (or by an agent reading GetDayNightInfo).
    class EditorDioramaDayNightComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaDayNightComponent,
            EditorDioramaDayNightComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        EditorDioramaDayNightComponent() = default;
        ~EditorDioramaDayNightComponent() override = default;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        DioramaDayNightConfig m_config;
    };
} // namespace Diorama
