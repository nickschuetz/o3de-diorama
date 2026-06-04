/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/AsepriteImport.h>

#include <AzCore/JSON/document.h>
#include <AzCore/std/string/conversions.h>

namespace Diorama::Aseprite
{
    namespace
    {
        int GetInt(const rapidjson::Value& obj, const char* key, int fallback)
        {
            const auto it = obj.FindMember(key);
            if (it != obj.MemberEnd() && it->value.IsInt())
            {
                return it->value.GetInt();
            }
            return fallback;
        }

        float GetMs(const rapidjson::Value& obj, const char* key, float fallbackSeconds)
        {
            const auto it = obj.FindMember(key);
            if (it != obj.MemberEnd() && it->value.IsNumber())
            {
                return static_cast<float>(it->value.GetDouble()) / 1000.0f;
            }
            return fallbackSeconds;
        }

        //! Read one frame entry's "frame" rect + "duration" into a FrameData.
        FrameData ReadFrame(const rapidjson::Value& entry)
        {
            FrameData frame;
            const auto rectIt = entry.FindMember("frame");
            if (rectIt != entry.MemberEnd() && rectIt->value.IsObject())
            {
                const rapidjson::Value& rect = rectIt->value;
                frame.m_x = GetInt(rect, "x", 0);
                frame.m_y = GetInt(rect, "y", 0);
                frame.m_w = GetInt(rect, "w", 0);
                frame.m_h = GetInt(rect, "h", 0);
            }
            frame.m_durationSeconds = GetMs(entry, "duration", 0.1f);
            return frame;
        }
    } // namespace

    Direction ParseDirection(AZStd::string_view text)
    {
        AZStd::string lowered(text);
        AZStd::to_lower(lowered.begin(), lowered.end());
        if (lowered == "reverse")
        {
            return Direction::Reverse;
        }
        if (lowered == "pingpong")
        {
            return Direction::PingPong;
        }
        return Direction::Forward;
    }

    bool ParseDocument(AZStd::string_view json, Document& out)
    {
        out = Document();

        rapidjson::Document doc;
        doc.Parse(json.data(), json.size());
        if (doc.HasParseError() || !doc.IsObject())
        {
            return false;
        }

        // meta.image + meta.size + meta.frameTags
        const auto metaIt = doc.FindMember("meta");
        if (metaIt != doc.MemberEnd() && metaIt->value.IsObject())
        {
            const rapidjson::Value& meta = metaIt->value;
            const auto imageIt = meta.FindMember("image");
            if (imageIt != meta.MemberEnd() && imageIt->value.IsString())
            {
                out.m_imageName = imageIt->value.GetString();
            }
            const auto sizeIt = meta.FindMember("size");
            if (sizeIt != meta.MemberEnd() && sizeIt->value.IsObject())
            {
                out.m_atlasWidth = GetInt(sizeIt->value, "w", 0);
                out.m_atlasHeight = GetInt(sizeIt->value, "h", 0);
            }
            const auto tagsIt = meta.FindMember("frameTags");
            if (tagsIt != meta.MemberEnd() && tagsIt->value.IsArray())
            {
                for (const rapidjson::Value& tagValue : tagsIt->value.GetArray())
                {
                    if (!tagValue.IsObject())
                    {
                        continue;
                    }
                    TagData tag;
                    const auto nameIt = tagValue.FindMember("name");
                    if (nameIt != tagValue.MemberEnd() && nameIt->value.IsString())
                    {
                        tag.m_name = nameIt->value.GetString();
                    }
                    tag.m_from = GetInt(tagValue, "from", 0);
                    tag.m_to = GetInt(tagValue, "to", 0);
                    const auto dirIt = tagValue.FindMember("direction");
                    if (dirIt != tagValue.MemberEnd() && dirIt->value.IsString())
                    {
                        tag.m_direction = ParseDirection(dirIt->value.GetString());
                    }
                    out.m_tags.push_back(AZStd::move(tag));
                }
            }
        }

        // frames: either an array (Array form) or an object keyed by name (Hash form).
        // Both preserve Aseprite's frame order; for Hash we iterate member order.
        const auto framesIt = doc.FindMember("frames");
        if (framesIt != doc.MemberEnd())
        {
            if (framesIt->value.IsArray())
            {
                for (const rapidjson::Value& entry : framesIt->value.GetArray())
                {
                    if (entry.IsObject())
                    {
                        out.m_frames.push_back(ReadFrame(entry));
                    }
                }
            }
            else if (framesIt->value.IsObject())
            {
                for (auto member = framesIt->value.MemberBegin(); member != framesIt->value.MemberEnd(); ++member)
                {
                    if (member->value.IsObject())
                    {
                        out.m_frames.push_back(ReadFrame(member->value));
                    }
                }
            }
        }

        return true;
    }
} // namespace Diorama::Aseprite
