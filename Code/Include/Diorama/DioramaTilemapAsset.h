/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Color.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace Diorama
{
    //! Bounds shared by the builder (validate untrusted source) and the asset
    //! (validate at load). No source value may exceed these before it sizes a
    //! buffer; see the VISION security criterion for asset-sourced tilemap data.
    namespace TilemapAssetLimits
    {
        static constexpr int MaxDimension = 4096; //!< per-axis cell count (columns or rows)
        static constexpr int MaxAtlasDimension = 1024; //!< per-axis atlas cell count
        static constexpr int MaxLayers = 64;
        //! Hard ceiling on cells per layer so columns * rows can never overflow or
        //! request an unreasonable allocation (4096 * 4096 = ~16.7M, the product of
        //! the per-axis maxima).
        static constexpr AZ::s64 MaxCells = static_cast<AZ::s64>(MaxDimension) * MaxDimension;
    } // namespace TilemapAssetLimits

    //! Per-tile orientation flags packed into the high bits of a tile value, so a
    //! cell can carry "atlas cell N, mirrored" without a parallel array. The low bits
    //! are the atlas cell index; -1 (EmptyTile) stays the empty sentinel. Tiled GIDs
    //! use the same idea; this is the gem's own bit layout. Diagonal/rotation is not
    //! represented yet (it needs a 90-degree UV rotation the sprite path lacks).
    namespace TilemapTile
    {
        static constexpr AZ::s32 FlipHorizontal = 0x40000000; //!< bit 30
        static constexpr AZ::s32 FlipVertical = 0x20000000; //!< bit 29
        static constexpr AZ::s32 IndexMask = 0x1FFFFFFF; //!< low 29 bits = atlas cell index

        //! True for the empty sentinel (-1). Check before decoding flags or the index.
        inline bool IsEmpty(AZ::s32 tile)
        {
            return tile < 0;
        }
        //! Atlas cell index with flip flags stripped (valid only when not empty).
        inline AZ::s32 CellIndex(AZ::s32 tile)
        {
            return tile & IndexMask;
        }
        inline bool FlipH(AZ::s32 tile)
        {
            return (tile & FlipHorizontal) != 0;
        }
        inline bool FlipV(AZ::s32 tile)
        {
            return (tile & FlipVertical) != 0;
        }
    } // namespace TilemapTile

    //! One layer of a tilemap: a row-major grid of atlas cell indices (length
    //! columns * rows on the owning asset), plus its own draw-order bias and tint.
    //! Multiple layers let a single asset carry background / main / foreground in
    //! one file (Tiled-shaped), even though Phase 1 rendering applies the first
    //! layer; storing them now avoids a product-format revision later.
    struct DioramaTilemapLayerData
    {
        AZ_TYPE_INFO(Diorama::DioramaTilemapLayerData, DioramaTilemapLayerDataTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        //! Row-major tile indices, one per cell. -1 (EmptyTile) draws nothing.
        AZStd::vector<AZ::s32> m_tiles;
        float m_sortOffset = 0.0f;
        AZ::Color m_tint = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
    };

    //! Product asset emitted by the tilemap AssetBuilder from a .dioramatilemap
    //! source. It holds the compiled, validated grid (dimensions, atlas reference,
    //! tile size, and one or more layers) so a TilemapComponent can reference it
    //! instead of inlining a large tile array into the prefab: the level stays
    //! small and the product loads without re-parsing the source.
    class DioramaTilemapAsset final : public AZ::Data::AssetData
    {
    public:
        AZ_RTTI(Diorama::DioramaTilemapAsset, DioramaTilemapAssetTypeId, AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(DioramaTilemapAsset, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Scan-folder-relative product path of the atlas image (e.g.
        //! "diorama/textures/tileset.png"), resolvable via SetAtlasByPath.
        AZStd::string m_atlasTexturePath;

        int m_columns = 1;
        int m_rows = 1;
        int m_atlasColumns = 1;
        int m_atlasRows = 1;

        float m_tileWidth = 1.0f;
        float m_tileHeight = 1.0f;

        //! At least one layer. Phase 1 renders m_layers[0]; the rest are carried for
        //! the multi-layer rendering follow-up.
        AZStd::vector<DioramaTilemapLayerData> m_layers;

        //! True when dimensions, layer count, and every layer's tile array length
        //! and indices are within TilemapAssetLimits and consistent. Checked at load
        //! so a corrupt or hostile product never feeds an unchecked size downstream.
        bool IsValid() const;
    };
} // namespace Diorama
