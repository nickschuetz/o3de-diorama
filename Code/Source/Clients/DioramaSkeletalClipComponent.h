/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SkeletalClip.h>
#include <Diorama/DioramaSkeletalBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

// The pure SkeletalClip::Ease enum lives in a dependency-free header, so its
// reflection type info is specialized here (the one place that reflects it).
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::SkeletalClip::Ease, Diorama::SkeletalEaseTypeId);
}

namespace Diorama
{
    //! One authored keyframe. Inspector-friendly form of SkeletalClip::Keyframe:
    //! rotation is Euler degrees and scale is a single uniform factor, matching
    //! O3DE's transform model (which carries a uniform scale, not a per-axis one).
    struct SkeletalKeyframeData final
    {
        AZ_TYPE_INFO(Diorama::SkeletalKeyframeData, SkeletalKeyframeDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        float m_time = 0.0f;
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        AZ::Vector3 m_rotationDegrees = AZ::Vector3::CreateZero();
        float m_uniformScale = 1.0f;
        SkeletalClip::Ease m_ease = SkeletalClip::Ease::Linear;
    };

    //! A keyframe track that drives one bone: a descendant entity matched by name.
    struct SkeletalBoneTrackData final
    {
        AZ_TYPE_INFO(Diorama::SkeletalBoneTrackData, SkeletalBoneTrackDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        //! Name of the descendant entity (sprite "bone") this track animates.
        AZStd::string m_boneName;
        //! Keyframes, sorted by time when the clip is built.
        AZStd::vector<SkeletalKeyframeData> m_keys;
    };

    //! Configuration for the cutout skeletal clip player. The character is the entity
    //! hierarchy under this entity; each track names a descendant and animates its
    //! local transform over the clip.
    class DioramaSkeletalClipConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaSkeletalClipConfig, DioramaSkeletalClipConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaSkeletalClipConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaSkeletalClipConfig() override = default;

        //! Clip length in seconds; the time base normalized time maps onto. Clamped > 0.
        float m_duration = 1.0f;
        //! Wrap at the end (true) or hold the last frame (false).
        bool m_looping = true;
        //! Playback rate multiplier; negative plays in reverse.
        float m_speed = 1.0f;
        //! Begin playing automatically on activate.
        bool m_autoPlay = true;
        //! One track per animated bone.
        AZStd::vector<SkeletalBoneTrackData> m_tracks;
    };

    //! Resolve each track's bone name to a descendant entity of root (breadth-first
    //! over the transform hierarchy). Returns one entity id per track in m_tracks
    //! order; an unmatched name yields an invalid id (that track is skipped).
    AZStd::vector<AZ::EntityId> ResolveSkeletalBones(AZ::EntityId root, const DioramaSkeletalClipConfig& config);

    //! Sample the config at timeSeconds and apply each track's local transform to its
    //! resolved bone entity via TransformBus. bones must align with config.m_tracks.
    void ApplySkeletalPose(const DioramaSkeletalClipConfig& config, const AZStd::vector<AZ::EntityId>& bones, float timeSeconds);

    //! Runtime cutout skeletal clip player. Drives a transform hierarchy of sprite
    //! "bones" from a keyframed clip, sampling the pure SkeletalClip core each tick and
    //! applying the result via TransformBus. Reuses sprite components for the bones, so
    //! it composes with every other Diorama 2D feature. Driven at runtime through
    //! DioramaSkeletalRequestBus for AI/human parity.
    class DioramaSkeletalClipComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaSkeletalRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaSkeletalClipComponent, DioramaSkeletalClipComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DioramaSkeletalClipComponent() = default;
        explicit DioramaSkeletalClipComponent(const DioramaSkeletalClipConfig& config);
        ~DioramaSkeletalClipComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSkeletalRequests
        void Play() override;
        void Stop() override;
        void SetNormalizedTime(float normalizedTime) override;
        void SetSpeed(float speed) override;
        void SetLooping(bool looping) override;
        void SetDuration(float seconds) override;
        bool IsPlaying() override;

    private:
        DioramaSkeletalClipConfig m_config;
        AZStd::vector<AZ::EntityId> m_bones;
        float m_time = 0.0f;
        bool m_playing = false;
    };
} // namespace Diorama
