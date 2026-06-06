/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAsepriteComponent.h>

#include <Diorama/SpriteBus.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    namespace
    {
        //! The tag currently selected, or a synthetic whole-sheet tag when none.
        Aseprite::TagData CurrentTag(const Aseprite::Document& doc, int tagIndex)
        {
            if (tagIndex >= 0 && tagIndex < static_cast<int>(doc.m_tags.size()))
            {
                return doc.m_tags[tagIndex];
            }
            Aseprite::TagData whole;
            whole.m_from = 0;
            whole.m_to = doc.m_frames.empty() ? 0 : static_cast<int>(doc.m_frames.size()) - 1;
            whole.m_direction = Aseprite::Direction::Forward;
            return whole;
        }

        //! Total seconds in one cycle of a tag's playlist (for end detection).
        float CycleSeconds(const Aseprite::Document& doc, const Aseprite::TagData& tag)
        {
            float total = 0.0f;
            for (const int index : Aseprite::BuildPlaylist(doc, tag))
            {
                total += AZ::GetMax(doc.m_frames[index].m_durationSeconds, 0.0f);
            }
            return total;
        }
    } // namespace

    void AsepriteFrameData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AsepriteFrameData>()
                ->Version(1)
                ->Field("x", &AsepriteFrameData::m_x)
                ->Field("y", &AsepriteFrameData::m_y)
                ->Field("w", &AsepriteFrameData::m_w)
                ->Field("h", &AsepriteFrameData::m_h)
                ->Field("duration", &AsepriteFrameData::m_durationSeconds);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<AsepriteFrameData>("Aseprite frame", "Atlas rect + duration for one frame")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteFrameData::m_x, "X", "Atlas X (pixels)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteFrameData::m_y, "Y", "Atlas Y (pixels)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteFrameData::m_w, "W", "Width (pixels)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteFrameData::m_h, "H", "Height (pixels)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &AsepriteFrameData::m_durationSeconds, "Duration", "Seconds this frame shows");
            }
        }
    }

    void AsepriteTagData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AsepriteTagData>()
                ->Version(1)
                ->Field("name", &AsepriteTagData::m_name)
                ->Field("from", &AsepriteTagData::m_from)
                ->Field("to", &AsepriteTagData::m_to)
                ->Field("direction", &AsepriteTagData::m_direction);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<AsepriteTagData>("Aseprite tag", "A named frame range")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteTagData::m_name, "Name", "Tag name to play")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteTagData::m_from, "From", "First frame index")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AsepriteTagData::m_to, "To", "Last frame index")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &AsepriteTagData::m_direction, "Direction", "Playback direction")
                    ->EnumAttribute(Aseprite::Direction::Forward, "Forward")
                    ->EnumAttribute(Aseprite::Direction::Reverse, "Reverse")
                    ->EnumAttribute(Aseprite::Direction::PingPong, "Ping-pong");
            }
        }
    }

    void DioramaAsepriteConfig::Reflect(AZ::ReflectContext* context)
    {
        AsepriteFrameData::Reflect(context);
        AsepriteTagData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaAsepriteConfig>()
                ->Version(2)
                ->Field("atlasTexturePath", &DioramaAsepriteConfig::m_atlasTexturePath)
                ->Field("atlasWidth", &DioramaAsepriteConfig::m_atlasWidth)
                ->Field("atlasHeight", &DioramaAsepriteConfig::m_atlasHeight)
                ->Field("frames", &DioramaAsepriteConfig::m_frames)
                ->Field("tags", &DioramaAsepriteConfig::m_tags)
                ->Field("defaultTag", &DioramaAsepriteConfig::m_defaultTag)
                ->Field("speed", &DioramaAsepriteConfig::m_speed)
                ->Field("looping", &DioramaAsepriteConfig::m_looping)
                ->Field("autoPlay", &DioramaAsepriteConfig::m_autoPlay)
                ->Field("sheet", &DioramaAsepriteConfig::m_sheet);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaAsepriteConfig>("Aseprite Config", "Imported sprite sheet + playback")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaAsepriteConfig::m_sheet,
                        "Sheet asset",
                        "A .dioramasheet from the native .aseprite import. Assign it to play directly; it "
                        "is loaded at runtime and takes precedence over the inline JSON-imported fields below.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaAsepriteConfig::m_atlasTexturePath,
                        "Atlas texture",
                        "Product path of the exported PNG (e.g. diorama/textures/hero.png)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaAsepriteConfig::m_atlasWidth, "Atlas width", "Filled by import")
                    ->Attribute(AZ::Edit::Attributes::ReadOnly, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaAsepriteConfig::m_atlasHeight, "Atlas height", "Filled by import")
                    ->Attribute(AZ::Edit::Attributes::ReadOnly, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaAsepriteConfig::m_frames, "Frames", "Filled by import")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaAsepriteConfig::m_tags, "Tags", "Named animations, filled by import")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaAsepriteConfig::m_defaultTag, "Default tag", "Tag to play on activate")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaAsepriteConfig::m_speed, "Speed", "Playback rate (negative reverses)")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &DioramaAsepriteConfig::m_looping, "Loop", "Wrap the default tag")
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &DioramaAsepriteConfig::m_autoPlay, "Auto play", "Play on activate");
            }
        }
    }

    Aseprite::Document BuildAsepriteDocument(const DioramaAsepriteConfig& config)
    {
        Aseprite::Document doc;
        doc.m_imageName = config.m_atlasTexturePath;
        doc.m_atlasWidth = config.m_atlasWidth;
        doc.m_atlasHeight = config.m_atlasHeight;
        doc.m_frames.reserve(config.m_frames.size());
        for (const AsepriteFrameData& frame : config.m_frames)
        {
            doc.m_frames.push_back({ frame.m_x, frame.m_y, frame.m_w, frame.m_h, frame.m_durationSeconds });
        }
        doc.m_tags.reserve(config.m_tags.size());
        for (const AsepriteTagData& tag : config.m_tags)
        {
            Aseprite::TagData out;
            out.m_name = tag.m_name;
            out.m_from = tag.m_from;
            out.m_to = tag.m_to;
            out.m_direction = tag.m_direction;
            doc.m_tags.push_back(AZStd::move(out));
        }
        return doc;
    }

    Aseprite::Document BuildAsepriteDocument(const DioramaAsepriteSheetAsset& sheet)
    {
        Aseprite::Document doc;
        doc.m_imageName = sheet.m_atlasTexturePath;
        doc.m_atlasWidth = sheet.m_atlasWidth;
        doc.m_atlasHeight = sheet.m_atlasHeight;
        doc.m_frames.reserve(sheet.m_frames.size());
        for (const AsepriteFrameData& frame : sheet.m_frames)
        {
            doc.m_frames.push_back({ frame.m_x, frame.m_y, frame.m_w, frame.m_h, frame.m_durationSeconds });
        }
        doc.m_tags.reserve(sheet.m_tags.size());
        for (const AsepriteTagData& tag : sheet.m_tags)
        {
            Aseprite::TagData out;
            out.m_name = tag.m_name;
            out.m_from = tag.m_from;
            out.m_to = tag.m_to;
            out.m_direction = tag.m_direction;
            doc.m_tags.push_back(AZStd::move(out));
        }
        return doc;
    }

    DioramaAsepriteComponent::DioramaAsepriteComponent(const DioramaAsepriteConfig& config)
        : m_config(config)
    {
    }

    void DioramaAsepriteComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaAsepriteConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaAsepriteComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaAsepriteComponent::m_config);
        }

        ReflectAsepriteBuses(context);
    }

    void DioramaAsepriteComponent::Activate()
    {
        DioramaAsepriteRequestBus::Handler::BusConnect(GetEntityId());

        if (m_config.m_sheet.GetId().IsValid())
        {
            // Asset-reference mode: load the .dioramasheet product and begin playback
            // from it when ready (OnAssetReady fires on connect if already loaded).
            m_config.m_sheet.QueueLoad();
            AZ::Data::AssetBus::Handler::BusConnect(m_config.m_sheet.GetId());
        }
        else
        {
            // Inline mode: frames/tags came from the editor JSON import.
            m_doc = BuildAsepriteDocument(m_config);
            BeginPlayback();
        }
    }

    void DioramaAsepriteComponent::BeginPlayback()
    {
        // Take over the sprite: point it at the atlas and stop its own frame-grid
        // animation so it does not fight our per-frame UVs.
        if (!m_doc.m_imageName.empty())
        {
            DioramaSpriteRequestBus::Event(GetEntityId(), &DioramaSpriteRequestBus::Events::SetTextureByPath, m_doc.m_imageName);
        }
        DioramaSpriteRequestBus::Event(GetEntityId(), &DioramaSpriteRequestBus::Events::SetAnimationEnabled, false);

        // Pick the default tag (by name, else the first tag, else the whole sheet).
        m_tagIndex = -1;
        if (!m_config.m_defaultTag.empty())
        {
            if (const Aseprite::TagData* tag = Aseprite::FindTag(m_doc, m_config.m_defaultTag))
            {
                m_tagIndex = static_cast<int>(tag - m_doc.m_tags.data());
            }
        }
        else if (!m_doc.m_tags.empty())
        {
            m_tagIndex = 0;
        }

        m_elapsed = 0.0f;
        ApplyFrame(Aseprite::FrameAtTime(m_doc, CurrentTag(m_doc, m_tagIndex), 0.0f, m_config.m_looping));
        m_playing = m_config.m_autoPlay;

        if (!m_ticking)
        {
            AZ::TickBus::Handler::BusConnect();
            m_ticking = true;
        }
    }

    void DioramaAsepriteComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        const auto* sheet = asset.GetAs<DioramaAsepriteSheetAsset>();
        if (sheet == nullptr)
        {
            return;
        }
        m_config.m_sheet = asset; // keep the loaded reference alive
        m_doc = BuildAsepriteDocument(*sheet);
        if (m_config.m_defaultTag.empty())
        {
            m_config.m_defaultTag = sheet->m_defaultTag;
        }
        BeginPlayback();
    }

    void DioramaAsepriteComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        OnAssetReady(asset);
    }

    void DioramaAsepriteComponent::Deactivate()
    {
        if (m_ticking)
        {
            AZ::TickBus::Handler::BusDisconnect();
            m_ticking = false;
        }
        AZ::Data::AssetBus::Handler::BusDisconnect();
        DioramaAsepriteRequestBus::Handler::BusDisconnect();
    }

    void DioramaAsepriteComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_playing)
        {
            return;
        }

        m_elapsed += deltaTime * m_config.m_speed;
        const Aseprite::TagData tag = CurrentTag(m_doc, m_tagIndex);

        if (!m_config.m_looping)
        {
            const float cycle = CycleSeconds(m_doc, tag);
            if (cycle > 0.0f && (m_elapsed >= cycle || m_elapsed < 0.0f))
            {
                m_elapsed = AZ::GetClamp(m_elapsed, 0.0f, cycle);
                m_playing = false;
            }
        }

        const int frame = Aseprite::FrameAtTime(m_doc, tag, m_elapsed, m_config.m_looping);
        if (frame != m_currentFrame)
        {
            ApplyFrame(frame);
        }
    }

    void DioramaAsepriteComponent::ApplyFrame(int frameIndex)
    {
        if (m_doc.m_frames.empty())
        {
            return;
        }
        m_currentFrame = AZ::GetClamp(frameIndex, 0, static_cast<int>(m_doc.m_frames.size()) - 1);

        float uMin, vMin, uMax, vMax;
        Aseprite::FrameUV(m_doc.m_frames[m_currentFrame], m_doc.m_atlasWidth, m_doc.m_atlasHeight, uMin, vMin, uMax, vMax);
        DioramaSpriteRequestBus::Event(GetEntityId(), &DioramaSpriteRequestBus::Events::SetUVRegion, uMin, vMin, uMax, vMax);
    }

    void DioramaAsepriteComponent::PlayTag(const AZStd::string& tagName)
    {
        const Aseprite::TagData* tag = Aseprite::FindTag(m_doc, tagName);
        if (tag == nullptr)
        {
            return;
        }
        m_tagIndex = static_cast<int>(tag - m_doc.m_tags.data());
        m_elapsed = 0.0f;
        m_playing = true;
        ApplyFrame(Aseprite::FrameAtTime(m_doc, *tag, 0.0f, m_config.m_looping));
    }

    void DioramaAsepriteComponent::Play()
    {
        m_playing = true;
    }

    void DioramaAsepriteComponent::Stop()
    {
        m_playing = false;
    }

    void DioramaAsepriteComponent::SetFrame(int frameIndex)
    {
        m_playing = false;
        ApplyFrame(frameIndex);
    }

    void DioramaAsepriteComponent::SetSpeed(float speed)
    {
        m_config.m_speed = speed;
    }

    void DioramaAsepriteComponent::SetLooping(bool looping)
    {
        m_config.m_looping = looping;
    }

    bool DioramaAsepriteComponent::IsPlaying()
    {
        return m_playing;
    }

    int DioramaAsepriteComponent::GetCurrentFrame()
    {
        return m_currentFrame;
    }
} // namespace Diorama
