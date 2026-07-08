/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DragonBonesImport.h>

#include <AzCore/JSON/document.h>
#include <AzCore/std/containers/unordered_map.h>

namespace Diorama::DragonBones
{
    namespace
    {
        float GetFloat(const rapidjson::Value& obj, const char* key, float fallback)
        {
            const auto it = obj.FindMember(key);
            if (it != obj.MemberEnd() && it->value.IsNumber())
            {
                return static_cast<float>(it->value.GetDouble());
            }
            return fallback;
        }

        AZStd::string GetString(const rapidjson::Value& obj, const char* key)
        {
            const auto it = obj.FindMember(key);
            if (it != obj.MemberEnd() && it->value.IsString())
            {
                return it->value.GetString();
            }
            return AZStd::string();
        }

        //! Read one number from a JSON array element (0 if not a number / out of range).
        float ArrayFloat(const rapidjson::Value& array, rapidjson::SizeType index)
        {
            if (index < array.Size() && array[index].IsNumber())
            {
                return static_cast<float>(array[index].GetDouble());
            }
            return 0.0f;
        }

        //! A DragonBones 6-tuple [a,b,c,d,tx,ty] as an Affine2D; identity if malformed.
        MeshSkin::Affine2D ReadAffine6(const rapidjson::Value& array)
        {
            if (!array.IsArray() || array.Size() < 6)
            {
                return MeshSkin::Affine2D::Identity();
            }
            MeshSkin::Affine2D out;
            out.m_a = ArrayFloat(array, 0);
            out.m_b = ArrayFloat(array, 1);
            out.m_c = ArrayFloat(array, 2);
            out.m_d = ArrayFloat(array, 3);
            out.m_tx = ArrayFloat(array, 4);
            out.m_ty = ArrayFloat(array, 5);
            return out;
        }

        //! Read a bone's { x, y, skX, skY, scX, scY } transform (all optional) into the bone's
        //! components and compute its composed bind-local affine.
        void ReadBoneTransform(const rapidjson::Value& bone, BoneData& out)
        {
            const auto it = bone.FindMember("transform");
            if (it != bone.MemberEnd() && it->value.IsObject())
            {
                const rapidjson::Value& t = it->value;
                out.m_x = GetFloat(t, "x", 0.0f);
                out.m_y = GetFloat(t, "y", 0.0f);
                out.m_skewXDegrees = GetFloat(t, "skX", 0.0f);
                out.m_skewYDegrees = GetFloat(t, "skY", 0.0f);
                out.m_scaleX = GetFloat(t, "scX", 1.0f);
                out.m_scaleY = GetFloat(t, "scY", 1.0f);
            }
            out.m_bindLocal = TransformToAffine(out.m_x, out.m_y, out.m_skewXDegrees, out.m_skewYDegrees, out.m_scaleX, out.m_scaleY);
        }

        //! Read a DragonBones "surface" bone: its segmentX/segmentY grid dimensions and its flat
        //! vertices[] (x,y pairs) as bind-pose control points. A surface has no transform block;
        //! its bind-local stays identity and it warps meshes parented to it (SurfaceDeform).
        void ReadSurfaceBone(const rapidjson::Value& bone, BoneData& out)
        {
            out.m_isSurface = true;
            out.m_segmentX = static_cast<int>(GetFloat(bone, "segmentX", 0.0f));
            out.m_segmentY = static_cast<int>(GetFloat(bone, "segmentY", 0.0f));
            out.m_bindLocal = MeshSkin::Affine2D::Identity();

            const auto vertsIt = bone.FindMember("vertices");
            if (vertsIt != bone.MemberEnd() && vertsIt->value.IsArray())
            {
                const rapidjson::Value& verts = vertsIt->value;
                const rapidjson::SizeType pairs = verts.Size() / 2;
                out.m_bindControlPoints.reserve(pairs);
                for (rapidjson::SizeType v = 0; v < pairs; ++v)
                {
                    out.m_bindControlPoints.push_back(AZ::Vector2(ArrayFloat(verts, v * 2), ArrayFloat(verts, v * 2 + 1)));
                }
            }
        }

        //! Parse the bone list (two passes: collect bones + their parent names, then resolve
        //! parents by name so a forward parent reference still resolves). Parent names are
        //! captured in the first pass parallel to the bones vector so any skipped non-object
        //! entries can never misalign the resolution. Sets outInOrder false if any bone's
        //! parent does not precede it.
        void ParseBones(const rapidjson::Value& boneArray, Armature& armature, bool& outInOrder)
        {
            AZStd::unordered_map<AZStd::string, int> nameToIndex;
            AZStd::vector<AZStd::string> parentNames;
            const rapidjson::SizeType count = boneArray.Size();
            armature.m_bones.reserve(count);
            parentNames.reserve(count);
            for (rapidjson::SizeType i = 0; i < count; ++i)
            {
                if (!boneArray[i].IsObject())
                {
                    continue;
                }
                BoneData bone;
                bone.m_name = GetString(boneArray[i], "name");
                if (GetString(boneArray[i], "type") == "surface")
                {
                    ReadSurfaceBone(boneArray[i], bone);
                }
                else
                {
                    ReadBoneTransform(boneArray[i], bone);
                }
                nameToIndex[bone.m_name] = static_cast<int>(armature.m_bones.size());
                armature.m_bones.push_back(AZStd::move(bone));
                parentNames.push_back(GetString(boneArray[i], "parent"));
            }
            for (size_t i = 0; i < armature.m_bones.size(); ++i)
            {
                const AZStd::string& parentName = parentNames[i];
                const auto found = nameToIndex.find(parentName);
                if (!parentName.empty() && found != nameToIndex.end())
                {
                    armature.m_bones[i].m_parentIndex = found->second;
                    if (found->second >= static_cast<int>(i))
                    {
                        outInOrder = false;
                    }
                }
            }
        }

        //! Parse one mesh display's weights[] into per-vertex influences, remapping each
        //! GLOBAL bone index to the compact local slot given by globalToLocal. Malformed
        //! runs are clamped so a bad count can never read past the array.
        void ParseWeights(
            const rapidjson::Value& weights,
            const AZStd::unordered_map<int, int>& globalToLocal,
            AZStd::vector<MeshSkin::SkinnedVertex>& vertices)
        {
            const rapidjson::SizeType size = weights.Size();
            rapidjson::SizeType cursor = 0;
            size_t vertexIndex = 0;
            while (cursor < size && vertexIndex < vertices.size())
            {
                const int influenceCount = static_cast<int>(ArrayFloat(weights, cursor));
                ++cursor;
                MeshSkin::SkinnedVertex& vertex = vertices[vertexIndex];
                vertex.m_influenceCount = 0;
                for (int n = 0; n < influenceCount && cursor + 1 < size; ++n, cursor += 2)
                {
                    const int global = static_cast<int>(ArrayFloat(weights, cursor));
                    const float weight = ArrayFloat(weights, cursor + 1);
                    const auto found = globalToLocal.find(global);
                    if (found == globalToLocal.end() || vertex.m_influenceCount >= static_cast<int>(MeshSkin::MaxInfluences))
                    {
                        continue;
                    }
                    MeshSkin::Influence& influence = vertex.m_influences[vertex.m_influenceCount];
                    influence.m_boneIndex = found->second;
                    influence.m_weight = weight;
                    ++vertex.m_influenceCount;
                }
                ++vertexIndex;
            }
        }

        //! Parse one skinned mesh display (must have weights). Returns false if it is not a
        //! weighted mesh (rigid displays are skipped by the caller).
        bool ParseMeshDisplay(const rapidjson::Value& display, AZStd::string_view slotName, SkinnedMesh& out)
        {
            const auto weightsIt = display.FindMember("weights");
            const auto vertsIt = display.FindMember("vertices");
            const auto bonePoseIt = display.FindMember("bonePose");
            if (weightsIt == display.MemberEnd() || !weightsIt->value.IsArray() || vertsIt == display.MemberEnd() ||
                !vertsIt->value.IsArray() || bonePoseIt == display.MemberEnd() || !bonePoseIt->value.IsArray())
            {
                return false;
            }

            out.m_slotName = slotName;
            out.m_displayName = GetString(display, "name");

            // bonePose: flat 7-tuples [globalBoneIndex, a,b,c,d,tx,ty]. Its order defines the
            // per-mesh local slots, and it is the authoritative per-bone bind world.
            AZStd::unordered_map<int, int> globalToLocal;
            const rapidjson::Value& bonePose = bonePoseIt->value;
            for (rapidjson::SizeType i = 0; i + 6 < bonePose.Size(); i += 7)
            {
                const int global = static_cast<int>(ArrayFloat(bonePose, i));
                MeshSkin::Affine2D bind;
                bind.m_a = ArrayFloat(bonePose, i + 1);
                bind.m_b = ArrayFloat(bonePose, i + 2);
                bind.m_c = ArrayFloat(bonePose, i + 3);
                bind.m_d = ArrayFloat(bonePose, i + 4);
                bind.m_tx = ArrayFloat(bonePose, i + 5);
                bind.m_ty = ArrayFloat(bonePose, i + 6);
                globalToLocal[global] = static_cast<int>(out.m_boneGlobalIndices.size());
                out.m_boneGlobalIndices.push_back(global);
                out.m_bindWorld.push_back(bind);
            }

            // slotPose maps slot-space vertices into armature space; usually identity.
            MeshSkin::Affine2D slotPose = MeshSkin::Affine2D::Identity();
            const auto slotPoseIt = display.FindMember("slotPose");
            if (slotPoseIt != display.MemberEnd() && slotPoseIt->value.IsArray())
            {
                slotPose = ReadAffine6(slotPoseIt->value);
            }

            // vertices: flat x,y pairs in slot space -> baked into armature-space bind pos.
            const rapidjson::Value& verts = vertsIt->value;
            const size_t vertexCount = verts.Size() / 2;
            out.m_vertices.resize(vertexCount);
            for (size_t v = 0; v < vertexCount; ++v)
            {
                const AZ::Vector2 raw(
                    ArrayFloat(verts, static_cast<rapidjson::SizeType>(v * 2)),
                    ArrayFloat(verts, static_cast<rapidjson::SizeType>(v * 2 + 1)));
                out.m_vertices[v].m_bindPos = slotPose.TransformPoint(raw);
            }

            // uvs: flat u,v pairs, parallel to vertices.
            const auto uvsIt = display.FindMember("uvs");
            if (uvsIt != display.MemberEnd() && uvsIt->value.IsArray())
            {
                const rapidjson::Value& uvs = uvsIt->value;
                for (size_t v = 0; v < vertexCount; ++v)
                {
                    out.m_vertices[v].m_uv = AZ::Vector2(
                        ArrayFloat(uvs, static_cast<rapidjson::SizeType>(v * 2)),
                        ArrayFloat(uvs, static_cast<rapidjson::SizeType>(v * 2 + 1)));
                }
            }

            ParseWeights(weightsIt->value, globalToLocal, out.m_vertices);

            // triangles: flat index list (3 per triangle).
            const auto trisIt = display.FindMember("triangles");
            if (trisIt != display.MemberEnd() && trisIt->value.IsArray())
            {
                const rapidjson::Value& tris = trisIt->value;
                out.m_indices.reserve(tris.Size());
                for (rapidjson::SizeType i = 0; i < tris.Size(); ++i)
                {
                    out.m_indices.push_back(static_cast<AZ::u16>(ArrayFloat(tris, i)));
                }
            }

            return true;
        }

        //! Parse a non-weighted mesh display bound to a surface bone: flat vertices[] (x,y in
        //! the surface's local space), uvs[], and triangles[]. No weights or bonePose. Returns
        //! false only if it has no vertices.
        bool ParseSurfaceMeshDisplay(const rapidjson::Value& display, AZStd::string_view slotName, int surfaceBoneIndex, SurfaceMesh& out)
        {
            const auto vertsIt = display.FindMember("vertices");
            if (vertsIt == display.MemberEnd() || !vertsIt->value.IsArray())
            {
                return false;
            }
            out.m_slotName = slotName;
            out.m_displayName = GetString(display, "name");
            out.m_surfaceBoneIndex = surfaceBoneIndex;

            const rapidjson::Value& verts = vertsIt->value;
            const rapidjson::SizeType vertPairs = verts.Size() / 2;
            out.m_bindVertices.reserve(vertPairs);
            for (rapidjson::SizeType v = 0; v < vertPairs; ++v)
            {
                out.m_bindVertices.push_back(AZ::Vector2(ArrayFloat(verts, v * 2), ArrayFloat(verts, v * 2 + 1)));
            }

            const auto uvsIt = display.FindMember("uvs");
            if (uvsIt != display.MemberEnd() && uvsIt->value.IsArray())
            {
                const rapidjson::Value& uvs = uvsIt->value;
                const rapidjson::SizeType uvPairs = uvs.Size() / 2;
                out.m_uvs.reserve(uvPairs);
                for (rapidjson::SizeType v = 0; v < uvPairs; ++v)
                {
                    out.m_uvs.push_back(AZ::Vector2(ArrayFloat(uvs, v * 2), ArrayFloat(uvs, v * 2 + 1)));
                }
            }

            const auto trisIt = display.FindMember("triangles");
            if (trisIt != display.MemberEnd() && trisIt->value.IsArray())
            {
                const rapidjson::Value& tris = trisIt->value;
                out.m_indices.reserve(tris.Size());
                for (rapidjson::SizeType i = 0; i < tris.Size(); ++i)
                {
                    out.m_indices.push_back(static_cast<AZ::u16>(ArrayFloat(tris, i)));
                }
            }
            return true;
        }

        //! Parse skin[].slot[].display[] into the armature's meshes. A slot whose parent bone is
        //! a "surface" produces non-weighted SurfaceMeshes (warped by that surface); any other
        //! slot produces weighted SkinnedMeshes (rigid displays are skipped). Each mesh's draw
        //! order comes from slotOrder (its slot's index in the armature slot list), so parts
        //! layer back-to-front the way DragonBones renders them.
        void ParseSkins(
            const rapidjson::Value& skinArray,
            const AZStd::unordered_map<AZStd::string, int>& slotOrder,
            const AZStd::unordered_map<AZStd::string, AZStd::string>& slotParent,
            const AZStd::unordered_map<AZStd::string, int>& nameToIndex,
            Armature& armature)
        {
            for (rapidjson::SizeType s = 0; s < skinArray.Size(); ++s)
            {
                const auto slotsIt = skinArray[s].FindMember("slot");
                if (slotsIt == skinArray[s].MemberEnd() || !slotsIt->value.IsArray())
                {
                    continue;
                }
                for (const rapidjson::Value& slot : slotsIt->value.GetArray())
                {
                    const AZStd::string slotName = GetString(slot, "name");
                    const auto displaysIt = slot.FindMember("display");
                    if (displaysIt == slot.MemberEnd() || !displaysIt->value.IsArray())
                    {
                        continue;
                    }
                    const auto orderIt = slotOrder.find(slotName);
                    const int drawOrder = (orderIt != slotOrder.end()) ? orderIt->second : 0;

                    // A slot whose parent bone is a surface is warped by it: its mesh displays are
                    // non-weighted surface meshes rather than skinned meshes.
                    int surfaceBoneIndex = -1;
                    const auto parentIt = slotParent.find(slotName);
                    if (parentIt != slotParent.end())
                    {
                        const auto boneIt = nameToIndex.find(parentIt->second);
                        if (boneIt != nameToIndex.end() && boneIt->second >= 0 &&
                            boneIt->second < static_cast<int>(armature.m_bones.size()) && armature.m_bones[boneIt->second].m_isSurface)
                        {
                            surfaceBoneIndex = boneIt->second;
                        }
                    }

                    for (const rapidjson::Value& display : displaysIt->value.GetArray())
                    {
                        if (!display.IsObject() || GetString(display, "type") != "mesh")
                        {
                            continue;
                        }
                        if (surfaceBoneIndex >= 0)
                        {
                            SurfaceMesh surfaceMesh;
                            if (ParseSurfaceMeshDisplay(display, slotName, surfaceBoneIndex, surfaceMesh))
                            {
                                surfaceMesh.m_drawOrder = drawOrder;
                                armature.m_surfaceMeshes.push_back(AZStd::move(surfaceMesh));
                            }
                        }
                        else
                        {
                            SkinnedMesh mesh;
                            if (ParseMeshDisplay(display, slotName, mesh))
                            {
                                mesh.m_drawOrder = drawOrder;
                                armature.m_meshes.push_back(AZStd::move(mesh));
                            }
                        }
                    }
                }
            }
        }

        //! Build slotName -> draw order (its index in the armature "slot" list, which
        //! DragonBones orders back-to-front).
        AZStd::unordered_map<AZStd::string, int> ReadSlotOrder(const rapidjson::Value& slotArray)
        {
            AZStd::unordered_map<AZStd::string, int> slotOrder;
            for (rapidjson::SizeType i = 0; i < slotArray.Size(); ++i)
            {
                if (slotArray[i].IsObject())
                {
                    slotOrder[GetString(slotArray[i], "name")] = static_cast<int>(i);
                }
            }
            return slotOrder;
        }

        //! Build slotName -> parent bone name, so a slot bound to a "surface" bone can be
        //! detected (its meshes are warped rather than skinned).
        AZStd::unordered_map<AZStd::string, AZStd::string> ReadSlotParents(const rapidjson::Value& slotArray)
        {
            AZStd::unordered_map<AZStd::string, AZStd::string> parents;
            for (rapidjson::SizeType i = 0; i < slotArray.Size(); ++i)
            {
                if (slotArray[i].IsObject())
                {
                    parents[GetString(slotArray[i], "name")] = GetString(slotArray[i], "parent");
                }
            }
            return parents;
        }

        //! Parse one channel's frame array (translate/rotate/scale) into keyframes. Frame
        //! durations are in frames (converted to seconds via frameRate) and are cumulative.
        //! keyX/keyY name the channel's value fields; defaultValue is 0 (translate/rotate) or
        //! 1 (scale) so a missing field means "no change". A "curve" array selects bezier
        //! easing, a present "tweenEasing" selects linear, and neither selects stepped/hold.
        void ParseFrameTrack(
            const rapidjson::Value& frames,
            float frameRate,
            const char* keyX,
            const char* keyY,
            float defaultValue,
            AZStd::vector<Keyframe>& out)
        {
            const float fps = frameRate > 0.0f ? frameRate : 30.0f;
            float startFrames = 0.0f;
            for (const rapidjson::Value& frame : frames.GetArray())
            {
                if (!frame.IsObject())
                {
                    continue;
                }
                Keyframe kf;
                const float durationFrames = GetFloat(frame, "duration", 0.0f);
                kf.m_startTime = startFrames / fps;
                kf.m_duration = durationFrames / fps;
                kf.m_value = AZ::Vector2(GetFloat(frame, keyX, defaultValue), GetFloat(frame, keyY, defaultValue));

                const auto curveIt = frame.FindMember("curve");
                if (curveIt != frame.MemberEnd() && curveIt->value.IsArray() && curveIt->value.Size() >= 4)
                {
                    kf.m_tween = TweenType::Curve;
                    for (int k = 0; k < 4; ++k)
                    {
                        kf.m_curve[k] = ArrayFloat(curveIt->value, static_cast<rapidjson::SizeType>(k));
                    }
                }
                else if (frame.HasMember("tweenEasing"))
                {
                    kf.m_tween = TweenType::Linear;
                }
                else
                {
                    kf.m_tween = TweenType::Stepped;
                }

                out.push_back(kf);
                startFrames += durationFrames;
            }
        }

        //! Parse a deform channel's frame array (FFD mesh deform or surface control-point
        //! deform). Frame durations are cumulative frames -> seconds. The delta run is keyed
        //! "value" (modern typed timeline) or "vertices" (legacy ffd); "offset" is its flat
        //! float start index. Tween follows the same curve/tweenEasing/stepped convention.
        void ParseDeformFrames(const rapidjson::Value& frames, float frameRate, AZStd::vector<DeformFrame>& out)
        {
            const float fps = frameRate > 0.0f ? frameRate : 30.0f;
            float startFrames = 0.0f;
            for (const rapidjson::Value& frame : frames.GetArray())
            {
                if (!frame.IsObject())
                {
                    continue;
                }
                DeformFrame df;
                const float durationFrames = GetFloat(frame, "duration", 0.0f);
                df.m_startTime = startFrames / fps;
                df.m_duration = durationFrames / fps;
                df.m_offset = static_cast<int>(GetFloat(frame, "offset", 0.0f));

                auto valIt = frame.FindMember("value");
                if (valIt == frame.MemberEnd() || !valIt->value.IsArray())
                {
                    valIt = frame.FindMember("vertices");
                }
                if (valIt != frame.MemberEnd() && valIt->value.IsArray())
                {
                    const rapidjson::Value& arr = valIt->value;
                    df.m_rawDeltas.reserve(arr.Size());
                    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
                    {
                        df.m_rawDeltas.push_back(ArrayFloat(arr, i));
                    }
                }

                const auto curveIt = frame.FindMember("curve");
                if (curveIt != frame.MemberEnd() && curveIt->value.IsArray() && curveIt->value.Size() >= 4)
                {
                    df.m_tween = TweenType::Curve;
                    for (int k = 0; k < 4; ++k)
                    {
                        df.m_curve[k] = ArrayFloat(curveIt->value, static_cast<rapidjson::SizeType>(k));
                    }
                }
                else if (frame.HasMember("tweenEasing"))
                {
                    df.m_tween = TweenType::Linear;
                }
                else
                {
                    df.m_tween = TweenType::Stepped;
                }

                out.push_back(AZStd::move(df));
                startFrames += durationFrames;
            }
        }

        //! Parse an animation's deform channels into the clip: the legacy ffd[] array (mesh
        //! FFD) and the modern typed timeline[] entries of type 22 (SlotDeform / mesh FFD) or
        //! 50 (Surface). Target indices are resolved later, once meshes and surfaces exist.
        void ParseDeforms(const rapidjson::Value& animValue, float fps, Animation& animation)
        {
            const auto ffdIt = animValue.FindMember("ffd");
            if (ffdIt != animValue.MemberEnd() && ffdIt->value.IsArray())
            {
                for (const rapidjson::Value& ffd : ffdIt->value.GetArray())
                {
                    if (!ffd.IsObject())
                    {
                        continue;
                    }
                    DeformTimeline dt;
                    dt.m_targetName = GetString(ffd, "name");
                    dt.m_kind = DeformTargetKind::MeshFfd;
                    const auto framesIt = ffd.FindMember("frame");
                    if (framesIt != ffd.MemberEnd() && framesIt->value.IsArray())
                    {
                        ParseDeformFrames(framesIt->value, fps, dt.m_frames);
                    }
                    animation.m_deforms.push_back(AZStd::move(dt));
                }
            }

            const auto timelineIt = animValue.FindMember("timeline");
            if (timelineIt != animValue.MemberEnd() && timelineIt->value.IsArray())
            {
                for (const rapidjson::Value& tl : timelineIt->value.GetArray())
                {
                    if (!tl.IsObject())
                    {
                        continue;
                    }
                    const int type = static_cast<int>(GetFloat(tl, "type", -1.0f));
                    if (type == 40) // AnimationProgress: drives a named sub-animation's progress
                    {
                        ProgressTimeline pt;
                        pt.m_targetName = GetString(tl, "name");
                        const auto framesIt = tl.FindMember("frame");
                        if (framesIt != tl.MemberEnd() && framesIt->value.IsArray())
                        {
                            ParseFrameTrack(framesIt->value, fps, "value", "value", 0.0f, pt.m_values);
                        }
                        animation.m_progress.push_back(AZStd::move(pt));
                        continue;
                    }
                    if (type != 22 && type != 50) // 22 = SlotDeform (mesh FFD), 50 = Surface
                    {
                        continue;
                    }
                    DeformTimeline dt;
                    dt.m_targetName = GetString(tl, "name");
                    dt.m_kind = (type == 50) ? DeformTargetKind::Surface : DeformTargetKind::MeshFfd;
                    const auto framesIt = tl.FindMember("frame");
                    if (framesIt != tl.MemberEnd() && framesIt->value.IsArray())
                    {
                        ParseDeformFrames(framesIt->value, fps, dt.m_frames);
                    }
                    animation.m_deforms.push_back(AZStd::move(dt));
                }
            }
        }

        //! Resolve each deform timeline's target name to an index: a Surface target to its
        //! surface bone, a MeshFfd target to a surface mesh (by display name). Skinned/rigid
        //! mesh FFD is out of scope, so those stay unresolved (m_targetIndex == -1).
        void ResolveDeformTargets(Armature& armature)
        {
            for (Animation& animation : armature.m_animations)
            {
                for (DeformTimeline& dt : animation.m_deforms)
                {
                    if (dt.m_kind == DeformTargetKind::Surface)
                    {
                        for (size_t i = 0; i < armature.m_bones.size(); ++i)
                        {
                            if (armature.m_bones[i].m_isSurface && armature.m_bones[i].m_name == dt.m_targetName)
                            {
                                dt.m_targetIndex = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < armature.m_surfaceMeshes.size(); ++i)
                        {
                            if (armature.m_surfaceMeshes[i].m_displayName == dt.m_targetName)
                            {
                                dt.m_targetIndex = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }

                // Resolve AnimationProgress targets to the sub-animation they drive, by name.
                for (ProgressTimeline& pt : animation.m_progress)
                {
                    for (size_t i = 0; i < armature.m_animations.size(); ++i)
                    {
                        if (armature.m_animations[i].m_name == pt.m_targetName)
                        {
                            pt.m_targetIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }
        }

        //! Parse the armature's animation[] into clips, resolving each bone timeline's name to
        //! a bone index via nameToIndex.
        void ParseAnimations(
            const rapidjson::Value& animationArray,
            float frameRate,
            const AZStd::unordered_map<AZStd::string, int>& nameToIndex,
            Armature& armature)
        {
            const float fps = frameRate > 0.0f ? frameRate : 30.0f;
            for (const rapidjson::Value& animValue : animationArray.GetArray())
            {
                if (!animValue.IsObject())
                {
                    continue;
                }
                Animation animation;
                animation.m_name = GetString(animValue, "name");
                animation.m_durationSeconds = GetFloat(animValue, "duration", 0.0f) / fps;
                animation.m_loop = static_cast<int>(GetFloat(animValue, "playTimes", 0.0f)) == 0;

                const auto bonesIt = animValue.FindMember("bone");
                if (bonesIt != animValue.MemberEnd() && bonesIt->value.IsArray())
                {
                    for (const rapidjson::Value& boneValue : bonesIt->value.GetArray())
                    {
                        if (!boneValue.IsObject())
                        {
                            continue;
                        }
                        BoneTimeline timeline;
                        timeline.m_boneName = GetString(boneValue, "name");
                        const auto found = nameToIndex.find(timeline.m_boneName);
                        timeline.m_boneIndex = (found != nameToIndex.end()) ? found->second : -1;

                        const auto tf = boneValue.FindMember("translateFrame");
                        if (tf != boneValue.MemberEnd() && tf->value.IsArray())
                        {
                            ParseFrameTrack(tf->value, fps, "x", "y", 0.0f, timeline.m_translate);
                        }
                        const auto rf = boneValue.FindMember("rotateFrame");
                        if (rf != boneValue.MemberEnd() && rf->value.IsArray())
                        {
                            ParseFrameTrack(rf->value, fps, "rotate", "skew", 0.0f, timeline.m_rotate);
                        }
                        const auto sf = boneValue.FindMember("scaleFrame");
                        if (sf != boneValue.MemberEnd() && sf->value.IsArray())
                        {
                            ParseFrameTrack(sf->value, fps, "x", "y", 1.0f, timeline.m_scale);
                        }
                        animation.m_bones.push_back(AZStd::move(timeline));
                    }
                }
                ParseDeforms(animValue, fps, animation);
                armature.m_animations.push_back(AZStd::move(animation));
            }
        }
    } // namespace

    void SampleAnimation(const Animation& animation, float timeSeconds, int boneCount, AZStd::vector<BonePoseDelta>& out)
    {
        out.assign(boneCount < 0 ? 0 : static_cast<size_t>(boneCount), BonePoseDelta{});
        for (const BoneTimeline& timeline : animation.m_bones)
        {
            if (timeline.m_boneIndex < 0 || timeline.m_boneIndex >= boneCount)
            {
                continue;
            }
            BonePoseDelta& delta = out[timeline.m_boneIndex];
            delta.m_translate = SampleTrack(timeline.m_translate, timeSeconds, AZ::Vector2::CreateZero());
            const AZ::Vector2 rotate = SampleTrack(timeline.m_rotate, timeSeconds, AZ::Vector2::CreateZero());
            delta.m_rotateDegrees = rotate.GetX();
            delta.m_skewDegrees = rotate.GetY();
            delta.m_scale = SampleTrack(timeline.m_scale, timeSeconds, AZ::Vector2(1.0f, 1.0f));
        }
    }

    bool ParseAtlas(AZStd::string_view json, Atlas& out)
    {
        out = Atlas();

        rapidjson::Document doc;
        doc.Parse(json.data(), json.size());
        if (doc.HasParseError() || !doc.IsObject())
        {
            return false;
        }

        out.m_width = GetFloat(doc, "width", 0.0f);
        out.m_height = GetFloat(doc, "height", 0.0f);

        const auto subsIt = doc.FindMember("SubTexture");
        if (subsIt != doc.MemberEnd() && subsIt->value.IsArray())
        {
            for (const rapidjson::Value& sub : subsIt->value.GetArray())
            {
                if (!sub.IsObject())
                {
                    continue;
                }
                SubTexture subTexture;
                subTexture.m_name = GetString(sub, "name");
                subTexture.m_x = GetFloat(sub, "x", 0.0f);
                subTexture.m_y = GetFloat(sub, "y", 0.0f);
                subTexture.m_width = GetFloat(sub, "width", 0.0f);
                subTexture.m_height = GetFloat(sub, "height", 0.0f);
                out.m_subTextures.push_back(AZStd::move(subTexture));
            }
        }
        return true;
    }

    void ApplyAtlasUVs(Document& document, const Atlas& atlas)
    {
        if (atlas.m_width <= 0.0f || atlas.m_height <= 0.0f)
        {
            return;
        }
        for (Armature& armature : document.m_armatures)
        {
            for (SkinnedMesh& mesh : armature.m_meshes)
            {
                const SubTexture* sub = FindSubTexture(atlas, mesh.m_displayName);
                if (sub == nullptr)
                {
                    continue;
                }
                for (MeshSkin::SkinnedVertex& vertex : mesh.m_vertices)
                {
                    vertex.m_uv = RemapAtlasUV(vertex.m_uv, *sub, atlas.m_width, atlas.m_height);
                }
            }
        }
    }

    bool ParseDocument(AZStd::string_view json, Document& out)
    {
        out = Document();

        rapidjson::Document doc;
        doc.Parse(json.data(), json.size());
        if (doc.HasParseError() || !doc.IsObject())
        {
            return false;
        }

        out.m_name = GetString(doc, "name");
        const float documentFrameRate = GetFloat(doc, "frameRate", 30.0f);

        const auto armaturesIt = doc.FindMember("armature");
        if (armaturesIt == doc.MemberEnd() || !armaturesIt->value.IsArray())
        {
            return true; // valid JSON, no armatures
        }

        for (const rapidjson::Value& armatureValue : armaturesIt->value.GetArray())
        {
            if (!armatureValue.IsObject())
            {
                continue;
            }
            Armature armature;
            armature.m_name = GetString(armatureValue, "name");
            armature.m_frameRate = GetFloat(armatureValue, "frameRate", documentFrameRate);

            const auto bonesIt = armatureValue.FindMember("bone");
            if (bonesIt != armatureValue.MemberEnd() && bonesIt->value.IsArray())
            {
                ParseBones(bonesIt->value, armature, out.m_bonesInOrder);
            }

            // Bone name -> index, used to resolve slot surface parents and animation targets.
            AZStd::unordered_map<AZStd::string, int> nameToIndex;
            for (size_t i = 0; i < armature.m_bones.size(); ++i)
            {
                nameToIndex[armature.m_bones[i].m_name] = static_cast<int>(i);
            }

            // The armature "slot" list is the back-to-front draw order and each slot's parent
            // bone; capture both so meshes layer correctly and surface-bound slots are detected.
            AZStd::unordered_map<AZStd::string, int> slotOrder;
            AZStd::unordered_map<AZStd::string, AZStd::string> slotParent;
            const auto slotsIt = armatureValue.FindMember("slot");
            if (slotsIt != armatureValue.MemberEnd() && slotsIt->value.IsArray())
            {
                slotOrder = ReadSlotOrder(slotsIt->value);
                slotParent = ReadSlotParents(slotsIt->value);
            }

            const auto skinsIt = armatureValue.FindMember("skin");
            if (skinsIt != armatureValue.MemberEnd() && skinsIt->value.IsArray())
            {
                ParseSkins(skinsIt->value, slotOrder, slotParent, nameToIndex, armature);
            }

            const auto animsIt = armatureValue.FindMember("animation");
            if (animsIt != armatureValue.MemberEnd() && animsIt->value.IsArray())
            {
                ParseAnimations(animsIt->value, armature.m_frameRate, nameToIndex, armature);
            }

            // Resolve deform targets now that bones, surface meshes, and animations all exist.
            ResolveDeformTargets(armature);

            out.m_armatures.push_back(AZStd::move(armature));
        }

        return true;
    }
} // namespace Diorama::DragonBones
