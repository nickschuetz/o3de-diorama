/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/Collision2D.h>
#include <Diorama/Collision2DBus.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>

namespace Diorama
{
    //! Which pair of world axes the collider lives in. A 2.5D game picks the plane
    //! its gameplay moves on: XY is the conventional screen plane (top-down looking
    //! along Z, the twin-stick sample), XZ is the horizontal ground plane (Y up).
    enum class CollisionPlane : AZ::u8
    {
        XY = 0,
        XZ = 1,
        YZ = 2
    };

    //! Shared configuration for a 2D collider. Authored by the editor twin and
    //! handed to the runtime Collider2DComponent. m_plane selects which world axes
    //! the shape lives in; m_offset shifts the shape within that plane.
    class Collider2DConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(Collider2DConfig, Collider2DConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(Collider2DConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~Collider2DConfig() override = default;

        //! World plane the collider lives in (default XY, the screen plane).
        CollisionPlane m_plane = CollisionPlane::XY;
        //! true: circle of m_radius; false: axis-aligned box of m_halfExtents.
        bool m_isCircle = true;
        float m_radius = 0.5f;
        AZ::Vector2 m_halfExtents = AZ::Vector2(0.5f, 0.5f);
        //! In-plane offset from the entity origin (plane is X, Z).
        AZ::Vector2 m_offset = AZ::Vector2(0.0f, 0.0f);
        //! Category bit(s) this collider belongs to.
        AZ::u32 m_layer = 1;
        //! Mask of categories this collider reports against.
        AZ::u32 m_collidesWith = 0xFFFFFFFFu;
        //! A trigger reports overlaps (OnTrigger*) but is never a solid contact.
        bool m_isTrigger = false;
        //! Registered but excluded from detection when false.
        bool m_enabled = true;
        //! One-way platform: solid only from above. A box lands on its top but passes
        //! through from below and the sides (ComputeBoxPushOut resolves it upward only).
        bool m_oneWay = false;
    };

    //! Runtime 2D collider. Publishes itself to the collision world (the
    //! Collision2DSystemComponent) and keeps its position current as the entity
    //! moves. The system detects overlaps and dispatches contact/trigger events;
    //! this component just describes the shape and answers per-collider verbs.
    class Collider2DComponent final
        : public AZ::Component
        , protected Diorama2DColliderRequestBus::Handler
        , protected AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::Collider2DComponent, Collider2DComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        Collider2DComponent() = default;
        explicit Collider2DComponent(const Collider2DConfig& config);
        ~Collider2DComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // Diorama2DColliderRequestBus
        void SetCircle(float radius) override;
        void SetBox(float halfWidth, float halfHeight) override;
        void SetOffset(float x, float z) override;
        void SetLayer(AZ::u32 layer) override;
        void SetCollidesWith(AZ::u32 mask) override;
        void SetTrigger(bool isTrigger) override;
        void SetEnabled(bool enabled) override;
        void SetOneWay(bool oneWay) override;
        Collider2DInfo GetColliderInfo() override;

        // AZ::TransformNotificationBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

    private:
        //! Build the core collider from the config and the cached world position.
        Collision2D::Collider BuildCollider() const;
        //! Push the current collider into the world (no-op if no world is present).
        void PushToWorld();

        Collider2DConfig m_config;
        AZ::Vector3 m_worldTranslation = AZ::Vector3::CreateZero();
    };
} // namespace Diorama
