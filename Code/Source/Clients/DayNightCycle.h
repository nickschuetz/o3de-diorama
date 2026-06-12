/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <cmath>
#include <cstddef>

// Pure, header-only day/night cycle math. It has no engine / component dependency: a
// runtime component advances a normalized time-of-day clock and samples this gradient
// to drive a Diorama light's color / intensity / direction, but the math is unit tested
// on its own (the DepthLane.h / BulletPattern.h pattern). Time of day is normalized to
// [0,1): 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
namespace Diorama::DayNightCycle
{
    //! Wrap a time-of-day into [0,1) (handles negative and >1 values).
    inline float Wrap01(float t)
    {
        t -= std::floor(t);
        // std::floor already maps into [0,1); guard the rounding edge at exactly 1.
        if (t >= 1.0f)
        {
            t -= 1.0f;
        }
        if (t < 0.0f)
        {
            t = 0.0f;
        }
        return t;
    }

    //! Advance a normalized clock by `dtSeconds`, given the real seconds in a full
    //! 24-hour cycle. A non-positive cycle holds the clock (just wraps the input).
    inline float Advance(float t, float dtSeconds, float cycleSeconds)
    {
        if (cycleSeconds <= 0.0f)
        {
            return Wrap01(t);
        }
        return Wrap01(t + dtSeconds / cycleSeconds);
    }

    //! A keyed lighting state at a normalized time of day. Colors are linear (the same
    //! space as DioramaLightConfig::m_color); elevation is the sun's height in degrees
    //! (+90 overhead, 0 at the horizon, negative below it = night).
    struct Keyframe
    {
        float m_time = 0.0f; //!< [0,1) time of day this key applies at.
        float m_sunR = 1.0f;
        float m_sunG = 1.0f;
        float m_sunB = 1.0f;
        float m_sunIntensity = 1.0f;
        float m_elevationDegrees = 0.0f;
    };

    namespace Internal
    {
        inline float Lerp(float a, float b, float f)
        {
            return a + (b - a) * f;
        }
    } // namespace Internal

    //! Linear-interpolate the keyframe ring at wrapped time `t`. `keys` must be sorted
    //! ascending by m_time with `count` >= 1; the interpolation is cyclic, wrapping from
    //! the last key across midnight back to the first. The returned keyframe carries the
    //! sampled color / intensity / elevation and m_time = the wrapped query time.
    inline Keyframe Sample(const Keyframe* keys, size_t count, float t)
    {
        Keyframe out;
        if (keys == nullptr || count == 0)
        {
            return out;
        }
        t = Wrap01(t);
        if (count == 1)
        {
            out = keys[0];
            out.m_time = t;
            return out;
        }

        // First key strictly after t (count if none: t is past the last key).
        size_t hi = count;
        for (size_t i = 0; i < count; ++i)
        {
            if (keys[i].m_time > t)
            {
                hi = i;
                break;
            }
        }

        size_t aIdx;
        size_t bIdx;
        float segLen;
        float pos;
        if (hi == 0 || hi == count)
        {
            // Wrap segment: last key -> first key, across midnight.
            aIdx = count - 1;
            bIdx = 0;
            segLen = keys[bIdx].m_time + 1.0f - keys[aIdx].m_time;
            const float tAdj = (t >= keys[aIdx].m_time) ? t : t + 1.0f;
            pos = tAdj - keys[aIdx].m_time;
        }
        else
        {
            aIdx = hi - 1;
            bIdx = hi;
            segLen = keys[bIdx].m_time - keys[aIdx].m_time;
            pos = t - keys[aIdx].m_time;
        }

        float f = (segLen > 0.0f) ? (pos / segLen) : 0.0f;
        f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);

        const Keyframe& a = keys[aIdx];
        const Keyframe& b = keys[bIdx];
        out.m_time = t;
        out.m_sunR = Internal::Lerp(a.m_sunR, b.m_sunR, f);
        out.m_sunG = Internal::Lerp(a.m_sunG, b.m_sunG, f);
        out.m_sunB = Internal::Lerp(a.m_sunB, b.m_sunB, f);
        out.m_sunIntensity = Internal::Lerp(a.m_sunIntensity, b.m_sunIntensity, f);
        out.m_elevationDegrees = Internal::Lerp(a.m_elevationDegrees, b.m_elevationDegrees, f);
        return out;
    }

    //! World-space travel direction of the sun light (the direction the light travels,
    //! pointing away from the sun) for a 2.5D scene, from the sun's elevation and azimuth
    //! in degrees. The sun sits at elevation (height) and azimuth (sweep across X/Z); the
    //! light travels from there into the scene. Output is unit length. Matches the
    //! semantics of DioramaLightConfig::m_direction. At elevation +90 the sun is overhead
    //! and the light travels straight down (0,-1,0).
    inline void SunDirection(float elevationDegrees, float azimuthDegrees, float& x, float& y, float& z)
    {
        constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
        const float e = elevationDegrees * DegToRad;
        const float a = azimuthDegrees * DegToRad;
        // Sun position direction (from scene toward the sun).
        const float sunX = std::cos(e) * std::cos(a);
        const float sunY = std::sin(e);
        const float sunZ = std::cos(e) * std::sin(a);
        // The light travels the opposite way (into the scene).
        x = -sunX;
        y = -sunY;
        z = -sunZ;
        const float len = std::sqrt(x * x + y * y + z * z);
        if (len > 1e-6f)
        {
            x /= len;
            y /= len;
            z /= len;
        }
    }
} // namespace Diorama::DayNightCycle
