/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/base.h>

#include <cmath>

// Pure, header-only core for frame-data hitboxes/hurtboxes: a box is active only on a
// window of animation frames (startup -> active -> recovery). Given the frame on
// screen, decide which boxes are live. It has no engine/component dependency (the
// component listens to OnAnimationFrame, builds Collision2D colliders from the active
// boxes, and registers them with the collision world), so the activation window is
// unit tested directly, the same pattern as SpriteCull.h / BulletPattern.h.
namespace Diorama::HitboxFrames
{
    //! What a box does. Names are functional, never genre-branded: the same kinds
    //! serve fighters, brawlers, platformer combat, and action RPGs.
    enum class BoxKind : AZ::u8
    {
        Hurtbox = 0, //!< vulnerable (gets hit)
        Hitbox = 1, //!< deals a hit while active
        Pushbox = 2, //!< body volume; overlapping pushboxes separate positionally
        Throwbox = 3, //!< grabs while active
        ThrowableBox = 4, //!< can be grabbed (throw-invulnerable states deactivate it)
        ArmorBox = 5, //!< absorbs hits while active (hyper armor windows)
        ProximityBox = 6, //!< presence signal (proximity guard, spacing AI)
    };

    //! Which guards stop a hit. Stored data only: the gem never applies guarding
    //! itself, the defender's combat script reads it off the event.
    enum class GuardHeight : AZ::u8
    {
        Any = 0, //!< blockable high or low
        High = 1, //!< must block standing
        Low = 2, //!< must block crouching
        Unblockable = 3,
    };

    //! Attack payload authored per box (meaningful on Hitbox / Throwbox), carried by
    //! the box and delivered with its event. The gem stores and delivers these
    //! numbers; it never interprets them (damage/stun application is the game's
    //! combat script), so fighting systems stay game-side.
    struct HitProperties
    {
        float m_damage = 0.0f;
        int m_hitstunFrames = 0; //!< sim frames the defender is in hitstun
        int m_blockstunFrames = 0; //!< sim frames when blocked
        int m_hitstopFrames = 0; //!< sim frames both sides freeze on contact
        AZ::Vector2 m_pushback = AZ::Vector2(0.0f, 0.0f); //!< applied to the defender (attacker pushback stays game-side)
        GuardHeight m_guardHeight = GuardHeight::Any;
        AZ::Vector2 m_launch = AZ::Vector2(0.0f, 0.0f); //!< zero means a grounded reaction
        int m_priority = 0; //!< clash resolution (hitbox vs hitbox)
        AZ::u32 m_customId = 0; //!< opaque to the gem: move id, sound key, anything
    };

    //! One authored box on the rig, live only on frames [m_startFrame, m_endFrame].
    struct Box
    {
        BoxKind m_kind = BoxKind::Hurtbox;
        AZ::Vector2 m_offset = AZ::Vector2(0.0f, 0.0f); //!< local center offset
        AZ::Vector2 m_halfExtents = AZ::Vector2(0.5f, 0.5f);
        int m_startFrame = 0; //!< first animation frame the box is active on
        int m_endFrame = 0; //!< last active frame (inclusive)
        HitProperties m_hit; //!< attack payload (Hitbox / Throwbox)
    };

    //! True when the box is active on the given animation frame. An empty/degenerate
    //! window (end before start) is never active.
    inline bool IsActiveAt(const Box& box, int frame)
    {
        return box.m_endFrame >= box.m_startFrame && frame >= box.m_startFrame && frame <= box.m_endFrame;
    }

    //! Axis-aligned overlap test between two plane boxes (touching edges count).
    inline bool BoxesOverlap(const AZ::Vector2& centerA, const AZ::Vector2& halfA, const AZ::Vector2& centerB, const AZ::Vector2& halfB)
    {
        return std::fabs(centerA.GetX() - centerB.GetX()) <= halfA.GetX() + halfB.GetX() &&
            std::fabs(centerA.GetY() - centerB.GetY()) <= halfA.GetY() + halfB.GetY();
    }

    //! Center of the intersection rectangle of two overlapping boxes: the approximate
    //! contact point delivered with a box event (spark placement). Only meaningful
    //! when BoxesOverlap is true.
    inline AZ::Vector2 OverlapCenter(
        const AZ::Vector2& centerA, const AZ::Vector2& halfA, const AZ::Vector2& centerB, const AZ::Vector2& halfB)
    {
        const float minX = AZ::GetMax(centerA.GetX() - halfA.GetX(), centerB.GetX() - halfB.GetX());
        const float maxX = AZ::GetMin(centerA.GetX() + halfA.GetX(), centerB.GetX() + halfB.GetX());
        const float minY = AZ::GetMax(centerA.GetY() - halfA.GetY(), centerB.GetY() - halfB.GetY());
        const float maxY = AZ::GetMin(centerA.GetY() + halfA.GetY(), centerB.GetY() + halfB.GetY());
        return AZ::Vector2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    //! Outcome of one overlapping box pair, per side.
    enum class BoxResult : AZ::u8
    {
        None = 0, //!< no event for this side (or no interaction at all)
        Hit = 1, //!< this side's box landed (hitbox on hurtbox, or won a priority contest)
        Clash = 2, //!< both hitboxes active at equal priority; mutual
        Beaten = 3, //!< lost a hitbox-vs-hitbox priority contest
        Absorbed = 4, //!< the hit met armor (attacker knows; defender spends its armor hit)
        Throw = 5, //!< this side's throwbox grabbed the other's throwable box
        Proximity = 6, //!< this side's proximity box is signalling over the other's hurtbox
    };

    //! The interaction matrix outcome for an overlapping pair: the result delivered
    //! to box A's side and to box B's side.
    struct Resolution
    {
        BoxResult m_forA = BoxResult::None;
        BoxResult m_forB = BoxResult::None;
    };

    //! Pure interaction matrix: what happens when an active box A overlaps an active
    //! box B. Symmetric in its arguments (call once per pair, either order). Priorities
    //! only matter for hitbox vs hitbox. Pushbox vs pushbox is positional, not an
    //! event, so it resolves to None here (separation goes through the push-out path).
    inline Resolution Resolve(BoxKind kindA, int priorityA, BoxKind kindB, int priorityB)
    {
        // Normalized cells are written A-acts-on-B; the swapped call mirrors them.
        if (kindA == BoxKind::Hitbox && kindB == BoxKind::Hurtbox)
        {
            return { BoxResult::Hit, BoxResult::None };
        }
        if (kindA == BoxKind::Hurtbox && kindB == BoxKind::Hitbox)
        {
            return { BoxResult::None, BoxResult::Hit };
        }
        if (kindA == BoxKind::Hitbox && kindB == BoxKind::Hitbox)
        {
            if (priorityA == priorityB)
            {
                return { BoxResult::Clash, BoxResult::Clash };
            }
            return priorityA > priorityB ? Resolution{ BoxResult::Hit, BoxResult::Beaten }
                                         : Resolution{ BoxResult::Beaten, BoxResult::Hit };
        }
        if (kindA == BoxKind::Hitbox && kindB == BoxKind::ArmorBox)
        {
            return { BoxResult::Absorbed, BoxResult::Absorbed };
        }
        if (kindA == BoxKind::ArmorBox && kindB == BoxKind::Hitbox)
        {
            return { BoxResult::Absorbed, BoxResult::Absorbed };
        }
        if (kindA == BoxKind::Throwbox && kindB == BoxKind::ThrowableBox)
        {
            return { BoxResult::Throw, BoxResult::None };
        }
        if (kindA == BoxKind::ThrowableBox && kindB == BoxKind::Throwbox)
        {
            return { BoxResult::None, BoxResult::Throw };
        }
        if (kindA == BoxKind::ProximityBox && kindB == BoxKind::Hurtbox)
        {
            return { BoxResult::Proximity, BoxResult::None };
        }
        if (kindA == BoxKind::Hurtbox && kindB == BoxKind::ProximityBox)
        {
            return { BoxResult::None, BoxResult::Proximity };
        }
        return {};
    }
} // namespace Diorama::HitboxFrames
