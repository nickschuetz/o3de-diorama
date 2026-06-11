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
#include <AzCore/std/containers/vector.h>

#include <cmath>

// Pure, header-only core for bullet-pattern (danmaku) emission: given a pattern and
// its parameters, compute the initial velocity of each bullet in a single shot. It
// has no engine/component dependency (the emitter component supplies the origin and
// maps the 2D velocities onto the world plane, integrates them like particles, and
// renders them through the sprite batch), so the geometry is unit tested directly,
// the same pattern as Particles2D.h / SpriteCull.h.
//
// Velocities are plain 2D vectors (the component decides which world plane they lie
// in). Angles are radians, measured the standard way: 0 points along +X, increasing
// counter-clockwise; a velocity is (cos a, sin a) * speed.
namespace Diorama::BulletPattern
{
    //! Which emission shape a shot uses. The component maps this to the matching
    //! function below; kept here so the choice lives with the pattern definitions.
    enum class Kind : AZ::u8
    {
        Ring = 0, //!< Even radial burst around a full circle.
        Fan = 1, //!< Spread across an arc centered on the aim angle.
        Spiral = 2, //!< A ring fired repeatedly, each shot rotated a little.
    };

    //! A ring/radial burst: `count` bullets evenly spaced around a full circle,
    //! the first at `baseAngle`. Appends one velocity per bullet to `out` (cleared
    //! first). A non-positive count or speed produces nothing.
    inline void Ring(int count, float baseAngle, float speed, AZStd::vector<AZ::Vector2>& out)
    {
        out.clear();
        if (count <= 0 || speed <= 0.0f)
        {
            return;
        }
        const float step = AZ::Constants::TwoPi / static_cast<float>(count);
        out.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const float a = baseAngle + step * static_cast<float>(i);
            out.push_back(AZ::Vector2(std::cos(a) * speed, std::sin(a) * speed));
        }
    }

    //! A fan/spread: `count` bullets across an arc of `spread` radians centered on
    //! `aimAngle`. count == 1 fires a single bullet straight along aimAngle; count > 1
    //! spans the full arc inclusive of both edges. Appends one velocity per bullet to
    //! `out` (cleared first). A non-positive count or speed produces nothing.
    inline void Fan(int count, float aimAngle, float spread, float speed, AZStd::vector<AZ::Vector2>& out)
    {
        out.clear();
        if (count <= 0 || speed <= 0.0f)
        {
            return;
        }
        out.reserve(static_cast<size_t>(count));
        if (count == 1)
        {
            out.push_back(AZ::Vector2(std::cos(aimAngle) * speed, std::sin(aimAngle) * speed));
            return;
        }
        const float start = aimAngle - spread * 0.5f;
        const float step = spread / static_cast<float>(count - 1);
        for (int i = 0; i < count; ++i)
        {
            const float a = start + step * static_cast<float>(i);
            out.push_back(AZ::Vector2(std::cos(a) * speed, std::sin(a) * speed));
        }
    }

    //! Advance a spiral's base angle by `spinPerShot` radians and wrap into [0, 2pi).
    //! A spiral is just a Ring (or single bullet) fired repeatedly, each shot rotated
    //! by this increment; the component keeps the running base angle and calls Ring
    //! with it. Pure so the rotation is testable on its own.
    inline float AdvanceSpiral(float baseAngle, float spinPerShot)
    {
        float a = baseAngle + spinPerShot;
        a = std::fmod(a, AZ::Constants::TwoPi);
        if (a < 0.0f)
        {
            a += AZ::Constants::TwoPi;
        }
        return a;
    }
} // namespace Diorama::BulletPattern
