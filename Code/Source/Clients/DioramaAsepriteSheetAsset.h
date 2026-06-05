/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaAsepriteComponent.h> // AsepriteFrameData / AsepriteTagData (shared reflected structs)
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/string/string.h>

namespace Diorama
{
    //! Product asset emitted by the .aseprite AssetBuilder: the imported sheet's
    //! playable metadata (the atlas image product path, atlas size, frames with
    //! durations, named tags). A DioramaAsepriteComponent can reference one and play it,
    //! so a native .aseprite import becomes a drop-in animated sprite. The fields mirror
    //! the component's own config so playback reuses the exact same code path as the
    //! editor-time JSON import.
    class DioramaAsepriteSheetAsset final : public AZ::Data::AssetData
    {
    public:
        AZ_RTTI(Diorama::DioramaAsepriteSheetAsset, DioramaAsepriteSheetAssetTypeId, AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(DioramaAsepriteSheetAsset, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Product path of the atlas image the builder produced (a .streamingimage),
        //! resolvable by the sprite's SetTextureByPath.
        AZStd::string m_atlasTexturePath;
        int m_atlasWidth = 0;
        int m_atlasHeight = 0;
        AZStd::vector<AsepriteFrameData> m_frames;
        AZStd::vector<AsepriteTagData> m_tags;
        AZStd::string m_defaultTag;
    };
} // namespace Diorama
