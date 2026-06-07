/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/TilemapLayers.h>
#include <Clients/TilemapPresenter.h>
#include <Clients/TilemapRequestHandler.h>
#include <Diorama/TilemapComponentConfig.h>
#include <Tools/TilemapPaintEditorBus.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor-side counterpart to the runtime TilemapComponent. It authors the
    //! shared TilemapComponentConfig in the entity inspector and renders a live
    //! preview in the editor viewport through the shared TilemapPresenter, so the
    //! tilemap is visible while editing without entering game mode. On export or
    //! play it builds the lightweight runtime TilemapComponent through
    //! BuildGameEntity. All Qt and AzToolsFramework dependencies stay in this
    //! editor module so the runtime client stays lean.
    class EditorTilemapComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private TilemapPaintEditorRequestBus::Handler
        , private AZ::Data::AssetBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorTilemapComponent, EditorTilemapComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        EditorTilemapComponent() = default;
        ~EditorTilemapComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::Components::EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        // TilemapPaintEditorRequestBus (the paint Component Mode talks to us through this)
        int GetPaintActiveTile() const override;
        int GetPaintBrushSize() const override;
        int GetPaintColumns() const override;
        int GetPaintRows() const override;
        int GetPaintTileAt(int col, int row) const override;
        bool PaintWorldRayToCell(const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection, int& outCol, int& outRow) const override;
        void PaintCells(const AZStd::vector<TilemapPaint::Cell>& cells, int tileId) override;

        // AZ::Data::AssetBus (preview a referenced tilemap asset in the editor)
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // Called by the edit context when a property changes; refreshes the preview.
        AZ::u32 OnConfigChanged();

        //! Reconnect/reload the referenced tilemap asset when its id changes (picked
        //! in the inspector or cleared), so the viewport preview follows the asset.
        //! When no asset is assigned, the inline config drives the preview as before.
        void RefreshTilemapAsset();

        // Mark the entity dirty so the (possibly script-driven) request-bus edits to
        // m_config are captured into the prefab on save. Done synchronously per edit
        // so a scripted edit-then-save reliably bakes (see the .cpp for why this
        // cannot defer); one SetTile maps to one undo step, matching how painting a
        // tile in the inspector records one undo step.
        void PersistConfig();

        TilemapComponentConfig m_config;
        TilemapPresenter m_presenter;
        TilemapRequestHandler m_requestHandler;
        //! Presenters for layers beyond the first of a referenced tilemap asset.
        TilemapExtraLayers m_extraLayers;
        //! Tilemap asset currently driving the preview, to detect inspector changes.
        AZ::Data::AssetId m_previewAssetId;

        //! Detects entry into the viewport paint Component Mode and creates it.
        AzToolsFramework::ComponentModeFramework::ComponentModeDelegate m_componentModeDelegate;
        //! Atlas tile id a left stroke paints (clamped to the atlas at use).
        int m_activeTile = 0;
        //! Square brush edge length in cells.
        int m_brushSize = 1;
    };
} // namespace Diorama
