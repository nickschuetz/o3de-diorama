/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/TilemapComponentConfig.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

#include <cmath>

namespace Diorama
{
    namespace
    {
        int AtLeastOne(int value)
        {
            return value < 1 ? 1 : value;
        }
    } // namespace

    int TilemapComponentConfig::GetColumns() const
    {
        return AtLeastOne(m_columns);
    }

    int TilemapComponentConfig::GetRows() const
    {
        return AtLeastOne(m_rows);
    }

    int TilemapComponentConfig::GetAtlasColumns() const
    {
        return AtLeastOne(m_atlasColumns);
    }

    int TilemapComponentConfig::GetAtlasRows() const
    {
        return AtLeastOne(m_atlasRows);
    }

    int TilemapComponentConfig::GetTileCount() const
    {
        return GetColumns() * GetRows();
    }

    int TilemapComponentConfig::GetAtlasCellCount() const
    {
        return GetAtlasColumns() * GetAtlasRows();
    }

    bool TilemapComponentConfig::InBounds(int col, int row) const
    {
        return col >= 0 && col < GetColumns() && row >= 0 && row < GetRows();
    }

    AZ::s32 TilemapComponentConfig::GetTile(int col, int row) const
    {
        if (!InBounds(col, row))
        {
            return EmptyTile;
        }
        const size_t index = static_cast<size_t>(row) * static_cast<size_t>(GetColumns()) + static_cast<size_t>(col);
        if (index >= m_tiles.size())
        {
            return EmptyTile;
        }
        return m_tiles[index];
    }

    int TilemapComponentConfig::CountFilledTiles() const
    {
        const int columns = GetColumns();
        const int rows = GetRows();
        int filled = 0;
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                if (GetTile(column, row) != EmptyTile)
                {
                    ++filled;
                }
            }
        }
        return filled;
    }

    void TilemapComponentConfig::SetTile(int col, int row, AZ::s32 value)
    {
        if (!InBounds(col, row))
        {
            return;
        }
        if (m_tiles.size() != static_cast<size_t>(GetTileCount()))
        {
            m_tiles.resize(static_cast<size_t>(GetTileCount()), EmptyTile);
        }
        const size_t index = static_cast<size_t>(row) * static_cast<size_t>(GetColumns()) + static_cast<size_t>(col);
        m_tiles[index] = value;
    }

    void TilemapComponentConfig::Fill(AZ::s32 value)
    {
        m_tiles.assign(static_cast<size_t>(GetTileCount()), value);
    }

    void TilemapComponentConfig::Clear()
    {
        Fill(EmptyTile);
    }

    void TilemapComponentConfig::GetTileUVRegion(AZ::s32 tileIndex, AZ::Vector2& outMin, AZ::Vector2& outMax) const
    {
        const int atlasColumns = GetAtlasColumns();
        const int atlasRows = GetAtlasRows();
        const int cellCount = atlasColumns * atlasRows;

        AZ::s32 clamped = tileIndex;
        if (clamped < 0)
        {
            clamped = 0;
        }
        else if (clamped >= cellCount)
        {
            clamped = cellCount - 1;
        }

        const int cellColumn = clamped % atlasColumns;
        const int cellRow = clamped / atlasColumns;

        const float uStep = 1.0f / static_cast<float>(atlasColumns);
        const float vStep = 1.0f / static_cast<float>(atlasRows);

        // v runs top-to-bottom (cell row 0 is the top of the atlas), matching the
        // sprite UV convention.
        outMin.Set(static_cast<float>(cellColumn) * uStep, static_cast<float>(cellRow) * vStep);
        outMax.Set(static_cast<float>(cellColumn + 1) * uStep, static_cast<float>(cellRow + 1) * vStep);
    }

    AZ::Vector3 TilemapComponentConfig::GetTileLocalPosition(int col, int row) const
    {
        const int columns = GetColumns();
        const int rows = GetRows();
        const float width = m_tileSize.GetX();
        const float height = m_tileSize.GetY();

        // Columns increase along +X, rows along -Z (row 0 at the top), centered so
        // the grid's middle sits at the entity origin.
        const float x = (static_cast<float>(col) + 0.5f) * width - static_cast<float>(columns) * width * 0.5f;
        const float z = static_cast<float>(rows) * height * 0.5f - (static_cast<float>(row) + 0.5f) * height;
        return AZ::Vector3(x, 0.0f, z);
    }

    bool TilemapComponentConfig::LocalPositionToCell(const AZ::Vector3& localPosition, int& outCol, int& outRow) const
    {
        const int columns = GetColumns();
        const int rows = GetRows();
        // Tile size is clamped to a small positive epsilon to avoid divide-by-zero
        // if an unconfigured/zero tile size reaches here.
        const float width = AZ::GetMax(m_tileSize.GetX(), 1e-4f);
        const float height = AZ::GetMax(m_tileSize.GetY(), 1e-4f);

        // Exact inverse of GetTileLocalPosition: solve for the cell whose extent
        // contains the point. floor maps any point within a cell to that cell index.
        const float colF = (localPosition.GetX() + static_cast<float>(columns) * width * 0.5f) / width;
        const float rowF = (static_cast<float>(rows) * height * 0.5f - localPosition.GetZ()) / height;
        outCol = static_cast<int>(std::floor(colF));
        outRow = static_cast<int>(std::floor(rowF));
        return InBounds(outCol, outRow);
    }

    void TilemapComponentConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TilemapComponentConfig, AZ::ComponentConfig>()
                ->Version(2)
                ->Field("tilemapAsset", &TilemapComponentConfig::m_tilemapAsset)
                ->Field("atlas", &TilemapComponentConfig::m_atlas)
                ->Field("columns", &TilemapComponentConfig::m_columns)
                ->Field("rows", &TilemapComponentConfig::m_rows)
                ->Field("atlasColumns", &TilemapComponentConfig::m_atlasColumns)
                ->Field("atlasRows", &TilemapComponentConfig::m_atlasRows)
                ->Field("tileSize", &TilemapComponentConfig::m_tileSize)
                ->Field("tiles", &TilemapComponentConfig::m_tiles)
                ->Field("tint", &TilemapComponentConfig::m_tint)
                ->Field("sortOffset", &TilemapComponentConfig::m_sortOffset);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<TilemapComponentConfig>("Diorama Tilemap", "A grid of atlas tiles drawn as one batched layer")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Source")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &TilemapComponentConfig::m_tilemapAsset,
                        "Tilemap Asset",
                        "Optional compiled tilemap (.dtilemapc). When set, it supplies the grid, atlas, and tiles; "
                        "the inline fields below are the fallback when no asset is assigned")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Atlas")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &TilemapComponentConfig::m_atlas, "Atlas", "Texture divided into tile cells")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &TilemapComponentConfig::m_atlasColumns, "Atlas Columns", "Cells across the atlas")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TilemapComponentConfig::m_atlasRows, "Atlas Rows", "Cells down the atlas")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Grid")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TilemapComponentConfig::m_columns, "Columns", "Tilemap width in cells")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TilemapComponentConfig::m_rows, "Rows", "Tilemap height in cells")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TilemapComponentConfig::m_tileSize, "Tile Size", "World size of one tile")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Appearance")
                    ->DataElement(AZ::Edit::UIHandlers::Color, &TilemapComponentConfig::m_tint, "Tint", "Color multiplied into every tile")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &TilemapComponentConfig::m_sortOffset,
                        "Sort Offset",
                        "Transparent draw-order bias for the layer");
                // m_tiles is authored through the request bus or a build script,
                // not the raw inspector (an integer grid is not usefully editable
                // as a flat array); it is intentionally not an edit element.
            }
        }
    }
} // namespace Diorama
