/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/TilemapPresenter.h>
#include <Diorama/DioramaTilemapAsset.h>
#include <Diorama/TilemapComponentConfig.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Diorama
{
    //! Build a render config for one layer of a tilemap asset: the shared grid,
    //! atlas, and tile size, plus that layer's tiles, tint, and sort offset. The
    //! atlas product path is resolved to a streaming-image id. Used to drive a
    //! TilemapPresenter per layer so every layer of a multi-layer asset renders.
    TilemapComponentConfig BuildTilemapLayerConfig(const DioramaTilemapAsset& asset, size_t layerIndex);

    //! Owns the presenters for the *secondary* layers (index >= 1) of a tilemap
    //! asset. Layer 0 stays on the owning component's own presenter (so the request
    //! bus keeps editing it); this renders the rest as additional batched layers at
    //! their own sort offsets. Rebuilt whenever the asset (re)loads.
    class TilemapExtraLayers final
    {
    public:
        TilemapExtraLayers() = default;
        ~TilemapExtraLayers();

        // The presenters are transient runtime state (rebuilt from the asset on
        // Activate), and a vector of unique_ptr is move-only. A component must stay
        // copy-constructible for SerializeContext's type registration, so copying
        // yields an empty set (the copy rebuilds its own layers when it activates).
        TilemapExtraLayers(const TilemapExtraLayers&)
        {
        }
        TilemapExtraLayers& operator=(const TilemapExtraLayers&)
        {
            Clear();
            return *this;
        }
        TilemapExtraLayers(TilemapExtraLayers&&) noexcept = default;
        TilemapExtraLayers& operator=(TilemapExtraLayers&&) noexcept = default;

        //! Release any existing extra-layer presenters and create one per layer
        //! beyond the first, each connected for the given entity.
        void Rebuild(AZ::EntityId entityId, const DioramaTilemapAsset& asset);

        //! Disconnect and release every extra-layer presenter.
        void Clear();

    private:
        AZStd::vector<AZStd::unique_ptr<TilemapPresenter>> m_presenters;
    };
} // namespace Diorama
