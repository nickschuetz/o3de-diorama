/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SkinnedSpritePresenter.h>
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
        void SetBoneRotation(const AZStd::string& boneName, float degrees) override;
        void SetBoneTranslation(const AZStd::string& boneName, const AZ::Vector2& offset) override;
        void ResetPose() override;
        SkinnedSpriteInfo GetSkinnedSpriteInfo() override;

    private:
        DioramaSkinnedSpriteConfig m_config;
        SkinnedSpritePresenter m_presenter;
    };
} // namespace Diorama
