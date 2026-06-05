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
    //! Asset builder for native Aseprite files (.aseprite / .ase). It runs in the
    //! AssetProcessor: parse the binary (AsepriteBinary.h), composite + pack the frames
    //! into an atlas (PackFramesToGrid), and hand the atlas to ImageProcessingAtom's
    //! ConvertImageObject so it becomes an ordinary StreamingImageAsset (we reuse the
    //! proven texture pipeline rather than reimplement compression). The .aseprite
    //! format is openly documented, so this carries no third-party license obligation.
    //!
    //! Phase 2b milestone 1 emits the atlas image product. The per-frame/tag metadata
    //! product + the component asset-reference consumption is milestone 2.
    class DioramaAsepriteBuilder final : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        static constexpr const char* JobKey = "Diorama Aseprite Sheet";
        static constexpr const char* BusIdString = "{B7E3D2A1-4C5F-4E6A-8B9C-1D2E3F405162}";

        DioramaAsepriteBuilder() = default;
        ~DioramaAsepriteBuilder() override;

        //! Build the descriptor (patterns, version, job/process callbacks), register it
        //! with the AssetProcessor, and connect this handler for ShutDown.
        void RegisterBuilder();

        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const;
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const;

        // AssetBuilderSDK::AssetBuilderCommandBus
        void ShutDown() override;

    private:
        bool m_isShuttingDown = false;
    };
} // namespace Diorama
