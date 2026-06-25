/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SpriteBatchPlan.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

#include <Atom/RHI.Reflect/Format.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawContext.h>
#include <Atom/RPI.Public/FeatureProcessor.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

namespace Diorama
{
    //! Scene feature processor that renders every active Diorama sprite as a
    //! world-space textured quad, batching sprites that share a texture and sort
    //! key into a single draw call. Replaces the earlier immediate-mode
    //! SpriteRenderer: it owns one DynamicDrawContext and submits batched draws
    //! from Render(), the idiomatic Atom scene-integration point.
    //!
    //! Components register through AcquireSprite/ReleaseSprite and push transform
    //! and configuration each time they change via UpdateSprite. The processor is
    //! free of any editor or tool dependency so it ships in the runtime module and
    //! also drives the editor viewport preview (both go through the scene).
    class SpriteFeatureProcessor final : public AZ::RPI::FeatureProcessor
    {
    public:
        AZ_CLASS_ALLOCATOR(SpriteFeatureProcessor, AZ::SystemAllocator)
        AZ_RTTI(Diorama::SpriteFeatureProcessor, "{2C0E8C2A-9B1E-4F3A-8D6C-1A2B3C4D5E6F}", AZ::RPI::FeatureProcessor);

        static void Reflect(AZ::ReflectContext* context);

        using SpriteHandle = AZ::u32;
        static constexpr SpriteHandle InvalidHandle = 0;

        using LightHandle = AZ::u32;
        //! Max gem-native lights gathered into the per-draw lighting constants each
        //! frame. Fixed + small (the shader loops it); bounded so no scene value
        //! sizes a GPU buffer. Must match DIORAMA_MAX_LIGHTS in DioramaSprite.azsl.
        static constexpr int MaxLights = 8;

        //! A gem-native 2D light, gathered by the feature processor into the sprite
        //! shader's per-draw lighting constants. Diorama owns its lights because
        //! Atom's light feature processors keep their CPU data private and SceneSrg
        //! is not guaranteed for our dynamic draws (see Docs/design/2d-lighting.md).
        struct LightData2D
        {
            bool m_isDirectional = false;
            AZ::Vector3 m_position = AZ::Vector3::CreateZero(); //!< point light world position
            AZ::Vector3 m_direction = AZ::Vector3(0.0f, 0.0f, -1.0f); //!< directional: travel direction
            AZ::Color m_color = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
            float m_intensity = 1.0f;
            float m_radius = 10.0f; //!< point light attenuation radius (world units)
            bool m_enabled = true;
        };

        SpriteFeatureProcessor() = default;
        virtual ~SpriteFeatureProcessor() = default;

        // RPI::FeatureProcessor
        void Activate() override;
        void Deactivate() override;
        void Render(const RenderPacket& packet) override;

        //! Register a sprite for drawing. Returns a stable handle (0 on failure).
        SpriteHandle AcquireSprite();

        //! Stop drawing the sprite for the given handle.
        void ReleaseSprite(SpriteHandle handle);

        //! Update the world transform and configuration of a registered sprite.
        void UpdateSprite(SpriteHandle handle, const AZ::Transform& worldTransform, const SpriteComponentConfig& config);

        //! Register a 2D light. Returns a stable handle (0 on failure).
        LightHandle AcquireLight();
        //! Stop using the light for the given handle.
        void ReleaseLight(LightHandle handle);
        //! Update a registered light's parameters.
        void UpdateLight(LightHandle handle, const LightData2D& light);

    private:
        struct SpriteVertex
        {
            float m_position[3];
            AZ::u32 m_color;
            float m_uv[2];
        };

        struct SpriteEntry
        {
            AZ::Transform m_worldTransform = AZ::Transform::CreateIdentity();
            SpriteComponentConfig m_config;
            //! Cached batch key for m_config's texture, refreshed in UpdateSprite
            //! so Render() does not recompute it per sprite per frame.
            SpriteBatchPlan::BatchKey m_batchKey;
        };

        bool EnsureInitialized();
        void AppendQuad(const SpriteEntry& entry, const AZ::Transform& cameraTransform, SpriteVertex outVertices[4]) const;
        //! Build a flat, ground-plane quad under a sprite for its drop shadow (lies in
        //! world XY at the sprite's position, squashed to an ellipse footprint, tinted
        //! black with the given alpha).
        void AppendShadowQuad(const SpriteEntry& entry, float alpha, SpriteVertex outVertices[4]) const;
        //! Draw one batched pass of ground shadows for all shadow-casting (billboarded)
        //! sprites, using the shared soft shadow texture, at the given sort key (placed
        //! above the floor and below the sprites).
        void DrawShadows(AZ::RHI::DrawItemSortKey sortKey, const AZ::Transform& cameraTransform);

        //! Sprite shader asset, loaded asynchronously starting in Activate(). It
        //! must never be loaded with a blocking call from Render(): Render() runs
        //! as a render job that the main thread waits on, so a blocking asset load
        //! there deadlocks the editor/runtime. EnsureInitialized() only polls
        //! IsReady() and builds the draw context once the asset has loaded.
        AZ::Data::Asset<AZ::RPI::ShaderAsset> m_shaderAsset;
        AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> m_dynamicDraw;
        bool m_initialized = false;

        //! Shared soft shadow texture (a gem asset), loaded asynchronously in
        //! Activate() like the shader; resolved to an Image in DrawShadows() once
        //! ready. Shadows simply do not draw until it has loaded.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_shadowImageAsset;

        //! Default texture substituted when a Sprite has no texture assigned, so a
        //! freshly added Sprite renders (tintable) instead of being dropped from
        //! batching. Loaded asynchronously in Activate() like the shadow texture.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_defaultImageAsset;

        AZStd::unordered_map<SpriteHandle, SpriteEntry> m_sprites;
        SpriteHandle m_nextHandle = 1;

        // Gem-native 2D lights. Components register here through AcquireLight and
        // push parameters via UpdateLight; Render() gathers up to MaxLights into the
        // per-draw lighting constants each frame and binds them on every batch.
        AZStd::unordered_map<LightHandle, LightData2D> m_lights;
        LightHandle m_nextLightHandle = 1;

        //! Pack the active lights + ambient into the given draw SRG's lighting
        //! constants. Reads the r_dioramaSpriteLighting / r_dioramaSpriteAmbient
        //! CVars; when lighting is off it writes ambient=1 and zero lights so the
        //! shader's lit math collapses to the unlit albedo * vertex color.
        void SetLightingConstants(AZ::RPI::ShaderResourceGroup* drawSrg);

        //! Pack the per-draw material constants (v1b): whether this batch has a normal
        //! map, and the billboard's camera-aligned tangent basis used to bring a
        //! tangent-space normal into world space. Set on every batch alongside the
        //! lighting constants so the shader's normal-mapped path has its inputs.
        void SetMaterialConstants(
            AZ::RPI::ShaderResourceGroup* drawSrg,
            const AZ::Transform& cameraTransform,
            bool hasNormalMap,
            float flashAmount,
            const AZ::Color& flashColor,
            float outlineThickness,
            const AZ::Color& outlineColor,
            const AZ::Color& emissiveColor,
            float emissiveIntensity,
            bool pointFilter,
            float paletteStrength,
            const AZ::Color& paletteShadow,
            const AZ::Color& paletteMid,
            const AZ::Color& paletteHighlight);

        // The batch plan (grouping + ordering) only changes when a sprite is
        // added, removed, or its batch key (texture or sort layer) changes. It is
        // rebuilt lazily on the next Render() when this is set, so static scenes
        // skip the per-frame scan and sort. Transform moves, animation frame
        // changes, and texture streaming do not dirty the plan; they are read
        // through the cached entry pointers when packing vertices each frame.
        bool m_planDirty = true;

        // Cached batch plan (valid while m_planDirty is false). The entry pointers
        // stay valid between rebuilds because add/remove set m_planDirty and
        // AZStd::unordered_map keeps element pointers stable across insert/erase.
        AZStd::vector<const SpriteEntry*> m_entryScratch;
        AZStd::vector<SpriteBatchPlan::Item> m_orderedScratch;
        AZStd::vector<SpriteBatchPlan::Batch> m_batchScratch;

        // Reused per-frame scratch buffers so Render() allocates nothing on the
        // steady-state path.
        AZStd::vector<SpriteBatchPlan::Item> m_itemScratch;
        AZStd::vector<SpriteVertex> m_vertexScratch;
        AZStd::vector<AZ::u32> m_indexScratch;

        // Separate scratch for the shadow pass so it does not clobber the per-batch
        // sprite buffers above (the shadow pass runs in the middle of the batch loop).
        AZStd::vector<SpriteVertex> m_shadowVertexScratch;
        AZStd::vector<AZ::u32> m_shadowIndexScratch;
    };
} // namespace Diorama
