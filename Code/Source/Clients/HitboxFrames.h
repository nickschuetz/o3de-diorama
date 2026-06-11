/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/base.h>

// Pure, header-only core for frame-data hitboxes/hurtboxes: a box is active only on a
// window of animation frames (startup -> active -> recovery). Given the frame on
// screen, decide which boxes are live. It has no engine/component dependency (the
// component listens to OnAnimationFrame, builds Collision2D colliders from the active
// boxes, and registers them with the collision world), so the activation window is
// unit tested directly, the same pattern as SpriteCull.h / BulletPattern.h.
namespace Diorama::HitboxFrames
{
    //! What a box does: a hurtbox is vulnerable (gets hit); a hitbox deals a hit while
    //! active. The component puts each kind on its own collision layer so a hit is a
    //! hitbox overlapping an opponent's hurtbox.
    enum class BoxKind : AZ::u8
    {
        Hurtbox = 0,
        Hitbox = 1,
    };

    //! One authored box on the rig, live only on frames [m_startFrame, m_endFrame].
    struct Box
    {
        BoxKind m_kind = BoxKind::Hurtbox;
        AZ::Vector2 m_offset = AZ::Vector2(0.0f, 0.0f); //!< local center offset
        AZ::Vector2 m_halfExtents = AZ::Vector2(0.5f, 0.5f);
        int m_startFrame = 0; //!< first animation frame the box is active on
        int m_endFrame = 0; //!< last active frame (inclusive)
    };

    //! True when the box is active on the given animation frame. An empty/degenerate
    //! window (end before start) is never active.
    inline bool IsActiveAt(const Box& box, int frame)
    {
        return box.m_endFrame >= box.m_startFrame && frame >= box.m_startFrame && frame <= box.m_endFrame;
    }
} // namespace Diorama::HitboxFrames
