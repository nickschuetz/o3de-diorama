/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorTilemapPaintComponentMode.h>
#include <Tools/TilemapPaintEditorBus.h>

#include <Clients/TilemapPaint.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    // Empty-cell sentinel, mirrors TilemapComponentConfig::EmptyTile (kept local so
    // the mode does not depend on the runtime config type).
    static constexpr int PaintEraseTile = -1;

    void EditorTilemapPaintComponentMode::Reflect([[maybe_unused]] AZ::ReflectContext* context)
    {
        // Intentionally empty: a Component Mode is constructed at runtime by the
        // ComponentModeDelegate (it has no default constructor and no serialized
        // state), so it must not be registered as a serializable class.
    }

    EditorTilemapPaintComponentMode::EditorTilemapPaintComponentMode(
        const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType)
        : AzToolsFramework::ComponentModeFramework::EditorBaseComponentMode(entityComponentIdPair, componentType)
    {
    }

    EditorTilemapPaintComponentMode::~EditorTilemapPaintComponentMode()
    {
        m_stroke.reset();
    }

    void EditorTilemapPaintComponentMode::Refresh()
    {
    }

    AZStd::string EditorTilemapPaintComponentMode::GetComponentModeName() const
    {
        return "Tilemap Paint";
    }

    AZ::Uuid EditorTilemapPaintComponentMode::GetComponentModeType() const
    {
        return azrtti_typeid<EditorTilemapPaintComponentMode>();
    }

    void EditorTilemapPaintComponentMode::StampAtRay(const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection, int tileId)
    {
        int col = 0;
        int row = 0;
        bool hit = false;
        TilemapPaintEditorRequestBus::EventResult(
            hit, GetEntityId(), &TilemapPaintEditorRequests::PaintWorldRayToCell, rayOrigin, rayDirection, col, row);
        if (!hit)
        {
            return;
        }

        int brush = 1;
        int columns = 1;
        int rows = 1;
        TilemapPaintEditorRequestBus::EventResult(brush, GetEntityId(), &TilemapPaintEditorRequests::GetPaintBrushSize);
        TilemapPaintEditorRequestBus::EventResult(columns, GetEntityId(), &TilemapPaintEditorRequests::GetPaintColumns);
        TilemapPaintEditorRequestBus::EventResult(rows, GetEntityId(), &TilemapPaintEditorRequests::GetPaintRows);

        AZStd::vector<TilemapPaint::Cell> cells;
        TilemapPaint::BrushCells(col, row, brush, columns, rows, cells);
        if (!cells.empty())
        {
            TilemapPaintEditorRequestBus::Event(GetEntityId(), &TilemapPaintEditorRequests::PaintCells, cells, tileId);
        }
    }

    bool EditorTilemapPaintComponentMode::HandleMouseInteraction(
        const AzToolsFramework::ViewportInteraction::MouseInteractionEvent& mouseInteraction)
    {
        const auto& interaction = mouseInteraction.m_mouseInteraction;
        const bool left = interaction.m_mouseButtons.Left();
        const bool right = interaction.m_mouseButtons.Right();
        const AzToolsFramework::ViewportInteraction::MouseEvent event = mouseInteraction.m_mouseEvent;

        // Left paints the active tile, right erases. Other buttons / hover moves are
        // left to the editor (camera, selection).
        if (!left && !right)
        {
            return false;
        }

        int activeTile = 0;
        TilemapPaintEditorRequestBus::EventResult(activeTile, GetEntityId(), &TilemapPaintEditorRequests::GetPaintActiveTile);
        const int tileId = right ? PaintEraseTile : activeTile;

        const AZ::Vector3& rayOrigin = interaction.m_mousePick.m_rayOrigin;
        const AZ::Vector3& rayDirection = interaction.m_mousePick.m_rayDirection;

        if (event == AzToolsFramework::ViewportInteraction::MouseEvent::Down)
        {
            m_stroke = AZStd::make_unique<AzToolsFramework::ScopedUndoBatch>("Paint Tiles");
            StampAtRay(rayOrigin, rayDirection, tileId);
            return true;
        }
        if (event == AzToolsFramework::ViewportInteraction::MouseEvent::Move && m_stroke)
        {
            StampAtRay(rayOrigin, rayDirection, tileId);
            return true;
        }
        if (event == AzToolsFramework::ViewportInteraction::MouseEvent::Up)
        {
            const bool wasPainting = m_stroke != nullptr;
            m_stroke.reset();
            return wasPainting;
        }
        return false;
    }
} // namespace Diorama
