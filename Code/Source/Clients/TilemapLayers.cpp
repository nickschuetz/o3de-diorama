/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/TilemapLayers.h>

#include <Clients/DioramaAssetUtils.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace Diorama
{
    TilemapComponentConfig BuildTilemapLayerConfig(const DioramaTilemapAsset& asset, size_t layerIndex)
    {
        TilemapComponentConfig cfg;
        cfg.m_columns = asset.m_columns;
        cfg.m_rows = asset.m_rows;
        cfg.m_atlasColumns = asset.m_atlasColumns;
        cfg.m_atlasRows = asset.m_atlasRows;
        cfg.m_tileSize = AZ::Vector2(asset.m_tileWidth, asset.m_tileHeight);

        if (layerIndex < asset.m_layers.size())
        {
            const DioramaTilemapLayerData& layer = asset.m_layers[layerIndex];
            cfg.m_tiles = layer.m_tiles;
            cfg.m_tint = layer.m_tint;
            cfg.m_sortOffset = layer.m_sortOffset;
        }

        if (!asset.m_atlasTexturePath.empty())
        {
            const AZ::Data::AssetId atlasId = ResolveStreamingImageAssetId(asset.m_atlasTexturePath);
            if (atlasId.IsValid())
            {
                cfg.m_atlas = AZ::Data::Asset<AZ::RPI::StreamingImageAsset>(atlasId, AZ::AzTypeInfo<AZ::RPI::StreamingImageAsset>::Uuid());
                cfg.m_atlas.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
            }
        }
        return cfg;
    }

    TilemapExtraLayers::~TilemapExtraLayers()
    {
        Clear();
    }

    void TilemapExtraLayers::Rebuild(AZ::EntityId entityId, const DioramaTilemapAsset& asset)
    {
        Clear();
        for (size_t i = 1; i < asset.m_layers.size(); ++i)
        {
            auto presenter = AZStd::make_unique<TilemapPresenter>();
            presenter->Connect(entityId, BuildTilemapLayerConfig(asset, i));
            m_presenters.push_back(AZStd::move(presenter));
        }
    }

    void TilemapExtraLayers::Clear()
    {
        for (auto& presenter : m_presenters)
        {
            presenter->Disconnect();
        }
        m_presenters.clear();
    }
} // namespace Diorama
