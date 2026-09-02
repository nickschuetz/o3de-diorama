/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Vector3.h>
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
    //! entity. Reflected Common, so a script, Script Canvas, or an agent drives the rig
    //! the same way it drives every other Diorama feature: play authored DragonBones clips
    //! (`PlayAnimation` / `StopAnimation` / `SetAnimationSpeed`) and layer per-bone pose
    //! overrides (an extra rotation / translation per named bone) on top of the playing pose.
    class DioramaSkinnedSpriteRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaSkinnedSpriteRequests, DioramaSkinnedSpriteRequestsTypeId);
        virtual ~DioramaSkinnedSpriteRequests() = default;

        //! Play an authored clip by name from the start; looping overrides the clip's own
        //! loop flag. An unknown name stops playback (the rig holds its bind pose).
        virtual void PlayAnimation(const AZStd::string& name, bool looping) = 0;

        //! Stop animation playback, holding the bind pose (plus any bone overrides).
        virtual void StopAnimation() = 0;

        //! Set the playback rate multiplier (negative plays in reverse).
        virtual void SetAnimationSpeed(float speed) = 0;

        //! Add an extra rotation (degrees) at the named bone, on top of the animated (or bind)
        //! pose, rotating that bone and its descendants about the bone's own origin. This is
        //! the primitive that bends a limb; unknown bone names are ignored.
        virtual void SetBoneRotation(const AZStd::string& boneName, float degrees) = 0;

        //! Add an extra translation (x, y, in armature units) at the named bone, on top of
        //! its bind pose. Unknown bone names are ignored.
        virtual void SetBoneTranslation(const AZStd::string& boneName, float x, float y) = 0;

        //! Clear every pose override, returning the rig to its bind pose.
        virtual void ResetPose() = 0;

        //! Whether the loaded armature has a bone with this name.
        virtual bool HasBone(const AZStd::string& boneName) = 0;

        //! World-space position of the named bone in the current pose (bind pose plus the
        //! playing clip plus any overrides), mapped through the entity's own basis exactly
        //! like the mesh's vertices (billboard is a visual effect and does not move bones).
        //! This is what pins a hitbox, an effect, or an attachment to a deforming limb.
        //! An unknown bone (or no loaded rig) returns the entity's world translation, so a
        //! caller that skipped HasBone degrades to the rig origin rather than garbage.
        virtual AZ::Vector3 GetBoneWorldPosition(const AZStd::string& boneName) = 0;

        //! Read-only snapshot of the loaded rig and draw state.
        virtual SkinnedSpriteInfo GetSkinnedSpriteInfo() = 0;
        //! Advance on the 2D Simulation Clock's fixed steps instead of the render tick, so the
        //! animation is deterministic and rollback-exact. With no clock in the level, falls back
        //! to the render tick (editor preview included).
        virtual void SetUseSimClock(bool enabled) = 0;
        //! Whether the rig advances on the simulation clock (see SetUseSimClock).
        virtual bool GetUseSimClock() = 0;
    };

    using DioramaSkinnedSpriteRequestBus = AZ::EBus<DioramaSkinnedSpriteRequests>;

    //! Reflect the skinned-sprite bus + info struct to the BehaviorContext (Common
    //! scope). Called from the skinned-sprite component's Reflect.
    void ReflectSkinnedSpriteBuses(AZ::ReflectContext* context);
} // namespace Diorama
