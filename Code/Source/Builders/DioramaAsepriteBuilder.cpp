/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Builders/DioramaAsepriteBuilder.h>

#include <Clients/AsepriteBinary.h>

#include <Atom/ImageProcessing/ImageObject.h>
#include <Atom/ImageProcessing/ImageProcessingBus.h>
#include <Atom/ImageProcessing/PixelFormats.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Utils/Utils.h>

#include <cmath>
#include <cstring>

namespace Diorama
{
    DioramaAsepriteBuilder::~DioramaAsepriteBuilder()
    {
        BusDisconnect();
    }

    void DioramaAsepriteBuilder::RegisterBuilder()
    {
        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Diorama Aseprite Builder";
        descriptor.m_busId = AZ::Uuid::CreateString(BusIdString);
        descriptor.m_version = 1;
        descriptor.m_patterns.emplace_back("*.aseprite", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
        descriptor.m_patterns.emplace_back("*.ase", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
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

    void DioramaAsepriteBuilder::ShutDown()
    {
        m_isShuttingDown = true;
    }

    void DioramaAsepriteBuilder::CreateJobs(
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

    void DioramaAsepriteBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        const auto fileResult = AZ::Utils::ReadFile<AZStd::vector<AZ::u8>>(request.m_fullPath);
        if (!fileResult.IsSuccess())
        {
            AZ_Error("DioramaAsepriteBuilder", false, "Could not read %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const AZStd::vector<AZ::u8>& bytes = fileResult.GetValue();
        Aseprite::BinarySprite sprite;
        if (!Aseprite::ParseAsepriteBinary(AZStd::span<const AZ::u8>(bytes.data(), bytes.size()), sprite) || sprite.m_frames.empty())
        {
            AZ_Error("DioramaAsepriteBuilder", false, "%s is not a valid .aseprite file", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        // Pack the composited frames into a squarish uniform grid.
        const int frameCount = static_cast<int>(sprite.m_frames.size());
        const int columns = AZ::GetMax(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(frameCount)))));

        AZ::IO::Path sourcePath(request.m_fullPath);
        const AZStd::string baseName = sourcePath.Stem().Native();
        const Aseprite::PackedAtlas atlas = Aseprite::PackFramesToGrid(sprite, columns, baseName + ".png");
        if (atlas.m_width <= 0 || atlas.m_height <= 0)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        // Create the image object through the bus (CreateImage's static factory lives in
        // the ImageProcessingAtom impl module; the bus avoids linking it here).
        ImageProcessingAtom::IImageObjectPtr image;
        ImageProcessingAtom::ImageBuilderRequestBus::BroadcastResult(
            image,
            &ImageProcessingAtom::ImageBuilderRequests::CreateImage,
            static_cast<AZ::u32>(atlas.m_width),
            static_cast<AZ::u32>(atlas.m_height),
            1u,
            ImageProcessingAtom::EPixelFormat::ePixelFormat_R8G8B8A8);
        if (image == nullptr)
        {
            AZ_Error("DioramaAsepriteBuilder", false, "ImageProcessing is not available to create the atlas image");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AZ::u8* destination = nullptr;
        AZ::u32 pitch = 0;
        image->GetImagePointer(0, destination, pitch);
        const AZ::u32 rowBytes = static_cast<AZ::u32>(atlas.m_width) * 4;
        for (int y = 0; y < atlas.m_height; ++y)
        {
            ::memcpy(destination + static_cast<size_t>(y) * pitch, atlas.m_rgba.data() + static_cast<size_t>(y) * rowBytes, rowBytes);
        }

        // Hand the image to the proven texture pipeline; it returns the streaming-image
        // job products (the implementation lives in ImageProcessingAtom.Editor, which
        // the AssetProcessor loads).
        AZStd::vector<AssetBuilderSDK::JobProduct> imageProducts;
        const AZ::Data::AssetId sourceAssetId(request.m_sourceFileUUID, 0);
        ImageProcessingAtom::ImageBuilderRequestBus::BroadcastResult(
            imageProducts,
            &ImageProcessingAtom::ImageBuilderRequests::ConvertImageObject,
            image,
            AZStd::string{ "AlbedoWithGenericAlpha" }, // sRGB color + alpha: the sprite-sheet preset
            request.m_platformInfo.m_identifier,
            request.m_tempDirPath,
            sourceAssetId,
            baseName);

        if (imageProducts.empty())
        {
            AZ_Error("DioramaAsepriteBuilder", false, "ImageProcessing produced no products for %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        for (AssetBuilderSDK::JobProduct& product : imageProducts)
        {
            response.m_outputProducts.push_back(AZStd::move(product));
        }
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }
} // namespace Diorama
