/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DragonBonesImport.h>
#include <Clients/MeshSkin.h>
#include <Clients/SpriteFeatureProcessor.h>
#include <Clients/SurfaceDeform.h>
#include <Diorama/DioramaSkinnedSpriteBus.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace Diorama
{
    //! Configuration for a skinned (mesh-deform) sprite: which DragonBones armature to
    //! load, the atlas texture its UVs address, and how it is placed and shaded.
    class DioramaSkinnedSpriteConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaSkinnedSpriteConfig, DioramaSkinnedSpriteConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaSkinnedSpriteConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaSkinnedSpriteConfig() override = default;

        //! Path to the DragonBones "*_ske.json" armature, resolved through the file IO
        //! aliases. For a shipped asset use the product path ("@products@/mygame/hero_ske.json"),
        //! which reads from the asset cache / pak; "@projectroot@/..." also works for a source
        //! file during authoring. The companion "*_tex.json" atlas is loaded from the same path
        //! (with "_ske" swapped for "_tex").
        AZStd::string m_sourcePath;
        //! Armature to use from the document; empty selects the first.
        AZStd::string m_armatureName;
        //! Atlas texture the mesh UVs (0..1) address.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_texture;
        //! Armature-units-to-world-units scale. DragonBones rigs are authored in pixels
        //! (hundreds/thousands of units tall), so this is small.
        float m_scale = 0.01f;
        //! Flip the vertical axis. DragonBones is y-down; on by default so the character
        //! stands upright in O3DE's y-up world.
        bool m_flipVertical = true;
        //! Face the camera like a sprite (true) or lie in the entity's local plane.
        bool m_billboard = true;
        //! Nearest-neighbor texture sampling (crisp pixel art) vs bilinear.
        bool m_pointFilter = true;
        //! Draw-order bias; higher draws later (on top of lower-offset meshes/sprites).
        float m_sortOffset = 0.0f;
        //! Multiplied into every vertex color.
        AZ::Color m_tint = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
        //! Animation clip to play on activate; empty leaves the rig in its bind pose.
        AZStd::string m_animationName;
        //! Start playing the clip automatically on activate.
        bool m_autoPlay = true;
        //! Playback rate multiplier (negative plays in reverse).
        float m_speed = 1.0f;
    };

    //! Shared helper that turns a DragonBones weighted-mesh config into deformed geometry on
    //! the sprite feature processor. It loads the armature, poses the bone hierarchy each
    //! frame (bind pose + per-bone overrides), CPU-skins every mesh with the pure MeshSkin
    //! core, and submits the result. Both the runtime component and the editor twin embed
    //! one, so the character deforms identically in game and in the editor viewport preview
    //! (the same pattern SpritePresenter uses for sprites). No Qt / tools dependency, so it
    //! lives in the runtime client code and the editor module reuses it.
    class SkinnedSpritePresenter
    {
    public:
        SkinnedSpritePresenter() = default;
        ~SkinnedSpritePresenter() = default;

        //! Store the config and (re)build the rig from it. Safe to call from Activate or on
        //! an editor property edit. Returns true if the armature loaded.
        bool SetConfig(const DioramaSkinnedSpriteConfig& config);

        //! Begin presenting for the given entity. Records the entity (its transform is read
        //! each tick) and clears any prior feature-processor handles.
        void Connect(AZ::EntityId entityId);

        //! Release the feature-processor handles. Safe from Deactivate.
        void Disconnect();

        //! Advance the animation by deltaTime, acquire the feature processor if needed, then
        //! pose + skin + push. Retried each call until the scene exists (level load / editor
        //! ordering).
        void Tick(float deltaTime);

        //! Play a clip by name from the start; looping overrides the clip's own loop flag.
        //! An unknown name stops playback (holds the bind pose + any bus overrides).
        void PlayAnimation(const AZStd::string& name, bool looping);
        //! Stop playback, holding the current pose's bind base.
        void StopAnimation();
        //! Set the playback rate (negative plays in reverse).
        void SetAnimationSpeed(float speed);

        //! Pose overrides (agent-facing), added on top of the animated (or bind) pose.
        void SetBoneRotation(const AZStd::string& boneName, float degrees);
        void SetBoneTranslation(const AZStd::string& boneName, float x, float y);
        void ResetPose();

        //! Read-only snapshot of the loaded rig and draw state.
        SkinnedSpriteInfo GetInfo() const;

    private:
        struct RuntimeMesh
        {
            const DragonBones::SkinnedMesh* m_source = nullptr;
            AZ::u32 m_handle = 0;
            AZStd::vector<MeshSkin::Affine2D> m_skinMatrices;
            AZStd::vector<AZ::u32> m_indices;
        };

        //! A surface-bound mesh: warped by its surface bone's control-point grid rather than
        //! skinned. m_surfaceBoneIndex indexes m_bones / m_surfaceGrids.
        struct SurfaceRuntimeMesh
        {
            const DragonBones::SurfaceMesh* m_source = nullptr;
            AZ::u32 m_handle = 0;
            int m_surfaceBoneIndex = -1;
            AZStd::vector<AZ::u32> m_indices;
        };

        bool BuildRig();
        bool TryAcquireFeatureProcessor();
        void SkinAndPush();
        //! Rebuild m_surfaceGrids for each surface bone from its bind control points plus the
        //! current animated deform deltas; nested surfaces are warped through their parent.
        //! Pass the posed bone worlds so nested-surface control points land correctly.
        void BuildSurfaceGrids();

        DioramaSkinnedSpriteConfig m_config;
        AZ::EntityId m_entityId;

        DragonBones::Document m_document;
        const DragonBones::Armature* m_armature = nullptr;
        AZStd::vector<MeshSkin::Bone> m_bones; //!< parent index per bone (bind local for reference)
        AZStd::unordered_map<AZStd::string, int> m_boneNameToIndex;
        AZStd::vector<RuntimeMesh> m_meshes;
        AZStd::vector<SurfaceRuntimeMesh> m_surfaceMeshes;
        //! One grid per bone (only surface bones are populated), rebuilt each frame from bind
        //! control points + animated deform deltas. Indexed by bone index.
        AZStd::vector<SurfaceDeform::SurfaceGrid> m_surfaceGrids;
        AZ::Vector2 m_center = AZ::Vector2::CreateZero();
        bool m_rigBuilt = false;

        // Animation playback: the active clip, playback time, and state.
        const DragonBones::Animation* m_animation = nullptr;
        float m_animTime = 0.0f;
        bool m_playing = false;
        bool m_loop = true;

        // Bus pose overrides (degrees / armature units), added on top of the animated pose.
        AZStd::vector<float> m_boneRotationDelta;
        AZStd::vector<AZ::Vector2> m_boneTranslationDelta;
        // Reused per-frame scratch.
        AZStd::vector<DragonBones::BonePoseDelta> m_poseScratch;
        AZStd::vector<MeshSkin::Affine2D> m_localOverrides;
        AZStd::vector<MeshSkin::Affine2D> m_world;
        AZStd::vector<SpriteFeatureProcessor::MeshVertex> m_vertexScratch;
        AZStd::vector<AZ::Vector2> m_deformScratch; //!< sampled surface control-point deltas

        SpriteFeatureProcessor* m_featureProcessor = nullptr;
        bool m_handlesAcquired = false;
    };
} // namespace Diorama
