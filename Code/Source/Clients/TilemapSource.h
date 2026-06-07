/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTilemapAsset.h>

#include <AzCore/std/string/string.h>

namespace Diorama::TilemapSource
{
    //! Parse a .dioramatilemap JSON source into a tilemap asset, treating the input
    //! as untrusted: every dimension, layer count, tile-array length, and tile index
    //! is bounded (TilemapAssetLimits) and the result is rejected if inconsistent, so
    //! a hostile or malformed source can never feed an unchecked size downstream.
    //!
    //! Source shape (a top-level "tiles" array is shorthand for a single layer):
    //! {
    //!   "columns": 8, "rows": 6,
    //!   "atlas": "diorama/textures/tileset.png", "atlasColumns": 4, "atlasRows": 4,
    //!   "tileWidth": 1.0, "tileHeight": 1.0,
    //!   "layers": [ { "name": "ground", "sortOffset": 0.0, "tint": [1,1,1,1], "tiles": [ ... ] } ]
    //! }
    //!
    //! Returns true and fills outAsset on success; returns false and writes a
    //! human-readable reason into outError otherwise (outAsset is left unspecified).
    //! Pure: no file or asset-system access, so it is unit-tested directly.
    bool Parse(AZStd::string_view json, DioramaTilemapAsset& outAsset, AZStd::string& outError);
} // namespace Diorama::TilemapSource
