/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaBulletBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void DioramaBulletInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaBulletInfo>()
                ->Version(1)
                ->Field("aliveCount", &DioramaBulletInfo::m_aliveCount)
                ->Field("maxBullets", &DioramaBulletInfo::m_maxBullets)
                ->Field("firing", &DioramaBulletInfo::m_firing)
                ->Field("pattern", &DioramaBulletInfo::m_pattern)
                ->Field("fireRate", &DioramaBulletInfo::m_fireRate);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<DioramaBulletInfo>("DioramaBulletInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Bullet")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("aliveCount", BehaviorValueGetter(&DioramaBulletInfo::m_aliveCount), nullptr)
                ->Property("maxBullets", BehaviorValueGetter(&DioramaBulletInfo::m_maxBullets), nullptr)
                ->Property("firing", BehaviorValueGetter(&DioramaBulletInfo::m_firing), nullptr)
                ->Property("pattern", BehaviorValueGetter(&DioramaBulletInfo::m_pattern), nullptr)
                ->Property("fireRate", BehaviorValueGetter(&DioramaBulletInfo::m_fireRate), nullptr);
        }
    }

    void ReflectBulletBuses(AZ::ReflectContext* context)
    {
        DioramaBulletInfo::Reflect(context);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->EBus<DioramaBulletRequestBus>("DioramaBulletRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Bullet")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("Fire", &DioramaBulletRequestBus::Events::Fire)
            ->Event("Play", &DioramaBulletRequestBus::Events::Play)
            ->Event("Stop", &DioramaBulletRequestBus::Events::Stop)
            ->Event("SetPattern", &DioramaBulletRequestBus::Events::SetPattern, { { { "pattern", "0 ring, 1 fan, 2 spiral." } } })
            ->Event(
                "SetFireRate", &DioramaBulletRequestBus::Events::SetFireRate, { { { "shotsPerSecond", "Shots per second; 0 = manual." } } })
            ->Event("SetCount", &DioramaBulletRequestBus::Events::SetCount, { { { "count", "Bullets per shot." } } })
            ->Event("SetSpeed", &DioramaBulletRequestBus::Events::SetSpeed, { { { "speed", "World units/sec." } } })
            ->Event("SetAim", &DioramaBulletRequestBus::Events::SetAim, { { { "degrees", "Base angle (0 = +X, 90 = +Y)." } } })
            ->Event("SetSpread", &DioramaBulletRequestBus::Events::SetSpread, { { { "degrees", "Fan arc width." } } })
            ->Event("SetSpin", &DioramaBulletRequestBus::Events::SetSpin, { { { "degreesPerShot", "Spiral rotation per shot." } } })
            ->Event("GetBulletInfo", &DioramaBulletRequestBus::Events::GetBulletInfo);

        behaviorContext->EBus<DioramaBulletNotificationBus>("DioramaBulletNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Bullet")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaBulletNotificationHandler>();
    }
} // namespace Diorama
