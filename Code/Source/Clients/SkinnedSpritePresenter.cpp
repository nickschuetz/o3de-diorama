/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SkinnedSpritePresenter.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/limits.h>

#include <Atom/RPI.Public/Scene.h>

namespace Diorama
{
    namespace
    {
        //! Read a whole text file through the file IO aliases. Empty on any failure.
        AZStd::string LoadTextFile(const AZStd::string& path)
        {
            AZ::IO::FileIOBase* io = AZ::IO::FileIOBase::GetInstance();
            if (io == nullptr || path.empty())
            {
                return {};
            }
            AZ::IO::HandleType handle = AZ::IO::InvalidHandle;
            if (!io->Open(path.c_str(), AZ::IO::OpenMode::ModeRead, handle))
            {
                return {};
            }
            AZ::u64 size = 0;
            io->Size(handle, size);
            AZStd::string buffer;
            buffer.resize(size);
            AZ::u64 bytesRead = 0;
            io->Read(handle, buffer.data(), size, false, &bytesRead);
            io->Close(handle);
            buffer.resize(bytesRead);
            return buffer;
        }
    } // namespace

    void DioramaSkinnedSpriteConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSkinnedSpriteConfig>()
                ->Version(1)
                ->Field("sourcePath", &DioramaSkinnedSpriteConfig::m_sourcePath)
                ->Field("armatureName", &DioramaSkinnedSpriteConfig::m_armatureName)
                ->Field("texture", &DioramaSkinnedSpriteConfig::m_texture)
                ->Field("scale", &DioramaSkinnedSpriteConfig::m_scale)
                ->Field("flipVertical", &DioramaSkinnedSpriteConfig::m_flipVertical)
                ->Field("billboard", &DioramaSkinnedSpriteConfig::m_billboard)
                ->Field("pointFilter", &DioramaSkinnedSpriteConfig::m_pointFilter)
                ->Field("sortOffset", &DioramaSkinnedSpriteConfig::m_sortOffset)
                ->Field("tint", &DioramaSkinnedSpriteConfig::m_tint);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaSkinnedSpriteConfig>("Skinned Sprite Config", "A DragonBones mesh-deform character")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_sourcePath,
                        "Source (ske.json)",
                        "Path to the DragonBones armature JSON, via file IO aliases (e.g. @projectroot@/...).")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_armatureName,
                        "Armature",
                        "Armature to use from the document; empty selects the first.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_texture,
                        "Texture",
                        "Atlas texture the mesh UVs address.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_scale,
                        "Scale",
                        "Armature-units to world-units. DragonBones rigs are pixel-sized, so this is small.")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaSkinnedSpriteConfig::m_flipVertical,
                        "Flip vertical",
                        "DragonBones is y-down; flip so the character stands upright.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaSkinnedSpriteConfig::m_billboard,
                        "Billboard",
                        "Face the camera like a sprite, or lie in the entity plane.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaSkinnedSpriteConfig::m_pointFilter,
                        "Point filter",
                        "Nearest-neighbor sampling (crisp pixel art) vs bilinear.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_sortOffset,
                        "Sort offset",
                        "Draw-order bias; higher draws on top.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color, &DioramaSkinnedSpriteConfig::m_tint, "Tint", "Multiplied into every vertex.");
            }
        }
    }

    bool SkinnedSpritePresenter::SetConfig(const DioramaSkinnedSpriteConfig& config)
    {
        m_config = config;
        // Kick off the atlas load so the mesh is textured (not the white fallback) as soon
        // as it is ready; this also drives the editor viewport preview, which otherwise has
        // nothing to trigger the load.
        if (m_config.m_texture.GetId().IsValid())
        {
            m_config.m_texture.QueueLoad();
        }
        return BuildRig();
    }

    void SkinnedSpritePresenter::Connect(AZ::EntityId entityId)
    {
        m_entityId = entityId;
        m_featureProcessor = nullptr;
        m_handlesAcquired = false;
    }

    void SkinnedSpritePresenter::Disconnect()
    {
        if (m_featureProcessor != nullptr)
        {
            for (RuntimeMesh& mesh : m_meshes)
            {
                if (mesh.m_handle != 0)
                {
                    m_featureProcessor->ReleaseMesh(mesh.m_handle);
                    mesh.m_handle = 0;
                }
            }
        }
        m_featureProcessor = nullptr;
        m_handlesAcquired = false;
    }

    bool SkinnedSpritePresenter::BuildRig()
    {
        // Drop any live handles for the previous rig before rebuilding (an editor edit can
        // rebuild while connected); a fresh acquire re-registers the new mesh set.
        Disconnect();

        m_rigBuilt = false;
        m_armature = nullptr;
        m_bones.clear();
        m_boneNameToIndex.clear();
        m_meshes.clear();

        const AZStd::string json = LoadTextFile(m_config.m_sourcePath);
        if (json.empty() || !DragonBones::ParseDocument(json, m_document))
        {
            return false;
        }

        // Remap mesh UVs through the companion "*_tex.json" atlas: DragonBones stores mesh UVs
        // normalized to each packed sub-texture, not to the whole page, so without this every
        // mesh samples the entire atlas (art plus transparent gutters). The atlas path is
        // derived from the source path (body_ske.json -> body_tex.json); if it is absent the
        // UVs are left as authored.
        AZStd::string atlasPath = m_config.m_sourcePath;
        const size_t skePos = atlasPath.find("_ske.json");
        if (skePos != AZStd::string::npos)
        {
            atlasPath.replace(skePos, 9, "_tex.json");
            const AZStd::string atlasJson = LoadTextFile(atlasPath);
            DragonBones::Atlas atlas;
            if (!atlasJson.empty() && DragonBones::ParseAtlas(atlasJson, atlas))
            {
                DragonBones::ApplyAtlasUVs(m_document, atlas);
            }
        }

        m_armature = m_config.m_armatureName.empty() ? (m_document.m_armatures.empty() ? nullptr : &m_document.m_armatures.front())
                                                     : DragonBones::FindArmature(m_document, m_config.m_armatureName);
        if (m_armature == nullptr || m_armature->m_meshes.empty())
        {
            m_armature = nullptr;
            return false;
        }

        // Bone hierarchy for the skinning core (parent-first bind locals).
        m_bones.resize(m_armature->m_bones.size());
        for (size_t i = 0; i < m_armature->m_bones.size(); ++i)
        {
            m_bones[i].m_parentIndex = m_armature->m_bones[i].m_parentIndex;
            m_bones[i].m_local = m_armature->m_bones[i].m_bindLocal;
            m_boneNameToIndex[m_armature->m_bones[i].m_name] = static_cast<int>(i);
        }

        // Runtime meshes + the bind-pose bounding box (its center recenters the rig on
        // the entity origin so scale/placement are predictable).
        float minX = AZStd::numeric_limits<float>::max();
        float minY = AZStd::numeric_limits<float>::max();
        float maxX = -AZStd::numeric_limits<float>::max();
        float maxY = -AZStd::numeric_limits<float>::max();
        m_meshes.reserve(m_armature->m_meshes.size());
        for (const DragonBones::SkinnedMesh& mesh : m_armature->m_meshes)
        {
            RuntimeMesh runtime;
            runtime.m_source = &mesh;
            runtime.m_skinMatrices.resize(mesh.m_bindWorld.size());
            // Emit each triangle with both windings so the billboarded mesh is visible
            // regardless of the source winding (it always faces the camera, so being
            // double-sided is free of artifacts and removes a whole class of "culled to
            // invisible" surprises across differently authored rigs).
            runtime.m_indices.reserve(mesh.m_indices.size() * 2);
            for (size_t t = 0; t + 2 < mesh.m_indices.size(); t += 3)
            {
                const AZ::u32 a = mesh.m_indices[t];
                const AZ::u32 b = mesh.m_indices[t + 1];
                const AZ::u32 c = mesh.m_indices[t + 2];
                runtime.m_indices.push_back(a);
                runtime.m_indices.push_back(b);
                runtime.m_indices.push_back(c);
                runtime.m_indices.push_back(a);
                runtime.m_indices.push_back(c);
                runtime.m_indices.push_back(b);
            }
            m_meshes.push_back(AZStd::move(runtime));

            for (const MeshSkin::SkinnedVertex& vertex : mesh.m_vertices)
            {
                minX = AZ::GetMin(minX, vertex.m_bindPos.GetX());
                minY = AZ::GetMin(minY, vertex.m_bindPos.GetY());
                maxX = AZ::GetMax(maxX, vertex.m_bindPos.GetX());
                maxY = AZ::GetMax(maxY, vertex.m_bindPos.GetY());
            }
        }
        m_center = AZ::Vector2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);

        // Per-frame pose scratch, sized once.
        const size_t boneCount = m_bones.size();
        m_boneRotationDelta.assign(boneCount, 0.0f);
        m_boneTranslationDelta.assign(boneCount, AZ::Vector2::CreateZero());
        m_localOverrides.resize(boneCount);
        m_world.resize(boneCount);

        m_rigBuilt = true;
        return true;
    }

    bool SkinnedSpritePresenter::TryAcquireFeatureProcessor()
    {
        if (m_handlesAcquired)
        {
            return true;
        }
        if (!m_rigBuilt || !m_entityId.IsValid())
        {
            return false;
        }

        AZ::RPI::Scene* scene = AZ::RPI::Scene::GetSceneForEntityId(m_entityId);
        if (scene == nullptr)
        {
            return false;
        }
        m_featureProcessor = scene->GetFeatureProcessor<SpriteFeatureProcessor>();
        if (m_featureProcessor == nullptr)
        {
            m_featureProcessor = scene->EnableFeatureProcessor<SpriteFeatureProcessor>();
        }
        if (m_featureProcessor == nullptr)
        {
            return false;
        }

        for (RuntimeMesh& mesh : m_meshes)
        {
            if (mesh.m_handle == 0)
            {
                mesh.m_handle = m_featureProcessor->AcquireMesh();
            }
        }
        m_handlesAcquired = true;
        return true;
    }

    void SkinnedSpritePresenter::Tick()
    {
        if (!TryAcquireFeatureProcessor())
        {
            return;
        }
        SkinAndPush();
    }

    void SkinnedSpritePresenter::SkinAndPush()
    {
        AZ::Transform worldTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTransform, m_entityId, &AZ::TransformBus::Events::GetWorldTM);

        // Pose: apply each bone's override rotation about its joint in the PARENT frame,
        // not in the bone's own bind-rotated/scaled frame. Composing on the right
        // (bindLocal * R_delta) would rotate inside the bind linear map, so any bone with
        // non-uniform bind scale or skew (common in DragonBones) would shear/twist rather
        // than bend. Instead: override = Translate(bindTranslation + transDelta) * R_delta *
        // bindLinear, which rotates the bone (and its descendants) cleanly about the joint.
        for (size_t i = 0; i < m_bones.size(); ++i)
        {
            const MeshSkin::Affine2D& bind = m_bones[i].m_local;
            MeshSkin::Affine2D bindLinear = bind; // bind minus its translation
            bindLinear.m_tx = 0.0f;
            bindLinear.m_ty = 0.0f;
            const MeshSkin::Affine2D rot =
                MeshSkin::Affine2D::FromTRS(AZ::Vector2::CreateZero(), m_boneRotationDelta[i], AZ::Vector2(1.0f, 1.0f));
            MeshSkin::Affine2D over = MeshSkin::Multiply(rot, bindLinear); // R_delta * bindLinear (translation stays 0)
            over.m_tx = bind.m_tx + m_boneTranslationDelta[i].GetX();
            over.m_ty = bind.m_ty + m_boneTranslationDelta[i].GetY();
            m_localOverrides[i] = over;
        }
        MeshSkin::ComputeWorldTransforms(
            AZStd::span<const MeshSkin::Bone>(m_bones.data(), m_bones.size()),
            AZStd::span<const MeshSkin::Affine2D>(m_localOverrides.data(), m_localOverrides.size()),
            AZStd::span<MeshSkin::Affine2D>(m_world.data(), m_world.size()));

        const float scaleX = m_config.m_scale;
        const float scaleY = m_config.m_flipVertical ? -m_config.m_scale : m_config.m_scale;
        const int worldCount = static_cast<int>(m_world.size());

        for (RuntimeMesh& runtime : m_meshes)
        {
            if (runtime.m_handle == 0 || runtime.m_source == nullptr)
            {
                continue;
            }
            const DragonBones::SkinnedMesh& source = *runtime.m_source;

            // Skinning matrix per local bone slot: currentWorld * inverse(bindWorld).
            for (size_t local = 0; local < source.m_bindWorld.size(); ++local)
            {
                const int global = source.m_boneGlobalIndices[local];
                const MeshSkin::Affine2D current = (global >= 0 && global < worldCount) ? m_world[global] : MeshSkin::Affine2D::Identity();
                runtime.m_skinMatrices[local] = MeshSkin::SkinningMatrix(current, source.m_bindWorld[local]);
            }

            // Deform each vertex and recenter/scale into entity-local world units.
            m_vertexScratch.clear();
            m_vertexScratch.resize(source.m_vertices.size());
            for (size_t v = 0; v < source.m_vertices.size(); ++v)
            {
                const AZ::Vector2 deformed = MeshSkin::SkinVertex(
                    source.m_vertices[v],
                    AZStd::span<const MeshSkin::Affine2D>(runtime.m_skinMatrices.data(), runtime.m_skinMatrices.size()));
                SpriteFeatureProcessor::MeshVertex& out = m_vertexScratch[v];
                out.m_position = AZ::Vector2((deformed.GetX() - m_center.GetX()) * scaleX, (deformed.GetY() - m_center.GetY()) * scaleY);
                out.m_uv = source.m_vertices[v].m_uv;
            }

            // Layer the parts back-to-front by their DragonBones slot draw order (a small
            // per-part bias on top of the configured sort offset), so the near arm draws
            // over the torso and the far arm behind it, instead of an arbitrary order.
            const float partSortOffset = m_config.m_sortOffset + static_cast<float>(source.m_drawOrder) * 0.001f;

            m_featureProcessor->UpdateMesh(
                runtime.m_handle,
                worldTransform,
                m_config.m_texture,
                m_config.m_tint,
                partSortOffset,
                m_config.m_billboard,
                m_config.m_pointFilter,
                AZStd::span<const SpriteFeatureProcessor::MeshVertex>(m_vertexScratch.data(), m_vertexScratch.size()),
                AZStd::span<const AZ::u32>(runtime.m_indices.data(), runtime.m_indices.size()));
        }
    }

    void SkinnedSpritePresenter::SetBoneRotation(const AZStd::string& boneName, float degrees)
    {
        const auto it = m_boneNameToIndex.find(boneName);
        if (it != m_boneNameToIndex.end())
        {
            m_boneRotationDelta[it->second] = AZ::DegToRad(degrees);
        }
    }

    void SkinnedSpritePresenter::SetBoneTranslation(const AZStd::string& boneName, const AZ::Vector2& offset)
    {
        const auto it = m_boneNameToIndex.find(boneName);
        if (it != m_boneNameToIndex.end())
        {
            m_boneTranslationDelta[it->second] = offset;
        }
    }

    void SkinnedSpritePresenter::ResetPose()
    {
        for (float& r : m_boneRotationDelta)
        {
            r = 0.0f;
        }
        for (AZ::Vector2& t : m_boneTranslationDelta)
        {
            t = AZ::Vector2::CreateZero();
        }
    }

    SkinnedSpriteInfo SkinnedSpritePresenter::GetInfo() const
    {
        SkinnedSpriteInfo info;
        info.m_loaded = m_rigBuilt;
        info.m_visible = m_handlesAcquired && m_config.m_texture.IsReady();
        info.m_boneCount = static_cast<int>(m_bones.size());
        info.m_meshCount = static_cast<int>(m_meshes.size());
        for (const RuntimeMesh& mesh : m_meshes)
        {
            if (mesh.m_source != nullptr)
            {
                info.m_vertexCount += static_cast<int>(mesh.m_source->m_vertices.size());
            }
        }
        return info;
    }
} // namespace Diorama
