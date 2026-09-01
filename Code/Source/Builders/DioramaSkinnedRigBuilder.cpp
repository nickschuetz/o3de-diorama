/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Builders/DioramaSkinnedRigBuilder.h>

#include <Clients/DioramaSkinnedRigAsset.h>
#include <Clients/DragonBonesImport.h>
#include <Clients/SkinnedRigBinary.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/string/string.h>

namespace Diorama
{
    namespace
    {
        //! Stable product sub-id for the compiled skinned-rig asset.
        constexpr AZ::u32 SkinnedRigSubId = 0xD5C1A716; // "DS-RIG"

        //! Swap a "*_ske.json" source path's suffix for the companion "*_tex.json" atlas.
        //! Returns false if the path does not carry the expected suffix.
        bool DeriveAtlasPath(const AZStd::string& skePath, AZStd::string& outTexPath)
        {
            const size_t pos = skePath.rfind("_ske.json");
            if (pos == AZStd::string::npos)
            {
                return false;
            }
            outTexPath = skePath;
            outTexPath.replace(pos, AZStd::string("_ske.json").size(), "_tex.json");
            return true;
        }
    } // namespace

    DioramaSkinnedRigBuilder::~DioramaSkinnedRigBuilder()
    {
        BusDisconnect();
    }

    void DioramaSkinnedRigBuilder::RegisterBuilder()
    {
        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = "Diorama Skinned Rig Builder";
        descriptor.m_busId = AZ::Uuid::CreateString(BusIdString);
        descriptor.m_version = 1;
        descriptor.m_patterns.emplace_back("*_ske.json", AssetBuilderSDK::AssetBuilderPattern::Wildcard);
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

    void DioramaSkinnedRigBuilder::ShutDown()
    {
        m_isShuttingDown = true;
    }

    void DioramaSkinnedRigBuilder::CreateJobs(
        const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response) const
    {
        // The product bakes in the companion "*_tex.json" atlas sub-texture UV rects, so the
        // rig must rebuild when that atlas is repacked even if the skeleton is untouched.
        // ProcessJob reads it with a plain ReadFile, which the AssetProcessor cannot track, so
        // declare it as a source dependency here. (A rig that ships no atlas simply has an unmet
        // dependency, which the AP tolerates.)
        AZStd::string atlasSource;
        if (DeriveAtlasPath(request.m_sourceFile, atlasSource))
        {
            AssetBuilderSDK::SourceFileDependency atlasDependency;
            atlasDependency.m_sourceFileDependencyPath = atlasSource;
            response.m_sourceFileDependencyList.push_back(atlasDependency);
        }

        for (const AssetBuilderSDK::PlatformInfo& platform : request.m_enabledPlatforms)
        {
            AssetBuilderSDK::JobDescriptor job;
            job.m_jobKey = JobKey;
            job.SetPlatformIdentifier(platform.m_identifier.c_str());
            response.m_createJobOutputs.push_back(job);
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    void DioramaSkinnedRigBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        const auto skeResult = AZ::Utils::ReadFile<AZStd::string>(request.m_fullPath);
        if (!skeResult.IsSuccess())
        {
            AZ_Error("DioramaSkinnedRigBuilder", false, "Could not read %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        DragonBones::Document document;
        if (!DragonBones::ParseDocument(skeResult.GetValue(), document))
        {
            AZ_Error("DioramaSkinnedRigBuilder", false, "%s is not a valid DragonBones armature", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        // Bake the atlas sub-texture UV remap into the mesh UVs (as the runtime used to do at
        // load): DragonBones mesh UVs are normalized per sub-texture, so without this every mesh
        // samples the whole page. Absent atlas -> UVs stay as authored (a single-image rig).
        AZStd::string atlasPath;
        if (DeriveAtlasPath(request.m_fullPath, atlasPath))
        {
            const auto texResult = AZ::Utils::ReadFile<AZStd::string>(atlasPath);
            if (texResult.IsSuccess())
            {
                DragonBones::Atlas atlas;
                if (DragonBones::ParseAtlas(texResult.GetValue(), atlas))
                {
                    DragonBones::ApplyAtlasUVs(document, atlas);
                }
            }
        }

        // Reject a rig with no drawable mesh now, so a broken source fails the job rather than
        // shipping an empty product the runtime silently drops.
        bool hasMesh = false;
        for (const DragonBones::Armature& armature : document.m_armatures)
        {
            if (!armature.m_meshes.empty() || !armature.m_surfaceMeshes.empty())
            {
                hasMesh = true;
                break;
            }
        }
        if (!hasMesh)
        {
            AZ_Error("DioramaSkinnedRigBuilder", false, "%s has no skinned or surface mesh", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        DioramaSkinnedRigAsset asset;
        SkinnedRig::Encode(document, asset.m_payload);

        // Name the product after the source with the "_ske" suffix trimmed (hero_ske.json ->
        // hero.dskinrigc), so the product path reads naturally.
        AZ::IO::Path sourcePath(request.m_fullPath);
        AZStd::string baseName = sourcePath.Stem().Native();
        const AZStd::string skeSuffix = "_ske";
        if (baseName.size() >= skeSuffix.size() && baseName.compare(baseName.size() - skeSuffix.size(), skeSuffix.size(), skeSuffix) == 0)
        {
            baseName.erase(baseName.size() - skeSuffix.size());
        }

        const AZStd::string productFile = AZStd::string::format("%s/%s.dskinrigc", request.m_tempDirPath.c_str(), baseName.c_str());
        // Binary ObjectStream wrapping the compact SkinnedRig payload: the product loads with a
        // copy of the byte buffer (then a fast binary decode), never a re-parse of the JSON.
        if (!AZ::Utils::SaveObjectToFile(productFile, AZ::DataStream::ST_BINARY, &asset))
        {
            AZ_Error("DioramaSkinnedRigBuilder", false, "Could not write the skinned-rig product for %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        AssetBuilderSDK::JobProduct product(productFile, azrtti_typeid<DioramaSkinnedRigAsset>(), SkinnedRigSubId);
        // The rig payload is fully self-contained (the atlas texture is wired on the component,
        // not referenced by the rig), so it has no asset dependencies.
        product.m_dependenciesHandled = true;
        response.m_outputProducts.push_back(AZStd::move(product));
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }
} // namespace Diorama
