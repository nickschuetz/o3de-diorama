/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>

// Pure, header-only fixed-timestep accumulator: turn variable render delta times into
// a whole number of fixed simulation steps, with a catch-up cap so a long hitch cannot
// spiral (each render frame runs at most maxCatchUpSteps and the excess backlog is
// dropped). It has no engine / component dependency: the 2D Simulation Clock component
// owns one and emits OnSimTick per step, but the stepping math is unit tested directly
// (the DayNightCycle.h / HitboxFrames.h pattern).
//
// Determinism note: the fixed step makes the simulation advance in exact frames, so
// the same per-frame inputs replay to the same result on any machine running the same
// build. That is the foundation rollback, replays, and the headless determinism tests
// build on (see Docs/design/2d-deterministic-sim.md).
namespace Diorama::SimClock
{
    //! Accumulator state. m_frame counts every simulation step ever run (monotonic).
    struct State
    {
        double m_accumulator = 0.0; //!< Unconsumed render seconds.
        AZ::s64 m_frame = 0; //!< Simulation frames run since reset.
    };

    //! Feed `dtSeconds` of render time and return how many fixed steps of
    //! `stepSeconds` to run now (0 when not enough time has accumulated). At most
    //! `maxCatchUpSteps` steps are granted per call; any backlog beyond that is
    //! dropped (the clock slows rather than spiraling). Non-positive step or dt
    //! grants nothing; negative dt is ignored. The caller advances m_frame once per
    //! step it actually runs (see Step).
    inline int Advance(State& state, double dtSeconds, double stepSeconds, int maxCatchUpSteps)
    {
        if (stepSeconds <= 0.0 || maxCatchUpSteps <= 0)
        {
            return 0;
        }
        if (dtSeconds > 0.0)
        {
            state.m_accumulator += dtSeconds;
        }
        int steps = 0;
        while (state.m_accumulator >= stepSeconds && steps < maxCatchUpSteps)
        {
            state.m_accumulator -= stepSeconds;
            ++steps;
        }
        if (state.m_accumulator >= stepSeconds)
        {
            // Backlog beyond the cap: drop it so the next frame starts fresh instead
            // of chasing an ever-growing debt.
            state.m_accumulator = 0.0;
        }
        return steps;
    }

    //! Record one executed step and return its frame number (the frame the step IS,
    //! counting from 0).
    inline AZ::s64 Step(State& state)
    {
        return state.m_frame++;
    }
} // namespace Diorama::SimClock
