/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>

#include <Clients/DioramaSimClockComponent.h>
#include <Diorama/DioramaSimClockBus.h>

// Super-freeze acceptance: FreezeFor(n) suppresses the clock's automatic stepping for
// exactly n fixed steps - so every OnSimTick consumer holds - then stepping resumes on
// its own, while StepOnce still forces a step (so a rollback re-simulation is
// unaffected). The clock runs at 64 steps/sec here so the step length (1/64) is an
// exact binary fraction and whole-step dt values produce exact step counts.
namespace Diorama
{
    namespace
    {
        //! Counts OnSimTick broadcasts (the heartbeat every gameplay system advances on).
        class TickCounter : public DioramaSimTickNotificationBus::Handler
        {
        public:
            void Listen()
            {
                DioramaSimTickNotificationBus::Handler::BusConnect();
            }
            void Stop()
            {
                DioramaSimTickNotificationBus::Handler::BusDisconnect();
            }
            void OnSimTick(AZ::s64, float) override
            {
                ++m_count;
            }
            int m_count = 0;
        };
    } // namespace

    class SimClockFreezeTest : public ::testing::Test
    {
    protected:
        static constexpr float StepsPerSecond = 64.0f;
        static constexpr float Step = 1.0f / StepsPerSecond; // 0.015625, exact in float

        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor desc;
            AZ::ComponentApplication::StartupParameters startup;
            startup.m_loadSettingsRegistry = false;
            m_systemEntity = m_app.Create(desc, startup);
            ASSERT_NE(m_systemEntity, nullptr);
            m_app.RegisterComponentDescriptor(DioramaSimClockComponent::CreateDescriptor());
            m_systemEntity->Init();
            m_systemEntity->Activate();

            DioramaSimClockConfig config;
            config.m_startPaused = false;
            config.m_stepsPerSecond = StepsPerSecond;
            config.m_maxCatchUpSteps = 1000; // never cap in these short drives
            config.m_randomSeed = 1;
            m_clockEntity = aznew AZ::Entity("SimClock");
            m_clockEntity->CreateComponent<DioramaSimClockComponent>(config);
            m_clockEntity->Init();
            m_clockEntity->Activate();
        }

        void TearDown() override
        {
            m_clockEntity->Deactivate();
            delete m_clockEntity;
            m_app.Destroy();
        }

        //! Drive `steps` fixed steps' worth of real time through the clock's tick.
        void Tick(int steps)
        {
            AZ::TickBus::Broadcast(&AZ::TickBus::Events::OnTick, Step * static_cast<float>(steps), AZ::ScriptTimePoint());
        }

        static AZ::s64 SimFrame()
        {
            AZ::s64 frame = 0;
            DioramaSimClockRequestBus::BroadcastResult(frame, &DioramaSimClockRequests::GetSimFrame);
            return frame;
        }

        AZ::ComponentApplication m_app;
        AZ::Entity* m_systemEntity = nullptr;
        AZ::Entity* m_clockEntity = nullptr;
    };

    TEST_F(SimClockFreezeTest, FreezeSuppressesStepsThenResumes)
    {
        TickCounter counter;
        counter.Listen();

        // Baseline: three real steps run three OnSimTick and advance the sim frame.
        Tick(3);
        EXPECT_EQ(counter.m_count, 3);
        EXPECT_EQ(SimFrame(), 3);

        // Freeze for two steps, then drive three: the first two are suppressed (no
        // tick, frame held), the third runs.
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::FreezeFor, AZ::s64{ 2 });
        Tick(3);
        EXPECT_EQ(counter.m_count, 4); // only the post-freeze step ticked
        EXPECT_EQ(SimFrame(), 4);

        // Freeze is spent; stepping is fully back to normal.
        Tick(2);
        EXPECT_EQ(counter.m_count, 6);
        EXPECT_EQ(SimFrame(), 6);

        counter.Stop();
    }

    TEST_F(SimClockFreezeTest, GetSimClockInfoReportsFreezeState)
    {
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::FreezeFor, AZ::s64{ 5 });
        DioramaSimClockInfo info;
        DioramaSimClockRequestBus::BroadcastResult(info, &DioramaSimClockRequests::GetSimClockInfo);
        EXPECT_TRUE(info.m_frozen);
        EXPECT_EQ(info.m_freezeFramesRemaining, 5);

        Tick(2); // consume two frozen steps
        DioramaSimClockRequestBus::BroadcastResult(info, &DioramaSimClockRequests::GetSimClockInfo);
        EXPECT_TRUE(info.m_frozen);
        EXPECT_EQ(info.m_freezeFramesRemaining, 3);

        // A non-positive count clears the freeze.
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::FreezeFor, AZ::s64{ 0 });
        DioramaSimClockRequestBus::BroadcastResult(info, &DioramaSimClockRequests::GetSimClockInfo);
        EXPECT_FALSE(info.m_frozen);
        EXPECT_EQ(info.m_freezeFramesRemaining, 0);
    }

    TEST_F(SimClockFreezeTest, StepOnceForcesAStepThroughAFreeze)
    {
        TickCounter counter;
        counter.Listen();

        // A long freeze is active, so automatic ticks are suppressed...
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::FreezeFor, AZ::s64{ 100 });
        Tick(3);
        EXPECT_EQ(counter.m_count, 0);

        // ...but StepOnce (the rollback / training frame-advance) still runs a step and
        // does not consume the freeze countdown.
        DioramaSimClockRequestBus::Broadcast(&DioramaSimClockRequests::StepOnce);
        EXPECT_EQ(counter.m_count, 1);
        EXPECT_EQ(SimFrame(), 1);
        DioramaSimClockInfo info;
        DioramaSimClockRequestBus::BroadcastResult(info, &DioramaSimClockRequests::GetSimClockInfo);
        EXPECT_EQ(info.m_freezeFramesRemaining, 97); // 100 - 3 frozen ticks (StepOnce did not decrement)

        counter.Stop();
    }
} // namespace Diorama
