/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <Clients/TilemapAnimation.h>
#include <Clients/TilemapAutotile.h>
#include <Clients/TilemapCollision.h>
#include <Clients/TilemapComponent.h>
#include <Diorama/TilemapComponentConfig.h>

namespace Diorama
{
    // ---- Custom autotile rules --------------------------------------------------

    TEST(TilemapRuleTest, MatchingRuleWinsOverBlobFallback)
    {
        // A fully surrounded cell normalizes to all eight neighbors present.
        const AZ::u8 full = TilemapAutotile::NormalizeBlobMask(
            TilemapAutotile::N | TilemapAutotile::E | TilemapAutotile::S | TilemapAutotile::W | TilemapAutotile::NE | TilemapAutotile::SE |
            TilemapAutotile::SW | TilemapAutotile::NW);

        const TilemapAutotile::RuleEntry rules[1] = { { full, 42 } };
        const int offset = TilemapAutotile::RuleSetOffset(0xFF, AZStd::span<const TilemapAutotile::RuleEntry>(rules, 1));
        EXPECT_EQ(offset, 42);
    }

    TEST(TilemapRuleTest, NoMatchFallsBackToBlobIndex)
    {
        // An isolated cell (no neighbors) -> blob index 0; with no matching rule the
        // rule resolver must return that canonical index, not a rule's offset.
        const TilemapAutotile::RuleEntry rules[1] = { { 0xF0, 7 } };
        const int offset = TilemapAutotile::RuleSetOffset(0x00, AZStd::span<const TilemapAutotile::RuleEntry>(rules, 1));
        EXPECT_EQ(offset, TilemapAutotile::BlobIndex(0x00));
    }

    TEST(TilemapRuleTest, FirstMatchingRuleWins)
    {
        const AZ::u8 norm = TilemapAutotile::NormalizeBlobMask(TilemapAutotile::N);
        const TilemapAutotile::RuleEntry rules[2] = { { norm, 1 }, { norm, 2 } };
        EXPECT_EQ(TilemapAutotile::RuleSetOffset(TilemapAutotile::N, AZStd::span<const TilemapAutotile::RuleEntry>(rules, 2)), 1);
    }

    // ---- Per-tile collision greedy meshing --------------------------------------

    namespace
    {
        // Build a solidity predicate from a row-major 0/1 grid literal.
        struct Grid
        {
            int m_columns;
            int m_rows;
            AZStd::vector<int> m_cells;
            bool operator()(int col, int row) const
            {
                return m_cells[static_cast<size_t>(row) * static_cast<size_t>(m_columns) + static_cast<size_t>(col)] != 0;
            }
        };

        int CellsCovered(const AZStd::vector<TilemapCollision::CellBox>& boxes)
        {
            int total = 0;
            for (const auto& b : boxes)
            {
                total += b.m_width * b.m_height;
            }
            return total;
        }
    } // namespace

    TEST(TilemapCollisionTest, EmptyGridYieldsNoBoxes)
    {
        Grid g{ 3, 3, AZStd::vector<int>(9, 0) };
        const auto boxes = TilemapCollision::MergeSolidCells(g.m_columns, g.m_rows, g);
        EXPECT_TRUE(boxes.empty());
    }

    TEST(TilemapCollisionTest, FullGridMergesToOneBox)
    {
        Grid g{ 4, 3, AZStd::vector<int>(12, 1) };
        const auto boxes = TilemapCollision::MergeSolidCells(g.m_columns, g.m_rows, g);
        ASSERT_EQ(boxes.size(), 1u);
        EXPECT_EQ(boxes[0].m_col, 0);
        EXPECT_EQ(boxes[0].m_row, 0);
        EXPECT_EQ(boxes[0].m_width, 4);
        EXPECT_EQ(boxes[0].m_height, 3);
    }

    TEST(TilemapCollisionTest, HorizontalRunMergesAcross)
    {
        // A single solid row of 5 -> one 5x1 box, not five boxes.
        Grid g{ 5, 1, { 1, 1, 1, 1, 1 } };
        const auto boxes = TilemapCollision::MergeSolidCells(g.m_columns, g.m_rows, g);
        ASSERT_EQ(boxes.size(), 1u);
        EXPECT_EQ(boxes[0].m_width, 5);
        EXPECT_EQ(boxes[0].m_height, 1);
    }

    TEST(TilemapCollisionTest, CoversExactlyTheSolidCellsWithoutOverlap)
    {
        // An L shape: a hole splits it but every solid cell is covered exactly once.
        // grid (rows top->bottom):
        // 1 1 0
        // 1 0 0
        // 1 1 1
        Grid g{ 3, 3, { 1, 1, 0, 1, 0, 0, 1, 1, 1 } };
        const auto boxes = TilemapCollision::MergeSolidCells(g.m_columns, g.m_rows, g);
        int solid = 0;
        for (int v : g.m_cells)
        {
            solid += v;
        }
        EXPECT_EQ(CellsCovered(boxes), solid); // exact cover, no double-count
        // Boxes never overlap a non-solid cell.
        for (const auto& b : boxes)
        {
            for (int r = b.m_row; r < b.m_row + b.m_height; ++r)
            {
                for (int c = b.m_col; c < b.m_col + b.m_width; ++c)
                {
                    EXPECT_TRUE(g(c, r));
                }
            }
        }
    }

    TEST(TilemapCollisionTest, ToWorldBoxCentersAndSizes)
    {
        // A 2x1 cell box, tile size 10x10, origin at top-left (0,0).
        const TilemapCollision::CellBox cell{ 0, 0, 2, 1 };
        const auto world = TilemapCollision::ToWorldBox(cell, 10.0f, 10.0f, 0.0f, 0.0f);
        EXPECT_NEAR(world.m_halfWidth, 10.0f, 1e-4f); // 2 cells * 10 / 2
        EXPECT_NEAR(world.m_halfHeight, 5.0f, 1e-4f);
        EXPECT_NEAR(world.m_centerX, 10.0f, 1e-4f); // spans X [0,20] -> center 10
        EXPECT_NEAR(world.m_centerY, -5.0f, 1e-4f); // row grows downward -> negative Y
    }

    // ---- BuildTilemapColliders: config solid tiles -> merged world boxes ----------

    TEST(TilemapCollisionTest, BuildTilemapCollidersMergesSolidRow)
    {
        TilemapComponentConfig config;
        config.m_columns = 3;
        config.m_rows = 1;
        config.m_tileSize = AZ::Vector2(2.0f, 2.0f);
        config.SetTile(0, 0, 5);
        config.SetTile(1, 0, 5);
        config.SetTile(2, 0, 5);
        config.m_solidTiles = { 5 };

        const AZStd::vector<Collision2D::Collider> boxes = BuildTilemapColliders(config, 0.0f, 0.0f, 4u);
        ASSERT_EQ(boxes.size(), 1u); // a 3-wide solid run merges into one box
        EXPECT_EQ(boxes[0].m_shape.m_type, Collision2D::ShapeType::Box);
        EXPECT_NEAR(boxes[0].m_shape.m_halfExtents.GetX(), 3.0f, 1e-4f); // 3 cells * 2 / 2
        EXPECT_NEAR(boxes[0].m_shape.m_halfExtents.GetY(), 1.0f, 1e-4f); // 1 cell * 2 / 2
        EXPECT_NEAR(boxes[0].m_center.GetX(), 0.0f, 1e-4f); // grid centered on the origin
        EXPECT_EQ(boxes[0].m_layer, 4u);
    }

    TEST(TilemapCollisionTest, BuildTilemapCollidersHonorsSolidSetAndWorldOffset)
    {
        TilemapComponentConfig config;
        config.m_columns = 2;
        config.m_rows = 1;
        config.m_tileSize = AZ::Vector2(1.0f, 1.0f);
        config.SetTile(0, 0, 1); // solid
        config.SetTile(1, 0, 9); // not in the solid set
        config.m_solidTiles = { 1 };

        const auto boxes = BuildTilemapColliders(config, 100.0f, 50.0f, 1u);
        ASSERT_EQ(boxes.size(), 1u); // only the tile-1 cell is solid
        EXPECT_NEAR(boxes[0].m_shape.m_halfExtents.GetX(), 0.5f, 1e-4f);
        EXPECT_GT(boxes[0].m_center.GetX(), 99.0f); // offset by the world position

        config.m_solidTiles.clear();
        EXPECT_TRUE(BuildTilemapColliders(config, 0.0f, 0.0f, 1u).empty());
    }

    // ---- Animated tiles: frame selection over time ------------------------------

    TEST(TilemapAnimationTest, StaticInputsHoldFrameZero)
    {
        // Single/empty frame list, non-positive fps, or t<=0 all hold the first frame.
        EXPECT_EQ(TilemapAnimation::FrameAtTime(5.0f, 8.0f, 1, true), 0); // one frame
        EXPECT_EQ(TilemapAnimation::FrameAtTime(5.0f, 8.0f, 0, true), 0); // no frames
        EXPECT_EQ(TilemapAnimation::FrameAtTime(5.0f, 0.0f, 4, true), 0); // fps 0
        EXPECT_EQ(TilemapAnimation::FrameAtTime(5.0f, -8.0f, 4, true), 0); // fps negative
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.0f, 8.0f, 4, true), 0); // t == 0
        EXPECT_EQ(TilemapAnimation::FrameAtTime(-1.0f, 8.0f, 4, true), 0); // t < 0
    }

    TEST(TilemapAnimationTest, AdvancesOneFramePerInterval)
    {
        // At 10 fps each frame lasts 0.1s; sample mid-interval to avoid boundary ties.
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.05f, 10.0f, 4, true), 0);
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.15f, 10.0f, 4, true), 1);
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.25f, 10.0f, 4, true), 2);
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.35f, 10.0f, 4, true), 3);
    }

    TEST(TilemapAnimationTest, LoopWrapsModuloFrameCount)
    {
        // After the 4th frame a looping sequence wraps back to frame 0.
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.45f, 10.0f, 4, true), 0);
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.55f, 10.0f, 4, true), 1);
        // A whole extra cycle later lands on the same phase.
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.85f, 10.0f, 4, true), 0);
    }

    TEST(TilemapAnimationTest, NonLoopHoldsLastFrame)
    {
        // A one-shot sequence clamps to the final frame and stays there.
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.35f, 10.0f, 4, false), 3);
        EXPECT_EQ(TilemapAnimation::FrameAtTime(0.45f, 10.0f, 4, false), 3);
        EXPECT_EQ(TilemapAnimation::FrameAtTime(100.0f, 10.0f, 4, false), 3);
    }
} // namespace Diorama
