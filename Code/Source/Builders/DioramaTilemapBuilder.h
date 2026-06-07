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
    //! Asset builder for dedicated tilemap sources (*.dtilemap, a JSON document). It
    //! runs in the AssetProcessor: parse + validate the source as untrusted input
    //! (TilemapSource::Parse / DioramaTilemapAsset::IsValid), then emit a compact
    //! DioramaTilemapAsset product (*.dtilemapc) a TilemapComponent can reference, so
    //! a large map no longer inlines its tile array into the prefab/level.
    //!
    //! The source format is Diorama's own open JSON; a Tiled (.tmj) importer emitting
    //! the same product is the planned follow-up.
    class DioramaTilemapBuilder final : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        static constexpr const char* JobKey = "Diorama Tilemap";
        static constexpr const char* BusIdString = "{C4D5E6F7-0819-4A2B-9C3D-4E5F60718293}";

        DioramaTilemapBuilder() = default;
        ~DioramaTilemapBuilder() override;

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
