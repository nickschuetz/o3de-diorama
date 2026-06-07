/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a tilemap, returned by GetTilemapInfo. Reports
    //! what the tilemap is actually doing (atlas loaded, drawing, how many cells
    //! are filled), so an agent can confirm an action took effect without a
    //! screenshot.
    struct TilemapInfo
    {
        AZ_TYPE_INFO(Diorama::TilemapInfo, TilemapInfoTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_atlasPath; //!< Resolved atlas product path, empty if none.
        bool m_atlasLoaded = false; //!< True once the atlas asset has streamed in.
        bool m_visible = false; //!< True when registered and drawable.
        int m_columns = 1;
        int m_rows = 1;
        int m_atlasColumns = 1;
        int m_atlasRows = 1;
        float m_tileWidth = 1.0f;
        float m_tileHeight = 1.0f;
        int m_filledTileCount = 0; //!< Number of non-empty cells.
        float m_sortOffset = 0.0f;
        bool m_hasSourceAsset = false; //!< True when a compiled tilemap asset drives this map.
    };

    //! Stable, typed, agent-facing API for driving a single tilemap, addressed by
    //! the tilemap entity's id. Every verb takes plain scalars and is forgiving
    //! (values are validated and clamped, never crash). It is the peer of the
    //! editor inspector over the same configuration.
    class DioramaTilemapRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaTilemapRequests, DioramaTilemapRequestsTypeId);
        virtual ~DioramaTilemapRequests() = default;

        //! Assign the atlas texture by product path. Returns false if the path
        //! does not resolve to an asset.
        virtual bool SetAtlasByPath(AZStd::string_view productPath) = 0;
        //! Reference a compiled tilemap asset by product path (a `.dtilemapc`), so
        //! the whole map (grid, atlas, layers) loads from the asset instead of the
        //! inline tiles. Empty clears it (back to inline). Returns false if the path
        //! does not resolve to an asset.
        virtual bool SetTilemapByPath(AZStd::string_view productPath) = 0;
        //! How the atlas is divided into tile cells; each value clamped to >= 1.
        virtual void SetAtlasGrid(int columns, int rows) = 0;
        //! Tilemap grid dimensions in cells; each value clamped to >= 1.
        virtual void SetGridSize(int columns, int rows) = 0;
        //! World size of one tile; negative values are clamped to zero.
        virtual void SetTileSize(float width, float height) = 0;
        //! Set one cell to an atlas cell index, or -1 to clear it. Out-of-range
        //! cells are ignored.
        virtual void SetTile(int column, int row, int tileIndex) = 0;
        //! Set every cell to the same atlas cell index (or -1 to clear all).
        virtual void Fill(int tileIndex) = 0;
        //! Clear every cell (draws nothing).
        virtual void Clear() = 0;
        //! Autotile every non-empty cell: rewrite each to baseTileIndex + a 4-bit edge
        //! mask of its non-empty cardinal neighbors, so the group's 16-cell art block
        //! (starting at baseTileIndex, laid out in edge-mask order) connects itself.
        //! baseTileIndex is clamped to >= 0.
        virtual void Autotile(int baseTileIndex) = 0;
        //! Autotile every non-empty cell with the 47-tile "blob" scheme (corners count
        //! when both adjacent edges are present): each cell becomes baseTileIndex + a
        //! blob index 0..46, so the group's 47-cell art block connects edges and
        //! corners. baseTileIndex is clamped to >= 0.
        virtual void AutotileBlob(int baseTileIndex) = 0;
        //! Tint multiplied into every tile; channels clamped to 0..1.
        virtual void SetTint(float r, float g, float b, float a) = 0;
        //! Transparent draw-order bias for the layer; larger draws on top.
        virtual void SetSortOffset(float sortOffset) = 0;
        //! Resolved runtime state of the tilemap. Safe to poll.
        virtual TilemapInfo GetTilemapInfo() = 0;
    };

    using DioramaTilemapRequestBus = AZ::EBus<DioramaTilemapRequests>;

    //! Reflect the agent-facing tilemap bus to the BehaviorContext. Called from
    //! the tilemap component Reflect.
    void ReflectTilemapBus(AZ::ReflectContext* context);
} // namespace Diorama
