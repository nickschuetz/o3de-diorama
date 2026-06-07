/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/TilemapBus.h>
#include <Diorama/TilemapComponentConfig.h>

#include <AzCore/std/functional.h>

namespace Diorama
{
    class TilemapPresenter;

    //! Shared implementation of DioramaTilemapRequestBus used by both the runtime
    //! TilemapComponent and the editor EditorTilemapComponent, so the verb logic
    //! exists once. It edits the owning component's authoritative config in place,
    //! then calls an "on changed" callback the owner supplies (the runtime pushes
    //! to the presenter; the editor also refreshes the inspector). Resolved query
    //! fields come from the presenter the owner points it at.
    //!
    //! Lives in the runtime client library (no Qt, no AzToolsFramework) so the
    //! editor module reuses it through the shared private object library.
    class TilemapRequestHandler : public DioramaTilemapRequestBus::Handler
    {
    public:
        using ChangedCallback = AZStd::function<void()>;

        //! Begin handling requests for the given entity. config and presenter must
        //! outlive this handler (they are members of the owning component).
        void Connect(AZ::EntityId entityId, TilemapComponentConfig& config, TilemapPresenter& presenter, ChangedCallback onChanged);
        void Disconnect();

        // DioramaTilemapRequests
        bool SetAtlasByPath(AZStd::string_view productPath) override;
        bool SetTilemapByPath(AZStd::string_view productPath) override;
        void SetAtlasGrid(int columns, int rows) override;
        void SetGridSize(int columns, int rows) override;
        void SetTileSize(float width, float height) override;
        void SetTile(int column, int row, int tileIndex) override;
        void Fill(int tileIndex) override;
        void Clear() override;
        void Autotile(int baseTileIndex) override;
        void AutotileBlob(int baseTileIndex) override;
        void SetTint(float r, float g, float b, float a) override;
        void SetSortOffset(float sortOffset) override;
        TilemapInfo GetTilemapInfo() override;

    private:
        void NotifyChanged();

        TilemapComponentConfig* m_config = nullptr;
        TilemapPresenter* m_presenter = nullptr;
        ChangedCallback m_onChanged;
        bool m_connected = false;
    };
} // namespace Diorama
