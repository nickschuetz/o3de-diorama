/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <Clients/AsepriteImport.h>
#include <Clients/Collider2DComponent.h>
#include <Clients/DioramaAsepriteComponent.h>
#include <Clients/DioramaCRTComponent.h>
#include <Clients/DioramaCamera2DComponent.h>
#include <Clients/DioramaLightComponent.h>
#include <Clients/DioramaLookComponent.h>
#include <Clients/DioramaParallaxComponent.h>
#include <Clients/DioramaSkeletalClipComponent.h>
#include <Clients/DioramaUIComponent.h>
#include <Clients/ParticleEmitterComponent.h>
#include <Clients/SkeletalClip.h>
#include <Clients/SpriteAnimation.h>
#include <Clients/SpriteBatchPlan.h>
#include <Clients/SpriteComponent.h>
#include <Clients/SpritePresenter.h>
#include <Clients/SpriteRequestHandler.h>
#include <Clients/TilemapAutotile.h>
#include <Clients/TilemapComponent.h>
#include <Clients/TilemapPaint.h>
#include <Diorama/DioramaLightBus.h>
#include <Diorama/DioramaLightConfig.h>
#include <Diorama/SpriteBus.h>
#include <Diorama/SpriteComponentConfig.h>
#include <Diorama/TilemapComponentConfig.h>

namespace Diorama
{
    // Corner order used by the renderer and by GetCornerUVs:
    // 0 bottom-left, 1 bottom-right, 2 top-right, 3 top-left.
    // Texture V increases downward, so the quad bottom edge samples the larger V.
    class SpriteUVTest : public ::testing::Test
    {
    protected:
        static constexpr float Eps = 1e-5f;

        void ExpectCorners(
            const SpriteComponentConfig& config, float blU, float blV, float brU, float brV, float trU, float trV, float tlU, float tlV)
        {
            float u[4];
            float v[4];
            config.GetCornerUVs(u, v);
            EXPECT_NEAR(u[0], blU, Eps);
            EXPECT_NEAR(v[0], blV, Eps);
            EXPECT_NEAR(u[1], brU, Eps);
            EXPECT_NEAR(v[1], brV, Eps);
            EXPECT_NEAR(u[2], trU, Eps);
            EXPECT_NEAR(v[2], trV, Eps);
            EXPECT_NEAR(u[3], tlU, Eps);
            EXPECT_NEAR(v[3], tlV, Eps);
        }
    };

    TEST_F(SpriteUVTest, DefaultRegion_SamplesWholeTexture)
    {
        SpriteComponentConfig config;
        // BL(0,1) BR(1,1) TR(1,0) TL(0,0)
        ExpectCorners(config, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    }

    TEST_F(SpriteUVTest, BottomRightCell_OfTwoByTwoAtlas)
    {
        SpriteComponentConfig config;
        config.m_uvMin = AZ::Vector2(0.5f, 0.5f);
        config.m_uvMax = AZ::Vector2(1.0f, 1.0f);
        // BL(0.5,1) BR(1,1) TR(1,0.5) TL(0.5,0.5)
        ExpectCorners(config, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f);
    }

    TEST_F(SpriteUVTest, TopLeftCell_OfTwoByTwoAtlas)
    {
        SpriteComponentConfig config;
        config.m_uvMin = AZ::Vector2(0.0f, 0.0f);
        config.m_uvMax = AZ::Vector2(0.5f, 0.5f);
        ExpectCorners(config, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f);
    }

    TEST_F(SpriteUVTest, FlipHorizontal_SwapsLeftAndRightU)
    {
        SpriteComponentConfig config;
        config.m_uvMin = AZ::Vector2(0.0f, 0.0f);
        config.m_uvMax = AZ::Vector2(1.0f, 1.0f);
        config.m_flipHorizontal = true;
        // U min/max swap: BL(1,1) BR(0,1) TR(0,0) TL(1,0)
        ExpectCorners(config, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }

    TEST_F(SpriteUVTest, FlipVertical_SwapsTopAndBottomV)
    {
        SpriteComponentConfig config;
        config.m_uvMin = AZ::Vector2(0.0f, 0.0f);
        config.m_uvMax = AZ::Vector2(1.0f, 1.0f);
        config.m_flipVertical = true;
        // V min/max swap: BL(0,0) BR(1,0) TR(1,1) TL(0,1)
        ExpectCorners(config, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    }

    TEST_F(SpriteUVTest, FlipBoth_OnSubRegion)
    {
        SpriteComponentConfig config;
        config.m_uvMin = AZ::Vector2(0.25f, 0.25f);
        config.m_uvMax = AZ::Vector2(0.75f, 0.75f);
        config.m_flipHorizontal = true;
        config.m_flipVertical = true;
        // uMin/uMax swap to (0.75,0.25), vMin/vMax swap to (0.75,0.25).
        // BL uses (uMin', vMax') = (0.75, 0.25); etc.
        ExpectCorners(config, 0.75f, 0.25f, 0.25f, 0.25f, 0.25f, 0.75f, 0.75f, 0.75f);
    }
    TEST_F(SpriteUVTest, FrameRegion_DefaultGrid_IsWholeRegion)
    {
        SpriteComponentConfig config;
        AZ::Vector2 min;
        AZ::Vector2 max;
        config.GetFrameUVRegion(0, min, max);

        EXPECT_NEAR(min.GetX(), 0.0f, Eps);
        EXPECT_NEAR(min.GetY(), 0.0f, Eps);
        EXPECT_NEAR(max.GetX(), 1.0f, Eps);
        EXPECT_NEAR(max.GetY(), 1.0f, Eps);
    }

    TEST_F(SpriteUVTest, FrameRegion_Frame0_Of4x4_IsTopLeftCell)
    {
        SpriteComponentConfig config;
        config.m_frameColumns = 4;
        config.m_frameRows = 4;
        config.m_frameCount = 16;
        AZ::Vector2 min;
        AZ::Vector2 max;
        config.GetFrameUVRegion(0, min, max);

        EXPECT_NEAR(min.GetX(), 0.0f, Eps);
        EXPECT_NEAR(min.GetY(), 0.0f, Eps);
        EXPECT_NEAR(max.GetX(), 0.25f, Eps);
        EXPECT_NEAR(max.GetY(), 0.25f, Eps);
    }

    TEST_F(SpriteUVTest, FrameRegion_Frame5_Of4x4_IsSecondRowSecondColumn)
    {
        SpriteComponentConfig config;
        config.m_frameColumns = 4;
        config.m_frameRows = 4;
        config.m_frameCount = 16;
        AZ::Vector2 min;
        AZ::Vector2 max;
        config.GetFrameUVRegion(5, min, max);

        EXPECT_NEAR(min.GetX(), 0.25f, Eps);
        EXPECT_NEAR(min.GetY(), 0.25f, Eps);
        EXPECT_NEAR(max.GetX(), 0.5f, Eps);
        EXPECT_NEAR(max.GetY(), 0.5f, Eps);
    }

    TEST_F(SpriteUVTest, FrameRegion_ComposesWithAtlasSubRegion)
    {
        // A 2x2 sheet packed inside the bottom-right quarter of a texture atlas.
        SpriteComponentConfig config;
        config.m_uvMin = AZ::Vector2(0.5f, 0.5f);
        config.m_uvMax = AZ::Vector2(1.0f, 1.0f);
        config.m_frameColumns = 2;
        config.m_frameRows = 2;
        config.m_frameCount = 4;
        AZ::Vector2 min;
        AZ::Vector2 max;

        config.GetFrameUVRegion(0, min, max); // top-left cell of the sub-region
        EXPECT_NEAR(min.GetX(), 0.5f, Eps);
        EXPECT_NEAR(min.GetY(), 0.5f, Eps);
        EXPECT_NEAR(max.GetX(), 0.75f, Eps);
        EXPECT_NEAR(max.GetY(), 0.75f, Eps);

        config.GetFrameUVRegion(3, min, max); // bottom-right cell of the sub-region
        EXPECT_NEAR(min.GetX(), 0.75f, Eps);
        EXPECT_NEAR(min.GetY(), 0.75f, Eps);
        EXPECT_NEAR(max.GetX(), 1.0f, Eps);
        EXPECT_NEAR(max.GetY(), 1.0f, Eps);
    }

    TEST_F(SpriteUVTest, FrameCount_ClampedToGridSize)
    {
        SpriteComponentConfig config;
        config.m_frameColumns = 2;
        config.m_frameRows = 2;
        config.m_frameCount = 10; // more than the 4 cells available
        EXPECT_EQ(config.GetFrameCount(), 4);
    }

    using SpriteAnimationTest = ::testing::Test;

    TEST_F(SpriteAnimationTest, Advance_SingleFrame_DoesNotMove)
    {
        const SpriteAnimation::FrameState next = SpriteAnimation::Advance({ 0, 0.0f, false }, 1.0f, 12.0f, 1, true);
        EXPECT_EQ(next.m_frame, 0);
        EXPECT_FALSE(next.m_finished);
    }

    TEST_F(SpriteAnimationTest, Advance_ZeroRate_DoesNotMove)
    {
        const SpriteAnimation::FrameState next = SpriteAnimation::Advance({ 0, 0.0f, false }, 1.0f, 0.0f, 4, true);
        EXPECT_EQ(next.m_frame, 0);
    }

    TEST_F(SpriteAnimationTest, Advance_SteppsOneFrame)
    {
        // 10 fps means 0.1s per frame.
        const SpriteAnimation::FrameState next = SpriteAnimation::Advance({ 0, 0.0f, false }, 0.1f, 10.0f, 4, true);
        EXPECT_EQ(next.m_frame, 1);
        EXPECT_FALSE(next.m_finished);
    }

    TEST_F(SpriteAnimationTest, Advance_MultipleFramesForLargeDelta)
    {
        const SpriteAnimation::FrameState next = SpriteAnimation::Advance({ 0, 0.0f, false }, 0.25f, 10.0f, 4, true);
        EXPECT_EQ(next.m_frame, 2);
    }

    TEST_F(SpriteAnimationTest, Advance_WrapsWhenLooping)
    {
        const SpriteAnimation::FrameState next = SpriteAnimation::Advance({ 3, 0.0f, false }, 0.1f, 10.0f, 4, true);
        EXPECT_EQ(next.m_frame, 0);
        EXPECT_FALSE(next.m_finished);
    }

    TEST_F(SpriteAnimationTest, Advance_HoldsAndFinishesWhenNotLooping)
    {
        const SpriteAnimation::FrameState next = SpriteAnimation::Advance({ 3, 0.0f, false }, 0.1f, 10.0f, 4, false);
        EXPECT_EQ(next.m_frame, 3);
        EXPECT_TRUE(next.m_finished);
    }

    class SpriteBatchPlanTest : public ::testing::Test
    {
    protected:
        // Synthesize a distinct, valid AssetId from a small integer so tests can
        // talk about "texture N" without real assets. id 0 yields an invalid id.
        static AZ::Data::AssetId Tex(AZ::u32 n)
        {
            if (n == 0)
            {
                return AZ::Data::AssetId();
            }
            return AZ::Data::AssetId(AZ::Uuid::CreateName(AZStd::string::format("tex%u", n).c_str()), 0);
        }

        SpriteBatchPlan::Item MakeItem(AZ::u32 texNum, float sortOffset, AZ::u32 index)
        {
            return SpriteBatchPlan::Item{ SpriteBatchPlan::BatchKey{ Tex(texNum), sortOffset }, index };
        }

        // Caller-owned output buffers, reused across calls like the renderer does.
        AZStd::vector<SpriteBatchPlan::Item> m_ordered;
        AZStd::vector<SpriteBatchPlan::Batch> m_batches;

        void Build(const AZStd::vector<SpriteBatchPlan::Item>& items)
        {
            SpriteBatchPlan::Build(items, m_ordered, m_batches);
        }
    };

    TEST_F(SpriteBatchPlanTest, Empty_ProducesNoBatches)
    {
        Build({});
        EXPECT_TRUE(m_batches.empty());
        EXPECT_TRUE(m_ordered.empty());
    }

    TEST_F(SpriteBatchPlanTest, SameTextureAndSort_CoalesceIntoOneBatch)
    {
        Build({ MakeItem(7, 0.0f, 0), MakeItem(7, 0.0f, 1), MakeItem(7, 0.0f, 2) });

        ASSERT_EQ(m_batches.size(), 1u);
        EXPECT_EQ(m_batches[0].Count(), 3u);
        EXPECT_EQ(m_batches[0].m_key.m_textureId, Tex(7));
    }

    TEST_F(SpriteBatchPlanTest, DifferentTextures_SplitIntoSeparateBatches)
    {
        // Three sprites, two textures at the same sort layer: texture 1 has two
        // sprites that coalesce, texture 2 has one. Two batches total.
        Build({ MakeItem(1, 0.0f, 0), MakeItem(2, 0.0f, 1), MakeItem(1, 0.0f, 2) });

        ASSERT_EQ(m_batches.size(), 2u);
        // Ordering between equal-sort, different-texture batches follows AssetId
        // ordering, so assert on counts/identity rather than a fixed sequence.
        AZ::u32 total = 0;
        for (const auto& b : m_batches)
        {
            total += b.Count();
        }
        EXPECT_EQ(total, 3u);
    }

    TEST_F(SpriteBatchPlanTest, OrdersBySortOffsetAscending)
    {
        Build({ MakeItem(1, 5.0f, 0), MakeItem(1, -3.0f, 1), MakeItem(1, 0.0f, 2) });

        ASSERT_EQ(m_batches.size(), 3u);
        EXPECT_FLOAT_EQ(m_batches[0].m_key.m_sortOffset, -3.0f);
        EXPECT_FLOAT_EQ(m_batches[1].m_key.m_sortOffset, 0.0f);
        EXPECT_FLOAT_EQ(m_batches[2].m_key.m_sortOffset, 5.0f);
    }

    TEST_F(SpriteBatchPlanTest, SameTextureDifferentSort_DoesNotCoalesce)
    {
        // Same texture must NOT merge across sort layers, or 2.5D layering breaks.
        Build({ MakeItem(9, 0.0f, 0), MakeItem(9, 1.0f, 1) });
        EXPECT_EQ(m_batches.size(), 2u);
    }

    TEST_F(SpriteBatchPlanTest, FractionalSortOffsets_StayDistinct)
    {
        // Regression: fractional offsets must not collapse to one layer. With an
        // integer sort key, 0.25 and 0.75 would both truncate to 0 and wrongly
        // coalesce; with a float key they remain two batches.
        Build({ MakeItem(4, 0.25f, 0), MakeItem(4, 0.75f, 1) });
        EXPECT_EQ(m_batches.size(), 2u);
    }

    TEST_F(SpriteBatchPlanTest, InvalidTextureItems_AreDropped)
    {
        Build({ MakeItem(0, 0.0f, 0), MakeItem(5, 0.0f, 1), MakeItem(0, 0.0f, 2) });

        ASSERT_EQ(m_ordered.size(), 1u);
        EXPECT_EQ(m_ordered[0].m_index, 1u);
        ASSERT_EQ(m_batches.size(), 1u);
        EXPECT_EQ(m_batches[0].m_key.m_textureId, Tex(5));
    }

    TEST_F(SpriteBatchPlanTest, StableWithinBatch_PreservesRegistrationOrder)
    {
        // Two sprites, same key, given indices 4 then 2; ordering within the
        // batch must keep their input order (stable), not sort by index.
        Build({ MakeItem(3, 0.0f, 4), MakeItem(3, 0.0f, 2) });

        ASSERT_EQ(m_ordered.size(), 2u);
        EXPECT_EQ(m_ordered[0].m_index, 4u);
        EXPECT_EQ(m_ordered[1].m_index, 2u);
    }

    TEST_F(SpriteBatchPlanTest, ReusedBuffers_ClearedAcrossCalls)
    {
        // Build into the same buffers twice; the second result must not retain
        // stale entries from the first (the renderer reuses these every frame).
        Build({ MakeItem(1, 0.0f, 0), MakeItem(2, 0.0f, 1) });
        Build({ MakeItem(3, 0.0f, 0) });

        EXPECT_EQ(m_ordered.size(), 1u);
        ASSERT_EQ(m_batches.size(), 1u);
        EXPECT_EQ(m_batches[0].m_key.m_textureId, Tex(3));
    }

    TEST_F(SpriteBatchPlanTest, ByDepth_OrdersFarToNearWithinLayer)
    {
        // Same texture and sort layer, different camera depth. With byDepth the
        // farther sprite (larger m_depth) is ordered first so it draws behind, and
        // the nearer one draws on top (the sprite shader does not depth-test). They
        // still share one batch because the BatchKey is identical.
        SpriteBatchPlan::Item nearItem{ SpriteBatchPlan::BatchKey{ Tex(1), 0.0f }, 0 };
        nearItem.m_depth = 1.0f;
        SpriteBatchPlan::Item farItem{ SpriteBatchPlan::BatchKey{ Tex(1), 0.0f }, 1 };
        farItem.m_depth = 100.0f;

        SpriteBatchPlan::Build({ nearItem, farItem }, m_ordered, m_batches, /*byDepth*/ true);

        ASSERT_EQ(m_ordered.size(), 2u);
        EXPECT_EQ(m_ordered[0].m_index, 1u); // farther first (drawn behind)
        EXPECT_EQ(m_ordered[1].m_index, 0u); // nearer last (drawn on top)
        EXPECT_EQ(m_batches.size(), 1u); // identical key -> one batch
    }

    TEST_F(SpriteBatchPlanTest, MaterialDifferences_SplitBatches)
    {
        // Per-draw material state (flash, outline, emissive, normal map) is bound
        // once per batch, so sprites that share a texture and sort layer but differ
        // in any of these must NOT coalesce, or one's material would bleed onto the
        // others. Five keys differing only by material yield five batches.
        const SpriteBatchPlan::BatchKey plain{ Tex(5), 0.0f };
        SpriteBatchPlan::BatchKey flash = plain;
        flash.m_flashAmount = 0.5f;
        SpriteBatchPlan::BatchKey emissive = plain;
        emissive.m_emissiveIntensity = 2.0f;
        SpriteBatchPlan::BatchKey outline = plain;
        outline.m_outlineThickness = 0.1f;
        SpriteBatchPlan::BatchKey normal = plain;
        normal.m_normalMapId = Tex(6);

        Build({ SpriteBatchPlan::Item{ plain, 0 },
                SpriteBatchPlan::Item{ flash, 1 },
                SpriteBatchPlan::Item{ emissive, 2 },
                SpriteBatchPlan::Item{ outline, 3 },
                SpriteBatchPlan::Item{ normal, 4 } });

        EXPECT_EQ(m_batches.size(), 5u);
    }

    // Exercises the AI request verbs directly against a handler bound to a local
    // config and presenter (methods are called directly, so no live entity/EBus
    // dispatch is needed). Confirms clamping, the changed callback, the one-shot
    // convenience verb, and that GetSpriteInfo reflects the config.
    class SpriteRequestHandlerTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_changeCount = 0;
            m_handler.Connect(
                AZ::EntityId(),
                m_config,
                m_presenter,
                [this]()
                {
                    ++m_changeCount;
                });
        }
        void TearDown() override
        {
            m_handler.Disconnect();
        }

        SpriteComponentConfig m_config;
        SpritePresenter m_presenter;
        SpriteRequestHandler m_handler;
        int m_changeCount = 0;
        static constexpr float Eps = 1e-5f;
    };

    TEST_F(SpriteRequestHandlerTest, SetSize_ClampsNegativeToZero)
    {
        m_handler.SetSize(-3.0f, 5.0f);
        EXPECT_NEAR(m_config.m_size.GetX(), 0.0f, Eps);
        EXPECT_NEAR(m_config.m_size.GetY(), 5.0f, Eps);
    }

    TEST_F(SpriteRequestHandlerTest, SetPivot_ClampsToUnitRange)
    {
        m_handler.SetPivot(-1.0f, 2.0f);
        EXPECT_NEAR(m_config.m_pivot.GetX(), 0.0f, Eps);
        EXPECT_NEAR(m_config.m_pivot.GetY(), 1.0f, Eps);
    }

    TEST_F(SpriteRequestHandlerTest, SetTint_ClampsChannels)
    {
        m_handler.SetTint(2.0f, -1.0f, 0.5f, 10.0f);
        EXPECT_NEAR(m_config.m_tint.GetR(), 1.0f, Eps);
        EXPECT_NEAR(m_config.m_tint.GetG(), 0.0f, Eps);
        EXPECT_NEAR(m_config.m_tint.GetB(), 0.5f, Eps);
        EXPECT_NEAR(m_config.m_tint.GetA(), 1.0f, Eps);
    }

    TEST_F(SpriteRequestHandlerTest, SetUVRegion_ClampsAndOrders)
    {
        // Pass max < min and out-of-range; expect clamped to 0..1 and min <= max.
        m_handler.SetUVRegion(0.9f, 1.5f, 0.2f, -0.3f);
        EXPECT_NEAR(m_config.m_uvMin.GetX(), 0.2f, Eps);
        EXPECT_NEAR(m_config.m_uvMax.GetX(), 0.9f, Eps);
        EXPECT_NEAR(m_config.m_uvMin.GetY(), 0.0f, Eps); // 1.5 clamps to 1, -0.3 clamps to 0; min is 0
        EXPECT_NEAR(m_config.m_uvMax.GetY(), 1.0f, Eps);
    }

    TEST_F(SpriteRequestHandlerTest, PlaySpriteSheet_SetsGridPlaybackAndEnables)
    {
        m_handler.PlaySpriteSheet(4, 4, 16, 12.0f, true);
        EXPECT_EQ(m_config.m_frameColumns, 4);
        EXPECT_EQ(m_config.m_frameRows, 4);
        EXPECT_EQ(m_config.m_frameCount, 16);
        EXPECT_NEAR(m_config.m_framesPerSecond, 12.0f, Eps);
        EXPECT_TRUE(m_config.m_loop);
        EXPECT_TRUE(m_config.m_animEnabled);
    }

    TEST_F(SpriteRequestHandlerTest, SetFrameGrid_ClampsToAtLeastOne)
    {
        m_handler.SetFrameGrid(0, -2, 0);
        EXPECT_EQ(m_config.m_frameColumns, 1);
        EXPECT_EQ(m_config.m_frameRows, 1);
        EXPECT_EQ(m_config.m_frameCount, 1);
    }

    TEST_F(SpriteRequestHandlerTest, EachSetter_FiresChangedCallback)
    {
        m_handler.SetBillboard(true);
        m_handler.SetDoubleSided(false);
        m_handler.SetFlip(true, true);
        m_handler.SetSortOffset(3.0f);
        EXPECT_EQ(m_changeCount, 4);
        EXPECT_TRUE(m_config.m_billboard);
        EXPECT_FALSE(m_config.m_doubleSided);
        EXPECT_TRUE(m_config.m_flipHorizontal);
        EXPECT_TRUE(m_config.m_flipVertical);
        EXPECT_NEAR(m_config.m_sortOffset, 3.0f, Eps);
    }

    TEST_F(SpriteRequestHandlerTest, GetSpriteInfo_ReflectsConfig)
    {
        m_handler.SetSize(4.0f, 2.0f);
        m_handler.SetSortOffset(7.0f);
        m_handler.SetDoubleSided(false);
        m_handler.PlaySpriteSheet(2, 2, 4, 10.0f, false);

        const SpriteInfo info = m_handler.GetSpriteInfo();
        EXPECT_NEAR(info.m_width, 4.0f, Eps);
        EXPECT_NEAR(info.m_height, 2.0f, Eps);
        EXPECT_NEAR(info.m_sortOffset, 7.0f, Eps);
        EXPECT_FALSE(info.m_doubleSided);
        EXPECT_TRUE(info.m_animEnabled);
        EXPECT_EQ(info.m_frameCount, 4);
        // No texture assigned and no live feature processor in a unit test.
        EXPECT_FALSE(info.m_textureLoaded);
        EXPECT_FALSE(info.m_visible);
        EXPECT_TRUE(info.m_texturePath.empty());
    }

    TEST_F(SpriteRequestHandlerTest, SetTextureByPath_EmptyClearsTexture)
    {
        // Empty path clears the texture and is reported as success.
        EXPECT_TRUE(m_handler.SetTextureByPath(""));
        EXPECT_FALSE(m_config.m_texture.GetId().IsValid());
    }

    // Verifies the EBus round-trip: a handler connected at an entity id receives
    // verbs sent via DioramaSpriteRequestBus::Event(id, ...) and answers a
    // GetSpriteInfo EventResult. This exercises the actual dispatch path an agent
    // uses (the other handler tests call the methods directly), independent of
    // any editor/component activation.
    class SpriteBusDispatchTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_changeCount = 0;
            m_handler.Connect(
                m_entityId,
                m_config,
                m_presenter,
                [this]()
                {
                    ++m_changeCount;
                });
        }
        void TearDown() override
        {
            m_handler.Disconnect();
        }

        const AZ::EntityId m_entityId{ AZ::EntityId(0x1234) };
        SpriteComponentConfig m_config;
        SpritePresenter m_presenter;
        SpriteRequestHandler m_handler;
        int m_changeCount = 0;
        static constexpr float Eps = 1e-5f;
    };

    TEST_F(SpriteBusDispatchTest, SetterVerb_DispatchesToConnectedHandler)
    {
        DioramaSpriteRequestBus::Event(m_entityId, &DioramaSpriteRequests::SetSize, 4.0f, 2.0f);
        EXPECT_NEAR(m_config.m_size.GetX(), 4.0f, Eps);
        EXPECT_NEAR(m_config.m_size.GetY(), 2.0f, Eps);
        EXPECT_EQ(m_changeCount, 1);
    }

    TEST_F(SpriteBusDispatchTest, PlaySpriteSheetVerb_DispatchesAndConfigures)
    {
        DioramaSpriteRequestBus::Event(m_entityId, &DioramaSpriteRequests::PlaySpriteSheet, 4, 4, 16, 12.0f, true);
        EXPECT_TRUE(m_config.m_animEnabled);
        EXPECT_EQ(m_config.m_frameColumns, 4);
        EXPECT_EQ(m_config.m_frameCount, 16);
    }

    TEST_F(SpriteBusDispatchTest, GetSpriteInfoVerb_ReturnsViaEventResult)
    {
        DioramaSpriteRequestBus::Event(m_entityId, &DioramaSpriteRequests::SetSortOffset, 9.0f);

        SpriteInfo info;
        DioramaSpriteRequestBus::EventResult(info, m_entityId, &DioramaSpriteRequests::GetSpriteInfo);
        EXPECT_NEAR(info.m_sortOffset, 9.0f, Eps);
        EXPECT_TRUE(info.m_doubleSided); // default
    }

    TEST_F(SpriteBusDispatchTest, NoHandlerAtOtherAddress_VerbIsNoOp)
    {
        // A different entity id has no handler; the call must be a safe no-op and
        // must not touch this handler's config.
        const AZ::EntityId other{ AZ::EntityId(0x9999) };
        DioramaSpriteRequestBus::Event(other, &DioramaSpriteRequests::SetSize, 7.0f, 7.0f);
        EXPECT_NEAR(m_config.m_size.GetX(), 1.0f, Eps); // unchanged default
        EXPECT_EQ(m_changeCount, 0);
    }

    // Minimal stub component that provides TransformService, so a SpriteComponent
    // (which requires it) can activate on a real entity without pulling in
    // AzFramework's TransformComponent.
    class TransformStubComponent : public AZ::Component
    {
    public:
        AZ_COMPONENT(TransformStubComponent, "{0E2F4A6C-1B3D-4E5F-8A9B-0C1D2E3F4A5B}", AZ::Component);
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
            {
                sc->Class<TransformStubComponent, AZ::Component>()->Version(1);
            }
        }
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC_CE("TransformService"));
        }
        void Activate() override
        {
        }
        void Deactivate() override
        {
        }
    };

    // Full end-to-end activation test, no editor: stand up a minimal
    // ComponentApplication, reflect the sprite types, build an entity with a
    // SpriteComponent, activate it, and confirm that activation connects the AI
    // request handler so a bus Event reaches it. This proves the one link the
    // editor --runpython flow could not (component activation -> handler connect),
    // independent of the editor's entity-creation instability.
    class SpriteComponentActivationTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);

            // Registering the descriptor reflects the component (and, via
            // SpriteComponent::Reflect, the config + buses), so do not call
            // Reflect again or the types register twice (duplicate-Uuid failure).
            m_app.RegisterComponentDescriptor(SpriteComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(TransformStubComponent::CreateDescriptor());

            m_systemEntity->Init();
            m_systemEntity->Activate();
        }
        void TearDown() override
        {
            if (m_entity)
            {
                m_entity->Deactivate();
                delete m_entity;
                m_entity = nullptr;
            }
            m_app.Destroy();
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_entity = nullptr;
    };

    TEST_F(SpriteComponentActivationTest, Activation_ConnectsRequestHandler_BusReaches)
    {
        m_entity = aznew AZ::Entity("SpriteEntity");
        m_entity->CreateComponent<TransformStubComponent>();
        m_entity->CreateComponent<SpriteComponent>();
        m_entity->Init();
        m_entity->Activate();
        ASSERT_EQ(m_entity->GetState(), AZ::Entity::State::Active);

        const AZ::EntityId id = m_entity->GetId();

        // Drive the sprite purely through the bus, as an agent would.
        DioramaSpriteRequestBus::Event(id, &DioramaSpriteRequests::SetSize, 5.0f, 6.0f);
        DioramaSpriteRequestBus::Event(id, &DioramaSpriteRequests::SetSortOffset, 2.0f);

        // If activation connected the handler, GetSpriteInfo reflects the writes.
        SpriteInfo info;
        DioramaSpriteRequestBus::EventResult(info, id, &DioramaSpriteRequests::GetSpriteInfo);
        EXPECT_NEAR(info.m_width, 5.0f, 1e-5f);
        EXPECT_NEAR(info.m_height, 6.0f, 1e-5f);
        EXPECT_NEAR(info.m_sortOffset, 2.0f, 1e-5f);
    }

    // ---- AI / human parity guard --------------------------------------------
    // Every field a component config serializes is persisted into the prefab and
    // can be written by an agent (over the request bus or by hand-authored JSON).
    // For parity, the same field must be visible to a human in the editor
    // Inspector, which means it needs an EditContext DataElement. A field that is
    // serialized but has no DataElement is an AI-only / hand-edit-only knob the
    // human cannot see or change in the Inspector: a parity regression.
    //
    // The EditContext records a DataElement by setting the matching serialize
    // ClassElement's m_editData. So the check is: for every non-base serialized
    // element of each config, m_editData must be non-null (or the field must be
    // explicitly allowlisted as not-Inspector-editable by design).
    //
    // Each config is reflected standalone into a fresh SerializeContext that owns
    // its own EditContext (created *before* the Reflect call, so the config's
    // `if (auto* ec = sc->GetEditContext())` block runs). No ComponentApplication
    // is stood up: the configs reflect in isolation, so the guard cannot be
    // perturbed by, or perturb, engine-wide reflection.
    template<typename ConfigT>
    AZStd::vector<AZStd::string> SerializedFieldsMissingFromInspector()
    {
        AZ::SerializeContext serializeContext;
        serializeContext.CreateEditContext();
        ConfigT::Reflect(&serializeContext);

        AZStd::vector<AZStd::string> missing;
        const AZ::SerializeContext::ClassData* classData = serializeContext.FindClassData(azrtti_typeid<ConfigT>());
        if (classData == nullptr)
        {
            // Sentinel: a config that reflected nothing is itself a failure the
            // caller surfaces (an empty "missing" list would falsely pass).
            missing.push_back("<config-not-reflected>");
            return missing;
        }
        for (const AZ::SerializeContext::ClassElement& element : classData->m_elements)
        {
            if (element.m_flags & AZ::SerializeContext::ClassElement::FLG_BASE_CLASS)
            {
                continue;
            }
            if (element.m_editData == nullptr)
            {
                missing.push_back(element.m_name);
            }
        }
        return missing;
    }

    // Fail the current test if ConfigT serializes a field that has no Inspector
    // DataElement and is not in allowlist. allowlist holds fields that are
    // serialized-but-not-Inspector by design (bulk data authored by tools/scripts).
    template<typename ConfigT>
    void ExpectInspectorParity(const char* name, std::initializer_list<const char*> allowlist)
    {
        AZStd::vector<AZStd::string> offenders;
        for (const AZStd::string& field : SerializedFieldsMissingFromInspector<ConfigT>())
        {
            bool allowed = false;
            for (const char* allow : allowlist)
            {
                if (field == allow)
                {
                    allowed = true;
                    break;
                }
            }
            if (!allowed)
            {
                offenders.push_back(field);
            }
        }

        AZStd::string joined;
        for (const AZStd::string& field : offenders)
        {
            joined += field;
            joined += " ";
        }
        EXPECT_TRUE(offenders.empty()) << name
                                       << " serializes field(s) with no Inspector DataElement (AI/hand-edit only): " << joined.c_str();
    }

    TEST(ParityGuardTest, EverySerializedConfigField_HasAnInspectorControl)
    {
        ExpectInspectorParity<SpriteComponentConfig>("SpriteComponentConfig", {});
        ExpectInspectorParity<DioramaParticleConfig>("DioramaParticleConfig", {});
        ExpectInspectorParity<DioramaLightConfig>("DioramaLightConfig", {});
        ExpectInspectorParity<DioramaParallaxConfig>("DioramaParallaxConfig", {});
        ExpectInspectorParity<Collider2DConfig>("Collider2DConfig", {});
        ExpectInspectorParity<DioramaUIConfig>("DioramaUIConfig", {});
        ExpectInspectorParity<DioramaCRTConfig>("DioramaCRTConfig", {});
        ExpectInspectorParity<DioramaLookConfig>("DioramaLookConfig", {});
        ExpectInspectorParity<DioramaCamera2DConfig>("DioramaCamera2DConfig", {});
        ExpectInspectorParity<DioramaSkeletalClipConfig>("DioramaSkeletalClipConfig", {});
        ExpectInspectorParity<DioramaAsepriteConfig>("DioramaAsepriteConfig", {});
        // m_tiles is the bulk integer grid, authored by the paint tool / request
        // bus / build script, not hand-typed cell by cell in the Inspector.
        ExpectInspectorParity<TilemapComponentConfig>("TilemapComponentConfig", { "tiles" });
    }

    // ---- Skeletal cutout clip player: keyframe sampling + easing ----------------
    // The clip-player component is transform-driven (verified in the editor/game),
    // but the sampling it relies on is pure and tested here: holding before/after the
    // clip, interpolating between keys, and the easing curves.

    TEST(SkeletalClipTest, SampleTrack_EmptyTrackIsIdentity)
    {
        const SkeletalClip::Pose pose = SkeletalClip::SampleTrack({}, 0.5f);
        EXPECT_TRUE(pose.m_translation.IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(pose.m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
        EXPECT_TRUE(pose.m_scale.IsClose(AZ::Vector3::CreateOne()));
    }

    TEST(SkeletalClipTest, SampleTrack_HoldsBeforeFirstAndAfterLast)
    {
        const SkeletalClip::Keyframe keys[2] = {
            { 1.0f, AZ::Vector3(1.0f, 0.0f, 0.0f), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateOne(), SkeletalClip::Ease::Linear },
            { 3.0f, AZ::Vector3(5.0f, 0.0f, 0.0f), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateOne(), SkeletalClip::Ease::Linear },
        };
        AZStd::span<const SkeletalClip::Keyframe> track(keys, 2);

        // Before the first key holds the first; after the last holds the last.
        EXPECT_NEAR(SkeletalClip::SampleTrack(track, 0.0f).m_translation.GetX(), 1.0f, 1e-4f);
        EXPECT_NEAR(SkeletalClip::SampleTrack(track, 10.0f).m_translation.GetX(), 5.0f, 1e-4f);
    }

    TEST(SkeletalClipTest, SampleTrack_LinearMidpointInterpolates)
    {
        const SkeletalClip::Keyframe keys[2] = {
            { 0.0f,
              AZ::Vector3(0.0f, 0.0f, 0.0f),
              AZ::Quaternion::CreateIdentity(),
              AZ::Vector3(1.0f, 1.0f, 1.0f),
              SkeletalClip::Ease::Linear },
            { 2.0f,
              AZ::Vector3(4.0f, 0.0f, 0.0f),
              AZ::Quaternion::CreateIdentity(),
              AZ::Vector3(3.0f, 3.0f, 3.0f),
              SkeletalClip::Ease::Linear },
        };
        const SkeletalClip::Pose pose =
            SkeletalClip::SampleTrack(AZStd::span<const SkeletalClip::Keyframe>(keys, 2), 1.0f); // halfway in time
        EXPECT_NEAR(pose.m_translation.GetX(), 2.0f, 1e-4f);
        EXPECT_NEAR(pose.m_scale.GetX(), 2.0f, 1e-4f);
    }

    TEST(SkeletalClipTest, SampleTrack_EasingBendsTheCurveVsLinear)
    {
        const SkeletalClip::Keyframe linear[2] = {
            { 0.0f, AZ::Vector3::CreateZero(), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateOne(), SkeletalClip::Ease::Linear },
            { 1.0f, AZ::Vector3(1.0f, 0.0f, 0.0f), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateOne(), SkeletalClip::Ease::Linear },
        };
        const SkeletalClip::Keyframe inQuad[2] = {
            { 0.0f, AZ::Vector3::CreateZero(), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateOne(), SkeletalClip::Ease::InQuad },
            { 1.0f, AZ::Vector3(1.0f, 0.0f, 0.0f), AZ::Quaternion::CreateIdentity(), AZ::Vector3::CreateOne(), SkeletalClip::Ease::InQuad },
        };
        // At a quarter of the way, ease-in lags behind linear (slower start).
        const float linX = SkeletalClip::SampleTrack(AZStd::span<const SkeletalClip::Keyframe>(linear, 2), 0.25f).m_translation.GetX();
        const float inX = SkeletalClip::SampleTrack(AZStd::span<const SkeletalClip::Keyframe>(inQuad, 2), 0.25f).m_translation.GetX();
        EXPECT_NEAR(linX, 0.25f, 1e-4f);
        EXPECT_LT(inX, linX);
    }

    TEST(SkeletalClipTest, ApplyEase_ClampsAndCurves)
    {
        EXPECT_NEAR(SkeletalClip::ApplyEase(SkeletalClip::Ease::Linear, -1.0f), 0.0f, 1e-4f);
        EXPECT_NEAR(SkeletalClip::ApplyEase(SkeletalClip::Ease::Linear, 2.0f), 1.0f, 1e-4f);
        EXPECT_NEAR(SkeletalClip::ApplyEase(SkeletalClip::Ease::InOutQuad, 0.5f), 0.5f, 1e-4f);
        EXPECT_NEAR(SkeletalClip::ApplyEase(SkeletalClip::Ease::SmoothStep, 0.5f), 0.5f, 1e-4f);
        EXPECT_LT(SkeletalClip::ApplyEase(SkeletalClip::Ease::InQuad, 0.5f), 0.5f);
    }

    // ---- Aseprite import: JSON parse + playback timeline -----------------------
    // The component is sprite-driven (it sets the sprite's texture + UV per frame),
    // but the parse and the per-frame-duration / direction timeline are pure and
    // tested here.

    namespace
    {
        // A minimal Aseprite "Array" export: a 2-frame strip in a 32x16 atlas with a
        // "walk" tag, frame 0 shows 100ms, frame 1 shows 200ms.
        constexpr const char* kArrayJson = R"JSON(
        {
          "frames": [
            { "frame": { "x": 0, "y": 0, "w": 16, "h": 16 }, "duration": 100 },
            { "frame": { "x": 16, "y": 0, "w": 16, "h": 16 }, "duration": 200 }
          ],
          "meta": {
            "image": "hero.png",
            "size": { "w": 32, "h": 16 },
            "frameTags": [ { "name": "walk", "from": 0, "to": 1, "direction": "forward" } ]
          }
        })JSON";

        // The same two frames in the "Hash" form (frames keyed by name).
        constexpr const char* kHashJson = R"JSON(
        {
          "frames": {
            "hero 0.png": { "frame": { "x": 0, "y": 0, "w": 16, "h": 16 }, "duration": 100 },
            "hero 1.png": { "frame": { "x": 16, "y": 0, "w": 16, "h": 16 }, "duration": 200 }
          },
          "meta": { "image": "hero.png", "size": { "w": 32, "h": 16 }, "frameTags": [] }
        })JSON";
    } // namespace

    TEST(AsepriteImportTest, ParsesArrayForm)
    {
        Aseprite::Document doc;
        ASSERT_TRUE(Aseprite::ParseDocument(kArrayJson, doc));
        EXPECT_EQ(doc.m_imageName, "hero.png");
        EXPECT_EQ(doc.m_atlasWidth, 32);
        EXPECT_EQ(doc.m_atlasHeight, 16);
        ASSERT_EQ(doc.m_frames.size(), 2u);
        EXPECT_EQ(doc.m_frames[1].m_x, 16);
        EXPECT_NEAR(doc.m_frames[0].m_durationSeconds, 0.1f, 1e-4f);
        EXPECT_NEAR(doc.m_frames[1].m_durationSeconds, 0.2f, 1e-4f);
        ASSERT_EQ(doc.m_tags.size(), 1u);
        EXPECT_EQ(doc.m_tags[0].m_name, "walk");
        EXPECT_EQ(doc.m_tags[0].m_direction, Aseprite::Direction::Forward);
    }

    TEST(AsepriteImportTest, ParsesHashFormInOrder)
    {
        Aseprite::Document doc;
        ASSERT_TRUE(Aseprite::ParseDocument(kHashJson, doc));
        ASSERT_EQ(doc.m_frames.size(), 2u);
        EXPECT_EQ(doc.m_frames[0].m_x, 0);
        EXPECT_EQ(doc.m_frames[1].m_x, 16);
    }

    TEST(AsepriteImportTest, MalformedJsonFails)
    {
        Aseprite::Document doc;
        EXPECT_FALSE(Aseprite::ParseDocument("{ not valid", doc));
        EXPECT_TRUE(doc.m_frames.empty());
    }

    TEST(AsepriteImportTest, FrameUVMapsPixelRectToAtlas)
    {
        Aseprite::FrameData frame{ 16, 0, 16, 16, 0.1f };
        float uMin, vMin, uMax, vMax;
        Aseprite::FrameUV(frame, 32, 16, uMin, vMin, uMax, vMax);
        EXPECT_NEAR(uMin, 0.5f, 1e-4f);
        EXPECT_NEAR(vMin, 0.0f, 1e-4f);
        EXPECT_NEAR(uMax, 1.0f, 1e-4f);
        EXPECT_NEAR(vMax, 1.0f, 1e-4f);
    }

    TEST(AsepriteImportTest, FrameAtTimeHonorsPerFrameDurations)
    {
        Aseprite::Document doc;
        ASSERT_TRUE(Aseprite::ParseDocument(kArrayJson, doc));
        const Aseprite::TagData& walk = doc.m_tags[0];
        // 0..100ms -> frame 0; 100..300ms -> frame 1 (the 200ms frame).
        EXPECT_EQ(Aseprite::FrameAtTime(doc, walk, 0.05f, true), 0);
        EXPECT_EQ(Aseprite::FrameAtTime(doc, walk, 0.15f, true), 1);
        EXPECT_EQ(Aseprite::FrameAtTime(doc, walk, 0.25f, true), 1);
        // Loops: 0.3s == one full cycle -> back to frame 0.
        EXPECT_EQ(Aseprite::FrameAtTime(doc, walk, 0.30f, true), 0);
        // Non-looping holds the last frame past the cycle.
        EXPECT_EQ(Aseprite::FrameAtTime(doc, walk, 5.0f, false), 1);
    }

    TEST(AsepriteImportTest, PingPongPlaylistDoesNotDuplicateEndpoints)
    {
        Aseprite::Document doc;
        doc.m_frames = { { 0, 0, 8, 8, 0.1f }, { 8, 0, 8, 8, 0.1f }, { 16, 0, 8, 8, 0.1f } };
        Aseprite::TagData tag;
        tag.m_from = 0;
        tag.m_to = 2;
        tag.m_direction = Aseprite::Direction::PingPong;
        const AZStd::vector<int> playlist = Aseprite::BuildPlaylist(doc, tag);
        // 0,1,2,1 -> back to 0 at the loop (endpoints 0 and 2 not duplicated).
        ASSERT_EQ(playlist.size(), 4u);
        EXPECT_EQ(playlist[0], 0);
        EXPECT_EQ(playlist[1], 1);
        EXPECT_EQ(playlist[2], 2);
        EXPECT_EQ(playlist[3], 1);
    }

    // ---- BehaviorContext reflection: *Info structs are read-only getters ---------
    // The Get*Info verbs return a snapshot struct. Its fields are reflected get-only
    // so they list once in Script Canvas (a getter + setter shows the field twice) and
    // cannot be mutated on the returned copy. This guards that a getter-only property
    // still reads back its value (the exact call azlmbr / Script Canvas make) -- it
    // does not read as a default/None -- and that no setter is reflected.

    TEST(BehaviorReflectionTest, InfoStruct_FieldsAreReadOnlyGetters_ThatReturnTheValue)
    {
        AZ::BehaviorContext behaviorContext;
        LightInfo::Reflect(&behaviorContext);

        auto classIt = behaviorContext.m_classes.find("DioramaLightInfo");
        ASSERT_NE(classIt, behaviorContext.m_classes.end());
        AZ::BehaviorClass* klass = classIt->second;

        auto propIt = klass->m_properties.find("intensity");
        ASSERT_NE(propIt, klass->m_properties.end());
        AZ::BehaviorProperty* prop = propIt->second;

        // Read-only: a getter, no setter (so the palette lists it once).
        ASSERT_NE(prop->m_getter, nullptr);
        EXPECT_EQ(prop->m_setter, nullptr);

        // The getter returns the value, not a default -- this is what a Script Canvas
        // GetLightInfo property pull or a Python `info.intensity` read resolves to.
        LightInfo info;
        info.m_intensity = 2.5f;
        float out = -1.0f;
        EXPECT_TRUE(prop->m_getter->InvokeResult(out, &info));
        EXPECT_FLOAT_EQ(out, 2.5f);
    }

    // ---- Tilemap paint tool: cell math + brush/rect/fill cores -----------------
    // The editor paint mode is interactive (verified in the editor), but the math it
    // relies on is pure and tested here: world->cell inversion and which cells a
    // brush, rectangle, or flood fill cover. All outputs stay inside the grid.

    TEST(TilemapPaintTest, LocalPositionToCell_RoundTripsWithGetTileLocalPosition)
    {
        TilemapComponentConfig config;
        config.m_columns = 5;
        config.m_rows = 3;
        config.m_tileSize = AZ::Vector2(2.0f, 4.0f); // non-square to catch axis mixups

        for (int row = 0; row < config.GetRows(); ++row)
        {
            for (int col = 0; col < config.GetColumns(); ++col)
            {
                const AZ::Vector3 center = config.GetTileLocalPosition(col, row);
                int outCol = -99;
                int outRow = -99;
                const bool inBounds = config.LocalPositionToCell(center, outCol, outRow);
                EXPECT_TRUE(inBounds);
                EXPECT_EQ(outCol, col);
                EXPECT_EQ(outRow, row);
            }
        }
    }

    TEST(TilemapPaintTest, LocalPositionToCell_YIsIgnored_AndOutOfGridReturnsFalse)
    {
        TilemapComponentConfig config;
        config.m_columns = 4;
        config.m_rows = 4;
        config.m_tileSize = AZ::Vector2(1.0f, 1.0f);

        // Same XZ at a different Y still maps to the same cell.
        const AZ::Vector3 c = config.GetTileLocalPosition(2, 1);
        int col = 0;
        int row = 0;
        EXPECT_TRUE(config.LocalPositionToCell(AZ::Vector3(c.GetX(), 123.0f, c.GetZ()), col, row));
        EXPECT_EQ(col, 2);
        EXPECT_EQ(row, 1);

        // A point well outside the grid reports not-in-bounds.
        int oc = 0;
        int orow = 0;
        EXPECT_FALSE(config.LocalPositionToCell(AZ::Vector3(1000.0f, 0.0f, 1000.0f), oc, orow));
    }

    TEST(TilemapPaintTest, BrushCells_SizeOneIsSingleCell_ThreeIsNineClampedAtEdge)
    {
        AZStd::vector<TilemapPaint::Cell> cells;

        TilemapPaint::BrushCells(2, 2, 1, 8, 8, cells);
        EXPECT_EQ(cells.size(), 1u);
        EXPECT_EQ(cells[0].m_col, 2);
        EXPECT_EQ(cells[0].m_row, 2);

        TilemapPaint::BrushCells(2, 2, 3, 8, 8, cells); // interior 3x3
        EXPECT_EQ(cells.size(), 9u);

        TilemapPaint::BrushCells(0, 0, 3, 8, 8, cells); // corner: only +quadrant in grid
        EXPECT_EQ(cells.size(), 4u);
    }

    TEST(TilemapPaintTest, RectCells_InclusiveAnyOrderAndClamped)
    {
        AZStd::vector<TilemapPaint::Cell> cells;
        TilemapPaint::RectCells(3, 2, 1, 0, 8, 8, cells); // corners given high->low
        EXPECT_EQ(cells.size(), 3u * 3u); // cols 1..3, rows 0..2

        TilemapPaint::RectCells(-5, -5, 1, 1, 4, 4, cells); // clamps to 0..1 x 0..1
        EXPECT_EQ(cells.size(), 2u * 2u);
    }

    TEST(TilemapPaintTest, FloodFill_SpreadsOverMatchingTilesOnly)
    {
        // 4x1 row: tiles [7,7,3,7]. Fill from col 0 covers the two leading 7s only;
        // the 3 blocks it, so the trailing 7 is not reached.
        const int grid[4] = { 7, 7, 3, 7 };
        auto tileAt = [&](int col, [[maybe_unused]] int row)
        {
            return grid[col];
        };
        AZStd::vector<TilemapPaint::Cell> cells;
        TilemapPaint::FloodFillCells(0, 0, 4, 1, tileAt, cells);
        EXPECT_EQ(cells.size(), 2u);

        // Out-of-grid seed yields nothing.
        TilemapPaint::FloodFillCells(9, 9, 4, 1, tileAt, cells);
        EXPECT_TRUE(cells.empty());
    }

    // ---- Tilemap autotiling: neighbor mask -> 4-bit edge -> tile index -----------
    // The editor/runtime Autotile verb is verified in the editor, but the bitmask math
    // is pure and tested here: which neighbors count, the cardinal-edge reduction, and
    // the block tile index.

    TEST(TilemapAutotileTest, NeighborMask8_IsolatedAndCrossAndDiagonal)
    {
        // A 3x3 plus-shape of members: center + N/E/S/W, no diagonals.
        auto plus = [](int c, int r)
        {
            return (c == 1 && (r == 0 || r == 1 || r == 2)) || (r == 1 && (c == 0 || c == 2));
        };
        // Center sees all four cardinals, no diagonals.
        const AZ::u8 center = TilemapAutotile::NeighborMask8(1, 1, plus);
        EXPECT_EQ(center & TilemapAutotile::N, TilemapAutotile::N);
        EXPECT_EQ(center & TilemapAutotile::E, TilemapAutotile::E);
        EXPECT_EQ(center & TilemapAutotile::S, TilemapAutotile::S);
        EXPECT_EQ(center & TilemapAutotile::W, TilemapAutotile::W);
        EXPECT_EQ(center & TilemapAutotile::NE, 0);
        EXPECT_EQ(center & TilemapAutotile::SW, 0);

        // An isolated cell (predicate always false) has no neighbors.
        auto none = [](int, int)
        {
            return false;
        };
        EXPECT_EQ(TilemapAutotile::NeighborMask8(5, 5, none), 0);
    }

    TEST(TilemapAutotileTest, EdgeMask4_DropsDiagonalsAndKeepsCardinals)
    {
        const AZ::u8 mask8 = TilemapAutotile::N | TilemapAutotile::NE | TilemapAutotile::W;
        const AZ::u8 edge = TilemapAutotile::EdgeMask4(mask8);
        EXPECT_EQ(edge, static_cast<AZ::u8>(TilemapAutotile::EdgeN | TilemapAutotile::EdgeW));

        // All eight neighbors -> all four edges set (0..15 max = 15).
        const AZ::u8 full = TilemapAutotile::EdgeMask4(0xFF);
        EXPECT_EQ(full, 15);
    }

    TEST(TilemapAutotileTest, EdgeTileIndex_OffsetsIntoTheBlock)
    {
        EXPECT_EQ(TilemapAutotile::EdgeTileIndex(64, 0), 64); // isolated -> first block cell
        EXPECT_EQ(TilemapAutotile::EdgeTileIndex(64, 15), 79); // surrounded -> last block cell
    }

    TEST(TilemapAutotileTest, HorizontalRunMiddleCell_HasEastAndWestEdgesOnly)
    {
        // A horizontal run on row 0: cells (0,0),(1,0),(2,0) are members.
        auto run = [](int c, int r)
        {
            return r == 0 && c >= 0 && c <= 2;
        };
        const AZ::u8 edge = TilemapAutotile::EdgeMask4(TilemapAutotile::NeighborMask8(1, 0, run));
        EXPECT_EQ(edge, static_cast<AZ::u8>(TilemapAutotile::EdgeE | TilemapAutotile::EdgeW));
        EXPECT_EQ(TilemapAutotile::EdgeTileIndex(0, edge), TilemapAutotile::EdgeE | TilemapAutotile::EdgeW);
    }

    TEST(TilemapAutotileTest, NormalizeBlobMask_DropsUnsupportedCorners)
    {
        // NE corner with only N present (no E) is not a real connection -> dropped.
        const AZ::u8 nOnlyNE = TilemapAutotile::N | TilemapAutotile::NE;
        EXPECT_EQ(TilemapAutotile::NormalizeBlobMask(nOnlyNE), TilemapAutotile::N);

        // NE with both N and E present -> the corner is kept.
        const AZ::u8 supported = TilemapAutotile::N | TilemapAutotile::E | TilemapAutotile::NE;
        EXPECT_EQ(
            TilemapAutotile::NormalizeBlobMask(supported),
            static_cast<AZ::u8>(TilemapAutotile::N | TilemapAutotile::E | TilemapAutotile::NE));
    }

    TEST(TilemapAutotileTest, BlobIndex_HasExactly47DistinctValues_InRange)
    {
        bool seen[TilemapAutotile::BlobTileCount] = {};
        int distinct = 0;
        for (int m = 0; m < 256; ++m)
        {
            const int idx = TilemapAutotile::BlobIndex(static_cast<AZ::u8>(m));
            ASSERT_GE(idx, 0);
            ASSERT_LT(idx, TilemapAutotile::BlobTileCount);
            if (!seen[idx])
            {
                seen[idx] = true;
                ++distinct;
            }
        }
        EXPECT_EQ(distinct, 47);
        // Isolated cell is the first block cell; fully surrounded is the last.
        EXPECT_EQ(TilemapAutotile::BlobIndex(0), 0);
        EXPECT_EQ(TilemapAutotile::BlobIndex(0xFF), 46);
        EXPECT_EQ(TilemapAutotile::BlobTileIndex(100, 0), 100);
    }

    TEST(TilemapAutotileTest, BlobIndex_IgnoresUnsupportedCornerBits)
    {
        // Two masks that normalize to the same thing (a lone NE corner has no effect
        // without both its edges) must map to the same blob tile.
        EXPECT_EQ(TilemapAutotile::BlobIndex(TilemapAutotile::N), TilemapAutotile::BlobIndex(TilemapAutotile::N | TilemapAutotile::NE));
    }
} // namespace Diorama
