/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>

// Pure, header-only seeded pseudo-random generator for deterministic gameplay:
// the same seed always yields the same sequence, on every platform, because the
// generator is exact integer math (no floating-point in the state transition).
// This is NOT a cryptographic generator and must never gate anything
// security-sensitive; it exists so gameplay randomness can be seeded, replayed,
// and (later) snapshotted for rollback (see Docs/design/2d-deterministic-sim.md).
//
// Algorithm: splitmix64 to spread the user seed into the state, then
// xorshift64* for the sequence. Small, fast, and statistically fine for
// gameplay (spawn positions, variance, shuffles).
namespace Diorama::SimRandom
{
    //! Generator state. Value-type: copy it to snapshot, assign to restore.
    struct State
    {
        AZ::u64 m_state = 0x9E3779B97F4A7C15ull; //!< Never zero (xorshift fixed point).
        AZ::u64 m_draws = 0; //!< Values drawn since the last seed (verify-loop readback).
    };

    //! Seed the generator. Any value is fine, including zero: the seed is spread
    //! through splitmix64 so similar seeds still produce unrelated sequences.
    inline void Seed(State& state, AZ::u64 seed)
    {
        // One splitmix64 step; its output is never zero for any input, which
        // xorshift64* requires of its state.
        AZ::u64 z = seed + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        state.m_state = (z != 0ull) ? z : 0x9E3779B97F4A7C15ull;
        state.m_draws = 0;
    }

    //! Next raw 64-bit value (xorshift64*).
    inline AZ::u64 NextU64(State& state)
    {
        AZ::u64 x = state.m_state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state.m_state = x;
        ++state.m_draws;
        return x * 0x2545F4914F6CDD1Dull;
    }

    //! Uniform float in [0, 1): the top 24 bits scaled, so every value is exactly
    //! representable and the result is identical on every platform.
    inline float Float01(State& state)
    {
        return static_cast<float>(NextU64(state) >> 40) * (1.0f / 16777216.0f);
    }

    //! Uniform float in [minValue, maxValue); a reversed or degenerate range
    //! returns minValue.
    inline float Range(State& state, float minValue, float maxValue)
    {
        if (!(maxValue > minValue))
        {
            return minValue;
        }
        return minValue + (maxValue - minValue) * Float01(state);
    }

    //! Uniform integer in [minValue, maxValue] (both inclusive); a reversed range
    //! returns minValue. Uses 64-bit modulo of a 64-bit draw: the bias for
    //! gameplay-sized ranges is negligible and the result is platform-identical.
    inline AZ::s64 RangeInt(State& state, AZ::s64 minValue, AZ::s64 maxValue)
    {
        if (maxValue <= minValue)
        {
            return minValue;
        }
        const AZ::u64 span = static_cast<AZ::u64>(maxValue - minValue) + 1ull;
        return minValue + static_cast<AZ::s64>(NextU64(state) % span);
    }
} // namespace Diorama::SimRandom
