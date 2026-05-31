/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace Diorama
{
    //! Resolve a StreamingImage asset id from either its product path
    //! ("x.png.streamingimage") or the more intuitive source path ("x.png").
    //! The StreamingImage product is the source path plus ".streamingimage", so
    //! when the path as given does not resolve, retry once with that suffix. This
    //! lets callers pass the path they see on disk as well as the O3DE product
    //! path, at the cost of a single extra catalog lookup only on a miss. Returns
    //! an invalid id if neither resolves.
    inline AZ::Data::AssetId ResolveStreamingImageAssetId(AZStd::string_view path)
    {
        AZ::Data::AssetId assetId =
            AZ::RPI::AssetUtils::GetAssetIdForProductPath(AZStd::string(path).c_str(), AZ::RPI::AssetUtils::TraceLevel::None);
        if (!assetId.IsValid() && !path.ends_with(".streamingimage"))
        {
            const AZStd::string productPath = AZStd::string(path) + ".streamingimage";
            assetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(productPath.c_str(), AZ::RPI::AssetUtils::TraceLevel::Warning);
        }
        return assetId;
    }
} // namespace Diorama
