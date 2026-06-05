/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Builders/DioramaAsepriteBuilder.h>

#include <Clients/AsepriteBinary.h>
#include <Clients/DioramaAsepriteSheetAsset.h>
#include <Clients/DioramaAssetUtils.h>

#include <Atom/ImageProcessing/ImageObject.h>
#include <Atom/ImageProcessing/ImageProcessingBus.h>
#include <Atom/ImageProcessing/PixelFormats.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/Utils.h>

#include <cmath>
#include <cstring>

namespace Diorama
{
    namespace
    {
        //! Stable product sub-id for the sheet metadata (distinct from the image products).
        constexpr AZ::u32 SheetSubId = 0xD10A5EE7;
    } // namespace

    DioramaAsepriteBuilder::~DioramaAsepriteBuilder()
    {
        BusDisconnect();
    }

    void DioramaAsepriteBuilder::RegisterBuilder()
    {
        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Diorama Aseprite Builder";
        descriptor.m_busId = AZ::Uuid::CreateString(BusIdString);
        descriptor.m_version = 3; // 2: emit .dioramasheet; 3: scan-folder-relative atlas path
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

        // Resolve the image's product AssetId (for the sheet's dependency) and the
        // image's runtime product path (for the sprite to resolve via SetTextureByPath):
        // same source-relative folder + base name + the streaming-image extension.
        const AZ::u32 imageSubId = imageProducts.front().m_productSubID;
        for (AssetBuilderSDK::JobProduct& product : imageProducts)
        {
            response.m_outputProducts.push_back(AZStd::move(product));
        }

        AZ::IO::Path imageProductPath(request.m_sourceFile);
        imageProductPath.ReplaceExtension("streamingimage");

        // Build the sheet metadata asset (frames/tags/atlas + the image path) and emit it
        // as a product, with a dependency on the image product. The atlas path is stored
        // scan-folder-relative so the runtime sprite can resolve it via SetTextureByPath.
        DioramaAsepriteSheetAsset sheet;
        sheet.m_atlasTexturePath = ToScanFolderRelativePath(imageProductPath.AsPosix());
        sheet.m_atlasWidth = atlas.m_width;
        sheet.m_atlasHeight = atlas.m_height;
        sheet.m_frames.reserve(atlas.m_document.m_frames.size());
        for (const Aseprite::FrameData& frame : atlas.m_document.m_frames)
        {
            AsepriteFrameData out;
            out.m_x = frame.m_x;
            out.m_y = frame.m_y;
            out.m_w = frame.m_w;
            out.m_h = frame.m_h;
            out.m_durationSeconds = frame.m_durationSeconds;
            sheet.m_frames.push_back(out);
        }
        sheet.m_tags.reserve(atlas.m_document.m_tags.size());
        for (const Aseprite::TagData& tag : atlas.m_document.m_tags)
        {
            AsepriteTagData out;
            out.m_name = tag.m_name;
            out.m_from = tag.m_from;
            out.m_to = tag.m_to;
            out.m_direction = tag.m_direction;
            sheet.m_tags.push_back(out);
        }
        if (!sheet.m_tags.empty())
        {
            sheet.m_defaultTag = sheet.m_tags.front().m_name;
        }

        const AZStd::string sheetFile = AZStd::string::format("%s/%s.dioramasheet", request.m_tempDirPath.c_str(), baseName.c_str());
        if (!AZ::Utils::SaveObjectToFile(sheetFile, AZ::DataStream::ST_XML, &sheet))
        {
            AZ_Error("DioramaAsepriteBuilder", false, "Could not write the sheet metadata for %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AssetBuilderSDK::JobProduct sheetProduct(sheetFile, azrtti_typeid<DioramaAsepriteSheetAsset>(), SheetSubId);
        sheetProduct.m_dependencies.emplace_back(
            AZ::Data::AssetId(request.m_sourceFileUUID, imageSubId),
            AZ::Data::ProductDependencyInfo::CreateFlags(AZ::Data::AssetLoadBehavior::PreLoad));
        sheetProduct.m_dependenciesHandled = true;
        response.m_outputProducts.push_back(AZStd::move(sheetProduct));

        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }
} // namespace Diorama
