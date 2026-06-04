/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaSkeletalClipComponent.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor twin for DioramaSkeletalClipComponent: authors the clip and exports the
    //! runtime component via BuildGameEntity. It also previews a pose directly in the
    //! editor viewport: tick the "Preview pose" slider to scrub the clip and watch the
    //! bones move, no game mode needed. The preview is non-destructive: the bones'
    //! original local transforms are captured and restored when preview is turned off
    //! or the component is deactivated, so it never alters the saved prefab.
    class EditorDioramaSkeletalClipComponent final : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaSkeletalClipComponent,
            EditorDioramaSkeletalClipComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        EditorDioramaSkeletalClipComponent() = default;
        ~EditorDioramaSkeletalClipComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        //! Re-apply (or remove) the preview when a property changes.
        AZ::u32 OnConfigChanged();
        //! Pose the bones at the preview time, capturing their rest transforms first.
        void ApplyPreview();
        //! Restore the captured rest transforms and clear the preview.
        void RemovePreview();

        DioramaSkeletalClipConfig m_config;

        //! Scrub the clip in the editor: pose at this 0..1 fraction of the duration.
        bool m_previewEnabled = false;
        float m_previewNormalizedTime = 0.0f;

        bool m_previewApplied = false;
        AZStd::vector<AZ::EntityId> m_previewBones;
        AZStd::vector<AZ::Transform> m_restTransforms;
    };
} // namespace Diorama
