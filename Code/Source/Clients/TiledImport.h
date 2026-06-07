/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTilemapAsset.h>

#include <AzCore/std/functional.h>
#include <AzCore/std/string/string.h>

namespace Diorama::TiledImport
{
    //! Resolves an external tileset reference (a `.tsj` named in a map's
    //! `tilesets[].source`) to its JSON text. The builder supplies one that reads the
    //! file relative to the map; when absent (e.g. unit tests with embedded tilesets)
    //! an external reference is rejected. Returns false if the source cannot be read.
    using ExternalTilesetResolver = AZStd::function<bool(AZStd::string_view source, AZStd::string& outJson)>;

    //! Parse a Tiled map exported as JSON (`.tmj`, finite orthogonal) into a tilemap
    //! asset, treating the input as untrusted (same bounds as the native source). The
    //! `.tmj`/`.tmx` format is openly documented, and this is a from-scratch parser
    //! (no Tiled/GPL library), so it carries no third-party license obligation.
    //!
    //! Supported in this phase: a single tileset (the first), finite orthogonal
    //! tile layers (`type == "tilelayer"` with an integer `data` array). Tiled GIDs
    //! map to atlas cells as `(gid & 0x1FFFFFFF) - firstgid`; `0` is an empty cell.
    //! Flip/rotation flag bits are masked off (not yet applied). Object/image layers
    //! and external tilesets (`.tsx`/`.tsj`) are skipped/unsupported for now. The
    //! world tile size defaults to 1.0 (Tiled's pixel sizes describe the atlas, not
    //! world units); adjust it on the component or in a native `.dtilemap`.
    //!
    //! Supported tileset orientation flags (horizontal and vertical mirroring) are
    //! carried into the product's per-tile flip bits; the diagonal/rotation flag is
    //! not applied yet (it needs a 90-degree UV rotation the sprite path lacks).
    //!
    //! Returns true and fills outAsset on success; false with a reason in outError
    //! otherwise. No direct file access: an embedded tileset needs no resolver, and
    //! an external `.tsj` is fetched through the supplied resolver, so the core stays
    //! unit-testable. Emits the same DioramaTilemapAsset the native builder does.
    bool Parse(
        AZStd::string_view json, DioramaTilemapAsset& outAsset, AZStd::string& outError, const ExternalTilesetResolver& resolver = {});
} // namespace Diorama::TiledImport
