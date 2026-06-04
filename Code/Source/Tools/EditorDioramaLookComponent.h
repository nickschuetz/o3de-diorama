/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaLookComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/TickBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor twin for the runtime DioramaLookComponent: authors the 2D look config
    //! and exports the runtime component via BuildGameEntity. It also applies the look
    //! to the editor viewport's PostProcessFeatureProcessor while selected/active, so
    //! you see the bloom + vignette live as you tune it (the same way Atom's own
    //! PostFx components preview), not only after entering game mode.
    class EditorDioramaLookComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AZ::TickBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaLookComponent, EditorDioramaLookComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        EditorDioramaLookComponent() = default;
        ~EditorDioramaLookComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    protected:
        // AZ::TickBus (only used to retry the preview until the viewport scene is ready)
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        //! Re-apply the look to the editor viewport when a property changes.
        AZ::u32 OnConfigChanged();
        void ApplyPreview();
        void RemovePreview();

        DioramaLookConfig m_config;
        bool m_previewApplied = false;
    };
} // namespace Diorama
