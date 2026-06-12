/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SimState.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/EBus/EBus.h>

// Internal (C++-only) buses for the snapshot/restore contract
// (Docs/design/2d-deterministic-sim.md phase B). Gem components implement these;
// the 2D Simulation Clock drives them from CaptureFrame / RestoreFrame. The binary
// format stays private to the gem, so these headers live in Source/, not the
// public Include/ API.
namespace Diorama
{
    //! Implemented by every snapshot-capable component on a participating entity
    //! (multiple handlers per entity: each component owns its own chunks).
    class DioramaSimStateParticipants : public AZ::ComponentBus
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        virtual ~DioramaSimStateParticipants() = default;

        //! Append this component's state as one or more tagged chunks
        //! (Writer::BeginChunk / EndChunk).
        virtual void SaveSimState(SimState::Writer& writer) = 0;

        //! Offered a chunk read back for this entity. Return true (and consume the
        //! payload) if the tag is yours; false to let another component take it.
        //! The reader covers exactly the chunk payload and is bounds-checked.
        virtual bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) = 0;
    };

    using DioramaSimStateParticipantBus = AZ::EBus<DioramaSimStateParticipants>;

    //! Connected by the Simulation State marker component to enroll its entity in
    //! frame capture. The clock enumerates handlers to build the (sorted)
    //! participant list, so there is no static registry to manage.
    class DioramaSimStateRegistry : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~DioramaSimStateRegistry() = default;

        //! The participating entity.
        virtual AZ::EntityId GetSimStateEntity() = 0;
    };

    using DioramaSimStateRegistryBus = AZ::EBus<DioramaSimStateRegistry>;
} // namespace Diorama
