/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>

namespace Diorama
{
    //! Asset builder for DragonBones weighted-mesh / surface armatures. It matches the
    //! "*_ske.json" source (the open, Apache-2.0 DragonBones skeleton export), runs in the
    //! AssetProcessor, and: parses the armature (DragonBones::ParseDocument, untrusted input),
    //! reads the companion "*_tex.json" atlas and bakes its sub-texture UV remap into the mesh
    //! UVs (DragonBones::ParseAtlas + ApplyAtlasUVs), then emits a compact DioramaSkinnedRigAsset
    //! product (".dskinrigc") whose payload is the SkinnedRig binary encoding of the imported
    //! document. A DioramaSkinnedSprite* then references the product and loads it with a fast,
    //! bounds-checked binary decode instead of parsing JSON at runtime.
    class DioramaSkinnedRigBuilder final : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        static constexpr const char* JobKey = "Diorama Skinned Rig";
        static constexpr const char* BusIdString = "{7A9C2E4F-3B1D-4C5E-8F60-1A2B3C4D5E6F}";

        DioramaSkinnedRigBuilder() = default;
        ~DioramaSkinnedRigBuilder() override;

        //! Build the descriptor (pattern, version, callbacks), register it with the
        //! AssetProcessor, and connect this handler for ShutDown.
        void RegisterBuilder();

        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const;
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const;

        // AssetBuilderSDK::AssetBuilderCommandBus
        void ShutDown() override;

    private:
        bool m_isShuttingDown = false;
    };
} // namespace Diorama
