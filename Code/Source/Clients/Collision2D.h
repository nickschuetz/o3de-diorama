/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utils.h>

#include <cmath>

// Pure, header-only 2D collision core for Diorama. It operates entirely in a 2D
// plane (AZ::Vector2); projecting an entity's world transform onto the chosen
// plane is the component layer's job, so this core has no engine dependencies and
// is exercised directly by unit tests (the same pattern as SpriteBatchPlan).
//
// Scope (v1): axis-aligned circle and box shapes, a mutual-opt-in layer/mask
// matrix, sort-and-sweep broadphase, exact narrowphase, ray casts, and a
// frame-to-frame contact tracker that turns overlapping pairs into begin/stay/end
// transitions. Rotated boxes (SAT) and a spatial-hash broadphase are v2 (see
// Docs/design/2d-collision.md).
namespace Diorama::Collision2D
{
    enum class ShapeType : AZ::u8
    {
        Circle,
        Box //!< Axis-aligned in the collision plane (v1).
    };

    //! A collider's shape. A circle uses m_radius; an axis-aligned box uses
    //! m_halfExtents. Only the field for m_type is meaningful.
    struct Shape
    {
        ShapeType m_type = ShapeType::Circle;
        float m_radius = 0.5f;
        AZ::Vector2 m_halfExtents = AZ::Vector2(0.5f, 0.5f);

        //! Half-width of the shape's axis-aligned bounds along X (used for the
        //! broadphase interval and the bounding box). Y is symmetric.
        float ExtentX() const
        {
            return m_type == ShapeType::Circle ? m_radius : m_halfExtents.GetX();
        }
        float ExtentY() const
        {
            return m_type == ShapeType::Circle ? m_radius : m_halfExtents.GetY();
        }
    };

    //! One collider in the plane. m_id is a stable identifier (the owning entity's
    //! id value) so frame-to-frame contact tracking survives reordering; array
    //! indices are not stable across frames and must not be used as keys.
    struct Collider
    {
        AZ::u64 m_id = 0;
        AZ::Vector2 m_center = AZ::Vector2(0.0f, 0.0f);
        Shape m_shape;
        //! Category bit(s) this collider belongs to.
        AZ::u32 m_layer = 1;
        //! Mask of categories this collider reports against.
        AZ::u32 m_collidesWith = 0xFFFFFFFFu;
        bool m_isTrigger = false; //!< The core ignores this; the component routes on it.
        bool m_enabled = true;
    };

    //! Box2D-style mutual opt-in: two colliders interact only if each one's mask
    //! includes the other's layer. Symmetric, so enabling A<->B needs no ordering.
    inline bool LayersInteract(const Collider& a, const Collider& b)
    {
        return (a.m_collidesWith & b.m_layer) != 0u && (b.m_collidesWith & a.m_layer) != 0u;
    }

    namespace Internal
    {
        inline float Clamp(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }
    } // namespace Internal

    //! Exact overlap test between two shapes at their centers. Touching exactly
    //! (distance equals the summed extents) counts as overlapping (inclusive).
    inline bool Overlaps(const Collider& a, const Collider& b)
    {
        const ShapeType ta = a.m_shape.m_type;
        const ShapeType tb = b.m_shape.m_type;

        if (ta == ShapeType::Circle && tb == ShapeType::Circle)
        {
            const float r = a.m_shape.m_radius + b.m_shape.m_radius;
            return (a.m_center - b.m_center).GetLengthSq() <= r * r;
        }

        if (ta == ShapeType::Box && tb == ShapeType::Box)
        {
            const float dx = std::fabs(a.m_center.GetX() - b.m_center.GetX());
            const float dy = std::fabs(a.m_center.GetY() - b.m_center.GetY());
            return dx <= (a.m_shape.m_halfExtents.GetX() + b.m_shape.m_halfExtents.GetX()) &&
                dy <= (a.m_shape.m_halfExtents.GetY() + b.m_shape.m_halfExtents.GetY());
        }

        // Circle vs box: closest point on the box to the circle center, then
        // compare its distance to the radius. Order the operands so 'c' is the
        // circle and 'box' is the box regardless of which argument was which.
        const Collider& c = (ta == ShapeType::Circle) ? a : b;
        const Collider& box = (ta == ShapeType::Circle) ? b : a;
        const float minX = box.m_center.GetX() - box.m_shape.m_halfExtents.GetX();
        const float maxX = box.m_center.GetX() + box.m_shape.m_halfExtents.GetX();
        const float minY = box.m_center.GetY() - box.m_shape.m_halfExtents.GetY();
        const float maxY = box.m_center.GetY() + box.m_shape.m_halfExtents.GetY();
        const float cx = Internal::Clamp(c.m_center.GetX(), minX, maxX);
        const float cy = Internal::Clamp(c.m_center.GetY(), minY, maxY);
        const AZ::Vector2 closest(cx, cy);
        return (c.m_center - closest).GetLengthSq() <= c.m_shape.m_radius * c.m_shape.m_radius;
    }

    //! Result of a ray cast: the hit collider's id, the parametric distance t
    //! along the (normalized) ray direction, and the world hit point.
    struct RayHit
    {
        AZ::u64 m_id = 0;
        float m_t = 0.0f;
        AZ::Vector2 m_point = AZ::Vector2(0.0f, 0.0f);
        bool m_hit = false;
    };

    //! Cast a ray (origin + t*dir, dir assumed unit length) against one collider,
    //! up to maxDistance. Returns the nearest non-negative t in [0, maxDistance].
    //! A ray starting inside the shape hits at t = 0.
    inline bool RaycastCollider(const AZ::Vector2& origin, const AZ::Vector2& dir, float maxDistance, const Collider& collider, float& outT)
    {
        if (collider.m_shape.m_type == ShapeType::Circle)
        {
            const AZ::Vector2 m = origin - collider.m_center;
            const float b = m.Dot(dir);
            const float cc = m.GetLengthSq() - collider.m_shape.m_radius * collider.m_shape.m_radius;
            // Origin outside and pointing away: no hit.
            if (cc > 0.0f && b > 0.0f)
            {
                return false;
            }
            const float disc = b * b - cc;
            if (disc < 0.0f)
            {
                return false;
            }
            const float sqrtDisc = std::sqrt(disc);
            float t = -b - sqrtDisc; // near root
            if (t < 0.0f)
            {
                t = 0.0f; // origin inside the circle
            }
            if (t > maxDistance)
            {
                return false;
            }
            outT = t;
            return true;
        }

        // Axis-aligned box: slab method. Guard against a zero direction component
        // (ray parallel to a slab) by rejecting if the origin is outside that slab.
        const float minX = collider.m_center.GetX() - collider.m_shape.m_halfExtents.GetX();
        const float maxX = collider.m_center.GetX() + collider.m_shape.m_halfExtents.GetX();
        const float minY = collider.m_center.GetY() - collider.m_shape.m_halfExtents.GetY();
        const float maxY = collider.m_center.GetY() + collider.m_shape.m_halfExtents.GetY();

        float tmin = 0.0f;
        float tmax = maxDistance;

        const float ox = origin.GetX();
        const float oy = origin.GetY();
        const float dx = dir.GetX();
        const float dy = dir.GetY();

        // X slab.
        if (std::fabs(dx) < 1e-8f)
        {
            if (ox < minX || ox > maxX)
            {
                return false;
            }
        }
        else
        {
            const float inv = 1.0f / dx;
            float t1 = (minX - ox) * inv;
            float t2 = (maxX - ox) * inv;
            if (t1 > t2)
            {
                AZStd::swap(t1, t2);
            }
            tmin = t1 > tmin ? t1 : tmin;
            tmax = t2 < tmax ? t2 : tmax;
            if (tmin > tmax)
            {
                return false;
            }
        }

        // Y slab.
        if (std::fabs(dy) < 1e-8f)
        {
            if (oy < minY || oy > maxY)
            {
                return false;
            }
        }
        else
        {
            const float inv = 1.0f / dy;
            float t1 = (minY - oy) * inv;
            float t2 = (maxY - oy) * inv;
            if (t1 > t2)
            {
                AZStd::swap(t1, t2);
            }
            tmin = t1 > tmin ? t1 : tmin;
            tmax = t2 < tmax ? t2 : tmax;
            if (tmin > tmax)
            {
                return false;
            }
        }

        outT = tmin;
        return true;
    }

    //! A normalized, order-independent pair of collider ids ((A,B) == (B,A)),
    //! used as the contact key so begin/end transitions are stable.
    struct PairKey
    {
        AZ::u64 m_lo = 0;
        AZ::u64 m_hi = 0;

        static PairKey Make(AZ::u64 a, AZ::u64 b)
        {
            return a <= b ? PairKey{ a, b } : PairKey{ b, a };
        }
        bool operator==(const PairKey& other) const
        {
            return m_lo == other.m_lo && m_hi == other.m_hi;
        }
    };

    struct PairKeyHash
    {
        size_t operator()(const PairKey& k) const
        {
            // Mix the two 64-bit ids; the multiply-xor keeps distinct ordered
            // pairs well separated in the hash table.
            return static_cast<size_t>(k.m_lo * 1099511628211ull ^ (k.m_hi + 0x9e3779b97f4a7c15ull));
        }
    };

    using PairSet = AZStd::unordered_set<PairKey, PairKeyHash>;

    //! Broadphase: sort-and-sweep on X. Fills outCandidates with index pairs
    //! (i<j) whose X bounds overlap, the superset the narrowphase then refines.
    //! Disabled colliders are skipped. scratchOrder is a caller-owned buffer
    //! (cleared and reused) so steady-state calls allocate nothing.
    inline void SweepAndPrune(
        const AZStd::vector<Collider>& colliders,
        AZStd::vector<AZStd::pair<AZ::u32, AZ::u32>>& outCandidates,
        AZStd::vector<AZ::u32>& scratchOrder)
    {
        outCandidates.clear();
        scratchOrder.clear();
        scratchOrder.reserve(colliders.size());
        for (AZ::u32 i = 0; i < colliders.size(); ++i)
        {
            if (colliders[i].m_enabled)
            {
                scratchOrder.push_back(i);
            }
        }

        AZStd::sort(
            scratchOrder.begin(),
            scratchOrder.end(),
            [&colliders](AZ::u32 lhs, AZ::u32 rhs)
            {
                const float lo = colliders[lhs].m_center.GetX() - colliders[lhs].m_shape.ExtentX();
                const float ro = colliders[rhs].m_center.GetX() - colliders[rhs].m_shape.ExtentX();
                return lo < ro;
            });

        for (AZ::u32 a = 0; a < scratchOrder.size(); ++a)
        {
            const Collider& ca = colliders[scratchOrder[a]];
            const float aMaxX = ca.m_center.GetX() + ca.m_shape.ExtentX();
            for (AZ::u32 b = a + 1; b < scratchOrder.size(); ++b)
            {
                const Collider& cb = colliders[scratchOrder[b]];
                const float bMinX = cb.m_center.GetX() - cb.m_shape.ExtentX();
                if (bMinX > aMaxX)
                {
                    break; // sorted by min X: no later interval can overlap a's max
                }
                // Emit with the lower array index first for a deterministic order.
                const AZ::u32 i = scratchOrder[a];
                const AZ::u32 j = scratchOrder[b];
                outCandidates.push_back(i < j ? AZStd::make_pair(i, j) : AZStd::make_pair(j, i));
            }
        }
    }

    //! Full overlap pass: broadphase, then filter candidates by the layer matrix
    //! and an exact overlap test, emitting normalized id pairs. Triggers are not
    //! special-cased here; the caller routes begin/end to contact vs trigger
    //! handlers by inspecting the colliders' m_isTrigger. Output and scratch
    //! buffers are caller-owned and reused.
    inline void ComputeContacts(
        const AZStd::vector<Collider>& colliders,
        AZStd::vector<PairKey>& outPairs,
        AZStd::vector<AZStd::pair<AZ::u32, AZ::u32>>& scratchCandidates,
        AZStd::vector<AZ::u32>& scratchOrder)
    {
        outPairs.clear();
        SweepAndPrune(colliders, scratchCandidates, scratchOrder);
        for (const auto& cand : scratchCandidates)
        {
            const Collider& a = colliders[cand.first];
            const Collider& b = colliders[cand.second];
            if (!LayersInteract(a, b))
            {
                continue;
            }
            if (Overlaps(a, b))
            {
                outPairs.push_back(PairKey::Make(a.m_id, b.m_id));
            }
        }
    }

    //! Frame-to-frame contact transitions. Feed it each frame's overlapping pairs
    //! (from ComputeContacts) and it reports which pairs began and which ended;
    //! pairs present this frame that did not just begin are continuing (stay).
    //! Holds two sets and swaps them, so it allocates nothing in steady state.
    class ContactTracker
    {
    public:
        //! current: this frame's overlapping pairs. outBegan/outEnded are cleared
        //! and filled. After the call, the tracker's active set equals current.
        void Update(const AZStd::vector<PairKey>& current, AZStd::vector<PairKey>& outBegan, AZStd::vector<PairKey>& outEnded)
        {
            outBegan.clear();
            outEnded.clear();
            m_next.clear();

            for (const PairKey& p : current)
            {
                m_next.insert(p);
                if (m_active.find(p) == m_active.end())
                {
                    outBegan.push_back(p);
                }
            }
            for (const PairKey& p : m_active)
            {
                if (m_next.find(p) == m_next.end())
                {
                    outEnded.push_back(p);
                }
            }
            m_active.swap(m_next);
        }

        //! Drop all active contacts (e.g. on level teardown). The next Update will
        //! report all current pairs as began.
        void Clear()
        {
            m_active.clear();
            m_next.clear();
        }

        bool IsActive(const PairKey& p) const
        {
            return m_active.find(p) != m_active.end();
        }
        size_t ActiveCount() const
        {
            return m_active.size();
        }

    private:
        PairSet m_active;
        PairSet m_next;
    };
} // namespace Diorama::Collision2D
