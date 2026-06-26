/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/std/containers/span.h>

#include <cmath>
#include <cstddef>

// Pure, header-only core for 2D mesh-deform (skinned) animation: the math that turns
// a skeleton pose plus a skinned mesh into deformed vertices. It has no engine,
// component, or asset dependency (the component supplies bones + vertices and submits
// the result through the sprite renderer), so it is unit tested directly, the same
// pattern as SkeletalClip.h / Collision2D.h / Camera2D.h.
//
// This is the v2 (DragonBones mesh-deform) foundation: a character is a 2D bone
// hierarchy and a mesh of vertices weighted to those bones; each frame the bones are
// posed and the mesh is CPU-skinned (linear blend) so the texture bends and stretches
// smoothly instead of pivoting as rigid cutout parts. The DragonBones import (a later
// phase) fills these structures; this header is the deformation math only.
namespace Diorama::MeshSkin
{
    //! A 2D affine transform: point' = (a*x + c*y + tx, b*x + d*y + ty). Captures
    //! translation, rotation, and (non-uniform) scale, which is all a 2D bone needs.
    struct Affine2D
    {
        float m_a = 1.0f;
        float m_b = 0.0f;
        float m_c = 0.0f;
        float m_d = 1.0f;
        float m_tx = 0.0f;
        float m_ty = 0.0f;

        //! The identity transform.
        static Affine2D Identity()
        {
            return Affine2D{};
        }

        //! Build from translation, rotation (radians, CCW), and per-axis scale. This is
        //! a bone's local transform; DragonBones supplies exactly these channels.
        static Affine2D FromTRS(const AZ::Vector2& translation, float rotationRadians, const AZ::Vector2& scale)
        {
            const float cosR = std::cos(rotationRadians);
            const float sinR = std::sin(rotationRadians);
            Affine2D out;
            out.m_a = cosR * scale.GetX();
            out.m_b = sinR * scale.GetX();
            out.m_c = -sinR * scale.GetY();
            out.m_d = cosR * scale.GetY();
            out.m_tx = translation.GetX();
            out.m_ty = translation.GetY();
            return out;
        }

        //! Apply this transform to a point.
        AZ::Vector2 TransformPoint(const AZ::Vector2& p) const
        {
            return AZ::Vector2(m_a * p.GetX() + m_c * p.GetY() + m_tx, m_b * p.GetX() + m_d * p.GetY() + m_ty);
        }
    };

    //! Compose two transforms: the result applies `rhs` first, then `lhs` (matrix
    //! product lhs * rhs). Used to build a bone's world transform from its parent's
    //! world transform and its own local transform: world = parentWorld * local.
    inline Affine2D Multiply(const Affine2D& lhs, const Affine2D& rhs)
    {
        Affine2D out;
        out.m_a = lhs.m_a * rhs.m_a + lhs.m_c * rhs.m_b;
        out.m_b = lhs.m_b * rhs.m_a + lhs.m_d * rhs.m_b;
        out.m_c = lhs.m_a * rhs.m_c + lhs.m_c * rhs.m_d;
        out.m_d = lhs.m_b * rhs.m_c + lhs.m_d * rhs.m_d;
        out.m_tx = lhs.m_a * rhs.m_tx + lhs.m_c * rhs.m_ty + lhs.m_tx;
        out.m_ty = lhs.m_b * rhs.m_tx + lhs.m_d * rhs.m_ty + lhs.m_ty;
        return out;
    }

    //! Inverse of an affine transform. Used once per bone to fold the bind pose into a
    //! skinning matrix (currentWorld * bindWorldInverse). A near-singular linear part
    //! (degenerate scale) falls back to identity rather than producing NaNs.
    inline Affine2D Inverse(const Affine2D& t)
    {
        const float det = t.m_a * t.m_d - t.m_b * t.m_c;
        Affine2D out;
        if (std::fabs(det) < 1e-8f)
        {
            return out; // identity fallback for a degenerate transform
        }
        const float invDet = 1.0f / det;
        out.m_a = t.m_d * invDet;
        out.m_b = -t.m_b * invDet;
        out.m_c = -t.m_c * invDet;
        out.m_d = t.m_a * invDet;
        out.m_tx = -(out.m_a * t.m_tx + out.m_c * t.m_ty);
        out.m_ty = -(out.m_b * t.m_tx + out.m_d * t.m_ty);
        return out;
    }

    //! Maximum bone influences per vertex (the common 2D rig limit; DragonBones rigs
    //! rarely exceed this and the cap bounds the per-vertex work and asset size).
    static constexpr size_t MaxInfluences = 4;

    //! One bone's pull on a vertex: which bone, and how strongly (0..1).
    struct Influence
    {
        int m_boneIndex = -1;
        float m_weight = 0.0f;
    };

    //! One skinned mesh vertex: its rest ("bind") position in mesh space, its texture
    //! coordinate, and up to MaxInfluences bone influences (weights need not be
    //! pre-normalized; SkinVertex normalizes by the total weight).
    struct SkinnedVertex
    {
        AZ::Vector2 m_bindPos = AZ::Vector2::CreateZero();
        AZ::Vector2 m_uv = AZ::Vector2::CreateZero();
        Influence m_influences[MaxInfluences];
        int m_influenceCount = 0;
    };

    //! Build the per-bone skinning matrix that maps a bind-pose vertex to its deformed
    //! position: skin = currentWorld * inverse(bindWorld). Apply once per bone, then
    //! SkinVertex reuses it across every vertex the bone touches.
    inline Affine2D SkinningMatrix(const Affine2D& currentWorld, const Affine2D& bindWorld)
    {
        return Multiply(currentWorld, Inverse(bindWorld));
    }

    //! Deform one vertex by linear-blend skinning: the weighted sum of each influencing
    //! bone's skinning matrix applied to the bind position. `skinMatrices[boneIndex]`
    //! is the bone's SkinningMatrix. Out-of-range bone indices and zero total weight are
    //! handled safely (the vertex holds at its bind position). Pure, unit tested.
    inline AZ::Vector2 SkinVertex(const SkinnedVertex& vertex, AZStd::span<const Affine2D> skinMatrices)
    {
        AZ::Vector2 sum = AZ::Vector2::CreateZero();
        float totalWeight = 0.0f;
        const int count =
            vertex.m_influenceCount < static_cast<int>(MaxInfluences) ? vertex.m_influenceCount : static_cast<int>(MaxInfluences);
        for (int i = 0; i < count; ++i)
        {
            const Influence& influence = vertex.m_influences[i];
            if (influence.m_boneIndex < 0 || influence.m_boneIndex >= static_cast<int>(skinMatrices.size()) || influence.m_weight <= 0.0f)
            {
                continue;
            }
            sum += skinMatrices[influence.m_boneIndex].TransformPoint(vertex.m_bindPos) * influence.m_weight;
            totalWeight += influence.m_weight;
        }
        if (totalWeight <= 0.0f)
        {
            return vertex.m_bindPos; // unweighted vertex holds its rest position
        }
        return sum / totalWeight;
    }

    //! One bone in the rest skeleton: its parent (-1 for a root) and its local transform.
    //! Bones are assumed topologically ordered (a parent precedes its children), which
    //! ComputeWorldTransforms relies on for a single forward pass; the builder validates
    //! this when importing.
    struct Bone
    {
        int m_parentIndex = -1;
        Affine2D m_local;
    };

    //! Compose each bone's world transform from its parent's world transform and its own
    //! local transform, in one forward pass: world[i] = (parent >= 0 ? world[parent] :
    //! identity) * local[i]. `bones` and `localOverrides` (the posed local transforms for
    //! this frame, same order/length as bones) drive it; pass the bones' own m_local as
    //! the overrides for the rest pose. Writes `outWorld` (sized to bones). Out-of-order
    //! or out-of-range parents are treated as roots (identity parent), never read past
    //! the current bone.
    inline void ComputeWorldTransforms(
        AZStd::span<const Bone> bones, AZStd::span<const Affine2D> localOverrides, AZStd::span<Affine2D> outWorld)
    {
        const size_t count = bones.size() < outWorld.size() ? bones.size() : outWorld.size();
        for (size_t i = 0; i < count; ++i)
        {
            const Affine2D& local = (i < localOverrides.size()) ? localOverrides[i] : bones[i].m_local;
            const int parent = bones[i].m_parentIndex;
            if (parent >= 0 && parent < static_cast<int>(i))
            {
                outWorld[i] = Multiply(outWorld[parent], local);
            }
            else
            {
                outWorld[i] = local; // root (or forward/invalid parent): world == local
            }
        }
    }
} // namespace Diorama::MeshSkin
