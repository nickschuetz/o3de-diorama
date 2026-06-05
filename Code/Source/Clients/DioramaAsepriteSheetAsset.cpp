/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAsepriteSheetAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaAsepriteSheetAsset::Reflect(AZ::ReflectContext* context)
    {
        // AsepriteFrameData / AsepriteTagData (and the Direction enum) are reflected by
        // DioramaAsepriteConfig::Reflect, which runs in every context this asset does
        // (the editor/builder module registers the runtime descriptors too), so the
        // field types resolve without being reflected twice.
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaAsepriteSheetAsset, AZ::Data::AssetData>()
                ->Version(1)
                ->Field("atlasTexturePath", &DioramaAsepriteSheetAsset::m_atlasTexturePath)
                ->Field("atlasWidth", &DioramaAsepriteSheetAsset::m_atlasWidth)
                ->Field("atlasHeight", &DioramaAsepriteSheetAsset::m_atlasHeight)
                ->Field("frames", &DioramaAsepriteSheetAsset::m_frames)
                ->Field("tags", &DioramaAsepriteSheetAsset::m_tags)
                ->Field("defaultTag", &DioramaAsepriteSheetAsset::m_defaultTag);
        }
    }
} // namespace Diorama
