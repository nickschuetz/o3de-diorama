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
        // Raw bind transform components (m_bindLocal is these composed). Animation applies
        // deltas to these components (x += frame.x, skX/skY += rotate, scale *= frame.scale),
        // so the animated local is rebuilt from them each frame; that is exact and rotates
        // cleanly about the joint regardless of bind scale/skew.
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_skewXDegrees = 0.0f; //!< skX (rotation + skew)
        float m_skewYDegrees = 0.0f; //!< skY (rotation)
        float m_scaleX = 1.0f;
        float m_scaleY = 1.0f;

        // Surface bones (DragonBones "surface" type) carry a control-point grid instead of a
        // transform: they warp the meshes parented to them (SurfaceDeform) rather than rigidly
        // posing them. For a regular bone these stay default. A surface's transform components
        // above are unused (m_bindLocal is identity); its pose lives in the control points.
        bool m_isSurface = false;
        int m_segmentX = 0;
        int m_segmentY = 0;
        //! Bind-pose control points, row-major and X-major (point (i,j) at i + j*(segmentX+1)),
        //! (segmentX+1)*(segmentY+1) of them, in the surface's local space. Feeds a
        //! SurfaceDeform::SurfaceGrid once per-frame animation deltas are added.
        AZStd::vector<AZ::Vector2> m_bindControlPoints;
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

    //! One surface-bound mesh: a DragonBones mesh display whose slot is parented to a
    //! "surface" bone. Unlike SkinnedMesh it has no weights; its vertices live in the
    //! surface's local space and are warped by that surface's control-point grid
    //! (SurfaceDeform::WarpPoint) rather than skinned to bones.
    struct SurfaceMesh
    {
        AZStd::string m_slotName;
        AZStd::string m_displayName;
        int m_drawOrder = 0;
        int m_surfaceBoneIndex = -1; //!< the surface bone (the slot's parent) that warps this mesh
        AZStd::vector<AZ::Vector2> m_bindVertices; //!< surface-local space; warped each frame
        AZStd::vector<AZ::Vector2> m_uvs; //!< parallel to m_bindVertices
        AZStd::vector<AZ::u16> m_indices; //!< triangle list, 3 indices per triangle
    };

    //! How a keyframe eases into the next: hold (stepped), linear, or a cubic bezier curve.
    enum class TweenType : AZ::u8
    {
        Stepped,
        Linear,
        Curve
    };

    //! One keyframe on a bone channel. Its value is a delta applied to the bone's bind
    //! components: (x, y) for translate, (scaleX, scaleY) for scale, and (rotateDeg, skewDeg)
    //! for rotate. m_tween/m_curve describe the ease from THIS frame to the next.
    struct Keyframe
    {
        float m_startTime = 0.0f; //!< seconds into the clip (cumulative)
        float m_duration = 0.0f; //!< seconds until the next frame
        AZ::Vector2 m_value = AZ::Vector2::CreateZero();
        TweenType m_tween = TweenType::Linear;
        float m_curve[4] = { 0.0f, 0.0f, 1.0f, 1.0f }; //!< cubic bezier control points (0,0)->(1,1)
    };

    //! Per-bone animation channels. Each track is a list of keyframes; an empty track leaves
    //! that channel at its bind value.
    struct BoneTimeline
    {
        AZStd::string m_boneName;
        int m_boneIndex = -1; //!< resolved against the armature bone list
        AZStd::vector<Keyframe> m_translate; //!< value = (dx, dy)
        AZStd::vector<Keyframe> m_rotate; //!< value = (rotateDeg, skewDeg)
        AZStd::vector<Keyframe> m_scale; //!< value = (scaleX, scaleY), 1 = unchanged
    };

    //! One keyframe of an FFD / surface deform channel. The deltas are stored as authored:
    //! m_offset leading FLOATS are omitted (they default to 0), then m_rawDeltas holds the
    //! flat dx,dy,... run. ExpandDeform reconstructs the full per-vertex (dx,dy) array against
    //! the target's vertex / control-point count. m_tween/m_curve ease to the next frame. The
    //! float-granular offset (which may be odd) is why the run is kept flat, not paired.
    struct DeformFrame
    {
        float m_startTime = 0.0f; //!< seconds into the clip (cumulative)
        float m_duration = 0.0f; //!< seconds until the next frame
        TweenType m_tween = TweenType::Linear;
        float m_curve[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        int m_offset = 0; //!< flat FLOAT index in the full delta array where m_rawDeltas begins
        AZStd::vector<float> m_rawDeltas; //!< flat dx,dy,... floats, from m_offset onward
    };

    //! What a deform timeline drives: per-vertex offsets on a mesh (FFD), or per-control-point
    //! offsets on a surface bone's grid.
    enum class DeformTargetKind : AZ::u8
    {
        MeshFfd,
        Surface
    };

    //! One FFD / surface deform channel: the target it drives (by name) and its keyframes.
    //! m_targetIndex resolves the name to a surface mesh / skinned mesh (MeshFfd) or a surface
    //! bone (Surface); -1 if it did not resolve.
    struct DeformTimeline
    {
        AZStd::string m_targetName;
        DeformTargetKind m_kind = DeformTargetKind::MeshFfd;
        int m_targetIndex = -1;
        AZStd::vector<DeformFrame> m_frames;
    };

    //! Reconstruct a deform frame's full-length per-vertex (dx, dy) deltas: the first m_offset
    //! FLOATS are 0, then m_rawDeltas, then 0 for any trailing entries. `vertexCount` is the
    //! target's vertex (FFD) or control-point (surface) count. Pure; unit tested.
    inline void ExpandDeform(const DeformFrame& frame, int vertexCount, AZStd::vector<AZ::Vector2>& out)
    {
        out.assign(vertexCount < 0 ? 0u : static_cast<size_t>(vertexCount), AZ::Vector2::CreateZero());
        const int totalFloats = vertexCount * 2;
        const int rawSize = static_cast<int>(frame.m_rawDeltas.size());
        for (int i = 0; i + 1 < totalFloats; i += 2)
        {
            const int dxi = i - frame.m_offset;
            const int dyi = i + 1 - frame.m_offset;
            const float dx = (i < frame.m_offset || dxi >= rawSize) ? 0.0f : frame.m_rawDeltas[dxi];
            const float dy = (i + 1 < frame.m_offset || dyi >= rawSize) ? 0.0f : frame.m_rawDeltas[dyi];
            out[i / 2] = AZ::Vector2(dx, dy);
        }
    }

    //! One authored animation clip: its name, length, whether it loops, and per-bone timelines.
    struct Animation
    {
        AZStd::string m_name;
        float m_durationSeconds = 0.0f;
        bool m_loop = true; //!< DragonBones playTimes 0 loops; >0 plays that many times
        AZStd::vector<BoneTimeline> m_bones;
        AZStd::vector<DeformTimeline> m_deforms; //!< FFD / surface deform channels
    };

    //! The sampled pose delta for one bone: what to add to / multiply the bind components by.
    struct BonePoseDelta
    {
        AZ::Vector2 m_translate = AZ::Vector2::CreateZero();
        float m_rotateDegrees = 0.0f;
        float m_skewDegrees = 0.0f;
        AZ::Vector2 m_scale = AZ::Vector2(1.0f, 1.0f);
    };

    //! One armature: its name, source frame rate, its bones (parent-first), its skinned
    //! meshes, and its animation clips. A DragonBones file usually holds exactly one.
    struct Armature
    {
        AZStd::string m_name;
        float m_frameRate = 30.0f;
        AZStd::vector<BoneData> m_bones;
        AZStd::vector<SkinnedMesh> m_meshes;
        AZStd::vector<SurfaceMesh> m_surfaceMeshes;
        AZStd::vector<Animation> m_animations;
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

    //! Evaluate a DragonBones cubic-bezier easing curve at normalized time t (0..1). The
    //! curve is the two control points [x1, y1, x2, y2] of a bezier from (0,0) to (1,1); this
    //! returns the eased y for the given x=t. Pure (a few Newton iterations); unit tested.
    inline float EvaluateCurve(const float curve[4], float t)
    {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float x1 = curve[0];
        const float y1 = curve[1];
        const float x2 = curve[2];
        const float y2 = curve[3];
        // Solve for the bezier parameter u where bezierX(u) == t, then return bezierY(u).
        auto bezier = [](float a, float b, float u)
        {
            const float v = 1.0f - u;
            // control points 0 and 1 are 0 and 1 (the curve spans (0,0)->(1,1)).
            return 3.0f * v * v * u * a + 3.0f * v * u * u * b + u * u * u;
        };
        float u = t;
        for (int i = 0; i < 8; ++i)
        {
            const float x = bezier(x1, x2, u) - t;
            const float v = 1.0f - u;
            const float dx = 3.0f * v * v * x1 + 6.0f * v * u * (x2 - x1) + 3.0f * u * u * (1.0f - x2);
            if (dx < 1e-6f && dx > -1e-6f)
            {
                break;
            }
            u -= x / dx;
            u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
        }
        return bezier(y1, y2, u);
    }

    //! Sample a keyframe track at timeSeconds, honoring each frame's tween (stepped / linear /
    //! bezier curve). Holds the first value before the track starts and the last after it ends.
    //! An empty track returns `fallback` (0 for translate/rotate, 1 for scale). Pure; tested.
    inline AZ::Vector2 SampleTrack(const AZStd::vector<Keyframe>& track, float timeSeconds, const AZ::Vector2& fallback)
    {
        if (track.empty())
        {
            return fallback;
        }
        if (timeSeconds <= track.front().m_startTime)
        {
            return track.front().m_value;
        }
        for (size_t i = 0; i < track.size(); ++i)
        {
            const Keyframe& frame = track[i];
            const float end = frame.m_startTime + frame.m_duration;
            if (timeSeconds >= end || frame.m_duration <= 0.0f || i + 1 >= track.size())
            {
                if (i + 1 >= track.size())
                {
                    return frame.m_value; // past the last frame: hold it
                }
                continue;
            }
            const Keyframe& next = track[i + 1];
            if (frame.m_tween == TweenType::Stepped)
            {
                return frame.m_value;
            }
            float local = (timeSeconds - frame.m_startTime) / frame.m_duration;
            if (frame.m_tween == TweenType::Curve)
            {
                local = EvaluateCurve(frame.m_curve, local);
            }
            return frame.m_value + (next.m_value - frame.m_value) * local;
        }
        return track.back().m_value;
    }

    //! Sample an FFD / surface deform timeline at timeSeconds into full-length per-vertex
    //! (dx, dy) deltas (out sized to vertexCount). Brackets the two frames around the time,
    //! expands each, and interpolates element-wise with the frame's tween (stepped / linear /
    //! bezier). Holds the ends. Allocation-free (the next frame is expanded inline into the
    //! lerp), so it is cheap to call for every surface each frame. Pure; unit tested.
    inline void SampleDeform(const DeformTimeline& timeline, float timeSeconds, int vertexCount, AZStd::vector<AZ::Vector2>& out)
    {
        const AZStd::vector<DeformFrame>& frames = timeline.m_frames;
        if (frames.empty() || vertexCount <= 0)
        {
            out.assign(vertexCount < 0 ? 0u : static_cast<size_t>(vertexCount), AZ::Vector2::CreateZero());
            return;
        }
        if (timeSeconds <= frames.front().m_startTime || frames.size() == 1)
        {
            ExpandDeform(frames.front(), vertexCount, out);
            return;
        }
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const DeformFrame& frame = frames[i];
            const float end = frame.m_startTime + frame.m_duration;
            if (timeSeconds >= end || frame.m_duration <= 0.0f || i + 1 >= frames.size())
            {
                if (i + 1 >= frames.size())
                {
                    ExpandDeform(frame, vertexCount, out); // past the last frame: hold it
                    return;
                }
                continue;
            }
            ExpandDeform(frame, vertexCount, out);
            if (frame.m_tween == TweenType::Stepped)
            {
                return;
            }
            float local = (timeSeconds - frame.m_startTime) / frame.m_duration;
            if (frame.m_tween == TweenType::Curve)
            {
                local = EvaluateCurve(frame.m_curve, local);
            }
            // Lerp out (frame i) toward frame[i+1], expanding the next frame inline so no
            // second full array is allocated.
            const DeformFrame& nextFrame = frames[i + 1];
            const int rawSize = static_cast<int>(nextFrame.m_rawDeltas.size());
            for (int k = 0; k < vertexCount; ++k)
            {
                const int fx = k * 2 - nextFrame.m_offset;
                const int fy = k * 2 + 1 - nextFrame.m_offset;
                const float nx = (k * 2 < nextFrame.m_offset || fx >= rawSize) ? 0.0f : nextFrame.m_rawDeltas[fx];
                const float ny = (k * 2 + 1 < nextFrame.m_offset || fy >= rawSize) ? 0.0f : nextFrame.m_rawDeltas[fy];
                out[k] = out[k] + (AZ::Vector2(nx, ny) - out[k]) * local;
            }
            return;
        }
    }

    //! Sample a whole animation at timeSeconds into per-bone pose deltas (out sized to
    //! boneCount, indexed by bone). Bones without a timeline get the identity delta. Pure;
    //! implemented in DragonBonesImport.cpp.
    void SampleAnimation(const Animation& animation, float timeSeconds, int boneCount, AZStd::vector<BonePoseDelta>& out);

    //! Find an animation by name (exact match); nullptr if absent.
    inline const Animation* FindAnimation(const Armature& armature, AZStd::string_view name)
    {
        for (const Animation& animation : armature.m_animations)
        {
            if (animation.m_name == name)
            {
                return &animation;
            }
        }
        return nullptr;
    }

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
