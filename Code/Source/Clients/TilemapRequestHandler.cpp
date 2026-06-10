/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAssetUtils.h>
#include <Clients/TilemapAutotile.h>
#include <Clients/TilemapPresenter.h>
#include <Clients/TilemapRequestHandler.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>

#include <AzCore/Math/MathUtils.h>
#include <AzFramework/Asset/AssetCatalogBus.h>

namespace Diorama
{
    void TilemapRequestHandler::Connect(
        AZ::EntityId entityId, TilemapComponentConfig& config, TilemapPresenter& presenter, ChangedCallback onChanged)
    {
        m_config = &config;
        m_presenter = &presenter;
        m_onChanged = AZStd::move(onChanged);
        m_connected = true;
        DioramaTilemapRequestBus::Handler::BusConnect(entityId);
    }

    void TilemapRequestHandler::Disconnect()
    {
        DioramaTilemapRequestBus::Handler::BusDisconnect();
        m_connected = false;
        m_config = nullptr;
        m_presenter = nullptr;
        m_onChanged = {};
    }

    void TilemapRequestHandler::NotifyChanged()
    {
        if (m_onChanged)
        {
            m_onChanged();
        }
    }

    bool TilemapRequestHandler::SetAtlasByPath(AZStd::string_view productPath)
    {
        if (m_config == nullptr)
        {
            return false;
        }

        if (productPath.empty())
        {
            m_config->m_atlas = {};
            NotifyChanged();
            return true;
        }

        const AZ::Data::AssetId assetId = ResolveStreamingImageAssetId(productPath);
        if (!assetId.IsValid())
        {
            return false;
        }

        m_config->m_atlas = AZ::Data::Asset<AZ::RPI::StreamingImageAsset>(assetId, AZ::AzTypeInfo<AZ::RPI::StreamingImageAsset>::Uuid());
        m_config->m_atlas.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
        NotifyChanged();
        return true;
    }

    bool TilemapRequestHandler::SetTilemapByPath(AZStd::string_view productPath)
    {
        if (m_config == nullptr)
        {
            return false;
        }

        if (productPath.empty())
        {
            m_config->m_tilemapAsset = {};
            NotifyChanged(); // the component reloads (clears the asset) on change
            return true;
        }

        AZ::Data::AssetId assetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            assetId,
            &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
            AZStd::string(productPath).c_str(),
            azrtti_typeid<DioramaTilemapAsset>(),
            false);
        if (!assetId.IsValid())
        {
            return false;
        }

        m_config->m_tilemapAsset = AZ::Data::Asset<DioramaTilemapAsset>(assetId, AZ::AzTypeInfo<DioramaTilemapAsset>::Uuid());
        m_config->m_tilemapAsset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
        NotifyChanged(); // the component queues the load + applies it on ready
        return true;
    }

    void TilemapRequestHandler::SetAtlasGrid(int columns, int rows)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_atlasColumns = AZ::GetMax(1, columns);
        m_config->m_atlasRows = AZ::GetMax(1, rows);
        NotifyChanged();
    }

    void TilemapRequestHandler::SetGridSize(int columns, int rows)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_columns = AZ::GetMax(1, columns);
        m_config->m_rows = AZ::GetMax(1, rows);
        NotifyChanged();
    }

    void TilemapRequestHandler::SetTileSize(float width, float height)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_tileSize = AZ::Vector2(AZ::GetMax(0.0f, width), AZ::GetMax(0.0f, height));
        NotifyChanged();
    }

    void TilemapRequestHandler::SetTile(int column, int row, int tileIndex)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->SetTile(column, row, static_cast<AZ::s32>(tileIndex));
        NotifyChanged();
    }

    void TilemapRequestHandler::Fill(int tileIndex)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->Fill(static_cast<AZ::s32>(tileIndex));
        NotifyChanged();
    }

    void TilemapRequestHandler::Clear()
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->Clear();
        NotifyChanged();
    }

    namespace
    {
        //! Re-tile every non-empty cell of config: a cell is a group member if it is
        //! non-empty; its new tile is baseTile + maskToOffset(neighborMask8). All new
        //! values are computed against the original grid before any write, so a cell's
        //! neighbors are read from the original grid, not a half-rewritten one.
        template<class MaskToOffset>
        void ResolveAutotile(TilemapComponentConfig& config, int baseTile, MaskToOffset&& maskToOffset)
        {
            const int columns = config.GetColumns();
            const int rows = config.GetRows();
            const auto isMember = [&config](int col, int row)
            {
                return config.GetTile(col, row) != TilemapComponentConfig::EmptyTile;
            };

            struct Write
            {
                int m_col;
                int m_row;
                AZ::s32 m_value;
            };
            AZStd::vector<Write> writes;
            writes.reserve(static_cast<size_t>(config.CountFilledTiles()));

            for (int row = 0; row < rows; ++row)
            {
                for (int col = 0; col < columns; ++col)
                {
                    if (!isMember(col, row))
                    {
                        continue;
                    }
                    const AZ::u8 mask8 = TilemapAutotile::NeighborMask8(col, row, isMember);
                    writes.push_back({ col, row, static_cast<AZ::s32>(baseTile + maskToOffset(mask8)) });
                }
            }

            for (const Write& write : writes)
            {
                config.SetTile(write.m_col, write.m_row, write.m_value);
            }
        }
    } // namespace

    void TilemapRequestHandler::Autotile(int baseTileIndex)
    {
        if (m_config == nullptr)
        {
            return;
        }
        ResolveAutotile(
            *m_config,
            AZ::GetMax(baseTileIndex, 0),
            [](AZ::u8 mask8)
            {
                return static_cast<int>(TilemapAutotile::EdgeMask4(mask8));
            });
        NotifyChanged();
    }

    void TilemapRequestHandler::AutotileBlob(int baseTileIndex)
    {
        if (m_config == nullptr)
        {
            return;
        }
        ResolveAutotile(
            *m_config,
            AZ::GetMax(baseTileIndex, 0),
            [](AZ::u8 mask8)
            {
                return TilemapAutotile::BlobIndex(mask8);
            });
        NotifyChanged();
    }

    void TilemapRequestHandler::AutotileRules(int baseTileIndex)
    {
        if (m_config == nullptr)
        {
            return;
        }

        // Build the pure rule list from the config once, then resolve every cell against
        // it (falling back to the canonical blob index inside RuleSetOffset).
        AZStd::vector<TilemapAutotile::RuleEntry> rules;
        rules.reserve(m_config->m_autotileRules.size());
        for (const TilemapAutotileRuleData& rule : m_config->m_autotileRules)
        {
            rules.push_back(TilemapAutotile::RuleEntry{ static_cast<AZ::u8>(rule.m_mask & 0xFF), rule.m_offset });
        }

        ResolveAutotile(
            *m_config,
            AZ::GetMax(baseTileIndex, 0),
            [&rules](AZ::u8 mask8)
            {
                return TilemapAutotile::RuleSetOffset(mask8, AZStd::span<const TilemapAutotile::RuleEntry>(rules.data(), rules.size()));
            });
        NotifyChanged();
    }

    void TilemapRequestHandler::DefineAnimatedTile(int tileIndex, const AZStd::vector<int>& frames, float fps, bool loop)
    {
        if (m_config == nullptr)
        {
            return;
        }

        // Replace any existing definition for this painted index (one definition per
        // tile). An empty frame list removes it instead of defining an empty cycle.
        auto& defs = m_config->m_animatedTiles;
        defs.erase(
            AZStd::remove_if(
                defs.begin(),
                defs.end(),
                [tileIndex](const TilemapAnimatedTileData& def)
                {
                    return def.m_tileIndex == tileIndex;
                }),
            defs.end());

        if (!frames.empty())
        {
            TilemapAnimatedTileData def;
            def.m_tileIndex = tileIndex;
            def.m_frames = frames;
            def.m_fps = fps;
            def.m_loop = loop;
            defs.push_back(AZStd::move(def));
        }
        NotifyChanged();
    }

    void TilemapRequestHandler::ClearAnimatedTiles()
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_animatedTiles.clear();
        NotifyChanged();
    }

    void TilemapRequestHandler::SetTint(float r, float g, float b, float a)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_tint =
            AZ::Color(AZ::GetClamp(r, 0.0f, 1.0f), AZ::GetClamp(g, 0.0f, 1.0f), AZ::GetClamp(b, 0.0f, 1.0f), AZ::GetClamp(a, 0.0f, 1.0f));
        NotifyChanged();
    }

    void TilemapRequestHandler::SetSortOffset(float sortOffset)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_sortOffset = sortOffset;
        NotifyChanged();
    }

    TilemapInfo TilemapRequestHandler::GetTilemapInfo()
    {
        TilemapInfo info;
        if (m_config == nullptr)
        {
            return info;
        }

        const AZ::Data::AssetId atlasId = m_config->m_atlas.GetId();
        if (atlasId.IsValid())
        {
            AZStd::string path;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(path, &AZ::Data::AssetCatalogRequests::GetAssetPathById, atlasId);
            info.m_atlasPath = path;
        }
        info.m_atlasLoaded = m_config->m_atlas.IsReady();
        info.m_columns = m_config->GetColumns();
        info.m_rows = m_config->GetRows();
        info.m_atlasColumns = m_config->GetAtlasColumns();
        info.m_atlasRows = m_config->GetAtlasRows();
        info.m_tileWidth = m_config->m_tileSize.GetX();
        info.m_tileHeight = m_config->m_tileSize.GetY();
        info.m_filledTileCount = m_config->CountFilledTiles();
        info.m_sortOffset = m_config->m_sortOffset;
        info.m_hasSourceAsset = m_config->m_tilemapAsset.GetId().IsValid();
        info.m_animatedTileCount = static_cast<int>(m_config->m_animatedTiles.size());

        if (m_presenter != nullptr)
        {
            info.m_visible = m_presenter->IsVisible();
        }
        return info;
    }
} // namespace Diorama
