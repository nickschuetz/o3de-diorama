/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorDioramaSkeletalClipComponent.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void EditorDioramaSkeletalClipComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorDioramaSkeletalClipComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(1)
                ->Field("Config", &EditorDioramaSkeletalClipComponent::m_config)
                ->Field("PreviewEnabled", &EditorDioramaSkeletalClipComponent::m_previewEnabled)
                ->Field("PreviewTime", &EditorDioramaSkeletalClipComponent::m_previewNormalizedTime);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<EditorDioramaSkeletalClipComponent>(
                        "Skeletal Clip", "Keyframed cutout animation over a bone hierarchy (drivable via DioramaSkeletalRequestBus)")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &EditorDioramaSkeletalClipComponent::m_config, "Config", "Clip configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorDioramaSkeletalClipComponent::OnConfigChanged)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Editor preview")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &EditorDioramaSkeletalClipComponent::m_previewEnabled,
                        "Preview pose",
                        "Pose the bones in the viewport without entering game mode")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorDioramaSkeletalClipComponent::OnConfigChanged)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &EditorDioramaSkeletalClipComponent::m_previewNormalizedTime,
                        "Preview time",
                        "Scrub 0..1 through the clip")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorDioramaSkeletalClipComponent::OnConfigChanged);
            }
        }
    }

    void EditorDioramaSkeletalClipComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        ApplyPreview();
    }

    void EditorDioramaSkeletalClipComponent::Deactivate()
    {
        RemovePreview();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    AZ::u32 EditorDioramaSkeletalClipComponent::OnConfigChanged()
    {
        if (m_previewEnabled)
        {
            ApplyPreview();
        }
        else
        {
            RemovePreview();
        }
        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
    }

    void EditorDioramaSkeletalClipComponent::ApplyPreview()
    {
        if (!m_previewEnabled)
        {
            return;
        }

        // Capture the rest transforms once, on the first preview, so turning preview
        // off restores exactly what the saved prefab holds.
        if (!m_previewApplied)
        {
            m_previewBones = ResolveSkeletalBones(GetEntityId(), m_config);
            m_restTransforms.clear();
            m_restTransforms.reserve(m_previewBones.size());
            for (const AZ::EntityId& bone : m_previewBones)
            {
                AZ::Transform local = AZ::Transform::CreateIdentity();
                if (bone.IsValid())
                {
                    AZ::TransformBus::EventResult(local, bone, &AZ::TransformBus::Events::GetLocalTM);
                }
                m_restTransforms.push_back(local);
            }
            m_previewApplied = true;
        }

        const float seconds =
            AZ::GetClamp(m_previewNormalizedTime, 0.0f, 1.0f) * AZ::GetMax(m_config.m_duration, AZ::Constants::FloatEpsilon);
        ApplySkeletalPose(m_config, m_previewBones, seconds);
    }

    void EditorDioramaSkeletalClipComponent::RemovePreview()
    {
        if (!m_previewApplied)
        {
            return;
        }
        const size_t count = AZ::GetMin(m_previewBones.size(), m_restTransforms.size());
        for (size_t i = 0; i < count; ++i)
        {
            if (m_previewBones[i].IsValid())
            {
                AZ::TransformBus::Event(m_previewBones[i], &AZ::TransformBus::Events::SetLocalTM, m_restTransforms[i]);
            }
        }
        m_previewBones.clear();
        m_restTransforms.clear();
        m_previewApplied = false;
    }

    void EditorDioramaSkeletalClipComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<DioramaSkeletalClipComponent>(m_config);
    }
} // namespace Diorama
