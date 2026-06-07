/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Builders/DioramaTilemapBuilder.h>

#include <Clients/TiledImport.h>
#include <Clients/TilemapSource.h>
#include <Diorama/DioramaTilemapAsset.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/string/string.h>

namespace Diorama
{
    namespace
    {
        //! Stable product sub-id for the compiled tilemap asset.
        constexpr AZ::u32 TilemapSubId = 0xD10A7117;
    } // namespace

    DioramaTilemapBuilder::~DioramaTilemapBuilder()
    {
        BusDisconnect();
    }

    void DioramaTilemapBuilder::RegisterBuilder()
    {
        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Diorama Tilemap Builder";
        descriptor.m_busId = AZ::Uuid::CreateString(BusIdString);
        descriptor.m_version = 2; // 2: also import Tiled .tmj
        descriptor.m_patterns.emplace_back("*.dtilemap", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        descriptor.m_patterns.emplace_back("*.tmj", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        descriptor.m_createJobFunction =
            [this](const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
        {
            CreateJobs(request, response);
        };
        descriptor.m_processJobFunction =
            [this](const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
        {
            ProcessJob(request, response);
        };

        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Events::RegisterBuilderInformation, descriptor);
        BusConnect(descriptor.m_busId);
    }

    void DioramaTilemapBuilder::ShutDown()
    {
        m_isShuttingDown = true;
    }

    void DioramaTilemapBuilder::CreateJobs(
        const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const
    {
        for (const AssetBuilderSDK::PlatformInfo& platform : request.m_enabledPlatforms)
        {
            AssetBuilderSDK::JobDescriptor job;
            job.m_jobKey = JobKey;
            job.SetPlatformIdentifier(platform.m_identifier.c_str());
            response.m_createJobOutputs.push_back(job);
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    void DioramaTilemapBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        const auto fileResult = AZ::Utils::ReadFile<AZStd::string>(request.m_fullPath);
        if (!fileResult.IsSuccess())
        {
            AZ_Error("DioramaTilemapBuilder", false, "Could not read %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AZ::IO::Path sourcePath(request.m_fullPath);
        const AZStd::string extension = sourcePath.Extension().Native(); // includes the dot, e.g. ".tmj"

        DioramaTilemapAsset asset;
        AZStd::string parseError;
        const bool parsed = (extension == ".tmj") ? TiledImport::Parse(fileResult.GetValue(), asset, parseError)
                                                  : TilemapSource::Parse(fileResult.GetValue(), asset, parseError);
        if (!parsed)
        {
            AZ_Error("DioramaTilemapBuilder", false, "%s: %s", request.m_fullPath.c_str(), parseError.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const AZStd::string baseName = sourcePath.Stem().Native();
        const AZStd::string productFile = AZStd::string::format("%s/%s.dtilemapc", request.m_tempDirPath.c_str(), baseName.c_str());
        // Binary ObjectStream: the product loads with a copy, not a re-parse of the
        // source JSON, which is the point of compiling the map to an asset.
        if (!AZ::Utils::SaveObjectToFile(productFile, AZ::DataStream::ST_BINARY, &asset))
        {
            AZ_Error("DioramaTilemapBuilder", false, "Could not write the tilemap product for %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AssetBuilderSDK::JobProduct product(productFile, azrtti_typeid<DioramaTilemapAsset>(), TilemapSubId);
        // The atlas is a pre-existing image asset referenced by path; declare a path
        // dependency so it is built and the runtime can resolve it via SetAtlasByPath.
        if (!asset.m_atlasTexturePath.empty())
        {
            product.m_pathDependencies.emplace(asset.m_atlasTexturePath, AssetBuilderSDK::ProductPathDependencyType::ProductFile);
        }
        product.m_dependenciesHandled = true;
        response.m_outputProducts.push_back(AZStd::move(product));
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }
} // namespace Diorama
