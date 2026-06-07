/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/Collision2D.h>
#include <Diorama/Collision2DBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    //! Internal C++ interface a Collider2DComponent uses to publish itself to the
    //! collision world. Kept off the BehaviorContext (it carries the internal
    //! Collision2D::Collider type); scripts use the reflected request/notification
    //! buses instead.
    class Collision2DWorldRequests
    {
    public:
        AZ_RTTI(Collision2DWorldRequests, "{2DFCBEB1-0FFF-42C1-BB2E-4F1F23A2C9FB}");
        virtual ~Collision2DWorldRequests() = default;

        //! Add or replace the collider record for an entity (called on activate and
        //! whenever the entity moves or its shape/flags change).
        virtual void UpsertCollider(AZ::EntityId entityId, const Collision2D::Collider& collider) = 0;
        //! Remove an entity's collider (on deactivate).
        virtual void RemoveCollider(AZ::EntityId entityId) = 0;
        //! Number of colliders this entity currently overlaps (for GetColliderInfo).
        virtual int GetContactCount(AZ::EntityId entityId) const = 0;
    };

    using Collision2DWorld = AZ::Interface<Collision2DWorldRequests>;

    //! Required system component that owns the single 2D collision world. Each
    //! tick it gathers the registered colliders, computes overlapping pairs, and
    //! dispatches contact/trigger transitions to the involved entities. It also
    //! answers the global spatial queries (overlap, raycast). Collision math lives
    //! in the pure Collision2D core; this component is the runtime glue.
    class Collision2DSystemComponent
        : public AZ::Component
        , public Collision2DWorldRequests
        , protected Diorama2DCollisionRequestBus::Handler
        , protected AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(Collision2DSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        Collision2DSystemComponent();
        ~Collision2DSystemComponent();

        //! Run one collision step (gather, detect, dispatch). Called from OnTick;
        //! exposed so tests can step deterministically without the tick bus.
        void StepSimulation();

        //! Current contact count for an entity (used by GetColliderInfo).
        int GetContactCount(AZ::EntityId entityId) const override;

    protected:
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // Collision2DWorldRequests
        void UpsertCollider(AZ::EntityId entityId, const Collision2D::Collider& collider) override;
        void RemoveCollider(AZ::EntityId entityId) override;

        // Diorama2DCollisionRequestBus
        AZStd::vector<AZ::EntityId> OverlapCircle(float x, float z, float radius, AZ::u32 layerMask) override;
        AZStd::vector<AZ::EntityId> OverlapBox(float x, float z, float halfWidth, float halfHeight, AZ::u32 layerMask) override;
        Raycast2DResult Raycast2D(float x, float z, float dirX, float dirZ, float maxDistance, AZ::u32 layerMask) override;
        AZ::Vector2 ComputeBoxPushOut(
            float x, float z, float halfWidth, float halfHeight, AZ::u32 layerMask, AZ::EntityId exclude) override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        enum class Phase
        {
            Begin,
            Stay,
            End
        };

        bool IsTrigger(AZ::EntityId entityId) const;
        void DispatchPair(const Collision2D::PairKey& pair, Phase phase);
        AZStd::vector<AZ::EntityId> OverlapShape(const Collision2D::Collider& query, AZ::u32 layerMask);

        //! Registered colliders, keyed by owning entity. Values carry the current
        //! center (pushed by the component on move) and shape/layer/trigger flags.
        AZStd::unordered_map<AZ::EntityId, Collision2D::Collider> m_colliders;

        Collision2D::ContactTracker m_tracker;

        // Per-step scratch, reused so the step does no steady-state allocation.
        AZStd::vector<Collision2D::Collider> m_scratchColliders;
        AZStd::vector<Collision2D::PairKey> m_currentPairs;
        AZStd::vector<Collision2D::PairKey> m_began;
        AZStd::vector<Collision2D::PairKey> m_ended;
        AZStd::vector<AZStd::pair<AZ::u32, AZ::u32>> m_scratchCandidates;
        AZStd::vector<AZ::u32> m_scratchOrder;
        Collision2D::PairSet m_beganSet;
    };
} // namespace Diorama
