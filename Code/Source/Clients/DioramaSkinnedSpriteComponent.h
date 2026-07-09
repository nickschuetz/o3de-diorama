/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SimStateBus.h>
#include <Clients/SkinnedSpritePresenter.h>
#include <Diorama/DioramaSimClockBus.h>
#include <Diorama/DioramaSkinnedSpriteBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

namespace Diorama
{
    //! Runtime skinned-sprite player. Loads a DragonBones weighted-mesh armature and each
    //! frame poses its bone hierarchy (bind pose plus any per-bone overrides pushed through
    //! DioramaSkinnedSpriteRequestBus), CPU-skins every mesh, and submits the deformed
    //! geometry to the SpriteFeatureProcessor's mesh path so it composes with the gem's 2D
    //! lighting and sorting. The load + skin + submit is shared with the editor twin through
    //! SkinnedSpritePresenter. Driven at runtime through the bus for AI/human parity.
    class DioramaSkinnedSpriteComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaSkinnedSpriteRequestBus::Handler
        , protected DioramaSimTickNotificationBus::Handler
        , protected DioramaSimStateParticipantBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaSkinnedSpriteComponent, DioramaSkinnedSpriteComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        DioramaSkinnedSpriteComponent() = default;
        explicit DioramaSkinnedSpriteComponent(const DioramaSkinnedSpriteConfig& config);
        ~DioramaSkinnedSpriteComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSkinnedSpriteRequests
        void PlayAnimation(const AZStd::string& name, bool looping) override;
        void StopAnimation() override;
        void SetAnimationSpeed(float speed) override;
        void SetBoneRotation(const AZStd::string& boneName, float degrees) override;
        void SetBoneTranslation(const AZStd::string& boneName, float x, float y) override;
        void ResetPose() override;
        SkinnedSpriteInfo GetSkinnedSpriteInfo() override;
        void SetUseSimClock(bool enabled) override;
        bool GetUseSimClock() override;

        // DioramaSimTickNotifications (Use Simulation Clock mode)
        void OnSimTick(AZ::s64 frame, float stepSeconds) override;

        // DioramaSimStateParticipantBus (snapshot / restore of playback position)
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;

    private:
        static constexpr AZ::u32 SkinnedChunkTag = 0x4E494B53; // 'SKIN' little-endian

        DioramaSkinnedSpriteConfig m_config;
        SkinnedSpritePresenter m_presenter;
    };
} // namespace Diorama
