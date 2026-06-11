/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaSkeletalClipComponent.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

#include <cmath>

namespace Diorama
{
    namespace
    {
        //! Build the pure, time-sorted keyframe track for one authored bone track.
        AZStd::vector<SkeletalClip::Keyframe> BuildPureTrack(const SkeletalBoneTrackData& track)
        {
            AZStd::vector<SkeletalClip::Keyframe> keys;
            keys.reserve(track.m_keys.size());
            for (const SkeletalKeyframeData& data : track.m_keys)
            {
                SkeletalClip::Keyframe key;
                key.m_time = data.m_time;
                key.m_translation = data.m_translation;
                key.m_rotation = AZ::ConvertEulerDegreesToQuaternion(data.m_rotationDegrees);
                key.m_scale = AZ::Vector3(data.m_uniformScale);
                key.m_ease = data.m_ease;
                keys.push_back(key);
            }
            AZStd::sort(
                keys.begin(),
                keys.end(),
                [](const SkeletalClip::Keyframe& lhs, const SkeletalClip::Keyframe& rhs)
                {
                    return lhs.m_time < rhs.m_time;
                });
            return keys;
        }

        //! Breadth-first search the transform hierarchy under root for a descendant
        //! entity whose name matches boneName. Returns an invalid id if none match.
        AZ::EntityId FindDescendantByName(AZ::EntityId root, const AZStd::string& boneName)
        {
            if (boneName.empty())
            {
                return AZ::EntityId();
            }

            AZStd::vector<AZ::EntityId> frontier;
            AZ::TransformBus::EventResult(frontier, root, &AZ::TransformBus::Events::GetChildren);

            for (size_t head = 0; head < frontier.size(); ++head)
            {
                const AZ::EntityId current = frontier[head];

                AZ::Entity* entity = nullptr;
                AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, current);
                if (entity != nullptr && entity->GetName() == boneName)
                {
                    return current;
                }

                AZStd::vector<AZ::EntityId> children;
                AZ::TransformBus::EventResult(children, current, &AZ::TransformBus::Events::GetChildren);
                frontier.insert(frontier.end(), children.begin(), children.end());
            }

            return AZ::EntityId();
        }
    } // namespace

    void SkeletalKeyframeData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SkeletalKeyframeData>()
                ->Version(1)
                ->Field("time", &SkeletalKeyframeData::m_time)
                ->Field("translation", &SkeletalKeyframeData::m_translation)
                ->Field("rotationDegrees", &SkeletalKeyframeData::m_rotationDegrees)
                ->Field("uniformScale", &SkeletalKeyframeData::m_uniformScale)
                ->Field("ease", &SkeletalKeyframeData::m_ease);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<SkeletalKeyframeData>("Keyframe", "One keyed local transform at a point in the clip")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletalKeyframeData::m_time, "Time", "Seconds into the clip")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &SkeletalKeyframeData::m_translation, "Translation", "Local offset of the bone")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SkeletalKeyframeData::m_rotationDegrees,
                        "Rotation",
                        "Local rotation in Euler degrees")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletalKeyframeData::m_uniformScale, "Scale", "Local uniform scale")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &SkeletalKeyframeData::m_ease, "Ease", "Curve from this key to the next")
                    ->EnumAttribute(SkeletalClip::Ease::Linear, "Linear")
                    ->EnumAttribute(SkeletalClip::Ease::InQuad, "Ease in")
                    ->EnumAttribute(SkeletalClip::Ease::OutQuad, "Ease out")
                    ->EnumAttribute(SkeletalClip::Ease::InOutQuad, "Ease in-out")
                    ->EnumAttribute(SkeletalClip::Ease::SmoothStep, "Smooth step");
            }
        }
    }

    void SkeletalBoneTrackData::Reflect(AZ::ReflectContext* context)
    {
        SkeletalKeyframeData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SkeletalBoneTrackData>()
                ->Version(1)
                ->Field("boneName", &SkeletalBoneTrackData::m_boneName)
                ->Field("keys", &SkeletalBoneTrackData::m_keys);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<SkeletalBoneTrackData>("Bone track", "Keyframes that animate one named descendant entity")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SkeletalBoneTrackData::m_boneName,
                        "Bone (entity name)",
                        "Name of the descendant entity this track drives")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletalBoneTrackData::m_keys, "Keyframes", "Sorted by time at play");
            }
        }
    }

    void SkeletalNamedClipData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SkeletalNamedClipData>()
                ->Version(1)
                ->Field("name", &SkeletalNamedClipData::m_name)
                ->Field("duration", &SkeletalNamedClipData::m_duration)
                ->Field("looping", &SkeletalNamedClipData::m_looping)
                ->Field("tracks", &SkeletalNamedClipData::m_tracks);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<SkeletalNamedClipData>("Skeletal clip", "A named clip on the same rig, selectable via CrossFadeTo")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletalNamedClipData::m_name, "Name", "Name used by CrossFadeTo")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletalNamedClipData::m_duration, "Duration", "Clip length in seconds")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &SkeletalNamedClipData::m_looping, "Loop", "Wrap at the end")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SkeletalNamedClipData::m_tracks,
                        "Tracks",
                        "One per animated bone (same bones as the default clip)");
            }
        }
    }

    void DioramaSkeletalClipConfig::Reflect(AZ::ReflectContext* context)
    {
        SkeletalBoneTrackData::Reflect(context);
        SkeletalNamedClipData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSkeletalClipConfig>()
                ->Version(2)
                ->Field("duration", &DioramaSkeletalClipConfig::m_duration)
                ->Field("looping", &DioramaSkeletalClipConfig::m_looping)
                ->Field("speed", &DioramaSkeletalClipConfig::m_speed)
                ->Field("autoPlay", &DioramaSkeletalClipConfig::m_autoPlay)
                ->Field("tracks", &DioramaSkeletalClipConfig::m_tracks)
                ->Field("clips", &DioramaSkeletalClipConfig::m_clips);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaSkeletalClipConfig>("Skeletal Clip Config", "Keyframed cutout animation over a bone hierarchy")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaSkeletalClipConfig::m_duration, "Duration", "Clip length in seconds")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &DioramaSkeletalClipConfig::m_looping, "Loop", "Wrap at the end")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaSkeletalClipConfig::m_speed, "Speed", "Playback rate (negative = reverse)")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &DioramaSkeletalClipConfig::m_autoPlay, "Auto play", "Play on activate")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaSkeletalClipConfig::m_tracks, "Tracks", "One per animated bone")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSkeletalClipConfig::m_clips,
                        "Clips",
                        "Named alternative clips on the same rig, selectable via CrossFadeTo");
            }
        }
    }

    AZStd::vector<AZ::EntityId> ResolveSkeletalBones(AZ::EntityId root, const DioramaSkeletalClipConfig& config)
    {
        AZStd::vector<AZ::EntityId> bones;
        bones.reserve(config.m_tracks.size());
        for (const SkeletalBoneTrackData& track : config.m_tracks)
        {
            bones.push_back(FindDescendantByName(root, track.m_boneName));
        }
        return bones;
    }

    void ApplySkeletalPose(const DioramaSkeletalClipConfig& config, const AZStd::vector<AZ::EntityId>& bones, float timeSeconds)
    {
        const size_t count = AZ::GetMin(bones.size(), config.m_tracks.size());
        for (size_t i = 0; i < count; ++i)
        {
            if (!bones[i].IsValid())
            {
                continue;
            }
            const AZStd::vector<SkeletalClip::Keyframe> keys = BuildPureTrack(config.m_tracks[i]);
            const SkeletalClip::Pose pose =
                SkeletalClip::SampleTrack(AZStd::span<const SkeletalClip::Keyframe>(keys.data(), keys.size()), timeSeconds);

            AZ::Transform tm = AZ::Transform::CreateFromQuaternionAndTranslation(pose.m_rotation, pose.m_translation);
            tm.SetUniformScale(AZ::GetMax(pose.m_scale.GetX(), AZ::Constants::FloatEpsilon));
            AZ::TransformBus::Event(bones[i], &AZ::TransformBus::Events::SetLocalTM, tm);
        }
    }

    void ApplySkeletalPoseBlended(
        const AZStd::vector<SkeletalBoneTrackData>& tracksA,
        float timeA,
        const AZStd::vector<SkeletalBoneTrackData>& tracksB,
        float timeB,
        const AZStd::vector<AZ::EntityId>& bones,
        float weight)
    {
        const size_t count = AZ::GetMin(bones.size(), tracksA.size());
        for (size_t i = 0; i < count; ++i)
        {
            if (!bones[i].IsValid())
            {
                continue;
            }
            const AZStd::vector<SkeletalClip::Keyframe> keysA = BuildPureTrack(tracksA[i]);
            const SkeletalClip::Pose poseA =
                SkeletalClip::SampleTrack(AZStd::span<const SkeletalClip::Keyframe>(keysA.data(), keysA.size()), timeA);

            SkeletalClip::Pose pose = poseA;
            // Blend in the target clip's matching bone (by index; clips share the rig).
            if (i < tracksB.size())
            {
                const AZStd::vector<SkeletalClip::Keyframe> keysB = BuildPureTrack(tracksB[i]);
                const SkeletalClip::Pose poseB =
                    SkeletalClip::SampleTrack(AZStd::span<const SkeletalClip::Keyframe>(keysB.data(), keysB.size()), timeB);
                pose = SkeletalClip::BlendPose(poseA, poseB, weight);
            }

            AZ::Transform tm = AZ::Transform::CreateFromQuaternionAndTranslation(pose.m_rotation, pose.m_translation);
            tm.SetUniformScale(AZ::GetMax(pose.m_scale.GetX(), AZ::Constants::FloatEpsilon));
            AZ::TransformBus::Event(bones[i], &AZ::TransformBus::Events::SetLocalTM, tm);
        }
    }

    DioramaSkeletalClipComponent::DioramaSkeletalClipComponent(const DioramaSkeletalClipConfig& config)
        : m_config(config)
    {
    }

    void DioramaSkeletalClipComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaSkeletalClipConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSkeletalClipComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaSkeletalClipComponent::m_config);
        }

        ReflectSkeletalBuses(context);
    }

    void DioramaSkeletalClipComponent::Activate()
    {
        DioramaSkeletalRequestBus::Handler::BusConnect(GetEntityId());

        m_bones = ResolveSkeletalBones(GetEntityId(), m_config);
        m_time = 0.0f;
        m_playing = m_config.m_autoPlay;

        // Apply the opening pose so the rig is posed even before the first tick.
        ApplySkeletalPose(m_config, m_bones, m_time);

        AZ::TickBus::Handler::BusConnect();
    }

    void DioramaSkeletalClipComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSkeletalRequestBus::Handler::BusDisconnect();
    }

    void DioramaSkeletalClipComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_playing)
        {
            return;
        }

        const float duration = AZ::GetMax(m_config.m_duration, AZ::Constants::FloatEpsilon);
        const bool fading = m_fadeIndex >= 0 && m_fadeIndex < static_cast<int>(m_config.m_clips.size());
        m_time += deltaTime * m_config.m_speed;

        if (m_config.m_looping)
        {
            // Wrap into [0, duration) for either play direction.
            m_time = std::fmod(m_time, duration);
            if (m_time < 0.0f)
            {
                m_time += duration;
            }
        }
        else if (m_time >= duration)
        {
            m_time = duration;
            m_playing = fading; // keep ticking so a cross-fade can finish
        }
        else if (m_time < 0.0f)
        {
            m_time = 0.0f;
            m_playing = fading;
        }

        if (fading)
        {
            const SkeletalNamedClipData& target = m_config.m_clips[m_fadeIndex];
            const float targetDuration = AZ::GetMax(target.m_duration, AZ::Constants::FloatEpsilon);
            m_fadeTime += deltaTime * m_config.m_speed;
            if (target.m_looping)
            {
                m_fadeTime = std::fmod(m_fadeTime, targetDuration);
                if (m_fadeTime < 0.0f)
                {
                    m_fadeTime += targetDuration;
                }
            }
            else
            {
                m_fadeTime = AZ::GetClamp(m_fadeTime, 0.0f, targetDuration);
            }

            m_fadeElapsed += deltaTime >= 0.0f ? deltaTime : -deltaTime; // fade by wall-time magnitude
            const float weight = SkeletalClip::CrossfadeWeight(m_fadeElapsed, m_fadeDuration);
            ApplySkeletalPoseBlended(m_config.m_tracks, m_time, target.m_tracks, m_fadeTime, m_bones, weight);

            if (weight >= 1.0f)
            {
                // The fade finished: promote the target to the current clip.
                m_config.m_tracks = target.m_tracks;
                m_config.m_duration = target.m_duration;
                m_config.m_looping = target.m_looping;
                m_time = m_fadeTime;
                m_fadeIndex = -1;
            }
            return;
        }

        ApplySkeletalPose(m_config, m_bones, m_time);
    }

    void DioramaSkeletalClipComponent::CrossFadeTo(const AZStd::string& clipName, float durationSeconds)
    {
        for (size_t i = 0; i < m_config.m_clips.size(); ++i)
        {
            if (m_config.m_clips[i].m_name != clipName)
            {
                continue;
            }
            m_playing = true; // ensure the player ticks the fade
            const float fadeDuration = durationSeconds > 0.0f ? durationSeconds : 0.0f;
            if (fadeDuration <= 0.0f)
            {
                // Instant switch: promote immediately and pose the target's start.
                m_config.m_duration = m_config.m_clips[i].m_duration;
                m_config.m_looping = m_config.m_clips[i].m_looping;
                m_config.m_tracks = m_config.m_clips[i].m_tracks;
                m_time = 0.0f;
                m_fadeIndex = -1;
                ApplySkeletalPose(m_config, m_bones, m_time);
                return;
            }
            m_fadeIndex = static_cast<int>(i);
            m_fadeTime = 0.0f;
            m_fadeElapsed = 0.0f;
            m_fadeDuration = fadeDuration;
            return;
        }
        // Unknown clip name: ignored (forgiving bus).
    }

    void DioramaSkeletalClipComponent::Play()
    {
        m_playing = true;
    }

    void DioramaSkeletalClipComponent::Stop()
    {
        m_playing = false;
    }

    void DioramaSkeletalClipComponent::SetNormalizedTime(float normalizedTime)
    {
        const float clamped = AZ::GetClamp(normalizedTime, 0.0f, 1.0f);
        m_time = clamped * AZ::GetMax(m_config.m_duration, AZ::Constants::FloatEpsilon);
        ApplySkeletalPose(m_config, m_bones, m_time);
    }

    void DioramaSkeletalClipComponent::SetSpeed(float speed)
    {
        m_config.m_speed = speed;
    }

    void DioramaSkeletalClipComponent::SetLooping(bool looping)
    {
        m_config.m_looping = looping;
    }

    void DioramaSkeletalClipComponent::SetDuration(float seconds)
    {
        m_config.m_duration = AZ::GetMax(seconds, AZ::Constants::FloatEpsilon);
    }

    bool DioramaSkeletalClipComponent::IsPlaying()
    {
        return m_playing;
    }
} // namespace Diorama
