/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DayNightCycle.h>
#include <Diorama/DioramaDayNightBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/array.h>

namespace Diorama
{
    //! Configuration for a day/night cycle. The cycle advances a normalized time-of-day
    //! clock and, each frame, samples a four-phase lighting gradient (night, sunrise,
    //! noon, sunset) and drives a target Diorama light's color, intensity, and direction
    //! over the day. It does NOT create a light: drop it on an entity that has a Diorama
    //! Light component (the default target), or point it at one. A thin complementary
    //! layer over the lighting feature, backed by the pure tested DayNightCycle core.
    class DioramaDayNightConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaDayNightConfig, DioramaDayNightConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaDayNightConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaDayNightConfig() override = default;

        float m_timeOfDay = 0.5f; //!< Starting normalized time of day (0.5 = noon).
        float m_cycleSeconds = 120.0f; //!< Real seconds for a full 24h cycle (0 freezes the clock).
        bool m_paused = false;
        //! Diorama light to drive. Invalid (the default) targets this same entity, so the
        //! cycle and a Diorama Light component sit together.
        AZ::EntityId m_targetLight;

        //! Whether to drive each light aspect (so a cycle can tint without re-aiming, etc.).
        bool m_driveColor = true;
        bool m_driveIntensity = true;
        bool m_driveDirection = true;

        // Four phase colors + intensities (linear). Elevations are fixed by phase: the sun
        // is below the horizon at midnight and high at noon.
        AZ::Color m_nightColor = AZ::Color(0.12f, 0.16f, 0.32f, 1.0f);
        float m_nightIntensity = 0.15f;
        AZ::Color m_dawnColor = AZ::Color(1.0f, 0.55f, 0.30f, 1.0f);
        float m_dawnIntensity = 0.60f;
        AZ::Color m_dayColor = AZ::Color(1.0f, 0.98f, 0.92f, 1.0f);
        float m_dayIntensity = 1.20f;
        AZ::Color m_duskColor = AZ::Color(1.0f, 0.45f, 0.28f, 1.0f);
        float m_duskIntensity = 0.60f;
    };

    //! Runtime day/night cycle. Advances the normalized time-of-day clock each tick (when
    //! not paused), samples the four-phase gradient with the pure DayNightCycle core, and
    //! drives the target Diorama light through DioramaLightRequestBus: color and intensity
    //! from the gradient, direction from the sun's elevation (which dips below the horizon
    //! at night) and an azimuth that sweeps with the time of day. The game / an agent can
    //! scrub or pause the clock over DioramaDayNightRequestBus. No new rendering: it reuses
    //! the existing light.
    class DioramaDayNightComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaDayNightRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaDayNightComponent, DioramaDayNightComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        DioramaDayNightComponent() = default;
        explicit DioramaDayNightComponent(const DioramaDayNightConfig& config);
        ~DioramaDayNightComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaDayNightRequests
        void SetTimeOfDay(float timeOfDay) override;
        void SetCycleSeconds(float cycleSeconds) override;
        void SetPaused(bool paused) override;
        void StepHours(float hours) override;
        float GetTimeOfDay() override;
        DioramaDayNightInfo GetDayNightInfo() override;

    public:
        //! Sample the gradient at the current time of day and push color / intensity /
        //! direction to the target light. Public so tests can apply deterministically
        //! without the tick bus.
        void ApplyLighting();

    private:
        //! Build the four-phase keyframe ring from the configured phase colors.
        AZStd::array<DayNightCycle::Keyframe, 4> BuildKeyframes() const;
        //! The light entity this cycle drives (m_targetLight, or self when invalid).
        AZ::EntityId TargetLight() const;

        DioramaDayNightConfig m_config;
    };
} // namespace Diorama
