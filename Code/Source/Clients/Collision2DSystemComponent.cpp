/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/Collision2DSystemComponent.h>

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Math/Vector2.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/sort.h>

namespace Diorama
{
    AZ_COMPONENT_IMPL(Collision2DSystemComponent, "Collision2DSystemComponent", Collision2DSystemComponentTypeId);

    void Collision2DSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        Collider2DInfo::Reflect(context);
        Raycast2DResult::Reflect(context);
        GroundProbe2DResult::Reflect(context);
        ReflectCollision2DBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Collision2DSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void Collision2DSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Diorama2DCollisionService"));
    }

    void Collision2DSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Diorama2DCollisionService"));
    }

    void Collision2DSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void Collision2DSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    Collision2DSystemComponent::Collision2DSystemComponent() = default;
    Collision2DSystemComponent::~Collision2DSystemComponent() = default;

    void Collision2DSystemComponent::Init()
    {
    }

    void Collision2DSystemComponent::Activate()
    {
        // Publish the world interface before colliders activate and register with
        // it. As a required system component this activates ahead of entity
        // components, so Collision2DWorld::Get() is valid when they register.
        if (Collision2DWorld::Get() == nullptr)
        {
            Collision2DWorld::Register(this);
        }
        Diorama2DCollisionRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void Collision2DSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        Diorama2DCollisionRequestBus::Handler::BusDisconnect();
        if (Collision2DWorld::Get() == this)
        {
            Collision2DWorld::Unregister(this);
        }
        m_colliders.clear();
        m_staticSets.clear();
        m_groundSets.clear();
        m_tracker.Clear();
    }

    void Collision2DSystemComponent::UpsertCollider(AZ::EntityId entityId, const Collision2D::Collider& collider)
    {
        Collision2D::Collider record = collider;
        // The id is the contact key across frames; pin it to the entity so the
        // component never has to compute it consistently itself.
        record.m_id = static_cast<AZ::u64>(entityId);
        m_colliders[entityId] = record;
    }

    void Collision2DSystemComponent::RemoveCollider(AZ::EntityId entityId)
    {
        m_colliders.erase(entityId);
    }

    void Collision2DSystemComponent::SetStaticColliders(AZ::EntityId owner, const AZStd::vector<Collision2D::Collider>& colliders)
    {
        if (colliders.empty())
        {
            m_staticSets.erase(owner);
            return;
        }
        AZStd::vector<Collision2D::Collider>& stored = m_staticSets[owner];
        stored = colliders;
        // Pin every box's contact id to the owner (queries report the owner entity).
        for (Collision2D::Collider& c : stored)
        {
            c.m_id = static_cast<AZ::u64>(owner);
        }
    }

    void Collision2DSystemComponent::ClearStaticColliders(AZ::EntityId owner)
    {
        m_staticSets.erase(owner);
    }

    void Collision2DSystemComponent::SetGroundSegments(AZ::EntityId owner, const AZStd::vector<SlopeCollision::FloorSegment>& segments)
    {
        if (segments.empty())
        {
            m_groundSets.erase(owner);
            return;
        }
        m_groundSets[owner] = segments;
    }

    void Collision2DSystemComponent::ClearGroundSegments(AZ::EntityId owner)
    {
        m_groundSets.erase(owner);
    }

    GroundProbe2DResult Collision2DSystemComponent::ProbeGroundY(float x, float footY, float maxDrop, float stepUp)
    {
        GroundProbe2DResult result;
        float best = 0.0f;
        for (const auto& set : m_groundSets)
        {
            const AZStd::span<const SlopeCollision::FloorSegment> segments(set.second.data(), set.second.size());
            float groundY = 0.0f;
            if (SlopeCollision::ProbeGround(segments, x, footY, maxDrop, stepUp, groundY))
            {
                if (!result.m_onGround || groundY > best)
                {
                    best = groundY;
                    result.m_onGround = true;
                }
            }
        }
        result.m_groundY = best;
        return result;
    }

    namespace
    {
        // Reserved owner key for ground segments authored from script/agent via
        // AddGroundSegment, distinct from any real entity that owns a tilemap's set.
        const AZ::EntityId ScriptGroundOwner{ 0xD10A5C9DE9D0ull };
    } // namespace

    void Collision2DSystemComponent::AddGroundSegment(float x0, float x1, float y0, float y1)
    {
        // Normalize so x0 <= x1 (the probe assumes an increasing span).
        SlopeCollision::FloorSegment seg;
        if (x0 <= x1)
        {
            seg = SlopeCollision::FloorSegment{ x0, x1, y0, y1 };
        }
        else
        {
            seg = SlopeCollision::FloorSegment{ x1, x0, y1, y0 };
        }
        m_groundSets[ScriptGroundOwner].push_back(seg);
    }

    void Collision2DSystemComponent::ClearScriptGroundSegments()
    {
        m_groundSets.erase(ScriptGroundOwner);
    }

    void Collision2DSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        StepSimulation();
    }

    void Collision2DSystemComponent::StepSimulation()
    {
        m_scratchColliders.clear();
        m_scratchColliders.reserve(m_colliders.size());
        for (const auto& entry : m_colliders)
        {
            m_scratchColliders.push_back(entry.second);
        }

        Collision2D::ComputeContacts(m_scratchColliders, m_currentPairs, m_scratchCandidates, m_scratchOrder);
        m_tracker.Update(m_currentPairs, m_began, m_ended);

        m_beganSet.clear();
        for (const Collision2D::PairKey& p : m_began)
        {
            m_beganSet.insert(p);
        }

        for (const Collision2D::PairKey& p : m_began)
        {
            DispatchPair(p, Phase::Begin);
        }
        for (const Collision2D::PairKey& p : m_ended)
        {
            DispatchPair(p, Phase::End);
        }
        for (const Collision2D::PairKey& p : m_currentPairs)
        {
            if (m_beganSet.find(p) == m_beganSet.end())
            {
                DispatchPair(p, Phase::Stay);
            }
        }
    }

    bool Collision2DSystemComponent::IsTrigger(AZ::EntityId entityId) const
    {
        const auto it = m_colliders.find(entityId);
        return it != m_colliders.end() && it->second.m_isTrigger;
    }

    void Collision2DSystemComponent::DispatchPair(const Collision2D::PairKey& pair, Phase phase)
    {
        const AZ::EntityId a(pair.m_lo);
        const AZ::EntityId b(pair.m_hi);

        // Either collider being a trigger makes the whole overlap a trigger overlap
        // (no solid contact), matching how trigger volumes behave elsewhere.
        const bool trigger = IsTrigger(a) || IsTrigger(b);

        if (trigger)
        {
            if (phase == Phase::Begin)
            {
                Diorama2DCollisionNotificationBus::Event(a, &Diorama2DCollisionNotifications::OnTriggerEnter, b);
                Diorama2DCollisionNotificationBus::Event(b, &Diorama2DCollisionNotifications::OnTriggerEnter, a);
            }
            else if (phase == Phase::End)
            {
                Diorama2DCollisionNotificationBus::Event(a, &Diorama2DCollisionNotifications::OnTriggerExit, b);
                Diorama2DCollisionNotificationBus::Event(b, &Diorama2DCollisionNotifications::OnTriggerExit, a);
            }
            // Triggers report only enter/exit, not a per-frame stay.
            return;
        }

        switch (phase)
        {
        case Phase::Begin:
            Diorama2DCollisionNotificationBus::Event(a, &Diorama2DCollisionNotifications::OnContactBegin, b);
            Diorama2DCollisionNotificationBus::Event(b, &Diorama2DCollisionNotifications::OnContactBegin, a);
            break;
        case Phase::Stay:
            Diorama2DCollisionNotificationBus::Event(a, &Diorama2DCollisionNotifications::OnContactStay, b);
            Diorama2DCollisionNotificationBus::Event(b, &Diorama2DCollisionNotifications::OnContactStay, a);
            break;
        case Phase::End:
            Diorama2DCollisionNotificationBus::Event(a, &Diorama2DCollisionNotifications::OnContactEnd, b);
            Diorama2DCollisionNotificationBus::Event(b, &Diorama2DCollisionNotifications::OnContactEnd, a);
            break;
        }
    }

    int Collision2DSystemComponent::GetContactCount(AZ::EntityId entityId) const
    {
        const AZ::u64 id = static_cast<AZ::u64>(entityId);
        int count = 0;
        for (const Collision2D::PairKey& p : m_currentPairs)
        {
            if (p.m_lo == id || p.m_hi == id)
            {
                ++count;
            }
        }
        return count;
    }

    AZStd::vector<AZ::EntityId> Collision2DSystemComponent::OverlapShape(const Collision2D::Collider& query, AZ::u32 layerMask)
    {
        AZStd::vector<AZ::EntityId> result;
        for (const auto& entry : m_colliders)
        {
            const Collision2D::Collider& c = entry.second;
            if (!c.m_enabled)
            {
                continue;
            }
            if (layerMask != 0u && (c.m_layer & layerMask) == 0u)
            {
                continue;
            }
            if (Collision2D::Overlaps(query, c))
            {
                result.push_back(entry.first);
            }
        }
        // Static sets (tilemap solid tiles): one result per owner that overlaps.
        for (const auto& set : m_staticSets)
        {
            for (const Collision2D::Collider& c : set.second)
            {
                if (!c.m_enabled || (layerMask != 0u && (c.m_layer & layerMask) == 0u))
                {
                    continue;
                }
                if (Collision2D::Overlaps(query, c))
                {
                    result.push_back(set.first);
                    break;
                }
            }
        }
        // Deterministic result order: the collider store is an unordered_map, so the
        // iteration (and therefore result) order depends on hash-map history. Sort by
        // entity id so "the first hit" is the same entity on every run, which scripted
        // gameplay relies on and rollback/replay determinism requires.
        AZStd::sort(result.begin(), result.end());
        return result;
    }

    AZStd::vector<AZ::EntityId> Collision2DSystemComponent::OverlapCircle(float x, float z, float radius, AZ::u32 layerMask)
    {
        Collision2D::Collider query;
        query.m_center = AZ::Vector2(x, z);
        query.m_shape.m_type = Collision2D::ShapeType::Circle;
        query.m_shape.m_radius = radius < 0.0f ? 0.0f : radius;
        return OverlapShape(query, layerMask);
    }

    AZStd::vector<AZ::EntityId> Collision2DSystemComponent::OverlapBox(
        float x, float z, float halfWidth, float halfHeight, AZ::u32 layerMask)
    {
        Collision2D::Collider query;
        query.m_center = AZ::Vector2(x, z);
        query.m_shape.m_type = Collision2D::ShapeType::Box;
        query.m_shape.m_halfExtents = AZ::Vector2(halfWidth < 0.0f ? 0.0f : halfWidth, halfHeight < 0.0f ? 0.0f : halfHeight);
        return OverlapShape(query, layerMask);
    }

    AZ::Vector2 Collision2DSystemComponent::ComputeBoxPushOut(
        float x, float z, float halfWidth, float halfHeight, AZ::u32 layerMask, AZ::EntityId exclude)
    {
        Collision2D::Collider query;
        query.m_center = AZ::Vector2(x, z);
        query.m_shape.m_type = Collision2D::ShapeType::Box;
        query.m_shape.m_halfExtents = AZ::Vector2(halfWidth < 0.0f ? 0.0f : halfWidth, halfHeight < 0.0f ? 0.0f : halfHeight);

        // Accumulate in sorted entity-id order: float addition is not associative,
        // so summing in unordered_map iteration order would make the result differ
        // across runs in the low bits (a replay/rollback divergence seed).
        m_pushScratch.clear();
        for (const auto& entry : m_colliders)
        {
            if (entry.first == exclude)
            {
                continue; // never push out of the caller's own collider
            }
            const Collision2D::Collider& c = entry.second;
            if (!c.m_enabled || (layerMask != 0u && (c.m_layer & layerMask) == 0u))
            {
                continue;
            }
            m_pushScratch.push_back({ entry.first, &c });
        }
        AZStd::sort(
            m_pushScratch.begin(),
            m_pushScratch.end(),
            [](const auto& a, const auto& b)
            {
                return a.first < b.first;
            });

        AZ::Vector2 total(0.0f, 0.0f);
        for (const auto& [id, collider] : m_pushScratch)
        {
            const Collision2D::Collider& c = *collider;
            total += c.m_oneWay ? Collision2D::OneWayPushOut(query, c) : Collision2D::MinimumTranslation(query, c);
        }
        // Push out of static tilemap geometry too, owners visited in sorted order
        // for the same reason (each owner's own boxes are already a stable vector).
        m_staticOrderScratch.clear();
        for (const auto& set : m_staticSets)
        {
            if (set.first != exclude)
            {
                m_staticOrderScratch.push_back(set.first);
            }
        }
        AZStd::sort(m_staticOrderScratch.begin(), m_staticOrderScratch.end());
        for (const AZ::EntityId& owner : m_staticOrderScratch)
        {
            for (const Collision2D::Collider& c : m_staticSets[owner])
            {
                if (!c.m_enabled || (layerMask != 0u && (c.m_layer & layerMask) == 0u))
                {
                    continue;
                }
                total += c.m_oneWay ? Collision2D::OneWayPushOut(query, c) : Collision2D::MinimumTranslation(query, c);
            }
        }
        return total;
    }

    Raycast2DResult Collision2DSystemComponent::Raycast2D(float x, float z, float dirX, float dirZ, float maxDistance, AZ::u32 layerMask)
    {
        Raycast2DResult result;

        AZ::Vector2 dir(dirX, dirZ);
        const float len = dir.GetLength();
        if (len < 1e-6f || maxDistance <= 0.0f)
        {
            return result; // no direction or no range: miss
        }
        dir /= len;
        const AZ::Vector2 origin(x, z);

        float bestT = maxDistance;
        bool found = false;
        AZ::EntityId bestEntity;
        for (const auto& entry : m_colliders)
        {
            const Collision2D::Collider& c = entry.second;
            if (!c.m_enabled)
            {
                continue;
            }
            if (layerMask != 0u && (c.m_layer & layerMask) == 0u)
            {
                continue;
            }
            float t = 0.0f;
            // Tie-break equal distances on the lower entity id: the collider store is
            // an unordered_map, so without it the winner of an exact tie would depend
            // on hash-map iteration order (nondeterministic across runs).
            if (Collision2D::RaycastCollider(origin, dir, maxDistance, c, t) &&
                (!found || t < bestT || (t == bestT && entry.first < bestEntity)))
            {
                found = true;
                bestT = t;
                bestEntity = entry.first;
            }
        }
        // Static tilemap geometry.
        for (const auto& set : m_staticSets)
        {
            for (const Collision2D::Collider& c : set.second)
            {
                if (!c.m_enabled || (layerMask != 0u && (c.m_layer & layerMask) == 0u))
                {
                    continue;
                }
                float t = 0.0f;
                if (Collision2D::RaycastCollider(origin, dir, maxDistance, c, t) &&
                    (!found || t < bestT || (t == bestT && set.first < bestEntity)))
                {
                    found = true;
                    bestT = t;
                    bestEntity = set.first;
                }
            }
        }

        if (found)
        {
            const AZ::Vector2 point = origin + dir * bestT;
            result.m_hit = true;
            result.m_entityId = bestEntity;
            result.m_distance = bestT;
            result.m_pointX = point.GetX();
            result.m_pointZ = point.GetY();
        }
        return result;
    }
} // namespace Diorama
