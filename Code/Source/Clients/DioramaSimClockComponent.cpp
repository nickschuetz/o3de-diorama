/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaSimClockComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    static float ClampStepsPerSecond(float stepsPerSecond)
    {
        if (stepsPerSecond < 1.0f)
        {
            return 1.0f;
        }
        if (stepsPerSecond > 1000.0f)
        {
            return 1000.0f;
        }
        return stepsPerSecond;
    }

    void DioramaSimClockConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSimClockConfig, AZ::ComponentConfig>()
                ->Version(1)
                ->Field("StepsPerSecond", &DioramaSimClockConfig::m_stepsPerSecond)
                ->Field("MaxCatchUpSteps", &DioramaSimClockConfig::m_maxCatchUpSteps)
                ->Field("StartPaused", &DioramaSimClockConfig::m_startPaused)
                ->Field("RandomSeed", &DioramaSimClockConfig::m_randomSeed);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaSimClockConfig>("2D Simulation Clock Config", "Fixed-step simulation heartbeat")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_stepsPerSecond,
                        "Steps Per Second",
                        "Fixed simulation steps per second (60 is the genre standard); clamped to 1..1000")
                    ->Attribute(AZ::Edit::Attributes::Min, 1.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1000.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_maxCatchUpSteps,
                        "Max Catch-Up Steps",
                        "Most steps run in one render frame after a hitch; the rest of the backlog is dropped")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, 30)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_startPaused,
                        "Start Paused",
                        "Begin frozen; advance with StepOnce or SetPaused(false)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaSimClockConfig::m_randomSeed,
                        "Random Seed",
                        "Initial seed for the deterministic gameplay RNG (DioramaRandomRequestBus)");
            }
        }
    }

    void DioramaSimClockComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaSimClockConfig::Reflect(context);
        ReflectSimClockBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSimClockComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaSimClockComponent::m_config);
        }
    }

    void DioramaSimClockComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaSimClockService"));
    }

    void DioramaSimClockComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        // One heartbeat per level: a second clock would double-tick every handler.
        incompatible.push_back(AZ_CRC_CE("DioramaSimClockService"));
    }

    DioramaSimClockComponent::DioramaSimClockComponent(const DioramaSimClockConfig& config)
        : m_config(config)
    {
    }

    void DioramaSimClockComponent::Activate()
    {
        m_config.m_stepsPerSecond = ClampStepsPerSecond(m_config.m_stepsPerSecond);
        m_clock = SimClock::State{};
        m_paused = m_config.m_startPaused;
        SimRandom::Seed(m_random, m_config.m_randomSeed);

        DioramaSimClockRequestBus::Handler::BusConnect();
        DioramaRandomRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void DioramaSimClockComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaRandomRequestBus::Handler::BusDisconnect();
        DioramaSimClockRequestBus::Handler::BusDisconnect();
    }

    void DioramaSimClockComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        if (m_paused)
        {
            return;
        }
        const int steps =
            SimClock::Advance(m_clock, static_cast<double>(deltaTime), static_cast<double>(StepSeconds()), m_config.m_maxCatchUpSteps);
        for (int i = 0; i < steps; ++i)
        {
            RunStep();
        }
    }

    AZ::s64 DioramaSimClockComponent::GetSimFrame()
    {
        return m_clock.m_frame;
    }

    void DioramaSimClockComponent::SetPaused(bool paused)
    {
        m_paused = paused;
    }

    void DioramaSimClockComponent::StepOnce()
    {
        RunStep();
    }

    void DioramaSimClockComponent::SetStepsPerSecond(float stepsPerSecond)
    {
        m_config.m_stepsPerSecond = ClampStepsPerSecond(stepsPerSecond);
    }

    DioramaSimClockInfo DioramaSimClockComponent::GetSimClockInfo()
    {
        DioramaSimClockInfo info;
        info.m_frame = m_clock.m_frame;
        info.m_stepsPerSecond = m_config.m_stepsPerSecond;
        info.m_paused = m_paused;
        info.m_randomDraws = static_cast<AZ::s64>(m_random.m_draws);
        return info;
    }

    void DioramaSimClockComponent::SetSeed(AZ::u64 seed)
    {
        SimRandom::Seed(m_random, seed);
    }

    float DioramaSimClockComponent::RandFloat()
    {
        return SimRandom::Float01(m_random);
    }

    float DioramaSimClockComponent::RandRange(float minValue, float maxValue)
    {
        return SimRandom::Range(m_random, minValue, maxValue);
    }

    AZ::s64 DioramaSimClockComponent::RandInt(AZ::s64 minValue, AZ::s64 maxValue)
    {
        return SimRandom::RangeInt(m_random, minValue, maxValue);
    }

    AZ::s64 DioramaSimClockComponent::GetRandomDraws()
    {
        return static_cast<AZ::s64>(m_random.m_draws);
    }

    void DioramaSimClockComponent::RunStep()
    {
        const AZ::s64 frame = SimClock::Step(m_clock);
        DioramaSimTickNotificationBus::Broadcast(&DioramaSimTickNotifications::OnSimTick, frame, StepSeconds());
    }

    float DioramaSimClockComponent::StepSeconds() const
    {
        return 1.0f / m_config.m_stepsPerSecond;
    }
} // namespace Diorama
