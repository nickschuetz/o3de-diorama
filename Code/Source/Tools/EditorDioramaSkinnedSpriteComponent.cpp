/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorDioramaSkinnedSpriteComponent.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void EditorDioramaSkinnedSpriteComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorDioramaSkinnedSpriteComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(1)
                ->Field("Config", &EditorDioramaSkinnedSpriteComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<EditorDioramaSkinnedSpriteComponent>(
                        "Skinned Sprite (mesh deform)",
                        "A DragonBones weighted-mesh character, CPU-skinned by a posed bone hierarchy and drawn through the "
                        "sprite renderer (drivable via DioramaSkinnedSpriteRequestBus)")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &EditorDioramaSkinnedSpriteComponent::m_config,
                        "Config",
                        "Skinned sprite configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorDioramaSkinnedSpriteComponent::OnConfigChanged);
            }
        }
    }

    void EditorDioramaSkinnedSpriteComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        m_presenter.SetConfig(m_config);
        m_presenter.Connect(GetEntityId());
        DioramaSkinnedSpriteRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
    }

    void EditorDioramaSkinnedSpriteComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSkinnedSpriteRequestBus::Handler::BusDisconnect();
        m_presenter.Disconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void EditorDioramaSkinnedSpriteComponent::SetBoneRotation(const AZStd::string& boneName, float degrees)
    {
        m_presenter.SetBoneRotation(boneName, degrees);
    }

    void EditorDioramaSkinnedSpriteComponent::SetBoneTranslation(const AZStd::string& boneName, float x, float y)
    {
        m_presenter.SetBoneTranslation(boneName, x, y);
    }

    void EditorDioramaSkinnedSpriteComponent::ResetPose()
    {
        m_presenter.ResetPose();
    }

    SkinnedSpriteInfo EditorDioramaSkinnedSpriteComponent::GetSkinnedSpriteInfo()
    {
        return m_presenter.GetInfo();
    }

    void EditorDioramaSkinnedSpriteComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_presenter.Tick(deltaTime);
    }

    void EditorDioramaSkinnedSpriteComponent::PlayAnimation(const AZStd::string& name, bool looping)
    {
        m_presenter.PlayAnimation(name, looping);
    }

    void EditorDioramaSkinnedSpriteComponent::StopAnimation()
    {
        m_presenter.StopAnimation();
    }

    void EditorDioramaSkinnedSpriteComponent::SetAnimationSpeed(float speed)
    {
        m_presenter.SetAnimationSpeed(speed);
    }

    AZ::u32 EditorDioramaSkinnedSpriteComponent::OnConfigChanged()
    {
        // Rebuild the rig (source / scale / texture may have changed) and re-present.
        m_presenter.SetConfig(m_config);
        m_presenter.Connect(GetEntityId());
        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
    }

    void EditorDioramaSkinnedSpriteComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<DioramaSkinnedSpriteComponent>(m_config);
    }
} // namespace Diorama
