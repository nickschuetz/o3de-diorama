/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaDayNightComponent.h>

#include <Diorama/DioramaLightBus.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    namespace
    {
        // Sun height per phase: below the horizon at midnight, low at dawn/dusk, high at
        // noon. The gradient interpolates between these across the day.
        constexpr float NightElevation = -60.0f;
        constexpr float DawnElevation = 5.0f;
        constexpr float NoonElevation = 80.0f;
        constexpr float DuskElevation = 5.0f;
    } // namespace

    void DioramaDayNightConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaDayNightConfig>()
                ->Version(1)
                ->Field("timeOfDay", &DioramaDayNightConfig::m_timeOfDay)
                ->Field("cycleSeconds", &DioramaDayNightConfig::m_cycleSeconds)
                ->Field("paused", &DioramaDayNightConfig::m_paused)
                ->Field("targetLight", &DioramaDayNightConfig::m_targetLight)
                ->Field("driveColor", &DioramaDayNightConfig::m_driveColor)
                ->Field("driveIntensity", &DioramaDayNightConfig::m_driveIntensity)
                ->Field("driveDirection", &DioramaDayNightConfig::m_driveDirection)
                ->Field("nightColor", &DioramaDayNightConfig::m_nightColor)
                ->Field("nightIntensity", &DioramaDayNightConfig::m_nightIntensity)
                ->Field("dawnColor", &DioramaDayNightConfig::m_dawnColor)
                ->Field("dawnIntensity", &DioramaDayNightConfig::m_dawnIntensity)
                ->Field("dayColor", &DioramaDayNightConfig::m_dayColor)
                ->Field("dayIntensity", &DioramaDayNightConfig::m_dayIntensity)
                ->Field("duskColor", &DioramaDayNightConfig::m_duskColor)
                ->Field("duskIntensity", &DioramaDayNightConfig::m_duskIntensity);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaDayNightConfig>("Day/Night Config", "Time-of-day clock driving a Diorama light")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &DioramaDayNightConfig::m_timeOfDay,
                        "Time Of Day",
                        "Start time: 0 midnight, 0.25 sunrise, 0.5 noon, 0.75 sunset")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDayNightConfig::m_cycleSeconds,
                        "Cycle Seconds",
                        "Real seconds for a full 24h cycle (0 freezes the clock)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaDayNightConfig::m_paused, "Paused", "Pause the automatic advance")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDayNightConfig::m_targetLight,
                        "Target Light",
                        "Diorama light to drive (empty = this entity)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaDayNightConfig::m_driveColor, "Drive Color", "Animate the light color")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDayNightConfig::m_driveIntensity,
                        "Drive Intensity",
                        "Animate the light intensity")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDayNightConfig::m_driveDirection,
                        "Drive Direction",
                        "Aim the light from the sun elevation / azimuth")
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaDayNightConfig::m_nightColor, "Night Color", "Sun color at midnight")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaDayNightConfig::m_nightIntensity,
                        "Night Intensity",
                        "Brightness at midnight")
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaDayNightConfig::m_dawnColor, "Dawn Color", "Sun color at sunrise")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaDayNightConfig::m_dawnIntensity, "Dawn Intensity", "Brightness at sunrise")
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaDayNightConfig::m_dayColor, "Day Color", "Sun color at noon")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaDayNightConfig::m_dayIntensity, "Day Intensity", "Brightness at noon")
                    ->DataElement(AZ::Edit::UIHandlers::Color, &DioramaDayNightConfig::m_duskColor, "Dusk Color", "Sun color at sunset")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaDayNightConfig::m_duskIntensity, "Dusk Intensity", "Brightness at sunset");
            }
        }
    }

    DioramaDayNightComponent::DioramaDayNightComponent(const DioramaDayNightConfig& config)
        : m_config(config)
    {
    }

    void DioramaDayNightComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaDayNightConfig::Reflect(context);
        ReflectDayNightBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaDayNightComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaDayNightComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaDayNightComponent>("Day/Night Cycle", "Drives a Diorama light's color and direction over the day")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaDayNightComponent::m_config, "Config", "Day/night configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void DioramaDayNightComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaDayNightService"));
    }

    void DioramaDayNightComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        // One cycle per entity: two would both drive the same target light each tick and
        // fight over its color / intensity / direction.
        incompatible.push_back(AZ_CRC_CE("DioramaDayNightService"));
    }

    AZStd::array<DayNightCycle::Keyframe, 4> DioramaDayNightComponent::BuildKeyframes() const
    {
        const auto key = [](float time, const AZ::Color& c, float intensity, float elevation)
        {
            return DayNightCycle::Keyframe{ time, c.GetR(), c.GetG(), c.GetB(), intensity, elevation };
        };
        return {
            key(0.00f, m_config.m_nightColor, m_config.m_nightIntensity, NightElevation),
            key(0.25f, m_config.m_dawnColor, m_config.m_dawnIntensity, DawnElevation),
            key(0.50f, m_config.m_dayColor, m_config.m_dayIntensity, NoonElevation),
            key(0.75f, m_config.m_duskColor, m_config.m_duskIntensity, DuskElevation),
        };
    }

    AZ::EntityId DioramaDayNightComponent::TargetLight() const
    {
        return m_config.m_targetLight.IsValid() ? m_config.m_targetLight : GetEntityId();
    }

    void DioramaDayNightComponent::Activate()
    {
        m_config.m_timeOfDay = DayNightCycle::Wrap01(m_config.m_timeOfDay);
        m_config.m_cycleSeconds = AZ::GetMax(0.0f, m_config.m_cycleSeconds);

        DioramaDayNightRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        ApplyLighting();
    }

    void DioramaDayNightComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaDayNightRequestBus::Handler::BusDisconnect();
    }

    void DioramaDayNightComponent::ApplyLighting()
    {
        const auto keys = BuildKeyframes();
        const DayNightCycle::Keyframe s = DayNightCycle::Sample(keys.data(), keys.size(), m_config.m_timeOfDay);
        const AZ::EntityId target = TargetLight();

        if (m_config.m_driveColor)
        {
            DioramaLightRequestBus::Event(target, &DioramaLightRequests::SetColor, s.m_sunR, s.m_sunG, s.m_sunB);
        }
        if (m_config.m_driveIntensity)
        {
            DioramaLightRequestBus::Event(target, &DioramaLightRequests::SetIntensity, s.m_sunIntensity);
        }
        if (m_config.m_driveDirection)
        {
            // Azimuth sweeps a full turn over the day, so the sun crosses the scene.
            const float azimuth = m_config.m_timeOfDay * 360.0f;
            float x = 0.0f;
            float y = -1.0f;
            float z = 0.0f;
            DayNightCycle::SunDirection(s.m_elevationDegrees, azimuth, x, y, z);
            DioramaLightRequestBus::Event(target, &DioramaLightRequests::SetDirection, x, y, z);
        }
    }

    void DioramaDayNightComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_config.m_paused && m_config.m_cycleSeconds > 0.0f)
        {
            m_config.m_timeOfDay = DayNightCycle::Advance(m_config.m_timeOfDay, deltaTime, m_config.m_cycleSeconds);
        }
        ApplyLighting();
    }

    void DioramaDayNightComponent::SetTimeOfDay(float timeOfDay)
    {
        m_config.m_timeOfDay = DayNightCycle::Wrap01(timeOfDay);
        ApplyLighting();
    }

    void DioramaDayNightComponent::SetCycleSeconds(float cycleSeconds)
    {
        m_config.m_cycleSeconds = AZ::GetMax(0.0f, cycleSeconds);
    }

    void DioramaDayNightComponent::SetPaused(bool paused)
    {
        m_config.m_paused = paused;
    }

    void DioramaDayNightComponent::StepHours(float hours)
    {
        m_config.m_timeOfDay = DayNightCycle::Wrap01(m_config.m_timeOfDay + hours / 24.0f);
        ApplyLighting();
    }

    float DioramaDayNightComponent::GetTimeOfDay()
    {
        return m_config.m_timeOfDay;
    }

    DioramaDayNightInfo DioramaDayNightComponent::GetDayNightInfo()
    {
        const auto keys = BuildKeyframes();
        const DayNightCycle::Keyframe s = DayNightCycle::Sample(keys.data(), keys.size(), m_config.m_timeOfDay);

        DioramaDayNightInfo info;
        info.m_timeOfDay = m_config.m_timeOfDay;
        info.m_sunR = s.m_sunR;
        info.m_sunG = s.m_sunG;
        info.m_sunB = s.m_sunB;
        info.m_sunIntensity = s.m_sunIntensity;
        info.m_elevationDegrees = s.m_elevationDegrees;
        info.m_isDaytime = s.m_elevationDegrees > 0.0f;
        info.m_paused = m_config.m_paused;
        return info;
    }
} // namespace Diorama
