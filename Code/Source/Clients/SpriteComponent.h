/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SimStateBus.h>
#include <Clients/SpritePresenter.h>
#include <Clients/SpriteRequestHandler.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Component/Component.h>

namespace Diorama
{
    //! Lightweight runtime component that draws a single world-space sprite.
    //! It holds the configuration and delegates all rendering to the shared
    //! SpritePresenter, which talks to the gem's SpriteRenderer. It has no Qt or
    //! tools dependency so it ships in game clients.
    class SpriteComponent final
        : public AZ::Component
        , protected DioramaSimStateParticipantBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::SpriteComponent, SpriteComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        SpriteComponent() = default;
        explicit SpriteComponent(const SpriteComponentConfig& config);
        ~SpriteComponent() override = default;

        //! Snapshot chunk: sprite-sheet playback position ('SPRA' little-endian).
        static constexpr AZ::u32 SpriteChunkTag = 0x41525053; // 'SPRA' little-endian

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // DioramaSimStateParticipantBus (snapshot/restore of playback position)
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;

    private:
        SpriteComponentConfig m_config;
        SpritePresenter m_presenter;
        SpriteRequestHandler m_requestHandler;
    };
} // namespace Diorama
