/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/HitboxFrames.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/std/containers/vector.h>

// Internal (C++-only) rig registry for typed box interactions
// (Docs/design/2d-box-interactions.md phase B). Each frame-data hitbox rig
// publishes its world-resolved active boxes here; attacking rigs enumerate the
// registry (sorted by entity id, so resolution order is deterministic) to pair
// their attacking boxes against other rigs' receiving boxes at box level. The
// collision world only answers entity-level queries, which is not enough for box
// indices, payloads, and contact points; non-rig colliders still interact through
// the collision world as in v1.
namespace Diorama
{
    //! One active box on a rig, world-resolved for the current evaluation (facing
    //! applied, plane-projected center).
    struct ActiveRigBox
    {
        int m_index = 0; //!< index into the rig's authored boxes
        HitboxFrames::BoxKind m_kind = HitboxFrames::BoxKind::Hurtbox;
        AZ::Vector2 m_center = AZ::Vector2(0.0f, 0.0f); //!< world plane center
        AZ::Vector2 m_halfExtents = AZ::Vector2(0.5f, 0.5f);
        HitboxFrames::HitProperties m_hit; //!< authored payload (Hitbox / Throwbox)
    };

    //! Implemented by every active hitbox rig component.
    class DioramaHitboxRigs : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~DioramaHitboxRigs() = default;

        //! The rig's entity.
        virtual AZ::EntityId GetRigEntity() const = 0;
        //! Layer bit(s) this rig's receiving boxes (hurt / armor / throwable) live on;
        //! an attacker pairs against this rig only when its target mask matches.
        virtual AZ::u32 GetRigReceiveLayer() const = 0;
        //! The rig's active boxes as of its last evaluation.
        virtual const AZStd::vector<ActiveRigBox>& GetRigActiveBoxes() const = 0;
    };

    using DioramaHitboxRigBus = AZ::EBus<DioramaHitboxRigs>;
} // namespace Diorama
