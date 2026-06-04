/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>

#include <cstddef>

// Pure, header-only core for the cutout skeletal clip player: sample a keyframed
// local-transform track at a time and get the interpolated, eased pose. It has no
// engine/component dependency (the component supplies the keyframes and applies the
// result via TransformBus), so it is unit tested directly, the same pattern as
// Collision2D.h / Camera2D.h / TilemapPaint.h.
//
// This is the v1 (cutout) foundation: a character is a transform hierarchy of sprite
// "bones", and a clip animates each bone's local transform over time. The same
// sampling will back the later DragonBones mesh-deform path.
namespace Diorama::SkeletalClip
{
    //! Interpolation curve from one keyframe to the next.
    enum class Ease : AZ::u8
    {
        Linear,
        InQuad,
        OutQuad,
        InOutQuad,
        SmoothStep
    };

    //! Remap a normalized 0..1 segment parameter through an easing curve. Input is
    //! clamped to [0,1]; output is in [0,1].
    inline float ApplyEase(Ease ease, float t)
    {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        switch (ease)
        {
        case Ease::InQuad:
            return t * t;
        case Ease::OutQuad:
            return t * (2.0f - t);
        case Ease::InOutQuad:
            return t < 0.5f ? 2.0f * t * t : (-1.0f + (4.0f - 2.0f * t) * t);
        case Ease::SmoothStep:
            return t * t * (3.0f - 2.0f * t);
        case Ease::Linear:
        default:
            return t;
        }
    }

    //! One keyed local transform at a point in time. m_ease is the curve used to
    //! interpolate from this key to the next.
    struct Keyframe
    {
        float m_time = 0.0f;
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        Ease m_ease = Ease::Linear;
    };

    //! A sampled local transform (decomposed; the component composes it into a TM).
    struct Pose
    {
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
    };

    //! Sample a track of keyframes (assumed sorted by time) at the given time.
    //! - empty track -> identity pose
    //! - one key, or time at/before the first / at/after the last -> that key (held)
    //! - otherwise -> eased interpolation between the bracketing keys (lerp
    //!   translation/scale, slerp rotation).
    inline Pose SampleTrack(AZStd::span<const Keyframe> keys, float time)
    {
        Pose pose;
        if (keys.empty())
        {
            return pose;
        }
        const Keyframe& first = keys.front();
        const Keyframe& last = keys.back();
        if (keys.size() == 1 || time <= first.m_time)
        {
            pose.m_translation = first.m_translation;
            pose.m_rotation = first.m_rotation;
            pose.m_scale = first.m_scale;
            return pose;
        }
        if (time >= last.m_time)
        {
            pose.m_translation = last.m_translation;
            pose.m_rotation = last.m_rotation;
            pose.m_scale = last.m_scale;
            return pose;
        }

        // Find the segment [a, b] with a.m_time <= time < b.m_time.
        size_t b = 1;
        while (b < keys.size() && keys[b].m_time <= time)
        {
            ++b;
        }
        const Keyframe& keyA = keys[b - 1];
        const Keyframe& keyB = keys[b];

        const float span = keyB.m_time - keyA.m_time;
        const float raw = span > 0.0f ? (time - keyA.m_time) / span : 0.0f;
        const float t = ApplyEase(keyA.m_ease, raw);

        pose.m_translation = keyA.m_translation.Lerp(keyB.m_translation, t);
        pose.m_rotation = keyA.m_rotation.Slerp(keyB.m_rotation, t);
        pose.m_scale = keyA.m_scale.Lerp(keyB.m_scale, t);
        return pose;
    }
} // namespace Diorama::SkeletalClip
