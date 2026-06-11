/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/HitboxFrames.h>

namespace Diorama
{
    TEST(HitboxFramesTest, IsActiveOnlyWithinTheWindowInclusive)
    {
        HitboxFrames::Box box;
        box.m_kind = HitboxFrames::BoxKind::Hitbox;
        box.m_startFrame = 3; // active frames
        box.m_endFrame = 5;

        EXPECT_FALSE(HitboxFrames::IsActiveAt(box, 2)); // startup
        EXPECT_TRUE(HitboxFrames::IsActiveAt(box, 3)); // first active
        EXPECT_TRUE(HitboxFrames::IsActiveAt(box, 4));
        EXPECT_TRUE(HitboxFrames::IsActiveAt(box, 5)); // last active
        EXPECT_FALSE(HitboxFrames::IsActiveAt(box, 6)); // recovery
    }

    TEST(HitboxFramesTest, DegenerateWindowIsNeverActive)
    {
        HitboxFrames::Box box;
        box.m_startFrame = 5;
        box.m_endFrame = 4; // end before start
        EXPECT_FALSE(HitboxFrames::IsActiveAt(box, 4));
        EXPECT_FALSE(HitboxFrames::IsActiveAt(box, 5));
    }

    TEST(HitboxFramesTest, SingleFrameWindow)
    {
        HitboxFrames::Box box;
        box.m_startFrame = 7;
        box.m_endFrame = 7; // one-frame active window
        EXPECT_FALSE(HitboxFrames::IsActiveAt(box, 6));
        EXPECT_TRUE(HitboxFrames::IsActiveAt(box, 7));
        EXPECT_FALSE(HitboxFrames::IsActiveAt(box, 8));
    }
} // namespace Diorama
