/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAssetUtils.h>
#include <Clients/TilemapComponent.h>
#include <Diorama/TilemapBus.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    TilemapComponent::TilemapComponent(const TilemapComponentConfig& config)
        : m_config(config)
    {
    }

    void TilemapComponent::Reflect(AZ::ReflectContext* context)
    {
        TilemapComponentConfig::Reflect(context);
        TilemapInfo::Reflect(context);
        ReflectTilemapBus(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TilemapComponent, AZ::Component>()->Version(1)->Field("Config", &TilemapComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                // No AppearsInAddComponentMenu: this lightweight runtime component is
                // built from EditorTilemapComponent via BuildGameEntity, never added
                // directly. Listing it in the Add Component menu collides with the
                // editor component's identical "Tilemap" name, so a name lookup
                // (FindComponentTypeIdsByEntityType) could resolve this preview-less,
                // request-bus-less runtime component instead of the editor one. The
                // EditContext stays so a built game entity's config is still
                // inspectable.
                editContext->Class<TilemapComponent>("Tilemap", "Draws a world-space grid of atlas tiles through Atom")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &TilemapComponent::m_config, "Config", "Tilemap configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void TilemapComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaTilemapService"));
    }

    void TilemapComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaTilemapService"));
    }

    void TilemapComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void TilemapComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        AZ_UNUSED(dependent);
    }

    void TilemapComponent::Activate()
    {
        m_presenter.Connect(GetEntityId(), m_config);

        // Expose the AI request API. A verb edits m_config in place, then the
        // callback pushes it to the presenter (the same live-update path the
        // editor uses), so agent-driven changes apply immediately.
        m_requestHandler.Connect(
            GetEntityId(),
            m_config,
            m_presenter,
            [this]()
            {
                m_presenter.SetConfig(m_config);
            });

        // Asset-reference mode: when a compiled tilemap asset is assigned, load it
        // and apply it on ready (over the inline config). With none, the inline
        // config the presenter already has is authoritative.
        if (m_config.m_tilemapAsset.GetId().IsValid())
        {
            m_config.m_tilemapAsset.QueueLoad();
            AZ::Data::AssetBus::Handler::BusConnect(m_config.m_tilemapAsset.GetId());
        }
    }

    void TilemapComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();
        m_requestHandler.Disconnect();
        m_presenter.Disconnect();
    }

    void TilemapComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        const auto* tilemap = asset.GetAs<DioramaTilemapAsset>();
        if (tilemap == nullptr || !tilemap->IsValid())
        {
            return;
        }
        m_config.m_tilemapAsset = asset; // keep the loaded reference alive
        ApplyTilemapAsset(*tilemap);
    }

    void TilemapComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        OnAssetReady(asset);
    }

    void TilemapComponent::ApplyTilemapAsset(const DioramaTilemapAsset& asset)
    {
        m_config.m_columns = asset.m_columns;
        m_config.m_rows = asset.m_rows;
        m_config.m_atlasColumns = asset.m_atlasColumns;
        m_config.m_atlasRows = asset.m_atlasRows;
        m_config.m_tileSize = AZ::Vector2(asset.m_tileWidth, asset.m_tileHeight);

        // Phase 1 renders the first layer; carrying the rest is the multi-layer
        // rendering follow-up. IsValid() guarantees at least one layer exists.
        const DioramaTilemapLayerData& layer = asset.m_layers.front();
        m_config.m_tiles = layer.m_tiles;
        m_config.m_tint = layer.m_tint;
        m_config.m_sortOffset = layer.m_sortOffset;

        // Resolve the atlas product path to a streaming image; SetConfig below
        // queue-loads it (the presenter detects the changed atlas id).
        if (!asset.m_atlasTexturePath.empty())
        {
            const AZ::Data::AssetId atlasId = ResolveStreamingImageAssetId(asset.m_atlasTexturePath);
            if (atlasId.IsValid())
            {
                m_config.m_atlas =
                    AZ::Data::Asset<AZ::RPI::StreamingImageAsset>(atlasId, AZ::AzTypeInfo<AZ::RPI::StreamingImageAsset>::Uuid());
                m_config.m_atlas.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
            }
        }

        m_presenter.SetConfig(m_config);
    }
} // namespace Diorama
