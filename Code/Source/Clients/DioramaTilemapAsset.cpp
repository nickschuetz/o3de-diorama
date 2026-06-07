/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaTilemapAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaTilemapLayerData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaTilemapLayerData>()
                ->Version(1)
                ->Field("name", &DioramaTilemapLayerData::m_name)
                ->Field("tiles", &DioramaTilemapLayerData::m_tiles)
                ->Field("sortOffset", &DioramaTilemapLayerData::m_sortOffset)
                ->Field("tint", &DioramaTilemapLayerData::m_tint);
        }
    }

    void DioramaTilemapAsset::Reflect(AZ::ReflectContext* context)
    {
        DioramaTilemapLayerData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaTilemapAsset, AZ::Data::AssetData>()
                ->Version(1)
                ->Field("atlasTexturePath", &DioramaTilemapAsset::m_atlasTexturePath)
                ->Field("columns", &DioramaTilemapAsset::m_columns)
                ->Field("rows", &DioramaTilemapAsset::m_rows)
                ->Field("atlasColumns", &DioramaTilemapAsset::m_atlasColumns)
                ->Field("atlasRows", &DioramaTilemapAsset::m_atlasRows)
                ->Field("tileWidth", &DioramaTilemapAsset::m_tileWidth)
                ->Field("tileHeight", &DioramaTilemapAsset::m_tileHeight)
                ->Field("layers", &DioramaTilemapAsset::m_layers);
        }
    }

    bool DioramaTilemapAsset::IsValid() const
    {
        if (m_columns < 1 || m_columns > TilemapAssetLimits::MaxDimension || m_rows < 1 || m_rows > TilemapAssetLimits::MaxDimension)
        {
            return false;
        }
        if (m_atlasColumns < 1 || m_atlasColumns > TilemapAssetLimits::MaxAtlasDimension || m_atlasRows < 1 ||
            m_atlasRows > TilemapAssetLimits::MaxAtlasDimension)
        {
            return false;
        }
        if (m_layers.empty() || m_layers.size() > static_cast<size_t>(TilemapAssetLimits::MaxLayers))
        {
            return false;
        }

        const AZ::s64 expectedCells = static_cast<AZ::s64>(m_columns) * m_rows;
        const int atlasCells = m_atlasColumns * m_atlasRows;
        for (const DioramaTilemapLayerData& layer : m_layers)
        {
            if (static_cast<AZ::s64>(layer.m_tiles.size()) != expectedCells)
            {
                return false;
            }
            for (const AZ::s32 tile : layer.m_tiles)
            {
                // -1 is EmptyTile (draws nothing). Otherwise the low bits index a real
                // atlas cell (the high bits may carry flip flags); an out-of-range
                // index would sample garbage UVs, and no bit outside the index +
                // flip masks may be set.
                if (TilemapTile::IsEmpty(tile))
                {
                    continue;
                }
                const AZ::s32 allowed = TilemapTile::IndexMask | TilemapTile::FlipHorizontal | TilemapTile::FlipVertical;
                if ((tile & ~allowed) != 0 || TilemapTile::CellIndex(tile) >= atlasCells)
                {
                    return false;
                }
            }
        }
        return true;
    }
} // namespace Diorama
