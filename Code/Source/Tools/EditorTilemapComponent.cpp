/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/TilemapComponent.h>
#include <Clients/TilemapLayers.h>
#include <Clients/TilemapPaint.h>
#include <Diorama/DioramaTilemapAsset.h>
#include <Tools/EditorTilemapComponent.h>
#include <Tools/EditorTilemapPaintComponentMode.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace Diorama
{
    void EditorTilemapComponent::Reflect(AZ::ReflectContext* context)
    {
        EditorTilemapPaintComponentMode::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorTilemapComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(2)
                ->Field("Config", &EditorTilemapComponent::m_config)
                ->Field("ActiveTile", &EditorTilemapComponent::m_activeTile)
                ->Field("BrushSize", &EditorTilemapComponent::m_brushSize)
                ->Field("PaintMode", &EditorTilemapComponent::m_componentModeDelegate);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorTilemapComponent>("Tilemap", "Draws a world-space grid of atlas tiles through Atom")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorTilemapComponent::m_config, "Config", "Tilemap configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorTilemapComponent::OnConfigChanged)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Painting")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &EditorTilemapComponent::m_activeTile,
                        "Active Tile",
                        "Atlas cell index a left-drag paints in the paint mode")
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &EditorTilemapComponent::m_brushSize,
                        "Brush Size",
                        "Square brush edge length in cells")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, 16)
                    // No row label: the delegate renders the standard Edit/Done
                    // component-mode button (its text is fixed by the framework), and
                    // a name here would show as a stray label beside it. The enclosing
                    // "Painting" group already says what it is.
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &EditorTilemapComponent::m_componentModeDelegate,
                        "",
                        "Enter the viewport paint mode: left-drag paints the active tile, right-drag erases");
            }
        }
    }

    void EditorTilemapComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaTilemapService"));
    }

    void EditorTilemapComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaTilemapService"));
    }

    void EditorTilemapComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void EditorTilemapComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        AZ_UNUSED(dependent);
    }

    void EditorTilemapComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        m_presenter.Connect(GetEntityId(), m_config);

        // Expose the same AI request API in the editor. A verb edits m_config,
        // pushes it to the live preview, and refreshes the inspector so an
        // agent-driven change shows up in the human UI too. Both faces drive the
        // one config.
        m_requestHandler.Connect(
            GetEntityId(),
            m_config,
            m_presenter,
            [this]()
            {
                m_presenter.SetConfig(m_config);
                AzToolsFramework::ToolsApplicationEvents::Bus::Broadcast(
                    &AzToolsFramework::ToolsApplicationEvents::InvalidatePropertyDisplay, AzToolsFramework::Refresh_Values);
                // A request-bus edit changes m_config in place but, unlike an
                // inspector edit, does not go through the property editor, so the
                // prefab never learns about it and the change is lost on save.
                // Persist it now so the new config is baked.
                PersistConfig();
            });

        // The paint Component Mode reaches back through this bus, and the delegate
        // adds the "Paint" button + enters the mode when the user clicks it.
        TilemapPaintEditorRequestBus::Handler::BusConnect(GetEntityId());
        m_componentModeDelegate.ConnectWithSingleComponentMode<EditorTilemapComponent, EditorTilemapPaintComponentMode>(
            AZ::EntityComponentIdPair(GetEntityId(), GetId()), nullptr);

        // If a tilemap asset is referenced, preview it (all layers) over the inline
        // config; otherwise the inline preview from Connect above stands.
        RefreshTilemapAsset();
    }

    void EditorTilemapComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();
        m_extraLayers.Clear();
        m_componentModeDelegate.Disconnect();
        TilemapPaintEditorRequestBus::Handler::BusDisconnect();
        m_requestHandler.Disconnect();
        m_presenter.Disconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void EditorTilemapComponent::RefreshTilemapAsset()
    {
        const AZ::Data::AssetId id = m_config.m_tilemapAsset.GetId();
        if (id == m_previewAssetId)
        {
            return;
        }
        AZ::Data::AssetBus::Handler::BusDisconnect();
        m_extraLayers.Clear();
        m_previewAssetId = id;
        if (id.IsValid())
        {
            m_config.m_tilemapAsset.QueueLoad();
            AZ::Data::AssetBus::Handler::BusConnect(id);
        }
        else
        {
            // Asset cleared: fall back to the inline config for the preview.
            m_presenter.SetConfig(m_config);
        }
    }

    void EditorTilemapComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        const auto* tilemap = asset.GetAs<DioramaTilemapAsset>();
        if (tilemap == nullptr || !tilemap->IsValid())
        {
            return;
        }
        // Preview the asset without baking it into the persisted config: drive the
        // preview with a transient layer-0 config and render the extra layers.
        m_presenter.SetConfig(BuildTilemapLayerConfig(*tilemap, 0));
        m_extraLayers.Rebuild(GetEntityId(), *tilemap);
    }

    void EditorTilemapComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        OnAssetReady(asset);
    }

    void EditorTilemapComponent::PersistConfig()
    {
        // Only an actualized entity may touch the undo/prefab system. During prefab
        // construction or propagation the entity can be inactive and a request-bus
        // edit must not dirty it (mirrors EditorComponentBase::SetDirty's guard).
        AZ::Entity* entity = GetEntity();
        if (!entity || entity->GetState() != AZ::Entity::State::Active)
        {
            return;
        }

        // Record the new config into the prefab DOM synchronously. This cannot be
        // deferred to a later tick to coalesce a burst: there is no pre-save hook
        // (OnSaveLevel fires AFTER the level is written), so a script that writes
        // tiles and immediately saves would outrun a deferred dirty and bake the
        // old config. MarkEntityDirty is a no-op outside an undo batch, and the
        // batch commits the entity's DOM delta when it closes here, so the edit is
        // captured before control returns to the caller (and before any save).
        AzToolsFramework::ScopedUndoBatch undoBatch("Edit Diorama Tilemap");
        undoBatch.MarkEntityDirty(GetEntityId());
    }

    void EditorTilemapComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        // Hand the authored configuration to the runtime component so the exported
        // game runs only the lightweight client code path.
        gameEntity->CreateComponent<TilemapComponent>(m_config);
    }

    AZ::u32 EditorTilemapComponent::OnConfigChanged()
    {
        // With no asset assigned, the inline config drives the preview. With one
        // assigned, the asset drives it, so do not clobber the preview with the
        // (empty) inline config; RefreshTilemapAsset handles a changed/cleared asset.
        if (!m_config.m_tilemapAsset.GetId().IsValid())
        {
            m_presenter.SetConfig(m_config);
        }
        RefreshTilemapAsset();
        return AZ::Edit::PropertyRefreshLevels::None;
    }

    int EditorTilemapComponent::GetPaintActiveTile() const
    {
        return m_activeTile;
    }

    int EditorTilemapComponent::GetPaintBrushSize() const
    {
        return m_brushSize < 1 ? 1 : m_brushSize;
    }

    int EditorTilemapComponent::GetPaintColumns() const
    {
        return m_config.GetColumns();
    }

    int EditorTilemapComponent::GetPaintRows() const
    {
        return m_config.GetRows();
    }

    int EditorTilemapComponent::GetPaintTileAt(int col, int row) const
    {
        return m_config.GetTile(col, row);
    }

    bool EditorTilemapComponent::PaintWorldRayToCell(
        const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection, int& outCol, int& outRow) const
    {
        // Bring the world ray into the tilemap's local space, where the grid lies on
        // the Y = 0 (XZ) plane, intersect that plane, then resolve the cell.
        AZ::Transform worldTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTransform, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
        const AZ::Transform inverse = worldTransform.GetInverse();
        const AZ::Vector3 localOrigin = inverse.TransformPoint(rayOrigin);
        const AZ::Vector3 localDirection = inverse.TransformPoint(rayOrigin + rayDirection) - localOrigin;

        if (AZ::GetAbs(localDirection.GetY()) < 1e-6f)
        {
            return false; // ray parallel to the grid plane
        }
        const float t = -localOrigin.GetY() / localDirection.GetY();
        if (t < 0.0f)
        {
            return false; // grid plane is behind the ray origin
        }
        const AZ::Vector3 localHit = localOrigin + localDirection * t;
        return m_config.LocalPositionToCell(localHit, outCol, outRow);
    }

    void EditorTilemapComponent::PaintCells(const AZStd::vector<TilemapPaint::Cell>& cells, int tileId)
    {
        bool changed = false;
        for (const TilemapPaint::Cell& cell : cells)
        {
            if (m_config.GetTile(cell.m_col, cell.m_row) != tileId)
            {
                m_config.SetTile(cell.m_col, cell.m_row, tileId);
                changed = true;
            }
        }
        if (!changed)
        {
            return;
        }
        m_presenter.SetConfig(m_config);
        // The paint mode holds the stroke's outer undo batch open; marking the entity
        // dirty here records this stamp into it (and bakes into the prefab on save).
        PersistConfig();
    }
} // namespace Diorama
