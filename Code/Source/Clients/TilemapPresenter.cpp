/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SpriteFeatureProcessor.h>
#include <Clients/TilemapPresenter.h>

#include <Atom/RPI.Public/Scene.h>
#include <AzCore/Casting/numeric_cast.h>

namespace Diorama
{
    TilemapPresenter::~TilemapPresenter()
    {
        Disconnect();
    }

    void TilemapPresenter::Connect(AZ::EntityId entityId, const TilemapComponentConfig& config)
    {
        if (m_connected)
        {
            Disconnect();
        }

        m_entityId = entityId;
        m_config = config;

        m_worldTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(m_worldTransform, m_entityId, &AZ::TransformBus::Events::GetWorldTM);

        m_connected = true;

        TryAcquireFeatureProcessor();
        QueueAtlasLoad();
        AZ::TransformNotificationBus::Handler::BusConnect(m_entityId);
        RefreshTickConnection();
        Rebuild();
    }

    void TilemapPresenter::Disconnect()
    {
        if (!m_connected)
        {
            return;
        }

        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AZ::Data::AssetBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        if (m_featureProcessor != nullptr)
        {
            for (const Cell& cell : m_cells)
            {
                if (cell.m_handle != 0)
                {
                    m_featureProcessor->ReleaseSprite(cell.m_handle);
                }
            }
        }
        m_cells.clear();
        m_featureProcessor = nullptr;
        m_connected = false;
    }

    void TilemapPresenter::SetConfig(const TilemapComponentConfig& config)
    {
        const bool atlasChanged = config.m_atlas.GetId() != m_config.m_atlas.GetId();
        m_config = config;

        if (atlasChanged)
        {
            AZ::Data::AssetBus::Handler::BusDisconnect();
            QueueAtlasLoad();
        }

        // Grid size or tile contents may have changed, so rebuild the cell set.
        Rebuild();
        RefreshTickConnection();
    }

    bool TilemapPresenter::TryAcquireFeatureProcessor()
    {
        if (m_featureProcessor != nullptr)
        {
            return true;
        }

        AZ::RPI::Scene* scene = AZ::RPI::Scene::GetSceneForEntityId(m_entityId);
        if (scene == nullptr)
        {
            return false;
        }

        m_featureProcessor = scene->GetFeatureProcessor<SpriteFeatureProcessor>();
        if (m_featureProcessor == nullptr)
        {
            m_featureProcessor = scene->EnableFeatureProcessor<SpriteFeatureProcessor>();
        }
        return m_featureProcessor != nullptr;
    }

    void TilemapPresenter::QueueAtlasLoad()
    {
        if (m_config.m_atlas.GetId().IsValid())
        {
            m_config.m_atlas.QueueLoad();
            AZ::Data::AssetBus::Handler::BusConnect(m_config.m_atlas.GetId());
        }
    }

    void TilemapPresenter::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_worldTransform = world;
        PushAll();
    }

    void TilemapPresenter::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() == m_config.m_atlas.GetId())
        {
            m_config.m_atlas = asset;
            PushAll();
        }
    }

    void TilemapPresenter::OnTick(float /*deltaTime*/, AZ::ScriptTimePoint /*time*/)
    {
        // The only reason a tilemap ticks is to retry the scene lookup when the
        // feature processor was not ready at Connect (level load / editor order).
        if (m_featureProcessor == nullptr)
        {
            if (TryAcquireFeatureProcessor())
            {
                Rebuild();
                RefreshTickConnection();
            }
        }
    }

    void TilemapPresenter::RefreshTickConnection()
    {
        const bool shouldTick = m_connected && m_featureProcessor == nullptr;
        const bool isTicking = AZ::TickBus::Handler::BusIsConnected();
        if (shouldTick && !isTicking)
        {
            AZ::TickBus::Handler::BusConnect();
        }
        else if (!shouldTick && isTicking)
        {
            AZ::TickBus::Handler::BusDisconnect();
        }
    }

    void TilemapPresenter::Rebuild()
    {
        if (m_featureProcessor != nullptr)
        {
            for (const Cell& cell : m_cells)
            {
                if (cell.m_handle != 0)
                {
                    m_featureProcessor->ReleaseSprite(cell.m_handle);
                }
            }
        }
        m_cells.clear();

        if (!m_connected || m_featureProcessor == nullptr)
        {
            return;
        }

        const int columns = m_config.GetColumns();
        const int rows = m_config.GetRows();
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                if (m_config.GetTile(column, row) == TilemapComponentConfig::EmptyTile)
                {
                    continue;
                }
                const AZ::u32 handle = m_featureProcessor->AcquireSprite();
                if (handle != 0)
                {
                    m_cells.push_back(Cell{ handle, column, row });
                }
            }
        }

        PushAll();
    }

    void TilemapPresenter::PushAll()
    {
        if (!m_connected || m_featureProcessor == nullptr)
        {
            return;
        }

        for (const Cell& cell : m_cells)
        {
            const AZ::s32 tileIndex = m_config.GetTile(cell.m_column, cell.m_row);
            const AZ::Transform tileTransform =
                m_worldTransform * AZ::Transform::CreateTranslation(m_config.GetTileLocalPosition(cell.m_column, cell.m_row));
            m_featureProcessor->UpdateSprite(cell.m_handle, tileTransform, BuildTileConfig(tileIndex));
        }
    }

    SpriteComponentConfig TilemapPresenter::BuildTileConfig(AZ::s32 tileIndex) const
    {
        SpriteComponentConfig tileConfig;
        tileConfig.m_texture = m_config.m_atlas;
        tileConfig.m_size = m_config.m_tileSize;
        tileConfig.m_pivot = AZ::Vector2(0.5f, 0.5f);
        tileConfig.m_tint = m_config.m_tint;
        tileConfig.m_sortOffset = m_config.m_sortOffset;
        tileConfig.m_billboard = false;
        tileConfig.m_doubleSided = true;
        m_config.GetTileUVRegion(tileIndex, tileConfig.m_uvMin, tileConfig.m_uvMax);
        // Apply packed per-tile orientation flags (e.g. a mirrored or rotated tile
        // from Tiled): H/V mirror plus anti-diagonal transpose (the three give all
        // eight square orientations).
        tileConfig.m_flipHorizontal = TilemapTile::FlipH(tileIndex);
        tileConfig.m_flipVertical = TilemapTile::FlipV(tileIndex);
        tileConfig.m_transpose = TilemapTile::FlipD(tileIndex);
        return tileConfig;
    }

    bool TilemapPresenter::IsVisible() const
    {
        return m_connected && m_featureProcessor != nullptr && !m_cells.empty() && m_config.m_atlas.IsReady();
    }

    int TilemapPresenter::GetFilledTileCount() const
    {
        return aznumeric_cast<int>(m_cells.size());
    }
} // namespace Diorama
