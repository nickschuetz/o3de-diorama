/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Tools/EditorDioramaAsepriteComponent.h>

#include <Clients/AsepriteImport.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/string/conversions.h>

namespace Diorama
{
    void EditorDioramaAsepriteComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorDioramaAsepriteComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(1)
                ->Field("JsonSourcePath", &EditorDioramaAsepriteComponent::m_jsonSourcePath)
                ->Field("ImportStatus", &EditorDioramaAsepriteComponent::m_importStatus)
                ->Field("Config", &EditorDioramaAsepriteComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<EditorDioramaAsepriteComponent>(
                        "Aseprite Animation",
                        "Play Aseprite tags on a sprite. Import an exported sprite-sheet JSON, then drive it via "
                        "DioramaAsepriteRequestBus.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Import")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &EditorDioramaAsepriteComponent::m_jsonSourcePath,
                        "Aseprite JSON",
                        "Path to an exported sprite-sheet .json; set it to (re)import frames + tags")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorDioramaAsepriteComponent::OnImport)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &EditorDioramaAsepriteComponent::m_importStatus, "Status", "Last import result")
                    ->Attribute(AZ::Edit::Attributes::ReadOnly, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &EditorDioramaAsepriteComponent::m_config, "Config", "Sprite sheet + playback")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    AZ::u32 EditorDioramaAsepriteComponent::OnImport()
    {
        if (m_jsonSourcePath.empty())
        {
            m_importStatus = "No JSON imported yet.";
            return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
        }

        const auto readResult = AZ::Utils::ReadFile<AZStd::string>(m_jsonSourcePath);
        if (!readResult.IsSuccess())
        {
            m_importStatus = AZStd::string::format("Could not read '%s'.", m_jsonSourcePath.c_str());
            return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
        }

        Aseprite::Document doc;
        if (!Aseprite::ParseDocument(readResult.GetValue(), doc))
        {
            m_importStatus = "JSON did not parse (not an Aseprite sprite-sheet export?).";
            return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
        }

        m_config.m_atlasWidth = doc.m_atlasWidth;
        m_config.m_atlasHeight = doc.m_atlasHeight;

        m_config.m_frames.clear();
        m_config.m_frames.reserve(doc.m_frames.size());
        for (const Aseprite::FrameData& frame : doc.m_frames)
        {
            m_config.m_frames.push_back({ frame.m_x, frame.m_y, frame.m_w, frame.m_h, frame.m_durationSeconds });
        }

        m_config.m_tags.clear();
        m_config.m_tags.reserve(doc.m_tags.size());
        for (const Aseprite::TagData& tag : doc.m_tags)
        {
            AsepriteTagData out;
            out.m_name = tag.m_name;
            out.m_from = tag.m_from;
            out.m_to = tag.m_to;
            out.m_direction = tag.m_direction;
            m_config.m_tags.push_back(AZStd::move(out));
        }

        if (m_config.m_defaultTag.empty() && !m_config.m_tags.empty())
        {
            m_config.m_defaultTag = m_config.m_tags.front().m_name;
        }

        m_importStatus = AZStd::string::format(
            "Imported %zu frame(s), %zu tag(s) from a %dx%d atlas (%s). Set the Atlas texture to the matching PNG.",
            m_config.m_frames.size(),
            m_config.m_tags.size(),
            m_config.m_atlasWidth,
            m_config.m_atlasHeight,
            doc.m_imageName.c_str());

        // The arrays changed wholesale; rebuild the whole property tree.
        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    void EditorDioramaAsepriteComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<DioramaAsepriteComponent>(m_config);
    }
} // namespace Diorama
