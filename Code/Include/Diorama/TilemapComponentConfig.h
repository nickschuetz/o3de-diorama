/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/std/containers/vector.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace Diorama
{
    //! Shared configuration for a world-space tilemap: a grid of cells, each
    //! drawing one cell of a shared atlas texture as a quad. The same struct is
    //! used by the runtime TilemapComponent and the EditorTilemapComponent so the
    //! editor authors values and hands an identical configuration to the runtime
    //! component through BuildGameEntity.
    //!
    //! Each tile is drawn through the same batched sprite feature processor as a
    //! Sprite, so a whole layer that shares the atlas collapses into one draw
    //! call. Tiles lie in the entity's local X (columns) and Z (rows) plane,
    //! centered on the entity origin; rotate the entity to lay the map flat as a
    //! floor or stand it up as a wall.
    class TilemapComponentConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(TilemapComponentConfig, TilemapComponentConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(TilemapComponentConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        virtual ~TilemapComponentConfig() = default;

        //! Empty-tile sentinel: a cell with this value draws nothing.
        static constexpr AZ::s32 EmptyTile = -1;

        //! Atlas texture sampled per tile. NoLoad keeps the reference cheap until
        //! the component activates and explicitly requests the load.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_atlas{ AZ::Data::AssetLoadBehavior::PreLoad };

        //! Tilemap grid dimensions in cells.
        int m_columns = 1;
        int m_rows = 1;

        //! How the atlas texture is divided into tile cells (left-to-right,
        //! top-to-bottom). A tile value indexes one of these cells.
        int m_atlasColumns = 1;
        int m_atlasRows = 1;

        //! World size of a single tile (width along local X, height along local Z).
        AZ::Vector2 m_tileSize = AZ::Vector2(1.0f, 1.0f);

        //! Row-major tile data, one entry per cell (length m_columns * m_rows).
        //! Each entry is an atlas cell index, or EmptyTile for a blank cell.
        AZStd::vector<AZ::s32> m_tiles;

        //! Color multiplied into every tile. Alpha drives transparency.
        AZ::Color m_tint = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);

        //! Transparent draw-order bias for the whole layer; larger draws on top.
        float m_sortOffset = 0.0f;

        // --- Pure helpers (no asset access), shared by the renderer and the bus,
        // and unit-tested independently of any running application. ---

        //! Grid/atlas dimensions clamped to at least 1.
        int GetColumns() const;
        int GetRows() const;
        int GetAtlasColumns() const;
        int GetAtlasRows() const;

        //! Number of cells in the grid (GetColumns() * GetRows()).
        int GetTileCount() const;
        //! Number of cells in the atlas (GetAtlasColumns() * GetAtlasRows()).
        int GetAtlasCellCount() const;

        //! True when (col, row) is inside the grid.
        bool InBounds(int col, int row) const;

        //! Tile value at (col, row), or EmptyTile when out of bounds or unset.
        AZ::s32 GetTile(int col, int row) const;

        //! Number of in-bounds cells whose value is not EmptyTile.
        int CountFilledTiles() const;

        //! Set the tile value at (col, row). Grows m_tiles to the full grid size
        //! (filling new cells with EmptyTile) on first write. No-op out of bounds.
        void SetTile(int col, int row, AZ::s32 value);

        //! Fill every cell with the same value (sizes m_tiles to the grid).
        void Fill(AZ::s32 value);

        //! Clear every cell to EmptyTile.
        void Clear();

        //! Normalized atlas sub-rectangle for an atlas cell index, in the same
        //! space as a sprite's UV region. The index is clamped to the atlas.
        void GetTileUVRegion(AZ::s32 tileIndex, AZ::Vector2& outMin, AZ::Vector2& outMax) const;

        //! Local-space center of the cell at (col, row). Columns run along +X and
        //! rows along -Z (row 0 at the top), centered so the grid's middle sits at
        //! the entity origin. Y is always 0.
        AZ::Vector3 GetTileLocalPosition(int col, int row) const;

        //! Inverse of GetTileLocalPosition: map a local-space point (its Y is
        //! ignored; the grid lies on the XZ plane) to the cell that contains it.
        //! Writes the cell into outCol/outRow and returns true when the point lands
        //! inside the grid. Out-of-grid points still write the (unclamped) cell they
        //! map to but return false, so the caller can clamp or reject.
        bool LocalPositionToCell(const AZ::Vector3& localPosition, int& outCol, int& outRow) const;
    };
} // namespace Diorama
