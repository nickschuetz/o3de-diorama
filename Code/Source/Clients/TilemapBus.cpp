/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/TilemapBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void TilemapInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TilemapInfo>()
                ->Version(1)
                ->Field("atlasPath", &TilemapInfo::m_atlasPath)
                ->Field("atlasLoaded", &TilemapInfo::m_atlasLoaded)
                ->Field("visible", &TilemapInfo::m_visible)
                ->Field("columns", &TilemapInfo::m_columns)
                ->Field("rows", &TilemapInfo::m_rows)
                ->Field("atlasColumns", &TilemapInfo::m_atlasColumns)
                ->Field("atlasRows", &TilemapInfo::m_atlasRows)
                ->Field("tileWidth", &TilemapInfo::m_tileWidth)
                ->Field("tileHeight", &TilemapInfo::m_tileHeight)
                ->Field("filledTileCount", &TilemapInfo::m_filledTileCount)
                ->Field("sortOffset", &TilemapInfo::m_sortOffset)
                ->Field("hasSourceAsset", &TilemapInfo::m_hasSourceAsset);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<TilemapInfo>("DioramaTilemapInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Tilemap")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                // An *Info struct is a returned snapshot, so each field is reflected
                // read-only (a getter, no setter): readable from Lua / Python /
                // Script Canvas, listed once (a setter would add a duplicate node).
                ->Property("atlasPath", BehaviorValueGetter(&TilemapInfo::m_atlasPath), nullptr)
                ->Property("atlasLoaded", BehaviorValueGetter(&TilemapInfo::m_atlasLoaded), nullptr)
                ->Property("visible", BehaviorValueGetter(&TilemapInfo::m_visible), nullptr)
                ->Property("columns", BehaviorValueGetter(&TilemapInfo::m_columns), nullptr)
                ->Property("rows", BehaviorValueGetter(&TilemapInfo::m_rows), nullptr)
                ->Property("atlasColumns", BehaviorValueGetter(&TilemapInfo::m_atlasColumns), nullptr)
                ->Property("atlasRows", BehaviorValueGetter(&TilemapInfo::m_atlasRows), nullptr)
                ->Property("tileWidth", BehaviorValueGetter(&TilemapInfo::m_tileWidth), nullptr)
                ->Property("tileHeight", BehaviorValueGetter(&TilemapInfo::m_tileHeight), nullptr)
                ->Property("filledTileCount", BehaviorValueGetter(&TilemapInfo::m_filledTileCount), nullptr)
                ->Property("sortOffset", BehaviorValueGetter(&TilemapInfo::m_sortOffset), nullptr)
                ->Property("hasSourceAsset", BehaviorValueGetter(&TilemapInfo::m_hasSourceAsset), nullptr);
        }
    }

    //! Reflect the agent-facing tilemap bus to the BehaviorContext so it is
    //! callable from Python/script with named, documented verbs.
    void ReflectTilemapBus(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext
            ->EBus<DioramaTilemapRequestBus>("DioramaTilemapRequestBus")
            // Common scope exports the bus to the editor Python bindings (azlmbr)
            // as well as runtime script, which is what makes it agent-drivable.
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Tilemap")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "SetAtlasByPath",
                &DioramaTilemapRequestBus::Events::SetAtlasByPath,
                { { { "productPath", "Atlas product path, e.g. 'diorama/textures/tiles.png'. Returns false if it does not resolve." } } })
            ->Event(
                "SetTilemapByPath",
                &DioramaTilemapRequestBus::Events::SetTilemapByPath,
                { { { "productPath",
                      "Compiled tilemap asset product path, e.g. 'diorama/maps/level1.dtilemapc'. Loads the whole map (grid, "
                      "atlas, layers) from the asset. Empty clears it (back to inline). Returns false if it does not resolve." } } })
            ->Event(
                "SetAtlasGrid",
                &DioramaTilemapRequestBus::Events::SetAtlasGrid,
                { { { "columns", "Atlas cells across, clamped to >= 1." }, { "rows", "Atlas cells down, clamped to >= 1." } } })
            ->Event(
                "SetGridSize",
                &DioramaTilemapRequestBus::Events::SetGridSize,
                { { { "columns", "Tilemap width in cells, clamped to >= 1." }, { "rows", "Tilemap height in cells, clamped to >= 1." } } })
            ->Event(
                "SetTileSize",
                &DioramaTilemapRequestBus::Events::SetTileSize,
                { { { "width", "Tile width in world units; negative is clamped to zero." },
                    { "height", "Tile height in world units; negative is clamped to zero." } } })
            ->Event(
                "SetTile",
                &DioramaTilemapRequestBus::Events::SetTile,
                { { { "column", "Cell column (0-based)." },
                    { "row", "Cell row (0-based, 0 is the top)." },
                    { "tileIndex", "Atlas cell index, or -1 to clear the cell." } } })
            ->Event(
                "Fill",
                &DioramaTilemapRequestBus::Events::Fill,
                { { { "tileIndex", "Atlas cell index to set in every cell, or -1 to clear all." } } })
            ->Event("Clear", &DioramaTilemapRequestBus::Events::Clear)
            ->Event(
                "Autotile",
                &DioramaTilemapRequestBus::Events::Autotile,
                { { { "baseTileIndex",
                      "First atlas cell of the 16-tile autotile block; each non-empty cell becomes baseTileIndex + edge mask." } } })
            ->Event(
                "AutotileBlob",
                &DioramaTilemapRequestBus::Events::AutotileBlob,
                { { { "baseTileIndex",
                      "First atlas cell of the 47-tile blob block; each non-empty cell becomes baseTileIndex + a blob index 0..46." } } })
            ->Event(
                "SetTint",
                &DioramaTilemapRequestBus::Events::SetTint,
                { { { "r", "Red tint multiplier, clamped to 0..1." },
                    { "g", "Green tint multiplier, clamped to 0..1." },
                    { "b", "Blue tint multiplier, clamped to 0..1." },
                    { "a", "Alpha (opacity) multiplier, clamped to 0..1." } } })
            ->Event(
                "SetSortOffset",
                &DioramaTilemapRequestBus::Events::SetSortOffset,
                { { { "sortOffset", "Transparent draw-order bias; larger values draw on top." } } })
            ->Event("GetTilemapInfo", &DioramaTilemapRequestBus::Events::GetTilemapInfo);
    }
} // namespace Diorama
