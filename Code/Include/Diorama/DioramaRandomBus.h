/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Global (broadcast) seeded gameplay randomness; handled by the 2D Simulation
    //! Clock component. The same seed always yields the same sequence on every
    //! platform (exact integer math), so randomness can be replayed and, in later
    //! phases, snapshotted and restored for rollback. NOT cryptographic: never use it
    //! for anything security-sensitive. Without a clock component in the level the bus
    //! has no handler and the verbs return defaults; non-deterministic games can keep
    //! using Lua's math.random.
    class DioramaRandomRequests : public AZ::EBusTraits
    {
    public:
        AZ_RTTI(DioramaRandomRequests, DioramaRandomRequestsTypeId);
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~DioramaRandomRequests() = default;

        //! Reset the sequence from a seed. Any value is fine (it is spread before
        //! use); the draw counter resets to 0.
        virtual void SetSeed(AZ::u64 seed) = 0;
        //! Uniform float in [0, 1).
        virtual float RandFloat() = 0;
        //! Uniform float in [minValue, maxValue); a reversed range returns minValue.
        virtual float RandRange(float minValue, float maxValue) = 0;
        //! Uniform integer in [minValue, maxValue], both inclusive; a reversed range
        //! returns minValue.
        virtual AZ::s64 RandInt(AZ::s64 minValue, AZ::s64 maxValue) = 0;
        //! Values drawn since the last seed (verify-loop readback: two runs that drew
        //! the same count from the same seed are in the same sequence position).
        virtual AZ::s64 GetRandomDraws() = 0;
    };

    using DioramaRandomRequestBus = AZ::EBus<DioramaRandomRequests>;
} // namespace Diorama
