/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SpriteFeatureProcessor.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

namespace Diorama
{
    namespace
    {
        // Product path of the sprite shader compiled from Assets/Diorama/Shaders.
        constexpr const char* SpriteShaderPath = "diorama/shaders/dioramasprite.azshader";

        // Shared soft shadow blob shipped with the gem (Assets/Diorama/Textures/shadow.png).
        constexpr const char* ShadowTexturePath = "diorama/textures/shadow.png.streamingimage";

        // Default texture for a Sprite with no texture assigned, so a freshly added
        // Sprite is visible (and tintable) instead of being dropped from batching.
        constexpr const char* DefaultTexturePath = "diorama/textures/white_sprite.png.streamingimage";

        // Lazily constructed: a static AZ::Name constructed at namespace scope
        // would run before the NameDictionary exists and crash on load. See the
        // matching note that prompted this pattern across the gem.
        const AZ::Name& TextureSrgInput()
        {
            static const AZ::Name name{ "m_texture" };
            return name;
        }

        const AZ::Name& NormalMapSrgInput()
        {
            static const AZ::Name name{ "m_normalMap" };
            return name;
        }

        // The batch key identifies sprites that can draw together: same texture
        // asset and same sort layer. The full asset id is used (not a hash) so
        // distinct textures never collide, and the sort offset is kept as a float
        // so fractional layers stay distinct.
        SpriteBatchPlan::BatchKey MakeBatchKey(const SpriteComponentConfig& config)
        {
            return SpriteBatchPlan::BatchKey{ config.m_texture.GetId(),      config.m_sortOffset,         config.m_normalMap.GetId(),
                                              config.m_flashAmount,          config.m_flashColor.ToU32(), config.m_outlineThickness,
                                              config.m_outlineColor.ToU32(), config.m_emissiveIntensity,  config.m_emissiveColor.ToU32(),
                                              config.m_pointFilter };
        }
    } // namespace

    // When on, sprites are ordered by distance to the camera every frame so nearer
    // ones draw in front (2.5D depth ordering), since the sprite shader does not
    // depth-test. Ordering is within each sort layer, so give the ground/background
    // a lower sort offset to keep it behind. Off restores the cached static order
    // driven solely by sort offset + texture.
    AZ_CVAR(
        bool,
        r_dioramaSpriteDepthSort,
        true,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Order Diorama sprites by camera distance each frame so nearer sprites draw in front (within their sort layer).");

    // Soft ground shadows under billboarded sprites: a 2.5D grounding cue. Drawn in
    // one batch just above the floor, below the sprites.
    AZ_CVAR(
        bool,
        r_dioramaSpriteShadows,
        true,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Render soft drop shadows on the ground under billboarded Diorama sprites.");
    AZ_CVAR(float, r_dioramaShadowAlpha, 0.35f, nullptr, AZ::ConsoleFunctorFlags::Null, "Opacity of Diorama sprite ground shadows (0..1).");

    // Gem-native 2D lighting: sprites are modulated by the registered DioramaLight
    // components (gathered into the per-draw lighting constants). Off, or a scene
    // with no lights, leaves sprites at full brightness (ambient forced to 1), so
    // existing unlit scenes are unchanged.
    AZ_CVAR(
        bool,
        r_dioramaSpriteLighting,
        true,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Modulate Diorama sprites by registered 2D lights (no effect in scenes without DioramaLight).");
    AZ_CVAR(
        float,
        r_dioramaSpriteAmbient,
        0.35f,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Ambient light level for Diorama sprites when a scene has 2D lights (0..1).");

    void SpriteFeatureProcessor::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SpriteFeatureProcessor, AZ::RPI::FeatureProcessor>()->Version(0);
        }
    }

    void SpriteFeatureProcessor::Activate()
    {
        m_initialized = false;

        // Kick off the shader load asynchronously (QueueLoad, non-blocking). The
        // draw context is created in EnsureInitialized once the asset is ready.
        // Crucially this never blocks: EnsureInitialized runs inside Render(),
        // which executes as a render job the main thread waits on, so a blocking
        // load there would deadlock the application.
        const AZ::Data::AssetId shaderAssetId =
            AZ::RPI::AssetUtils::GetAssetIdForProductPath(SpriteShaderPath, AZ::RPI::AssetUtils::TraceLevel::Warning);
        if (shaderAssetId.IsValid())
        {
            m_shaderAsset =
                AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::ShaderAsset>(shaderAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
            m_shaderAsset.QueueLoad();
        }

        // Soft shadow texture (non-blocking load, same pattern as the shader).
        const AZ::Data::AssetId shadowImageId =
            AZ::RPI::AssetUtils::GetAssetIdForProductPath(ShadowTexturePath, AZ::RPI::AssetUtils::TraceLevel::Warning);
        if (shadowImageId.IsValid())
        {
            m_shadowImageAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(
                shadowImageId, AZ::Data::AssetLoadBehavior::PreLoad);
            m_shadowImageAsset.QueueLoad();
        }

        // Default sprite texture (same non-blocking pattern), substituted in
        // UpdateSprite when a Sprite has no texture so it renders visibly.
        const AZ::Data::AssetId defaultImageId =
            AZ::RPI::AssetUtils::GetAssetIdForProductPath(DefaultTexturePath, AZ::RPI::AssetUtils::TraceLevel::Warning);
        if (defaultImageId.IsValid())
        {
            m_defaultImageAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(
                defaultImageId, AZ::Data::AssetLoadBehavior::PreLoad);
            m_defaultImageAsset.QueueLoad();
        }
    }

    void SpriteFeatureProcessor::Deactivate()
    {
        m_dynamicDraw = nullptr;
        m_shaderAsset = {};
        m_shadowImageAsset = {};
        m_initialized = false;
        m_sprites.clear();

        // Drop the cached plan (its entry pointers are about to dangle) and force
        // a rebuild if this processor is reactivated.
        m_entryScratch.clear();
        m_orderedScratch.clear();
        m_batchScratch.clear();
        m_planDirty = true;
    }

    SpriteFeatureProcessor::SpriteHandle SpriteFeatureProcessor::AcquireSprite()
    {
        const SpriteHandle handle = m_nextHandle++;
        m_sprites.emplace(handle, SpriteEntry{});
        m_planDirty = true; // a new sprite changes the batch set
        return handle;
    }

    void SpriteFeatureProcessor::ReleaseSprite(SpriteHandle handle)
    {
        if (m_sprites.erase(handle) != 0)
        {
            m_planDirty = true; // a removed sprite changes the batch set
        }
    }

    void SpriteFeatureProcessor::UpdateSprite(SpriteHandle handle, const AZ::Transform& worldTransform, const SpriteComponentConfig& config)
    {
        auto it = m_sprites.find(handle);
        if (it == m_sprites.end())
        {
            return;
        }

        SpriteEntry& entry = it->second;
        entry.m_worldTransform = worldTransform;
        entry.m_config = config;

        // Robustness: a Sprite with no texture assigned is otherwise dropped from
        // batching (invisible). Substitute the bundled default texture so a freshly
        // added Sprite renders; the per-entry Tint still applies on top.
        if (!entry.m_config.m_texture.GetId().IsValid() && m_defaultImageAsset.GetId().IsValid())
        {
            entry.m_config.m_texture = m_defaultImageAsset;
        }

        // Only a texture or sort-layer change affects batching; transform and
        // animation changes are read per frame and do not dirty the plan. Key off
        // entry.m_config so the default-texture substitution is reflected.
        const SpriteBatchPlan::BatchKey newKey = MakeBatchKey(entry.m_config);
        if (newKey != entry.m_batchKey)
        {
            entry.m_batchKey = newKey;
            m_planDirty = true;
        }
    }

    SpriteFeatureProcessor::LightHandle SpriteFeatureProcessor::AcquireLight()
    {
        const LightHandle handle = m_nextLightHandle++;
        m_lights.emplace(handle, LightData2D{});
        return handle;
    }

    void SpriteFeatureProcessor::ReleaseLight(LightHandle handle)
    {
        m_lights.erase(handle);
    }

    void SpriteFeatureProcessor::UpdateLight(LightHandle handle, const LightData2D& light)
    {
        auto it = m_lights.find(handle);
        if (it != m_lights.end())
        {
            it->second = light;
        }
    }

    void SpriteFeatureProcessor::SetLightingConstants(AZ::RPI::ShaderResourceGroup* drawSrg)
    {
        // Pack up to MaxLights enabled lights into parallel float4 arrays matching
        // the DioramaSprite shader's SpriteSrg lighting constants.
        float posDir[MaxLights * 4] = {};
        float color[MaxLights * 4] = {};
        int count = 0;

        const bool lightingOn = static_cast<bool>(r_dioramaSpriteLighting);
        if (lightingOn)
        {
            for (const auto& [handle, light] : m_lights)
            {
                if (!light.m_enabled || count >= MaxLights)
                {
                    continue;
                }
                const int b = count * 4;
                const float r = light.m_color.GetR() * light.m_intensity;
                const float g = light.m_color.GetG() * light.m_intensity;
                const float bl = light.m_color.GetB() * light.m_intensity;
                if (light.m_isDirectional)
                {
                    const AZ::Vector3 toLight = (-light.m_direction).GetNormalizedSafe();
                    posDir[b + 0] = toLight.GetX();
                    posDir[b + 1] = toLight.GetY();
                    posDir[b + 2] = toLight.GetZ();
                    posDir[b + 3] = 1.0f; // directional marker
                    color[b + 0] = r;
                    color[b + 1] = g;
                    color[b + 2] = bl;
                    color[b + 3] = 0.0f;
                }
                else
                {
                    posDir[b + 0] = light.m_position.GetX();
                    posDir[b + 1] = light.m_position.GetY();
                    posDir[b + 2] = light.m_position.GetZ();
                    posDir[b + 3] = 0.0f; // point marker
                    color[b + 0] = r;
                    color[b + 1] = g;
                    color[b + 2] = bl;
                    color[b + 3] = light.m_radius > 0.0f ? 1.0f / (light.m_radius * light.m_radius) : 0.0f;
                }
                ++count;
            }
        }

        // No lights (or lighting off) -> full-bright ambient, so unlit scenes are
        // unchanged; otherwise the configured ambient floor under the lights.
        const float ambient = (!lightingOn || count == 0) ? 1.0f : static_cast<float>(r_dioramaSpriteAmbient);
        if (!lightingOn)
        {
            count = 0;
        }

        const AZ::RHI::ShaderInputConstantIndex ambientIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_ambientAndCount" });
        if (ambientIdx.IsValid())
        {
            drawSrg->SetConstant(ambientIdx, AZ::Vector4(ambient, ambient, ambient, static_cast<float>(count)));
        }
        const AZ::RHI::ShaderInputConstantIndex posDirIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_lightPosDir" });
        if (posDirIdx.IsValid())
        {
            drawSrg->SetConstantRaw(posDirIdx, posDir, sizeof(posDir));
        }
        const AZ::RHI::ShaderInputConstantIndex colorIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_lightColor" });
        if (colorIdx.IsValid())
        {
            drawSrg->SetConstantRaw(colorIdx, color, sizeof(color));
        }
    }

    void SpriteFeatureProcessor::SetMaterialConstants(
        AZ::RPI::ShaderResourceGroup* drawSrg,
        const AZ::Transform& cameraTransform,
        bool hasNormalMap,
        float flashAmount,
        const AZ::Color& flashColor,
        float outlineThickness,
        const AZ::Color& outlineColor,
        const AZ::Color& emissiveColor,
        float emissiveIntensity,
        bool pointFilter)
    {
        // Billboard tangent basis: right and up are the camera's, the face normal is
        // their cross product (pointing toward the camera). This matches AppendQuad's
        // billboard basis, so a tangent-space normal maps correctly for billboards.
        const AZ::Matrix3x3 cameraBasis = AZ::Matrix3x3::CreateFromTransform(cameraTransform);
        const AZ::Vector3 right = cameraBasis.GetColumn(0);
        const AZ::Vector3 up = cameraBasis.GetColumn(2);
        const AZ::Vector3 fwd = right.Cross(up);

        const float flash = flashAmount < 0.0f ? 0.0f : (flashAmount > 1.0f ? 1.0f : flashAmount);
        const float outline = outlineThickness < 0.0f ? 0.0f : outlineThickness;
        const AZ::RHI::ShaderInputConstantIndex materialIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_material" });
        if (materialIdx.IsValid())
        {
            // .w carries the point-filter flag: the shader picks the nearest sampler
            // when it is >= 0.5, for crisp pixel art (see DioramaSprite.azsl).
            drawSrg->SetConstant(materialIdx, AZ::Vector4(hasNormalMap ? 1.0f : 0.0f, flash, outline, pointFilter ? 1.0f : 0.0f));
        }
        const AZ::RHI::ShaderInputConstantIndex flashIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_flashColor" });
        if (flashIdx.IsValid())
        {
            drawSrg->SetConstant(flashIdx, AZ::Vector4(flashColor.GetR(), flashColor.GetG(), flashColor.GetB(), flashColor.GetA()));
        }
        // Emissive: premultiply the color by intensity on the CPU so the shader just
        // adds it. Intensity > 1 pushes the result above HDR 1.0 and it blooms.
        const float emissive = emissiveIntensity < 0.0f ? 0.0f : emissiveIntensity;
        const AZ::RHI::ShaderInputConstantIndex emissiveIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_emissiveColor" });
        if (emissiveIdx.IsValid())
        {
            drawSrg->SetConstant(
                emissiveIdx,
                AZ::Vector4(emissiveColor.GetR() * emissive, emissiveColor.GetG() * emissive, emissiveColor.GetB() * emissive, 0.0f));
        }
        const AZ::RHI::ShaderInputConstantIndex outlineIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_outlineColor" });
        if (outlineIdx.IsValid())
        {
            drawSrg->SetConstant(
                outlineIdx, AZ::Vector4(outlineColor.GetR(), outlineColor.GetG(), outlineColor.GetB(), outlineColor.GetA()));
        }
        const AZ::RHI::ShaderInputConstantIndex rightIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_camBasisRight" });
        if (rightIdx.IsValid())
        {
            drawSrg->SetConstant(rightIdx, AZ::Vector4(right.GetX(), right.GetY(), right.GetZ(), 0.0f));
        }
        const AZ::RHI::ShaderInputConstantIndex upIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_camBasisUp" });
        if (upIdx.IsValid())
        {
            drawSrg->SetConstant(upIdx, AZ::Vector4(up.GetX(), up.GetY(), up.GetZ(), 0.0f));
        }
        const AZ::RHI::ShaderInputConstantIndex fwdIdx = drawSrg->FindShaderInputConstantIndex(AZ::Name{ "m_camBasisFwd" });
        if (fwdIdx.IsValid())
        {
            drawSrg->SetConstant(fwdIdx, AZ::Vector4(fwd.GetX(), fwd.GetY(), fwd.GetZ(), 0.0f));
        }
    }

    bool SpriteFeatureProcessor::EnsureInitialized()
    {
        if (m_initialized)
        {
            return true;
        }

        AZ::RPI::Scene* scene = GetParentScene();
        if (scene == nullptr)
        {
            return false;
        }

        // Poll the async shader load; never block here (this runs in a render
        // job). Retry on a later frame until the asset is ready.
        if (!m_shaderAsset.IsReady())
        {
            return false;
        }

        AZ::Data::Instance<AZ::RPI::Shader> shader = AZ::RPI::Shader::FindOrCreate(m_shaderAsset);
        if (!shader)
        {
            return false;
        }

        m_dynamicDraw = AZ::RPI::DynamicDrawInterface::Get()->CreateDynamicDrawContext();
        if (!m_dynamicDraw)
        {
            return false;
        }

        m_dynamicDraw->InitShader(shader);
        m_dynamicDraw->InitVertexFormat({ { "POSITION", AZ::RHI::Format::R32G32B32_FLOAT },
                                          { "COLOR", AZ::RHI::Format::R8G8B8A8_UNORM },
                                          { "TEXCOORD", AZ::RHI::Format::R32G32_FLOAT } });
        m_dynamicDraw->SetOutputScope(scene);
        m_dynamicDraw->EndInit();

        m_initialized = m_dynamicDraw->IsReady();
        return m_initialized;
    }

    void SpriteFeatureProcessor::AppendQuad(
        const SpriteEntry& entry, const AZ::Transform& cameraTransform, SpriteVertex outVertices[4]) const
    {
        const SpriteComponentConfig& config = entry.m_config;

        const float halfWidth = config.m_size.GetX() * 0.5f;
        const float halfHeight = config.m_size.GetY() * 0.5f;

        const float pivotOffsetX = (0.5f - config.m_pivot.GetX()) * config.m_size.GetX();
        const float pivotOffsetY = (0.5f - config.m_pivot.GetY()) * config.m_size.GetY();

        const float cornerX[4] = { -halfWidth, halfWidth, halfWidth, -halfWidth };
        const float cornerY[4] = { -halfHeight, -halfHeight, halfHeight, halfHeight };

        float uvU[4];
        float uvV[4];
        config.GetCornerUVs(uvU, uvV);

        AZ::Vector3 right;
        AZ::Vector3 up;
        const AZ::Vector3 origin = entry.m_worldTransform.GetTranslation();

        if (config.m_billboard)
        {
            const AZ::Matrix3x3 cameraBasis = AZ::Matrix3x3::CreateFromTransform(cameraTransform);
            right = cameraBasis.GetColumn(0);
            up = cameraBasis.GetColumn(2);
        }
        else
        {
            const AZ::Matrix3x3 entityBasis = AZ::Matrix3x3::CreateFromTransform(entry.m_worldTransform);
            const float uniformScale = entry.m_worldTransform.GetUniformScale();
            right = entityBasis.GetColumn(0) * uniformScale;
            up = entityBasis.GetColumn(2) * uniformScale;
        }

        const AZ::u32 packedColor = config.m_tint.ToU32();

        for (int i = 0; i < 4; ++i)
        {
            const AZ::Vector3 position = origin + right * (cornerX[i] + pivotOffsetX) + up * (cornerY[i] + pivotOffsetY);
            outVertices[i].m_position[0] = position.GetX();
            outVertices[i].m_position[1] = position.GetY();
            outVertices[i].m_position[2] = position.GetZ();
            outVertices[i].m_color = packedColor;
            outVertices[i].m_uv[0] = uvU[i];
            outVertices[i].m_uv[1] = uvV[i];
        }
    }

    void SpriteFeatureProcessor::AppendShadowQuad(const SpriteEntry& entry, float alpha, SpriteVertex outVertices[4]) const
    {
        const AZ::Vector3 c = entry.m_worldTransform.GetTranslation();
        // Footprint sized off the sprite width, squashed in world Y into a ground ellipse.
        const float halfW = entry.m_config.m_size.GetX() * 0.42f;
        const float halfH = entry.m_config.m_size.GetX() * 0.20f;
        const float cornerX[4] = { -halfW, halfW, halfW, -halfW };
        const float cornerY[4] = { -halfH, -halfH, halfH, halfH };
        static const float uvU[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
        static const float uvV[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        const AZ::u32 packed = AZ::Color(0.0f, 0.0f, 0.0f, alpha).ToU32();
        for (int i = 0; i < 4; ++i)
        {
            outVertices[i].m_position[0] = c.GetX() + cornerX[i];
            outVertices[i].m_position[1] = c.GetY() + cornerY[i];
            outVertices[i].m_position[2] = c.GetZ();
            outVertices[i].m_color = packed;
            outVertices[i].m_uv[0] = uvU[i];
            outVertices[i].m_uv[1] = uvV[i];
        }
    }

    void SpriteFeatureProcessor::DrawShadows(AZ::RHI::DrawItemSortKey sortKey, const AZ::Transform& cameraTransform)
    {
        if (!m_shadowImageAsset.IsReady())
        {
            return; // texture not loaded yet; shadows simply do not draw
        }
        AZ::Data::Instance<AZ::RPI::Image> shadowImage = AZ::RPI::StreamingImage::FindOrCreate(m_shadowImageAsset);
        if (!shadowImage)
        {
            return;
        }
        float alpha = static_cast<float>(r_dioramaShadowAlpha);
        alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);

        m_shadowVertexScratch.clear();
        m_shadowIndexScratch.clear();
        static const AZ::u32 shadowIndices[6] = { 0, 1, 2, 0, 2, 3 };
        AZ::u32 n = 0;
        for (const auto& [handle, entry] : m_sprites)
        {
            // Only billboarded sprites with a real (loaded) texture cast a shadow --
            // this excludes the flat tilemap floor and any fallback-textured sprite.
            if (!entry.m_config.m_billboard || !entry.m_config.m_texture.IsReady())
            {
                continue;
            }
            m_shadowVertexScratch.resize((n + 1) * 4);
            AppendShadowQuad(entry, alpha, &m_shadowVertexScratch[n * 4]);
            const AZ::u32 base = n * 4;
            for (int k = 0; k < 6; ++k)
            {
                m_shadowIndexScratch.push_back(base + shadowIndices[k]);
            }
            ++n;
        }
        if (n == 0)
        {
            return;
        }

        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> drawSrg = m_dynamicDraw->NewDrawSrg();
        if (!drawSrg)
        {
            return;
        }
        const AZ::RHI::ShaderInputImageIndex imageIndex = drawSrg->FindShaderInputImageIndex(TextureSrgInput());
        if (!imageIndex.IsValid())
        {
            return;
        }
        drawSrg->SetImage(imageIndex, shadowImage);
        // Bind the shadow image into the normal-map slot too (a required bind) and
        // mark the batch as having no normal map; shadows are flat-lit (and black).
        const AZ::RHI::ShaderInputImageIndex normalIndex = drawSrg->FindShaderInputImageIndex(NormalMapSrgInput());
        if (normalIndex.IsValid())
        {
            drawSrg->SetImage(normalIndex, shadowImage);
        }
        SetLightingConstants(drawSrg.get());
        SetMaterialConstants(
            drawSrg.get(),
            cameraTransform,
            false,
            0.0f,
            AZ::Color(1.0f, 1.0f, 1.0f, 1.0f),
            0.0f,
            AZ::Color(1.0f, 1.0f, 1.0f, 1.0f),
            AZ::Color(1.0f, 1.0f, 1.0f, 1.0f),
            0.0f,
            false);
        drawSrg->Compile();

        m_dynamicDraw->SetSortKey(sortKey);
        m_dynamicDraw->DrawIndexed(
            m_shadowVertexScratch.data(),
            static_cast<uint32_t>(m_shadowVertexScratch.size()),
            m_shadowIndexScratch.data(),
            static_cast<uint32_t>(m_shadowIndexScratch.size()),
            AZ::RHI::IndexFormat::Uint32,
            drawSrg);
    }

    void SpriteFeatureProcessor::Render(const RenderPacket& packet)
    {
        if (m_sprites.empty() || !EnsureInitialized())
        {
            return;
        }

        // Billboarded sprites face the primary view. Query its camera once.
        AZ::Transform cameraTransform = AZ::Transform::CreateIdentity();
        if (!packet.m_views.empty() && packet.m_views.front())
        {
            cameraTransform = packet.m_views.front()->GetCameraTransform();
        }

        AZ::Data::Instance<AZ::RPI::Image> fallbackImage;
        if (auto* imageSystem = AZ::RPI::ImageSystemInterface::Get())
        {
            fallbackImage = imageSystem->GetSystemImage(AZ::RPI::SystemImage::White);
        }

        const AZ::Vector3 cameraPosition = cameraTransform.GetTranslation();
        const bool depthSort = static_cast<bool>(r_dioramaSpriteDepthSort);

        // Rebuild the batch plan when the sprite set or a batch key changed -- or
        // every frame when depth sorting, since the order then depends on the live
        // sprite and camera positions. For a static scene without depth sort this
        // skips the per-frame scan and stable_sort; the vertex packing and draw
        // below still run every frame (the draw context is immediate-mode and
        // billboards re-orient to the camera each frame). The cached entry pointers
        // remain valid because add/remove set m_planDirty.
        if (depthSort || m_planDirty)
        {
            m_itemScratch.clear();
            m_entryScratch.clear();
            m_itemScratch.reserve(m_sprites.size());
            m_entryScratch.reserve(m_sprites.size());
            for (const auto& [handle, entry] : m_sprites)
            {
                const AZ::u32 index = static_cast<AZ::u32>(m_entryScratch.size());
                SpriteBatchPlan::Item item{ entry.m_batchKey, index };
                if (depthSort)
                {
                    // Squared distance to the camera (cheaper than the true
                    // distance and monotonic, so it orders identically). Sorting
                    // far-to-near within a layer puts nearer sprites on top.
                    item.m_depth = (entry.m_worldTransform.GetTranslation() - cameraPosition).GetLengthSq();
                }
                m_itemScratch.push_back(item);
                m_entryScratch.push_back(&entry);
            }

            SpriteBatchPlan::Build(m_itemScratch, m_orderedScratch, m_batchScratch, depthSort);
            // The depth-sorted plan is per-frame; keep it marked dirty so a clean
            // rebuild happens at once if depth sort is later turned off.
            m_planDirty = depthSort;
        }

        static const AZ::u32 quadIndices[6] = { 0, 1, 2, 0, 2, 3 };
        // Reversed winding for the back face of a double-sided sprite. With
        // back-face culling on, emitting both windings makes exactly one face
        // visible from either side; a single-sided sprite emits only the front.
        static const AZ::u32 quadIndicesBack[6] = { 0, 2, 1, 0, 3, 2 };
        const AZ::u32 debugTint = AZ::Color(1.0f, 0.0f, 1.0f, 1.0f).ToU32();

        // Draw batches in their computed order, giving each an increasing sort key
        // so the transparent pass keeps that order (earlier batch = drawn behind).
        const bool drawShadows = static_cast<bool>(r_dioramaSpriteShadows);
        bool shadowsDrawn = false;
        AZ::RHI::DrawItemSortKey batchSortKey = 0;
        for (const SpriteBatchPlan::Batch& batch : m_batchScratch)
        {
            // Shadows draw once, between the ground (sort offset < 0) and the sprites
            // (>= 0), so the soft blobs sit on the floor beneath the billboards.
            if (drawShadows && !shadowsDrawn && batch.m_key.m_sortOffset >= 0.0f)
            {
                DrawShadows(batchSortKey++, cameraTransform);
                shadowsDrawn = true;
            }

            // All sprites in a batch share a texture; resolve it once from the
            // first member. Build drops invalid-texture items, so a batch always
            // references a valid texture asset.
            const SpriteEntry* first = m_entryScratch[m_orderedScratch[batch.m_begin].m_index];
            AZ::Data::Instance<AZ::RPI::Image> image;
            if (first->m_config.m_texture.IsReady())
            {
                image = AZ::RPI::StreamingImage::FindOrCreate(first->m_config.m_texture);
            }
            const bool usingFallback = !image;
            if (usingFallback)
            {
                image = fallbackImage;
            }
            if (!image)
            {
                continue;
            }

            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> drawSrg = m_dynamicDraw->NewDrawSrg();
            if (!drawSrg)
            {
                continue;
            }
            const AZ::RHI::ShaderInputImageIndex imageIndex = drawSrg->FindShaderInputImageIndex(TextureSrgInput());
            if (!imageIndex.IsValid())
            {
                continue;
            }
            drawSrg->SetImage(imageIndex, image);

            // v1b: bind the batch's normal map if it has one (and it is ready),
            // otherwise bind the albedo image as a required-but-unused placeholder
            // and flag the batch as flat (the shader skips the N.L term then).
            bool hasNormalMap = false;
            AZ::Data::Instance<AZ::RPI::Image> normalImage = image;
            if (first->m_config.m_normalMap.GetId().IsValid() && first->m_config.m_normalMap.IsReady())
            {
                AZ::Data::Instance<AZ::RPI::Image> resolved = AZ::RPI::StreamingImage::FindOrCreate(first->m_config.m_normalMap);
                if (resolved)
                {
                    normalImage = resolved;
                    hasNormalMap = true;
                }
            }
            const AZ::RHI::ShaderInputImageIndex normalIndex = drawSrg->FindShaderInputImageIndex(NormalMapSrgInput());
            if (normalIndex.IsValid())
            {
                drawSrg->SetImage(normalIndex, normalImage);
            }

            SetLightingConstants(drawSrg.get());
            SetMaterialConstants(
                drawSrg.get(),
                cameraTransform,
                hasNormalMap,
                first->m_config.m_flashAmount,
                first->m_config.m_flashColor,
                first->m_config.m_outlineThickness,
                first->m_config.m_outlineColor,
                first->m_config.m_emissiveColor,
                first->m_config.m_emissiveIntensity,
                first->m_config.m_pointFilter);
            drawSrg->Compile();

            // Pack all quads in this batch into one vertex/index stream. 32-bit
            // indices so a batch can exceed 16384 sprites without overflowing.
            m_vertexScratch.clear();
            m_indexScratch.clear();
            const AZ::u32 spriteCount = batch.Count();
            m_vertexScratch.resize(spriteCount * 4);
            m_indexScratch.reserve(spriteCount * 12); // up to 12 indices for a double-sided quad

            for (AZ::u32 n = 0; n < spriteCount; ++n)
            {
                const SpriteEntry* entry = m_entryScratch[m_orderedScratch[batch.m_begin + n].m_index];
                SpriteVertex* quad = &m_vertexScratch[n * 4];
                AppendQuad(*entry, cameraTransform, quad);

                if (usingFallback)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        quad[i].m_color = debugTint;
                    }
                }

                const AZ::u32 base = n * 4;
                for (int k = 0; k < 6; ++k)
                {
                    m_indexScratch.push_back(base + quadIndices[k]);
                }
                // A double-sided sprite also emits the back face (reversed
                // winding) so it stays visible when viewed from behind. A
                // billboard always faces the camera, so it never needs the back.
                if (entry->m_config.m_doubleSided && !entry->m_config.m_billboard)
                {
                    for (int k = 0; k < 6; ++k)
                    {
                        m_indexScratch.push_back(base + quadIndicesBack[k]);
                    }
                }
            }

            m_dynamicDraw->SetSortKey(batchSortKey++);
            m_dynamicDraw->DrawIndexed(
                m_vertexScratch.data(),
                static_cast<uint32_t>(m_vertexScratch.size()),
                m_indexScratch.data(),
                static_cast<uint32_t>(m_indexScratch.size()),
                AZ::RHI::IndexFormat::Uint32,
                drawSrg);
        }
    }
} // namespace Diorama
