/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaSkinnedSpriteBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void SkinnedSpriteInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SkinnedSpriteInfo>()
                ->Version(1)
                ->Field("loaded", &SkinnedSpriteInfo::m_loaded)
                ->Field("visible", &SkinnedSpriteInfo::m_visible)
                ->Field("boneCount", &SkinnedSpriteInfo::m_boneCount)
                ->Field("meshCount", &SkinnedSpriteInfo::m_meshCount)
                ->Field("vertexCount", &SkinnedSpriteInfo::m_vertexCount);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<SkinnedSpriteInfo>("DioramaSkinnedSpriteInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/SkinnedSprite")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("loaded", BehaviorValueGetter(&SkinnedSpriteInfo::m_loaded), nullptr)
                ->Property("visible", BehaviorValueGetter(&SkinnedSpriteInfo::m_visible), nullptr)
                ->Property("boneCount", BehaviorValueGetter(&SkinnedSpriteInfo::m_boneCount), nullptr)
                ->Property("meshCount", BehaviorValueGetter(&SkinnedSpriteInfo::m_meshCount), nullptr)
                ->Property("vertexCount", BehaviorValueGetter(&SkinnedSpriteInfo::m_vertexCount), nullptr);
        }
    }

    void ReflectSkinnedSpriteBuses(AZ::ReflectContext* context)
    {
        SkinnedSpriteInfo::Reflect(context);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaSkinnedSpriteRequestBus>("DioramaSkinnedSpriteRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/SkinnedSprite")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event(
                "PlayAnimation",
                &DioramaSkinnedSpriteRequestBus::Events::PlayAnimation,
                { { { "name", "Clip name to play from the start." }, { "looping", "Loop the clip (overrides its own loop flag)." } } })
            ->Event("StopAnimation", &DioramaSkinnedSpriteRequestBus::Events::StopAnimation)
            ->Event(
                "SetAnimationSpeed",
                &DioramaSkinnedSpriteRequestBus::Events::SetAnimationSpeed,
                { { { "speed", "Playback rate multiplier (negative plays in reverse)." } } })
            ->Event(
                "SetBoneRotation",
                &DioramaSkinnedSpriteRequestBus::Events::SetBoneRotation,
                { { { "boneName", "Name of the bone to bend." },
                    { "degrees", "Extra rotation added at the bone, on top of its bind pose." } } })
            ->Event(
                "SetBoneTranslation",
                &DioramaSkinnedSpriteRequestBus::Events::SetBoneTranslation,
                { { { "boneName", "Name of the bone to shift." },
                    { "x", "Extra X translation (armature units) added at the bone." },
                    { "y", "Extra Y translation (armature units) added at the bone." } } })
            ->Event("ResetPose", &DioramaSkinnedSpriteRequestBus::Events::ResetPose)
            ->Event(
                "HasBone",
                &DioramaSkinnedSpriteRequestBus::Events::HasBone,
                { { { "boneName", "Bone name to look up in the loaded armature." } } })
            ->Event(
                "GetBoneWorldPosition",
                &DioramaSkinnedSpriteRequestBus::Events::GetBoneWorldPosition,
                { { { "boneName",
                      "Bone whose posed world position to return; an unknown name returns the entity's world translation." } } })
            ->Event("GetSkinnedSpriteInfo", &DioramaSkinnedSpriteRequestBus::Events::GetSkinnedSpriteInfo)
            ->Event(
                "SetUseSimClock",
                &DioramaSkinnedSpriteRequestBus::Events::SetUseSimClock,
                { { { "enabled",
                      "Advance on the 2D Simulation Clock's fixed steps (deterministic / rollback-exact) vs the render tick." } } })
            ->Event("GetUseSimClock", &DioramaSkinnedSpriteRequestBus::Events::GetUseSimClock);
    }
} // namespace Diorama
