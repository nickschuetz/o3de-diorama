/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTilemapAsset.h>

#include <AzCore/std/string/string.h>

namespace Diorama::TiledImport
{
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
    //! Returns true and fills outAsset on success; false with a reason in outError
    //! otherwise. Pure (no file/asset access), so it is unit-tested directly. Emits
    //! the same DioramaTilemapAsset the native builder does.
    bool Parse(AZStd::string_view json, DioramaTilemapAsset& outAsset, AZStd::string& outError);
} // namespace Diorama::TiledImport
