/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaBulletEmitterComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime DioramaBulletEmitterComponent. Authors
    //! the pattern, motion, and collision config and builds the runtime component via
    //! BuildGameEntity. No edit preview: bullets are emitted and simulated at game
    //! time, so the pattern is exercised in game mode (or by an agent reading
    //! GetBulletInfo).
    class EditorDioramaBulletEmitterComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaBulletEmitterComponent,
            EditorDioramaBulletEmitterComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        EditorDioramaBulletEmitterComponent() = default;
        ~EditorDioramaBulletEmitterComponent() override = default;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        DioramaBulletConfig m_config;
    };
} // namespace Diorama
