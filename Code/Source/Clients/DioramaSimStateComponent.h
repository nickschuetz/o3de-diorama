/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SimStateBus.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/RTTI/RTTI.h>

namespace Diorama
{
    //! Simulation State marker: enrolls this entity in the 2D Simulation Clock's
    //! frame capture (CaptureFrame / SaveToSlot). The marker itself snapshots and
    //! restores the entity's WORLD TRANSLATION, which in transform-driven Diorama
    //! gameplay is the bulk of an entity's sim state; snapshot-capable Diorama
    //! components on the same entity contribute their own chunks through the same
    //! participant bus. No configuration: adding the component is the enrollment.
    //! See Docs/design/2d-deterministic-sim.md (phase B).
    class DioramaSimStateComponent final
        : public AZ::Component
        , protected DioramaSimStateRegistryBus::Handler
        , protected DioramaSimStateParticipantBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaSimStateComponent, DioramaSimStateComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! Chunk tag for the entity world-translation payload (three F32s).
        static constexpr AZ::u32 TranslationChunkTag = 0x534E5254; // 'TRNS' little-endian

        DioramaSimStateComponent() = default;
        ~DioramaSimStateComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // DioramaSimStateRegistry
        AZ::EntityId GetSimStateEntity() override;

        // DioramaSimStateParticipants
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;
    };
} // namespace Diorama
