/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a skinned sprite, for the agent-facing query API. A
    //! read-only snapshot so a script / Script Canvas / agent can inspect the loaded rig
    //! the same way it reads sprites and cameras (the parity Info-getter pattern).
    struct SkinnedSpriteInfo
    {
        AZ_TYPE_INFO(Diorama::SkinnedSpriteInfo, SkinnedSpriteInfoTypeId);
        static void Reflect(AZ::ReflectContext* context);

        bool m_loaded = false; //!< the DragonBones armature parsed and built
        bool m_visible = false; //!< registered with a feature processor and drawing
        int m_boneCount = 0;
        int m_meshCount = 0;
        int m_vertexCount = 0; //!< total skinned vertices across all meshes
    };

    //! Typed, agent-facing API for a skinned (mesh-deform) sprite, addressed by its
    //! entity. Reflected Common, so a script, Script Canvas, or an agent poses the rig
    //! the same way it drives every other Diorama feature. In this phase the pose is
    //! driven directly (an extra rotation / offset per named bone); authored DragonBones
    //! clips are a following phase and will layer under the same bus.
    class DioramaSkinnedSpriteRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaSkinnedSpriteRequests, DioramaSkinnedSpriteRequestsTypeId);
        virtual ~DioramaSkinnedSpriteRequests() = default;

        //! Add an extra rotation (degrees) at the named bone, on top of its bind pose,
        //! rotating that bone and its descendants about the bone's own origin. This is
        //! the primitive that bends a limb; unknown bone names are ignored.
        virtual void SetBoneRotation(const AZStd::string& boneName, float degrees) = 0;

        //! Add an extra translation (in armature units) at the named bone, on top of its
        //! bind pose. Unknown bone names are ignored.
        virtual void SetBoneTranslation(const AZStd::string& boneName, const AZ::Vector2& offset) = 0;

        //! Clear every pose override, returning the rig to its bind pose.
        virtual void ResetPose() = 0;

        //! Read-only snapshot of the loaded rig and draw state.
        virtual SkinnedSpriteInfo GetSkinnedSpriteInfo() = 0;
    };

    using DioramaSkinnedSpriteRequestBus = AZ::EBus<DioramaSkinnedSpriteRequests>;

    //! Reflect the skinned-sprite bus + info struct to the BehaviorContext (Common
    //! scope). Called from the skinned-sprite component's Reflect.
    void ReflectSkinnedSpriteBuses(AZ::ReflectContext* context);
} // namespace Diorama
