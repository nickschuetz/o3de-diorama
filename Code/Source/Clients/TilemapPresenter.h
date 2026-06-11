/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/SpriteComponentConfig.h>
#include <Diorama/TilemapComponentConfig.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    class SpriteFeatureProcessor;

    //! Shared helper that connects a tilemap (runtime or editor) to the sprite
    //! feature processor. Each non-empty cell is registered as one feature-
    //! processor sprite configured with the atlas texture, that cell's UV
    //! sub-region, and the cell's local transform; because every cell shares the
    //! atlas and sort key they collapse into a single batched draw call. This
    //! reuses the entire tested sprite render path with no new rendering code.
    //!
    //! Lives in the runtime client code (no Qt, no AzToolsFramework), so the
    //! editor module reuses it through the shared private object library.
    //!
    //! Per-cell handles are a deliberate reuse-first choice; a dedicated batched
    //! tile mesh (one handle per layer) would use less memory for very large maps
    //! and is a natural future optimization.
    class TilemapPresenter final
        : private AZ::TransformNotificationBus::Handler
        , private AZ::Data::AssetBus::Handler
        , private AZ::TickBus::Handler
    {
    public:
        AZ_TYPE_INFO(Diorama::TilemapPresenter, "{1B9C2D3E-4F50-6172-8394-A5B6C7D8E9F0}");

        TilemapPresenter() = default;
        ~TilemapPresenter();

        //! Begin presenting the tilemap for the given entity with the given config.
        void Connect(AZ::EntityId entityId, const TilemapComponentConfig& config);

        //! Stop presenting and release every cell handle.
        void Disconnect();

        //! Replace the configuration (after an editor edit or a bus verb) and push
        //! the change, reloading the atlas if it changed and rebuilding cells.
        void SetConfig(const TilemapComponentConfig& config);

        //! True when registered with a feature processor and the atlas is ready.
        bool IsVisible() const;

        //! Number of non-empty cells currently presented.
        int GetFilledTileCount() const;

    private:
        // AZ::TransformNotificationBus::Handler
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        // AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void QueueAtlasLoad();
        bool TryAcquireFeatureProcessor();
        void RefreshTickConnection();

        //! Release current handles and re-acquire one per non-empty cell, then push.
        void Rebuild();
        //! Push transform + per-cell config to every cell handle.
        void PushAll();
        //! Build the per-cell sprite draw spec for a given atlas cell index.
        SpriteComponentConfig BuildTileConfig(AZ::s32 tileIndex) const;

        //! True when the config has at least one animated-tile definition.
        bool HasAnimatedTiles() const;
        //! World transform of a cell (world TM * the cell's local position).
        AZ::Transform CellTransform(int column, int row) const;
        //! Advance the animation clock and re-push only the animated cells whose
        //! current frame changed this tick.
        void AdvanceAnimation(float deltaTime);

        struct Cell
        {
            AZ::u32 m_handle = 0;
            int m_column = 0;
            int m_row = 0;
            //! Atlas tile last pushed to the feature processor for this cell, so an
            //! animated cell re-pushes only when its frame actually changes.
            AZ::s32 m_displayIndex = -1;
        };

        AZ::EntityId m_entityId;
        TilemapComponentConfig m_config;
        AZ::Transform m_worldTransform = AZ::Transform::CreateIdentity();
        SpriteFeatureProcessor* m_featureProcessor = nullptr;
        AZStd::vector<Cell> m_cells;
        //! Indices into m_cells that hold an animated painted tile (subset advanced
        //! each tick); rebuilt with the cell set.
        AZStd::vector<AZ::u32> m_animatedCells;
        //! Map-wide animation clock in seconds; all animated cells share it so every
        //! instance stays in phase.
        float m_animTime = 0.0f;
        bool m_connected = false;
    };
} // namespace Diorama
