/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaAsepriteComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/std/string/string.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor twin for DioramaAsepriteComponent. Its one job beyond authoring is the
    //! import: point "Aseprite JSON" at an exported sprite-sheet .json and it parses the
    //! frames + tags + atlas size into the config (serialized into the prefab, so the
    //! runtime needs no file IO). Exports the runtime component via BuildGameEntity.
    class EditorDioramaAsepriteComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaAsepriteComponent,
            EditorDioramaAsepriteComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        EditorDioramaAsepriteComponent() = default;
        ~EditorDioramaAsepriteComponent() override = default;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        //! Parse m_jsonSourcePath into m_config (frames/tags/atlas size). Returns a
        //! refresh level so the Inspector redraws the filled fields.
        AZ::u32 OnImport();

        //! Path to the Aseprite "Export Sprite Sheet" JSON (project-relative or alias).
        AZStd::string m_jsonSourcePath;
        //! Last import status, shown read-only in the Inspector.
        AZStd::string m_importStatus = "No JSON imported yet.";

        DioramaAsepriteConfig m_config;
    };
} // namespace Diorama
