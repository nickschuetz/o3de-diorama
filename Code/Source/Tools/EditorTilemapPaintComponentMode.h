/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ComponentMode/EditorBaseComponentMode.h>
#include <AzToolsFramework/ViewportSelection/EditorInteractionSystemViewportSelectionRequestBus.h>

namespace Diorama
{
    //! Viewport Component Mode for painting the tilemap. Entered from the
    //! EditorTilemapComponent's "Paint" button; while active, a left-drag stamps the
    //! active tile and a right-drag erases, using a square brush. It only does tool
    //! logic + mouse handling: the cell math, config edits, preview refresh, and undo
    //! all live on the component, reached through TilemapPaintEditorRequestBus. Each
    //! stroke (mouse down to up) is one undo step.
    class EditorTilemapPaintComponentMode
        : public AzToolsFramework::ComponentModeFramework::EditorBaseComponentMode
        , public AzToolsFramework::ViewportInteraction::ViewportSelectionRequests
    {
    public:
        AZ_CLASS_ALLOCATOR(EditorTilemapPaintComponentMode, AZ::SystemAllocator)
        AZ_RTTI(
            EditorTilemapPaintComponentMode,
            EditorTilemapPaintComponentModeTypeId,
            AzToolsFramework::ComponentModeFramework::EditorBaseComponentMode)

        static void Reflect(AZ::ReflectContext* context);

        EditorTilemapPaintComponentMode(const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType);
        ~EditorTilemapPaintComponentMode() override;

        // AzToolsFramework::ComponentModeFramework::EditorBaseComponentMode
        void Refresh() override;
        AZStd::string GetComponentModeName() const override;
        AZ::Uuid GetComponentModeType() const override;

    private:
        // AzToolsFramework::ViewportInteraction::ViewportSelectionRequests
        bool HandleMouseInteraction(const AzToolsFramework::ViewportInteraction::MouseInteractionEvent& mouseInteraction) override;

        //! Resolve the cell under the ray and stamp the brush there with tileId.
        void StampAtRay(const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection, int tileId);

        //! Alive for the duration of a single stroke so it is one undo step.
        AZStd::unique_ptr<AzToolsFramework::ScopedUndoBatch> m_stroke;
    };
} // namespace Diorama
