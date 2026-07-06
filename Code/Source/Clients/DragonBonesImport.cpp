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

        //! Read a bone's { x, y, skX, skY, scX, scY } transform (all optional) into an affine.
        MeshSkin::Affine2D ReadBoneTransform(const rapidjson::Value& bone)
        {
            const auto it = bone.FindMember("transform");
            if (it == bone.MemberEnd() || !it->value.IsObject())
            {
                return MeshSkin::Affine2D::Identity();
            }
            const rapidjson::Value& t = it->value;
            return TransformToAffine(
                GetFloat(t, "x", 0.0f),
                GetFloat(t, "y", 0.0f),
                GetFloat(t, "skX", 0.0f),
                GetFloat(t, "skY", 0.0f),
                GetFloat(t, "scX", 1.0f),
                GetFloat(t, "scY", 1.0f));
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
                bone.m_bindLocal = ReadBoneTransform(boneArray[i]);
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

        //! Parse skin[].slot[].display[] into the armature's skinned meshes. A slot's mesh
        //! display is matched by type "mesh"; only weighted ones are kept. Each mesh's draw
        //! order comes from slotOrder (its slot's index in the armature slot list), so parts
        //! layer back-to-front the way DragonBones renders them.
        void ParseSkins(const rapidjson::Value& skinArray, const AZStd::unordered_map<AZStd::string, int>& slotOrder, Armature& armature)
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
                    for (const rapidjson::Value& display : displaysIt->value.GetArray())
                    {
                        if (!display.IsObject() || GetString(display, "type") != "mesh")
                        {
                            continue;
                        }
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
    } // namespace

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

            // The armature "slot" list is the back-to-front draw order; capture it so each
            // mesh knows how to layer against the others.
            AZStd::unordered_map<AZStd::string, int> slotOrder;
            const auto slotsIt = armatureValue.FindMember("slot");
            if (slotsIt != armatureValue.MemberEnd() && slotsIt->value.IsArray())
            {
                slotOrder = ReadSlotOrder(slotsIt->value);
            }

            const auto skinsIt = armatureValue.FindMember("skin");
            if (skinsIt != armatureValue.MemberEnd() && skinsIt->value.IsArray())
            {
                ParseSkins(skinsIt->value, slotOrder, armature);
            }

            out.m_armatures.push_back(AZStd::move(armature));
        }

        return true;
    }
} // namespace Diorama::DragonBones
