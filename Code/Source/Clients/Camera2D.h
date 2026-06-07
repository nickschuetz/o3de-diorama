/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/base.h>

#include <cmath>

// Pure, header-only math for the 2D camera component (Docs/design/2d-camera.md).
// These are the frame-rate-independent building blocks of camera follow: smoothing,
// deadzone, bounds, lookahead, pixel snapping, and trauma-based screen shake. They
// have no engine dependencies and are exercised directly by unit tests; the
// component is the thin glue that reads transforms and applies these each tick.
//
// All vectors are in the camera's working plane (the component projects world to
// that plane). Functions are deterministic; the only non-deterministic input is the
// shake angle, which the caller supplies from noise so the math stays testable.
namespace Diorama::Camera2D
{
    namespace Internal
    {
        inline float Clamp(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }
    } // namespace Internal

    //! Frame-rate-independent exponential smoothing toward a target. smoothTime is
    //! roughly the time to close most of the gap; 0 snaps immediately. The blend
    //! factor is 1 - exp(-dt / smoothTime), so the result is identical regardless of
    //! frame rate for the same elapsed time.
    inline AZ::Vector2 SmoothFollow(const AZ::Vector2& current, const AZ::Vector2& target, float smoothTime, float deltaTime)
    {
        if (smoothTime <= 0.0f || deltaTime <= 0.0f)
        {
            return target;
        }
        const float factor = 1.0f - std::exp(-deltaTime / smoothTime);
        return current + (target - current) * factor;
    }

    //! Move the camera only by the amount the target has left a centered deadzone
    //! rectangle (half-extents deadzoneHalf). Inside the deadzone the camera does not
    //! move, which kills micro-jitter; crossing an edge pulls the camera by the
    //! excess so the target sits back on the deadzone boundary.
    inline AZ::Vector2 ApplyDeadzone(const AZ::Vector2& cameraCenter, const AZ::Vector2& target, const AZ::Vector2& deadzoneHalf)
    {
        AZ::Vector2 result = cameraCenter;
        for (int axis = 0; axis < 2; ++axis)
        {
            const float c = cameraCenter.GetElement(axis);
            const float t = target.GetElement(axis);
            const float half = deadzoneHalf.GetElement(axis) < 0.0f ? 0.0f : deadzoneHalf.GetElement(axis);
            const float delta = t - c;
            if (delta > half)
            {
                result.SetElement(axis, t - half);
            }
            else if (delta < -half)
            {
                result.SetElement(axis, t + half);
            }
        }
        return result;
    }

    //! Clamp the camera center to a world rectangle. Pass the level extents already
    //! inset by half the view so the view never shows past the playfield. If a min
    //! exceeds its max (bounds smaller than the view) the axis is centered.
    inline AZ::Vector2 ClampToBounds(const AZ::Vector2& center, const AZ::Vector2& boundsMin, const AZ::Vector2& boundsMax)
    {
        AZ::Vector2 result = center;
        for (int axis = 0; axis < 2; ++axis)
        {
            const float lo = boundsMin.GetElement(axis);
            const float hi = boundsMax.GetElement(axis);
            if (lo > hi)
            {
                result.SetElement(axis, (lo + hi) * 0.5f);
            }
            else
            {
                result.SetElement(axis, Internal::Clamp(center.GetElement(axis), lo, hi));
            }
        }
        return result;
    }

    //! Offset that leads the target in its direction of motion. Returns zero for a
    //! (near) stationary target, so the view eases back when the target stops.
    inline AZ::Vector2 Lookahead(const AZ::Vector2& targetVelocity, float distance)
    {
        if (targetVelocity.GetLengthSq() < 1e-8f || distance == 0.0f)
        {
            return AZ::Vector2(0.0f, 0.0f);
        }
        return targetVelocity.GetNormalized() * distance;
    }

    //! Midpoint of two target positions, the follow point when framing two
    //! characters (a fighting camera centers on the pair).
    inline AZ::Vector2 Midpoint(const AZ::Vector2& a, const AZ::Vector2& b)
    {
        return (a + b) * 0.5f;
    }

    //! Dolly distance (how far to pull the camera back from the play plane) for a
    //! given separation between framed targets: base + separation * perUnit, clamped
    //! to [minDolly, maxDolly]. Pulls the view out as two fighters move apart and back
    //! in as they close, the classic versus-camera behaviour. perUnit <= 0 disables
    //! the separation term (a constant base dolly).
    inline float SeparationDolly(float separation, float base, float perUnit, float minDolly, float maxDolly)
    {
        const float raw = base + (perUnit > 0.0f ? separation * perUnit : 0.0f);
        const float hi = maxDolly < minDolly ? minDolly : maxDolly;
        return Internal::Clamp(raw, minDolly, hi);
    }

    //! Snap a position to a whole-texel grid for pixel-perfect rendering. ppu is
    //! pixels per world unit; <= 0 disables snapping (returns the input).
    inline AZ::Vector2 PixelSnap(const AZ::Vector2& pos, float pixelsPerUnit)
    {
        if (pixelsPerUnit <= 0.0f)
        {
            return pos;
        }
        const float x = std::floor(pos.GetX() * pixelsPerUnit + 0.5f) / pixelsPerUnit;
        const float y = std::floor(pos.GetY() * pixelsPerUnit + 0.5f) / pixelsPerUnit;
        return AZ::Vector2(x, y);
    }

    // --- trauma-based screen shake ------------------------------------------------
    // Events add trauma (0..1). Each frame the shake amount is maxShake * trauma^2
    // (so small hits are subtle and big hits punchy) and trauma decays linearly. The
    // squared curve and the after-follow application keep shake from fighting the
    // tracking.

    //! Add trauma from an event (e.g. a hit), clamped to 0..1.
    inline float AddTrauma(float current, float amount)
    {
        return Internal::Clamp(current + amount, 0.0f, 1.0f);
    }

    //! Linear trauma decay over time, clamped to 0..1.
    inline float DecayTrauma(float current, float decayPerSecond, float deltaTime)
    {
        return Internal::Clamp(current - decayPerSecond * deltaTime, 0.0f, 1.0f);
    }

    //! Shake displacement magnitude for the current trauma (maxShake * trauma^2).
    inline float ShakeMagnitude(float trauma, float maxShake)
    {
        const float t = Internal::Clamp(trauma, 0.0f, 1.0f);
        return maxShake * t * t;
    }

    //! Shake offset for a given magnitude along an angle (radians). The caller
    //! supplies the angle from smooth noise so this stays deterministic and testable.
    inline AZ::Vector2 ShakeOffset(float magnitude, float angle)
    {
        return AZ::Vector2(std::cos(angle) * magnitude, std::sin(angle) * magnitude);
    }
} // namespace Diorama::Camera2D
