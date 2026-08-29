/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    //! Product asset the skinned-rig AssetBuilder bakes from a DragonBones "*_ske.json"
    //! (+ "*_tex.json" atlas) source (product extension ".dskinrigc"). It carries a single
    //! opaque field: the fully-imported, atlas-UV-remapped armature document encoded as the
    //! compact SkinnedRig binary (SkinnedRigBinary.h). The runtime SkinnedSpritePresenter
    //! decodes it with SkinnedRig::Decode -- a fast, bounds-checked binary read -- instead of
    //! parsing the source JSON at load, which is the VISION efficiency goal ("product assets
    //! load without runtime parsing") and keeps untrusted asset data bounded on the load path.
    //!
    //! The payload is stored as a byte vector so the stock GenericAssetHandler + ObjectStream
    //! plumbing (shared with the tilemap / aseprite-sheet assets) loads it, while the actual
    //! rig format stays the hand-rolled compact binary rather than reflected structs.
    class DioramaSkinnedRigAsset final : public AZ::Data::AssetData
    {
    public:
        AZ_RTTI(Diorama::DioramaSkinnedRigAsset, DioramaSkinnedRigAssetTypeId, AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(DioramaSkinnedRigAsset, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! SkinnedRig binary (magic 'DSKR'): the imported DragonBones document. Decode with
        //! SkinnedRig::Decode (bounds-checked); never index it directly. A meshless rig is
        //! rejected by the builder at bake time, and a corrupt payload fails Decode at load,
        //! so the presenter never builds from invalid data.
        AZStd::vector<AZ::u8> m_payload;
    };
} // namespace Diorama
