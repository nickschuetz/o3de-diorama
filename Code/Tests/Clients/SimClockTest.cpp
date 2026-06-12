/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/SimClock.h>
#include <Clients/SimRandom.h>

namespace Diorama
{
    // ---- SimClock: fixed-step accumulation ------------------------------------------

    TEST(SimClockTest, ExactStepGrantsExactlyOne)
    {
        SimClock::State s;
        EXPECT_EQ(SimClock::Advance(s, 1.0 / 60.0, 1.0 / 60.0, 5), 1);
        EXPECT_NEAR(s.m_accumulator, 0.0, 1e-12);
    }

    TEST(SimClockTest, ShortFramesAccumulateToOneStep)
    {
        SimClock::State s;
        const double step = 1.0 / 60.0;
        // Three quarter-steps: no step, no step, then one (with a quarter left over).
        EXPECT_EQ(SimClock::Advance(s, step * 0.5, step, 5), 0);
        EXPECT_EQ(SimClock::Advance(s, step * 0.5, step, 5), 1);
        EXPECT_EQ(SimClock::Advance(s, step * 0.25, step, 5), 0);
        EXPECT_NEAR(s.m_accumulator, step * 0.25, 1e-12);
    }

    TEST(SimClockTest, LongFrameGrantsMultipleSteps)
    {
        SimClock::State s;
        const double step = 1.0 / 60.0;
        EXPECT_EQ(SimClock::Advance(s, step * 3.5, step, 5), 3);
        EXPECT_NEAR(s.m_accumulator, step * 0.5, 1e-12);
    }

    TEST(SimClockTest, CatchUpCapClampsAndDropsBacklog)
    {
        SimClock::State s;
        const double step = 1.0 / 60.0;
        // A one-second hitch at 60 Hz wants 60 steps; the cap grants 5 and drops the
        // rest so the next frame starts fresh instead of chasing the debt.
        EXPECT_EQ(SimClock::Advance(s, 1.0, step, 5), 5);
        EXPECT_NEAR(s.m_accumulator, 0.0, 1e-12);
        EXPECT_EQ(SimClock::Advance(s, step, step, 5), 1);
    }

    TEST(SimClockTest, DegenerateInputsGrantNothing)
    {
        SimClock::State s;
        EXPECT_EQ(SimClock::Advance(s, 1.0, 0.0, 5), 0); // no step length
        EXPECT_EQ(SimClock::Advance(s, 1.0, -1.0, 5), 0); // negative step
        EXPECT_EQ(SimClock::Advance(s, 1.0, 1.0 / 60.0, 0), 0); // no step budget
        SimClock::State s2;
        EXPECT_EQ(SimClock::Advance(s2, -1.0, 1.0 / 60.0, 5), 0); // negative dt ignored
        EXPECT_NEAR(s2.m_accumulator, 0.0, 1e-12);
    }

    TEST(SimClockTest, StepCountsFramesMonotonically)
    {
        SimClock::State s;
        EXPECT_EQ(SimClock::Step(s), 0);
        EXPECT_EQ(SimClock::Step(s), 1);
        EXPECT_EQ(SimClock::Step(s), 2);
        EXPECT_EQ(s.m_frame, 3);
    }

    TEST(SimClockTest, SameFeedReplaysToSameFrames)
    {
        // Determinism at the accumulator level: the same dt sequence grants the same
        // step counts in the same order.
        const double feed[] = { 0.013, 0.021, 0.016, 0.050, 0.008, 0.017, 0.300, 0.016 };
        const double step = 1.0 / 60.0;
        SimClock::State a;
        SimClock::State b;
        for (double dt : feed)
        {
            const int sa = SimClock::Advance(a, dt, step, 5);
            const int sb = SimClock::Advance(b, dt, step, 5);
            EXPECT_EQ(sa, sb);
            for (int i = 0; i < sa; ++i)
            {
                SimClock::Step(a);
                SimClock::Step(b);
            }
        }
        EXPECT_EQ(a.m_frame, b.m_frame);
        EXPECT_EQ(a.m_accumulator, b.m_accumulator);
    }

    // ---- SimRandom: seeded reproducible randomness -----------------------------------

    TEST(SimRandomTest, SameSeedSameSequence)
    {
        SimRandom::State a;
        SimRandom::State b;
        SimRandom::Seed(a, 12345);
        SimRandom::Seed(b, 12345);
        for (int i = 0; i < 100; ++i)
        {
            EXPECT_EQ(SimRandom::NextU64(a), SimRandom::NextU64(b));
        }
        EXPECT_EQ(a.m_draws, 100u);
    }

    TEST(SimRandomTest, DifferentSeedsDiverge)
    {
        SimRandom::State a;
        SimRandom::State b;
        SimRandom::Seed(a, 1);
        SimRandom::Seed(b, 2); // adjacent seeds must still produce unrelated streams
        int differing = 0;
        for (int i = 0; i < 10; ++i)
        {
            if (SimRandom::NextU64(a) != SimRandom::NextU64(b))
            {
                ++differing;
            }
        }
        EXPECT_GT(differing, 8);
    }

    TEST(SimRandomTest, ZeroSeedIsValid)
    {
        SimRandom::State s;
        SimRandom::Seed(s, 0);
        EXPECT_NE(s.m_state, 0u); // xorshift state must never be zero
        EXPECT_NE(SimRandom::NextU64(s), 0u);
    }

    TEST(SimRandomTest, Float01StaysInRange)
    {
        SimRandom::State s;
        SimRandom::Seed(s, 7);
        for (int i = 0; i < 1000; ++i)
        {
            const float v = SimRandom::Float01(s);
            EXPECT_GE(v, 0.0f);
            EXPECT_LT(v, 1.0f);
        }
    }

    TEST(SimRandomTest, RangeRespectsBoundsAndRejectsReversed)
    {
        SimRandom::State s;
        SimRandom::Seed(s, 7);
        for (int i = 0; i < 1000; ++i)
        {
            const float v = SimRandom::Range(s, -3.0f, 5.0f);
            EXPECT_GE(v, -3.0f);
            EXPECT_LT(v, 5.0f);
        }
        EXPECT_FLOAT_EQ(SimRandom::Range(s, 5.0f, -3.0f), 5.0f); // reversed: min returned
        EXPECT_FLOAT_EQ(SimRandom::Range(s, 2.0f, 2.0f), 2.0f); // degenerate: min returned
    }

    TEST(SimRandomTest, RangeIntInclusiveBothEnds)
    {
        SimRandom::State s;
        SimRandom::Seed(s, 42);
        bool sawMin = false;
        bool sawMax = false;
        for (int i = 0; i < 2000; ++i)
        {
            const AZ::s64 v = SimRandom::RangeInt(s, -2, 2);
            EXPECT_GE(v, -2);
            EXPECT_LE(v, 2);
            sawMin = sawMin || (v == -2);
            sawMax = sawMax || (v == 2);
        }
        EXPECT_TRUE(sawMin);
        EXPECT_TRUE(sawMax);
        EXPECT_EQ(SimRandom::RangeInt(s, 3, 3), 3); // degenerate
        EXPECT_EQ(SimRandom::RangeInt(s, 3, 1), 3); // reversed: min returned
    }

    TEST(SimRandomTest, ReseedRestartsTheSequence)
    {
        SimRandom::State s;
        SimRandom::Seed(s, 99);
        const AZ::u64 first = SimRandom::NextU64(s);
        SimRandom::NextU64(s);
        SimRandom::Seed(s, 99);
        EXPECT_EQ(s.m_draws, 0u);
        EXPECT_EQ(SimRandom::NextU64(s), first);
    }
} // namespace Diorama
