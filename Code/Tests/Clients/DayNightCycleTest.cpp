/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <Clients/DayNightCycle.h>

#include <array>
#include <cmath>

namespace Diorama
{
    using DayNightCycle::Keyframe;

    namespace
    {
        // A four-phase day: night (midnight), sunrise, noon, sunset. Cool dim night,
        // warm low sun at dawn/dusk, bright white overhead at noon.
        std::array<Keyframe, 4> MakeDay()
        {
            return {
                Keyframe{ 0.00f, 0.10f, 0.12f, 0.25f, 0.15f, -60.0f }, // midnight
                Keyframe{ 0.25f, 1.00f, 0.55f, 0.30f, 0.60f, 5.0f }, // sunrise
                Keyframe{ 0.50f, 1.00f, 0.98f, 0.95f, 1.20f, 80.0f }, // noon
                Keyframe{ 0.75f, 1.00f, 0.45f, 0.25f, 0.60f, 5.0f }, // sunset
            };
        }
    } // namespace

    // ---- Clock --------------------------------------------------------------------

    TEST(DayNightCycleTest, Wrap01NormalizesOutOfRange)
    {
        EXPECT_FLOAT_EQ(DayNightCycle::Wrap01(0.25f), 0.25f);
        EXPECT_FLOAT_EQ(DayNightCycle::Wrap01(1.25f), 0.25f);
        EXPECT_FLOAT_EQ(DayNightCycle::Wrap01(-0.75f), 0.25f);
        EXPECT_FLOAT_EQ(DayNightCycle::Wrap01(0.0f), 0.0f);
    }

    TEST(DayNightCycleTest, AdvanceWrapsAcrossMidnight)
    {
        // A 100s cycle: 10s is 0.1 of a day.
        EXPECT_NEAR(DayNightCycle::Advance(0.0f, 10.0f, 100.0f), 0.1f, 1e-5f);
        // Crossing midnight wraps back into range.
        EXPECT_NEAR(DayNightCycle::Advance(0.95f, 10.0f, 100.0f), 0.05f, 1e-5f);
        // A non-positive cycle holds (just wraps the input).
        EXPECT_NEAR(DayNightCycle::Advance(0.3f, 10.0f, 0.0f), 0.3f, 1e-5f);
    }

    // ---- Gradient sampling --------------------------------------------------------

    TEST(DayNightCycleTest, SampleAtKeyReturnsThatKey)
    {
        const auto day = MakeDay();
        const Keyframe noon = DayNightCycle::Sample(day.data(), day.size(), 0.5f);
        EXPECT_FLOAT_EQ(noon.m_sunIntensity, 1.20f);
        EXPECT_FLOAT_EQ(noon.m_elevationDegrees, 80.0f);
        EXPECT_FLOAT_EQ(noon.m_sunR, 1.0f);
    }

    TEST(DayNightCycleTest, SampleMidpointAveragesNeighbors)
    {
        const auto day = MakeDay();
        // Halfway between sunrise (0.25) and noon (0.50): intensity averages 0.6 and 1.2.
        const Keyframe mid = DayNightCycle::Sample(day.data(), day.size(), 0.375f);
        EXPECT_NEAR(mid.m_sunIntensity, 0.90f, 1e-5f);
        EXPECT_NEAR(mid.m_elevationDegrees, 42.5f, 1e-4f); // (5 + 80) / 2
    }

    TEST(DayNightCycleTest, SampleWrapsAcrossMidnight)
    {
        const auto day = MakeDay();
        // 0.875 is halfway between sunset (0.75) and midnight (0.00 = 1.00 on the ring).
        const Keyframe night = DayNightCycle::Sample(day.data(), day.size(), 0.875f);
        EXPECT_NEAR(night.m_sunIntensity, 0.375f, 1e-5f); // (0.60 + 0.15) / 2
        EXPECT_NEAR(night.m_elevationDegrees, -27.5f, 1e-4f); // (5 + -60) / 2
        // Before the first key (0.125, between midnight and sunrise) also interpolates.
        const Keyframe predawn = DayNightCycle::Sample(day.data(), day.size(), 0.125f);
        EXPECT_NEAR(predawn.m_sunIntensity, 0.375f, 1e-5f); // (0.15 + 0.60) / 2
        EXPECT_NEAR(predawn.m_elevationDegrees, -27.5f, 1e-4f); // (-60 + 5) / 2
    }

    TEST(DayNightCycleTest, SampleEmptyOrSingleKey)
    {
        const Keyframe none = DayNightCycle::Sample(nullptr, 0, 0.5f);
        EXPECT_FLOAT_EQ(none.m_sunIntensity, 1.0f); // default

        const Keyframe one[1] = { Keyframe{ 0.3f, 0.2f, 0.4f, 0.6f, 0.7f, 33.0f } };
        const Keyframe got = DayNightCycle::Sample(one, 1, 0.9f);
        EXPECT_FLOAT_EQ(got.m_sunIntensity, 0.7f); // the only key, regardless of t
        EXPECT_FLOAT_EQ(got.m_elevationDegrees, 33.0f);
    }

    // ---- Sun direction ------------------------------------------------------------

    TEST(DayNightCycleTest, SunOverheadTravelsStraightDown)
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        DayNightCycle::SunDirection(90.0f, 180.0f, x, y, z);
        EXPECT_NEAR(x, 0.0f, 1e-5f);
        EXPECT_NEAR(y, -1.0f, 1e-5f);
        EXPECT_NEAR(z, 0.0f, 1e-5f);
    }

    TEST(DayNightCycleTest, SunOnHorizonIsHorizontalAndUnitLength)
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        DayNightCycle::SunDirection(0.0f, 0.0f, x, y, z);
        // Sun at azimuth 0 on the horizon sits toward +X, so its light travels toward -X.
        EXPECT_NEAR(x, -1.0f, 1e-5f);
        EXPECT_NEAR(y, 0.0f, 1e-5f);
        const float len = std::sqrt(x * x + y * y + z * z);
        EXPECT_NEAR(len, 1.0f, 1e-5f);
    }
} // namespace Diorama
