/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/MeshSkin.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <cmath>

// Core for importing DragonBones "*_ske.json" armature data (the open, Apache-2.0
// 2D skeletal format; Spine is excluded for its proprietary runtime license). This
// phase decodes the WEIGHTED-MESH family: a bone hierarchy plus meshes whose vertices
// are skinned to those bones, which is exactly what MeshSkin.h deforms. The result is
// a data model that drops straight onto MeshSkin's SkinnedVertex / Affine2D, so the
// runtime component (a later phase) only has to pose the bones and CPU-skin.
//
// The data model here and the transform decode are pure and unit tested; the only
// engine-touching part is ParseDocument, which reads the JSON with AzCore's rapidjson
// (split into DragonBonesImport.cpp so this header stays cheap to include), the same
// split as AsepriteImport.h / .cpp.
//
// Decoded from the DragonBones 5.5 format (github.com/DragonBones/Tools) and validated
// against the open you_xin/body sample (DragonBonesJS, Apache-2.0):
//  - bone.transform = { x, y, skX, skY (rotation/skew, degrees), scX, scY (default 1) };
//    a bone's local matrix is a=cos(skY)*scX, b=sin(skY)*scX, c=-sin(skX)*scY,
//    d=cos(skX)*scY, tx=x, ty=y. Bones are listed parent-first.
//  - a skinned mesh display carries flat vertices[] (x,y pairs, slot space), uvs[],
//    triangles[] (indices), weights[], slotPose[6], bonePose[].
//  - weights = [count, (GLOBAL_boneIndex, weight) x count, ...] per vertex. The bone
//    index is GLOBAL (into the armature bone list); we remap it to a compact per-mesh
//    slot so MeshSkin's dense skinMatrices array stays small.
//  - bonePose = flat 7-tuples [GLOBAL_boneIndex, a, b, c, d, tx, ty]: each used bone's
//    BIND-pose world matrix. This is authoritative and NOT derivable from the bone
//    hierarchy (a mesh's bind pose can differ from the default skeleton pose), so it is
//    stored per mesh and used directly as MeshSkin's bindWorld.
//  - slotPose[6] = [a,b,c,d,tx,ty] maps a vertex from slot space into armature space
//    (usually identity); we bake it into each vertex's bind position.
namespace Diorama::DragonBones
{
    //! Build a 2D affine from a DragonBones bone/slot transform. Angles are in DEGREES
    //! (skX/skY, which double as rotation when equal and skew when they differ); scX/scY
    //! default to 1. Pure so the decode is unit tested without touching JSON.
    inline MeshSkin::Affine2D TransformToAffine(float x, float y, float skXDegrees, float skYDegrees, float scaleX, float scaleY)
    {
        const float skX = AZ::DegToRad(skXDegrees);
        const float skY = AZ::DegToRad(skYDegrees);
        MeshSkin::Affine2D out;
        out.m_a = std::cos(skY) * scaleX;
        out.m_b = std::sin(skY) * scaleX;
        out.m_c = -std::sin(skX) * scaleY;
        out.m_d = std::cos(skX) * scaleY;
        out.m_tx = x;
        out.m_ty = y;
        return out;
    }

    //! One bone in the rest skeleton: its name, its parent's index (-1 for a root), and
    //! its default local transform. Bones are stored parent-first (parent index < own
    //! index), which MeshSkin::ComputeWorldTransforms relies on for its single forward
    //! pass; ParseDocument validates this and reports it via Document::m_bonesInOrder.
    struct BoneData
    {
        AZStd::string m_name;
        int m_parentIndex = -1;
        MeshSkin::Affine2D m_bindLocal;
    };

    //! One skinned mesh (a DragonBones mesh display with weights). Vertices carry their
    //! armature-space bind position, UV, and up to MeshSkin::MaxInfluences influences whose
    //! bone indices are PER-MESH-LOCAL slots (0..m_boneGlobalIndices.size()-1). The two
    //! parallel bone arrays turn a local slot back into skinning inputs: m_boneGlobalIndices
    //! selects which posed bone drives the slot, and m_bindWorld is that slot's bind-pose
    //! world matrix (from bonePose). Skinning is then, per slot s:
    //! skin[s] = SkinningMatrix(currentWorld[m_boneGlobalIndices[s]], m_bindWorld[s]).
    struct SkinnedMesh
    {
        AZStd::string m_slotName; //!< the owning slot's name (e.g. "a_head")
        AZStd::string m_displayName; //!< the mesh display's name (e.g. "body/a_head")
        //! Draw order: the slot's index in the armature's slot list. DragonBones layers
        //! slots back-to-front in that order, so a lower value draws further back.
        int m_drawOrder = 0;
        AZStd::vector<MeshSkin::SkinnedVertex> m_vertices;
        AZStd::vector<AZ::u16> m_indices; //!< triangle list, 3 indices per triangle
        AZStd::vector<int> m_boneGlobalIndices;
        AZStd::vector<MeshSkin::Affine2D> m_bindWorld;
    };

    //! One armature: its name, source frame rate, its bones (parent-first), and its
    //! skinned meshes. A DragonBones file usually holds exactly one.
    struct Armature
    {
        AZStd::string m_name;
        float m_frameRate = 30.0f;
        AZStd::vector<BoneData> m_bones;
        AZStd::vector<SkinnedMesh> m_meshes;
    };

    //! A parsed DragonBones document: its name and its armatures.
    struct Document
    {
        AZStd::string m_name;
        AZStd::vector<Armature> m_armatures;
        //! True when every bone's parent precedes it (the export invariant MeshSkin needs).
        //! Real exports always satisfy this; a false here flags a malformed/reordered file.
        bool m_bonesInOrder = true;
    };

    //! One packed sub-image in the DragonBones "*_tex.json" atlas: its name (which matches a
    //! mesh display name) and its pixel rect on the texture page. DragonBones stores mesh UVs
    //! normalized to the SubTexture (0..1 within this rect), so they must be remapped into
    //! page space before sampling the atlas.
    struct SubTexture
    {
        AZStd::string m_name;
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_width = 0.0f;
        float m_height = 0.0f;
    };

    //! A parsed DragonBones texture atlas ("*_tex.json"): the page size and its sub-images.
    struct Atlas
    {
        float m_width = 0.0f;
        float m_height = 0.0f;
        AZStd::vector<SubTexture> m_subTextures;
    };

    //! Parse a DragonBones "*_tex.json" atlas. Returns false on malformed JSON (out cleared).
    //! Implemented in DragonBonesImport.cpp.
    bool ParseAtlas(AZStd::string_view json, Atlas& out);

    //! Find a sub-texture by name (exact match); nullptr if absent.
    inline const SubTexture* FindSubTexture(const Atlas& atlas, AZStd::string_view name)
    {
        for (const SubTexture& sub : atlas.m_subTextures)
        {
            if (sub.m_name == name)
            {
                return &sub;
            }
        }
        return nullptr;
    }

    //! Remap a sub-texture-local UV (0..1 within a SubTexture) into page space (0..1 over the
    //! whole atlas). Pure so the atlas math is unit tested.
    inline AZ::Vector2 RemapAtlasUV(const AZ::Vector2& localUV, const SubTexture& sub, float atlasWidth, float atlasHeight)
    {
        if (atlasWidth <= 0.0f || atlasHeight <= 0.0f)
        {
            return localUV;
        }
        return AZ::Vector2((sub.m_x + localUV.GetX() * sub.m_width) / atlasWidth, (sub.m_y + localUV.GetY() * sub.m_height) / atlasHeight);
    }

    //! Remap every skinned mesh's UVs from sub-texture-local space into page space, matching
    //! each mesh to its SubTexture by display name. Meshes without a matching sub-texture (or
    //! a degenerate atlas) are left unchanged. Call after ParseDocument + ParseAtlas.
    void ApplyAtlasUVs(Document& document, const Atlas& atlas);

    //! Parse a DragonBones "*_ske.json" armature document. Returns false on malformed JSON
    //! (out is left cleared). Only mesh displays with weights (the skinned family) are
    //! imported; rigid image/mesh displays without weights are skipped (a documented later
    //! phase). Implemented in DragonBonesImport.cpp against AzCore rapidjson.
    bool ParseDocument(AZStd::string_view json, Document& out);

    //! Find an armature by name (exact match); nullptr if absent.
    inline const Armature* FindArmature(const Document& doc, AZStd::string_view name)
    {
        for (const Armature& armature : doc.m_armatures)
        {
            if (armature.m_name == name)
            {
                return &armature;
            }
        }
        return nullptr;
    }

    //! Find a skinned mesh in an armature by slot name (exact match); nullptr if absent.
    inline const SkinnedMesh* FindMesh(const Armature& armature, AZStd::string_view slotName)
    {
        for (const SkinnedMesh& mesh : armature.m_meshes)
        {
            if (mesh.m_slotName == slotName)
            {
                return &mesh;
            }
        }
        return nullptr;
    }
} // namespace Diorama::DragonBones
