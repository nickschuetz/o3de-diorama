/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/TilemapPresenter.h>
#include <Clients/TilemapRequestHandler.h>
#include <Diorama/TilemapComponentConfig.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>

namespace Diorama
{
    //! Lightweight runtime component that draws a world-space tilemap: a grid of
    //! atlas cells batched into a single draw call. It holds the configuration and
    //! delegates all rendering to the shared TilemapPresenter. No Qt or tools
    //! dependency, so it ships in game clients.
    //!
    //! When the config references a compiled tilemap asset (.dtilemapc), the
    //! component loads it and applies its grid/atlas/first layer over the inline
    //! config, so a large map is referenced rather than inlined into the prefab.
    class TilemapComponent final
        : public AZ::Component
        , protected AZ::Data::AssetBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::TilemapComponent, TilemapComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        TilemapComponent() = default;
        explicit TilemapComponent(const TilemapComponentConfig& config);
        ~TilemapComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::Data::AssetBus (asset-reference mode: load the .dtilemapc, then apply it)
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

    private:
        //! Copy a loaded tilemap asset's grid, atlas, and first layer into m_config,
        //! then push it to the presenter. The atlas path is resolved to a streaming
        //! image id; the inline config is the fallback when no asset is assigned.
        void ApplyTilemapAsset(const DioramaTilemapAsset& asset);

        TilemapComponentConfig m_config;
        TilemapPresenter m_presenter;
        TilemapRequestHandler m_requestHandler;
    };
} // namespace Diorama
