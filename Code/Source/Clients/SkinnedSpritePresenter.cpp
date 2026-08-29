/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SkinnedSpritePresenter.h>

#include <Clients/SkinnedRigBinary.h>

#include <AzCore/Asset/AssetManager.h>
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
                ->Version(2)
                ->Field("rigAsset", &DioramaSkinnedSpriteConfig::m_rigAsset)
                ->Field("sourcePath", &DioramaSkinnedSpriteConfig::m_sourcePath)
                ->Field("armatureName", &DioramaSkinnedSpriteConfig::m_armatureName)
                ->Field("texture", &DioramaSkinnedSpriteConfig::m_texture)
                ->Field("scale", &DioramaSkinnedSpriteConfig::m_scale)
                ->Field("flipVertical", &DioramaSkinnedSpriteConfig::m_flipVertical)
                ->Field("billboard", &DioramaSkinnedSpriteConfig::m_billboard)
                ->Field("pointFilter", &DioramaSkinnedSpriteConfig::m_pointFilter)
                ->Field("sortOffset", &DioramaSkinnedSpriteConfig::m_sortOffset)
                ->Field("tint", &DioramaSkinnedSpriteConfig::m_tint)
                ->Field("animationName", &DioramaSkinnedSpriteConfig::m_animationName)
                ->Field("autoPlay", &DioramaSkinnedSpriteConfig::m_autoPlay)
                ->Field("speed", &DioramaSkinnedSpriteConfig::m_speed)
                ->Field("useSimClock", &DioramaSkinnedSpriteConfig::m_useSimClock);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaSkinnedSpriteConfig>("Skinned Sprite Config", "A DragonBones mesh-deform character")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_rigAsset,
                        "Rig Asset",
                        "Compiled DragonBones rig product (.dskinrigc), baked by the AssetBuilder from a *_ske.json. "
                        "Preferred over Source: loads from the compact binary (no runtime JSON parsing). Leave unset "
                        "to load the Source path directly.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_sourcePath,
                        "Source (ske.json)",
                        "Path to the DragonBones armature JSON, via file IO aliases (e.g. @projectroot@/...). Used only "
                        "when no Rig Asset is set.")
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
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaSkinnedSpriteConfig::m_tint, "Tint", "Multiplied into every vertex.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_animationName,
                        "Animation",
                        "Clip to play on activate; empty leaves the rig in its bind pose.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaSkinnedSpriteConfig::m_autoPlay,
                        "Auto play",
                        "Start the clip automatically on activate.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkinnedSpriteConfig::m_speed,
                        "Speed",
                        "Playback rate multiplier (negative plays in reverse).")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaSkinnedSpriteConfig::m_useSimClock,
                        "Use simulation clock",
                        "Advance on the 2D Simulation Clock's fixed steps (deterministic / rollback-exact) instead of the render "
                        "tick. Falls back to the render tick with no clock in the level.");
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
        const bool built = BuildRig();
        // Auto-play the configured clip (if any) so the character animates in game and in the
        // editor preview without a script.
        if (built && m_config.m_autoPlay && !m_config.m_animationName.empty() && m_armature != nullptr)
        {
            const DragonBones::Animation* clip = DragonBones::FindAnimation(*m_armature, m_config.m_animationName);
            if (clip != nullptr)
            {
                m_animation = clip;
                m_animTime = 0.0f;
                m_playing = true;
                m_loop = clip->m_loop;
            }
        }
        return built;
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
            for (SurfaceRuntimeMesh& mesh : m_surfaceMeshes)
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
        m_surfaceMeshes.clear();
        m_surfaceGrids.clear();
        // The clip points into m_document, which is about to be re-parsed; drop it so it can
        // never dangle. SetConfig re-resolves it after the rebuild.
        m_animation = nullptr;
        m_playing = false;
        m_animTime = 0.0f;

        // Populate m_document. Prefer the compiled rig-asset product: it is a compact binary the
        // AssetBuilder baked from the DragonBones JSON (atlas UVs already remapped in), so it
        // loads with a fast bounds-checked decode and no runtime JSON parsing (the VISION
        // efficiency goal). With no rig asset assigned, fall back to reading the "*_ske.json"
        // source path directly (authoring / back-compat), applying the atlas remap at load.
        if (m_config.m_rigAsset.GetId().IsValid())
        {
            AZ::Data::Asset<DioramaSkinnedRigAsset> rig = AZ::Data::AssetManager::Instance().GetAsset<DioramaSkinnedRigAsset>(
                m_config.m_rigAsset.GetId(), AZ::Data::AssetLoadBehavior::PreLoad);
            rig.BlockUntilLoadComplete();
            if (!rig.IsReady() || rig.Get() == nullptr || !SkinnedRig::Decode(rig.Get()->m_payload, m_document))
            {
                return false;
            }
        }
        else
        {
            const AZStd::string json = LoadTextFile(m_config.m_sourcePath);
            if (json.empty() || !DragonBones::ParseDocument(json, m_document))
            {
                return false;
            }

            // Remap mesh UVs through the companion "*_tex.json" atlas: DragonBones stores mesh UVs
            // normalized to each packed sub-texture, not to the whole page, so without this every
            // mesh samples the entire atlas (art plus transparent gutters). The atlas path is
            // derived from the source path (body_ske.json -> body_tex.json); if it is absent the
            // UVs are left as authored. (The rig-asset path bakes this in at build time instead.)
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
        }

        m_armature = m_config.m_armatureName.empty() ? (m_document.m_armatures.empty() ? nullptr : &m_document.m_armatures.front())
                                                     : DragonBones::FindArmature(m_document, m_config.m_armatureName);
        if (m_armature == nullptr || (m_armature->m_meshes.empty() && m_armature->m_surfaceMeshes.empty()))
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

        // Emit each source triangle with both windings so a billboarded mesh is visible
        // regardless of its authored winding (it always faces the camera, so double-sided is
        // free of artifacts and removes a class of "culled to invisible" surprises).
        const auto doubleWind = [](const AZStd::vector<AZ::u16>& src, AZStd::vector<AZ::u32>& dst)
        {
            dst.reserve(src.size() * 2);
            for (size_t t = 0; t + 2 < src.size(); t += 3)
            {
                const AZ::u32 a = src[t];
                const AZ::u32 b = src[t + 1];
                const AZ::u32 c = src[t + 2];
                dst.push_back(a);
                dst.push_back(b);
                dst.push_back(c);
                dst.push_back(a);
                dst.push_back(c);
                dst.push_back(b);
            }
        };

        // Weighted (skinned) meshes and non-weighted surface meshes are built into separate
        // runtime lists; each is deformed by its own path (bone skinning vs surface warp) and
        // submitted through the same sprite mesh-draw.
        m_meshes.reserve(m_armature->m_meshes.size());
        for (const DragonBones::SkinnedMesh& mesh : m_armature->m_meshes)
        {
            RuntimeMesh runtime;
            runtime.m_source = &mesh;
            runtime.m_skinMatrices.resize(mesh.m_bindWorld.size());
            doubleWind(mesh.m_indices, runtime.m_indices);
            m_meshes.push_back(AZStd::move(runtime));
        }
        m_surfaceMeshes.reserve(m_armature->m_surfaceMeshes.size());
        for (const DragonBones::SurfaceMesh& mesh : m_armature->m_surfaceMeshes)
        {
            SurfaceRuntimeMesh runtime;
            runtime.m_source = &mesh;
            runtime.m_surfaceBoneIndex = mesh.m_surfaceBoneIndex;
            doubleWind(mesh.m_indices, runtime.m_indices);
            m_surfaceMeshes.push_back(AZStd::move(runtime));
        }

        // Per-frame pose scratch, sized once.
        const size_t boneCount = m_bones.size();
        m_boneRotationDelta.assign(boneCount, 0.0f);
        m_boneTranslationDelta.assign(boneCount, AZ::Vector2::CreateZero());
        m_localOverrides.resize(boneCount);
        m_world.resize(boneCount);
        m_surfaceGrids.assign(boneCount, SurfaceDeform::SurfaceGrid{});

        // Bind-pose placement for the bounding box (its center recenters the rig on the entity
        // origin so scale/placement are predictable). Pose the bones at bind and build the
        // bind-pose surface grids so surface meshes are placed in armature space, not their own
        // +/-200 local space.
        for (size_t i = 0; i < boneCount; ++i)
        {
            m_localOverrides[i] = m_bones[i].m_local;
        }
        MeshSkin::ComputeWorldTransforms(
            AZStd::span<const MeshSkin::Bone>(m_bones.data(), boneCount),
            AZStd::span<const MeshSkin::Affine2D>(m_localOverrides.data(), boneCount),
            AZStd::span<MeshSkin::Affine2D>(m_world.data(), boneCount));
        m_activeContribs.clear(); // bind-pose bbox: no animated deform
        BuildSurfaceGrids();

        float minX = AZStd::numeric_limits<float>::max();
        float minY = AZStd::numeric_limits<float>::max();
        float maxX = -AZStd::numeric_limits<float>::max();
        float maxY = -AZStd::numeric_limits<float>::max();
        const auto expand = [&](const AZ::Vector2& p)
        {
            minX = AZ::GetMin(minX, p.GetX());
            minY = AZ::GetMin(minY, p.GetY());
            maxX = AZ::GetMax(maxX, p.GetX());
            maxY = AZ::GetMax(maxY, p.GetY());
        };
        for (const DragonBones::SkinnedMesh& mesh : m_armature->m_meshes)
        {
            for (const MeshSkin::SkinnedVertex& vertex : mesh.m_vertices)
            {
                expand(vertex.m_bindPos);
            }
        }
        for (const SurfaceRuntimeMesh& runtime : m_surfaceMeshes)
        {
            const int sb = runtime.m_surfaceBoneIndex;
            if (sb < 0 || sb >= static_cast<int>(boneCount) || runtime.m_source == nullptr)
            {
                continue;
            }
            const MeshSkin::Affine2D& world = m_world[sb];
            const SurfaceDeform::SurfaceGrid& grid = m_surfaceGrids[sb];
            for (const AZ::Vector2& v : runtime.m_source->m_bindVertices)
            {
                expand(world.TransformPoint(SurfaceDeform::WarpPoint(grid, v)));
            }
        }
        if (minX > maxX) // no vertices at all
        {
            minX = maxX = minY = maxY = 0.0f;
        }
        m_center = AZ::Vector2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);

        m_rigBuilt = true;
        return true;
    }

    void SkinnedSpritePresenter::BuildSurfaceGrids()
    {
        // Bones are parent-first, so a surface's parent grid is already built when we reach it.
        for (size_t i = 0; i < m_bones.size(); ++i)
        {
            const DragonBones::BoneData& bone = m_armature->m_bones[i];
            SurfaceDeform::SurfaceGrid& grid = m_surfaceGrids[i];
            if (!bone.m_isSurface)
            {
                grid.m_segmentX = 0;
                grid.m_segmentY = 0;
                grid.m_controlPoints.clear();
                continue;
            }
            grid.m_segmentX = bone.m_segmentX;
            grid.m_segmentY = bone.m_segmentY;
            grid.m_controlPoints = bone.m_bindControlPoints; // bind, in this surface's local space

            // Add this frame's animated surface-deform deltas for this surface, composed
            // additively onto the bind control points. Driven by m_activeContribs (empty during
            // the bind-pose bbox build, so that pass stays pure bind).
            const int cpCount = static_cast<int>(bone.m_bindControlPoints.size());
            AccumulateSurfaceDeltas(static_cast<int>(i), cpCount, grid.m_controlPoints);

            // Nesting: if the parent bone is also a surface, warp this surface's control points
            // through the parent's already-built grid, moving them from this surface's local
            // space up into the parent chain's space. Recursing this way (parent-first) lands a
            // deeply nested surface's control points in the space of its nearest regular-bone
            // ancestor, whose world transform (m_world) then places the warped mesh correctly.
            const int parent = bone.m_parentIndex;
            if (parent >= 0 && parent < static_cast<int>(m_surfaceGrids.size()) && parent < static_cast<int>(m_armature->m_bones.size()) &&
                m_armature->m_bones[parent].m_isSurface)
            {
                const SurfaceDeform::SurfaceGrid& parentGrid = m_surfaceGrids[parent];
                for (AZ::Vector2& cp : grid.m_controlPoints)
                {
                    cp = SurfaceDeform::WarpPoint(parentGrid, cp);
                }
            }
        }
    }

    void SkinnedSpritePresenter::BuildActiveContributions()
    {
        m_activeContribs.clear();
        if (m_animation == nullptr || m_armature == nullptr || m_armature->m_animations.empty())
        {
            return;
        }
        // m_animation always points into m_armature->m_animations (FindAnimation), so its index
        // is the pointer offset. CollectProgressContributions walks the type-40 tree from there.
        const int rootIndex = static_cast<int>(m_animation - m_armature->m_animations.data());
        DragonBones::CollectProgressContributions(*m_armature, rootIndex, m_animTime, kMaxProgressDepth, m_activeContribs);
    }

    void SkinnedSpritePresenter::AccumulateSurfaceDeltas(int surfaceBoneIndex, int cpCount, AZStd::vector<AZ::Vector2>& target)
    {
        // Every active contribution (the playing clip and each PARAM_* it scrubs) adds its
        // surface-deform deltas for this surface onto the same bind-relative base, so they sum.
        for (const DragonBones::AnimationSample& contrib : m_activeContribs)
        {
            const DragonBones::Animation& anim = m_armature->m_animations[contrib.m_animIndex];
            for (const DragonBones::DeformTimeline& deform : anim.m_deforms)
            {
                if (deform.m_kind != DragonBones::DeformTargetKind::Surface || deform.m_targetIndex != surfaceBoneIndex)
                {
                    continue;
                }
                DragonBones::SampleDeform(deform, contrib.m_time, cpCount, m_deformScratch);
                const size_t n = AZ::GetMin(target.size(), m_deformScratch.size());
                for (size_t k = 0; k < n; ++k)
                {
                    target[k] += m_deformScratch[k] * contrib.m_weight;
                }
            }
        }
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
        for (SurfaceRuntimeMesh& mesh : m_surfaceMeshes)
        {
            if (mesh.m_handle == 0)
            {
                mesh.m_handle = m_featureProcessor->AcquireMesh();
            }
        }
        m_handlesAcquired = true;
        return true;
    }

    void SkinnedSpritePresenter::Tick(float deltaTime)
    {
        // Advance the clip (wrap when looping, hold the end otherwise).
        if (m_playing && m_animation != nullptr && m_animation->m_durationSeconds > 0.0f)
        {
            m_animTime += deltaTime * m_config.m_speed;
            const float duration = m_animation->m_durationSeconds;
            if (m_loop)
            {
                m_animTime = std::fmod(m_animTime, duration);
                if (m_animTime < 0.0f)
                {
                    m_animTime += duration;
                }
            }
            else
            {
                m_animTime = AZ::GetClamp(m_animTime, 0.0f, duration);
            }
        }

        if (!TryAcquireFeatureProcessor())
        {
            return;
        }
        SkinAndPush();
    }

    void SkinnedSpritePresenter::PlayAnimation(const AZStd::string& name, bool looping)
    {
        m_animation = (m_armature != nullptr) ? DragonBones::FindAnimation(*m_armature, name) : nullptr;
        m_animTime = 0.0f;
        m_playing = m_animation != nullptr;
        m_loop = looping;
    }

    void SkinnedSpritePresenter::StopAnimation()
    {
        m_playing = false;
    }

    void SkinnedSpritePresenter::SetAnimationSpeed(float speed)
    {
        m_config.m_speed = speed;
    }

    void SkinnedSpritePresenter::SkinAndPush()
    {
        AZ::Transform worldTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTransform, m_entityId, &AZ::TransformBus::Events::GetWorldTM);

        // Compose the active clip and every PARAM_* it scrubs (the type-40 progress tree, walked
        // once here) into per-bone deltas: translate / rotate / skew add, scale multiplies, over an
        // identity base (so a parameter at its neutral value contributes nothing). Identity when
        // nothing is playing. The surface grids below reuse the same contribution list.
        const int boneCount = static_cast<int>(m_bones.size());
        BuildActiveContributions();
        m_poseScratch.assign(static_cast<size_t>(boneCount), DragonBones::BonePoseDelta{});
        for (const DragonBones::AnimationSample& contrib : m_activeContribs)
        {
            DragonBones::SampleAnimation(m_armature->m_animations[contrib.m_animIndex], contrib.m_time, boneCount, m_poseAccum);
            // The contribution's weight (type-41 envelope / 1D blend) scales its deltas:
            // translate / rotate / skew linearly, scale toward identity ((s - 1) * w + 1,
            // the reference runtime's blended-scale rule).
            const float w = contrib.m_weight;
            const AZ::Vector2 one(1.0f, 1.0f);
            for (int i = 0; i < boneCount; ++i)
            {
                m_poseScratch[i].m_translate += m_poseAccum[i].m_translate * w;
                m_poseScratch[i].m_rotateDegrees += m_poseAccum[i].m_rotateDegrees * w;
                m_poseScratch[i].m_skewDegrees += m_poseAccum[i].m_skewDegrees * w;
                m_poseScratch[i].m_scale *= one + (m_poseAccum[i].m_scale - one) * w;
            }
        }

        // Rebuild each bone's local from its bind COMPONENTS plus the animation delta and any
        // bus override, exactly as DragonBones composes a pose: add to x/y and to skX/skY
        // (rotation is degrees), multiply the scale. Rebuilding from components (rather than
        // composing a rotation onto the bind affine) rotates cleanly about the joint even when
        // the bone has non-uniform bind scale or skew.
        for (int i = 0; i < boneCount; ++i)
        {
            const DragonBones::BoneData& bind = m_armature->m_bones[i];
            const DragonBones::BonePoseDelta& delta = m_poseScratch[i];
            const float x = bind.m_x + delta.m_translate.GetX() + m_boneTranslationDelta[i].GetX();
            const float y = bind.m_y + delta.m_translate.GetY() + m_boneTranslationDelta[i].GetY();
            const float skewX = bind.m_skewXDegrees + delta.m_rotateDegrees + delta.m_skewDegrees + m_boneRotationDelta[i];
            const float skewY = bind.m_skewYDegrees + delta.m_rotateDegrees + m_boneRotationDelta[i];
            const float scaleX = bind.m_scaleX * delta.m_scale.GetX();
            const float scaleY = bind.m_scaleY * delta.m_scale.GetY();
            m_localOverrides[i] = DragonBones::TransformToAffine(x, y, skewX, skewY, scaleX, scaleY);
        }
        MeshSkin::ComputeWorldTransforms(
            AZStd::span<const MeshSkin::Bone>(m_bones.data(), m_bones.size()),
            AZStd::span<const MeshSkin::Affine2D>(m_localOverrides.data(), m_localOverrides.size()),
            AZStd::span<MeshSkin::Affine2D>(m_world.data(), m_world.size()));

        // Rebuild the surface grids for this frame's pose (bind control points + animated
        // deform deltas), so surface meshes warp with the animation.
        BuildSurfaceGrids();

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

        // Surface meshes: apply any per-vertex FFD offset, warp each vertex through its
        // surface's control-point grid, place it by the surface bone's world transform, and
        // submit through the same mesh-draw.
        for (size_t smIndex = 0; smIndex < m_surfaceMeshes.size(); ++smIndex)
        {
            SurfaceRuntimeMesh& runtime = m_surfaceMeshes[smIndex];
            const int sb = runtime.m_surfaceBoneIndex;
            if (runtime.m_handle == 0 || runtime.m_source == nullptr || sb < 0 || sb >= worldCount ||
                sb >= static_cast<int>(m_surfaceGrids.size()))
            {
                continue;
            }
            const DragonBones::SurfaceMesh& source = *runtime.m_source;
            const MeshSkin::Affine2D& surfaceWorld = m_world[sb];
            const SurfaceDeform::SurfaceGrid& grid = m_surfaceGrids[sb];

            // Sample this mesh's FFD deform channel (per-vertex offsets), if any, applied in the
            // mesh's local space BEFORE the surface warp.
            const int vertCount = static_cast<int>(source.m_bindVertices.size());
            bool hasFfd = false;
            if (m_animation != nullptr)
            {
                for (const DragonBones::DeformTimeline& deform : m_animation->m_deforms)
                {
                    if (deform.m_kind == DragonBones::DeformTargetKind::MeshFfd && deform.m_targetIndex == static_cast<int>(smIndex))
                    {
                        DragonBones::SampleDeform(deform, m_animTime, vertCount, m_deformScratch);
                        hasFfd = true;
                        break;
                    }
                }
            }

            m_vertexScratch.clear();
            m_vertexScratch.resize(source.m_bindVertices.size());
            for (size_t v = 0; v < source.m_bindVertices.size(); ++v)
            {
                AZ::Vector2 bind = source.m_bindVertices[v];
                if (hasFfd && v < m_deformScratch.size())
                {
                    bind = SurfaceDeform::ApplyDeform(bind, m_deformScratch[v]);
                }
                const AZ::Vector2 warped = SurfaceDeform::WarpPoint(grid, bind);
                const AZ::Vector2 armature = surfaceWorld.TransformPoint(warped);
                SpriteFeatureProcessor::MeshVertex& out = m_vertexScratch[v];
                out.m_position = AZ::Vector2((armature.GetX() - m_center.GetX()) * scaleX, (armature.GetY() - m_center.GetY()) * scaleY);
                out.m_uv = (v < source.m_uvs.size()) ? source.m_uvs[v] : AZ::Vector2::CreateZero();
            }

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
            m_boneRotationDelta[it->second] = degrees; // added directly to the bone's skX/skY
        }
    }

    void SkinnedSpritePresenter::SetBoneTranslation(const AZStd::string& boneName, float x, float y)
    {
        const auto it = m_boneNameToIndex.find(boneName);
        if (it != m_boneNameToIndex.end())
        {
            m_boneTranslationDelta[it->second] = AZ::Vector2(x, y);
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
        info.m_meshCount = static_cast<int>(m_meshes.size() + m_surfaceMeshes.size());
        for (const RuntimeMesh& mesh : m_meshes)
        {
            if (mesh.m_source != nullptr)
            {
                info.m_vertexCount += static_cast<int>(mesh.m_source->m_vertices.size());
            }
        }
        for (const SurfaceRuntimeMesh& mesh : m_surfaceMeshes)
        {
            if (mesh.m_source != nullptr)
            {
                info.m_vertexCount += static_cast<int>(mesh.m_source->m_bindVertices.size());
            }
        }
        return info;
    }

    void SkinnedSpritePresenter::SaveState(SimState::Writer& writer) const
    {
        // The playing clip is a pointer into the armature's animations; store its index.
        int clipIndex = -1;
        if (m_animation != nullptr && m_armature != nullptr && !m_armature->m_animations.empty())
        {
            clipIndex = static_cast<int>(m_animation - m_armature->m_animations.data());
        }
        writer.S64(clipIndex);
        writer.F32(m_animTime);
        writer.U8(m_playing ? 1 : 0);
        writer.U8(m_loop ? 1 : 0);
        writer.F32(m_config.m_speed);
        // Per-bone pose overrides (SetBoneRotation / SetBoneTranslation): count, then the rotation
        // and translation for each bone.
        const AZ::u32 count = static_cast<AZ::u32>(m_boneRotationDelta.size());
        writer.U32(count);
        for (AZ::u32 i = 0; i < count; ++i)
        {
            writer.F32(m_boneRotationDelta[i]);
            const AZ::Vector2 t = (i < m_boneTranslationDelta.size()) ? m_boneTranslationDelta[i] : AZ::Vector2::CreateZero();
            writer.F32(t.GetX());
            writer.F32(t.GetY());
        }
    }

    bool SkinnedSpritePresenter::RestoreState(SimState::Reader& reader)
    {
        AZ::s64 clipIndex = -1;
        float animTime = 0.0f;
        float speed = 1.0f;
        AZ::u8 playing = 0;
        AZ::u8 loop = 1;
        AZ::u32 count = 0;
        if (!reader.S64(clipIndex) || !reader.F32(animTime) || !reader.U8(playing) || !reader.U8(loop) || !reader.F32(speed) ||
            !reader.U32(count))
        {
            return false;
        }
        // A re-authored rig can change the animation list; an out-of-range index clears the clip
        // rather than indexing past the end.
        if (m_armature != nullptr && clipIndex >= 0 && clipIndex < static_cast<AZ::s64>(m_armature->m_animations.size()))
        {
            m_animation = &m_armature->m_animations[static_cast<size_t>(clipIndex)];
        }
        else
        {
            m_animation = nullptr;
        }
        m_animTime = animTime;
        m_playing = playing != 0;
        m_loop = loop != 0;
        m_config.m_speed = speed;
        for (AZ::u32 i = 0; i < count; ++i)
        {
            float rot = 0.0f;
            float tx = 0.0f;
            float ty = 0.0f;
            if (!reader.F32(rot) || !reader.F32(tx) || !reader.F32(ty))
            {
                return false;
            }
            if (i < m_boneRotationDelta.size())
            {
                m_boneRotationDelta[i] = rot;
            }
            if (i < m_boneTranslationDelta.size())
            {
                m_boneTranslationDelta[i] = AZ::Vector2(tx, ty);
            }
        }
        return true;
    }
} // namespace Diorama
