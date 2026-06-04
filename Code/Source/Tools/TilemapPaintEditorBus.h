/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/TilemapPaint.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    //! Editor-only bridge between the tilemap paint Component Mode and the
    //! EditorTilemapComponent that owns the data. The mode handles viewport mouse
    //! input and tool logic; everything that needs the component's config, world
    //! transform, presenter, or the undo system lives on the component side and is
    //! reached through this bus (addressed by the entity id). This keeps the mode
    //! free of engine/asset state and keeps all edits on the one editor code path.
    class TilemapPaintEditorRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(TilemapPaintEditorRequests, TilemapPaintEditorRequestsTypeId);
        ~TilemapPaintEditorRequests() override = default;

        //! Tile id painted by a left stroke, and the brush square edge in cells.
        virtual int GetPaintActiveTile() const = 0;
        virtual int GetPaintBrushSize() const = 0;

        //! Grid size and the current tile at a cell (for rect/fill tools later).
        virtual int GetPaintColumns() const = 0;
        virtual int GetPaintRows() const = 0;
        virtual int GetPaintTileAt(int col, int row) const = 0;

        //! Intersect a world-space mouse ray with the tilemap's grid plane and
        //! resolve the cell it hits. Returns false (and leaves out-params unset) when
        //! the ray misses the plane or lands outside the grid.
        virtual bool PaintWorldRayToCell(const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection, int& outCol, int& outRow) const = 0;

        //! Set every listed cell to tileId (TilemapComponentConfig::EmptyTile to
        //! erase), refresh the editor preview, and mark the entity dirty so the
        //! enclosing undo batch captures the stroke and it persists to the prefab.
        virtual void PaintCells(const AZStd::vector<TilemapPaint::Cell>& cells, int tileId) = 0;
    };

    using TilemapPaintEditorRequestBus = AZ::EBus<TilemapPaintEditorRequests>;
} // namespace Diorama
