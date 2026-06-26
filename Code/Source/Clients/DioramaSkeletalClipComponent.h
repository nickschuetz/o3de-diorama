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

    //! A named alternative clip the player can cross-fade to. It shares the rig: its
    //! tracks animate the same bones (matched by name) as the default clip, so blending
    //! is per-bone. Authored alongside the default clip; selected by name via CrossFadeTo.
    struct SkeletalNamedClipData final
    {
        AZ_TYPE_INFO(Diorama::SkeletalNamedClipData, SkeletalNamedClipDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        //! Name used to select this clip (CrossFadeTo).
        AZStd::string m_name;
        //! Clip length in seconds. Clamped > 0.
        float m_duration = 1.0f;
        //! Wrap at the end (true) or hold the last frame (false).
        bool m_looping = true;
        //! One track per animated bone (same bones as the default clip).
        AZStd::vector<SkeletalBoneTrackData> m_tracks;
    };

    //! One entry in a 1D blend tree: a clip (by name) anchored at a blend-parameter
    //! value. The player blends the two entries that bracket the live parameter
    //! (SetBlendParam), so e.g. idle@0, walk@1, run@2 cross-blend as a speed value
    //! rises. An empty clip name refers to the component's default clip.
    struct SkeletalBlendEntryData final
    {
        AZ_TYPE_INFO(Diorama::SkeletalBlendEntryData, SkeletalBlendEntryDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        //! Name of a clip in the clip library, or empty for the default clip.
        AZStd::string m_clipName;
        //! Blend-parameter value this clip sits at. Entries are sorted by this at play.
        float m_anchor = 0.0f;
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
        //! Named alternative clips on the same rig, selectable via CrossFadeTo. Empty
        //! leaves the player single-clip (unchanged from v1).
        AZStd::vector<SkeletalNamedClipData> m_clips;
        //! 1D blend tree: clips (from the library / default) anchored on a blend
        //! parameter. Non-empty makes this a blend rig: each tick the player blends the
        //! two clips bracketing the live SetBlendParam value, phase-synced. Empty leaves
        //! the player on the single-clip / cross-fade path (unchanged from v1).
        AZStd::vector<SkeletalBlendEntryData> m_blendTree;
    };

    //! Resolve each track's bone name to a descendant entity of root (breadth-first
    //! over the transform hierarchy). Returns one entity id per track in m_tracks
    //! order; an unmatched name yields an invalid id (that track is skipped).
    AZStd::vector<AZ::EntityId> ResolveSkeletalBones(AZ::EntityId root, const DioramaSkeletalClipConfig& config);

    //! Sample the config at timeSeconds and apply each track's local transform to its
    //! resolved bone entity via TransformBus. bones must align with config.m_tracks.
    void ApplySkeletalPose(const DioramaSkeletalClipConfig& config, const AZStd::vector<AZ::EntityId>& bones, float timeSeconds);

    //! Apply a per-bone cross-fade of two clips: sample tracksA at timeA and tracksB at
    //! timeB, blend each bone's pose by weight (0 = A, 1 = B) with SkeletalClip::BlendPose,
    //! and write the result to the resolved bone via TransformBus. bones align with
    //! tracksA; tracksB is matched by index (clips share the rig / bone order).
    void ApplySkeletalPoseBlended(
        const AZStd::vector<SkeletalBoneTrackData>& tracksA,
        float timeA,
        const AZStd::vector<SkeletalBoneTrackData>& tracksB,
        float timeB,
        const AZStd::vector<AZ::EntityId>& bones,
        float weight);

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
        void CrossFadeTo(const AZStd::string& clipName, float durationSeconds) override;
        void SetBlendParam(float value) override;
        float GetBlendParam() override;
        bool IsPlaying() override;

    private:
        //! One resolved blend-tree entry: an anchor and which clip backs it
        //! (m_clipIndex into m_config.m_clips, or -1 for the default clip).
        struct ResolvedBlendEntry
        {
            float m_anchor = 0.0f;
            int m_clipIndex = -1;
        };

        //! Build m_blendEntries / m_blendAnchors from m_config.m_blendTree (sorted by
        //! anchor, names resolved to clip indices). Sets m_blendActive.
        void ResolveBlendTree();
        //! Tracks / duration backing a resolved blend entry's clip index (-1 = default).
        const AZStd::vector<SkeletalBoneTrackData>& BlendTracks(int clipIndex) const;
        float BlendClipDuration(int clipIndex) const;

        DioramaSkeletalClipConfig m_config;
        AZStd::vector<AZ::EntityId> m_bones;
        float m_time = 0.0f;
        bool m_playing = false;
        // Cross-fade state: when m_fadeIndex >= 0 the player blends from the current
        // clip toward m_config.m_clips[m_fadeIndex] over m_fadeDuration seconds, then
        // promotes the target to the current clip.
        int m_fadeIndex = -1;
        float m_fadeTime = 0.0f; //!< playback time of the fade-target clip
        float m_fadeElapsed = 0.0f; //!< seconds into the fade
        float m_fadeDuration = 0.0f;
        // 1D blend-tree state (active when m_config.m_blendTree is non-empty). The tick
        // blends the two clips bracketing m_blendParam, phase-synced across durations.
        bool m_blendActive = false;
        float m_blendParam = 0.0f;
        float m_blendPhase = 0.0f; //!< shared normalized phase [0,1) across the blended clips
        AZStd::vector<ResolvedBlendEntry> m_blendEntries; //!< sorted by anchor
        AZStd::vector<float> m_blendAnchors; //!< parallel anchors, for ResolveBlend1D
    };
} // namespace Diorama
