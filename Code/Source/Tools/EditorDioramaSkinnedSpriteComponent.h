/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaSkinnedSpriteComponent.h>
#include <Clients/SkinnedSpritePresenter.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/TickBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Diorama
{
    //! Editor twin for the runtime DioramaSkinnedSpriteComponent: authors the skinned
    //! (DragonBones mesh-deform) config in the Inspector, previews the deformed character
    //! live in the editor viewport (through the shared SkinnedSpritePresenter, the same way
    //! sprites preview), and exports the runtime component via BuildGameEntity.
    class EditorDioramaSkinnedSpriteComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AZ::TickBus::Handler
        , protected DioramaSkinnedSpriteRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(
            Diorama::EditorDioramaSkinnedSpriteComponent,
            EditorDioramaSkinnedSpriteComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        EditorDioramaSkinnedSpriteComponent() = default;
        ~EditorDioramaSkinnedSpriteComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    protected:
        // AZ::TickBus (drives the viewport preview each frame)
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSkinnedSpriteRequests (so the preview is drivable from script/agent while
        // authoring, mirroring the runtime component)
        void PlayAnimation(const AZStd::string& name, bool looping) override;
        void StopAnimation() override;
        void SetAnimationSpeed(float speed) override;
        void SetBoneRotation(const AZStd::string& boneName, float degrees) override;
        void SetBoneTranslation(const AZStd::string& boneName, const AZ::Vector2& offset) override;
        void ResetPose() override;
        SkinnedSpriteInfo GetSkinnedSpriteInfo() override;

    private:
        //! Rebuild the preview rig when a property changes.
        AZ::u32 OnConfigChanged();

        DioramaSkinnedSpriteConfig m_config;
        SkinnedSpritePresenter m_presenter;
    };
} // namespace Diorama
