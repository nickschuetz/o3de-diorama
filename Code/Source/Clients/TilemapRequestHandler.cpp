/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAssetUtils.h>
#include <Clients/TilemapPresenter.h>
#include <Clients/TilemapRequestHandler.h>

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

        if (m_presenter != nullptr)
        {
            info.m_visible = m_presenter->IsVisible();
        }
        return info;
    }
} // namespace Diorama
