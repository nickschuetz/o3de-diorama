/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/MathUtils.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <cmath>
#include <cstddef>

// Core for importing Aseprite's "Export Sprite Sheet" JSON (the documented PNG +
// JSON output that the whole ecosystem consumes). The data model and the playback
// timeline (per-frame durations, tag directions) are pure and unit tested here; the
// only engine-touching part is ParseDocument, which reads the JSON text with AzCore's
// rapidjson (split into AsepriteImport.cpp so this stays cheap to include).
//
// v1 scope: the standard non-trimmed grid/packed export. Native .aseprite binary
// parsing (layers, cels, blend modes) is a documented v2 follow-up that will reuse
// this same Document model and timeline; only the reader changes.
namespace Diorama::Aseprite
{
    //! Playback direction of a tag, from Aseprite's "direction" field.
    enum class Direction : AZ::u8
    {
        Forward,
        Reverse,
        PingPong
    };

    //! One frame: its pixel rect in the atlas and how long it shows.
    struct FrameData
    {
        int m_x = 0;
        int m_y = 0;
        int m_w = 0;
        int m_h = 0;
        float m_durationSeconds = 0.1f;
    };

    //! A named animation: an inclusive frame range played in a direction.
    struct TagData
    {
        AZStd::string m_name;
        int m_from = 0;
        int m_to = 0;
        Direction m_direction = Direction::Forward;
    };

    //! The parsed sprite sheet: the atlas image name + size, the frames in order, and
    //! the named tags. Frames are indexed 0..N-1 in document order.
    struct Document
    {
        AZStd::string m_imageName;
        int m_atlasWidth = 0;
        int m_atlasHeight = 0;
        AZStd::vector<FrameData> m_frames;
        AZStd::vector<TagData> m_tags;
    };

    //! Map a direction string ("forward"/"reverse"/"pingpong", case-insensitive) to
    //! the enum; anything else is Forward.
    Direction ParseDirection(AZStd::string_view text);

    //! Parse the Aseprite "Export Sprite Sheet" JSON (both the "Hash" and "Array"
    //! frame forms). Returns false on malformed JSON; out is left cleared. Durations
    //! are converted from milliseconds to seconds.
    bool ParseDocument(AZStd::string_view json, Document& out);

    //! UV rect for a frame against the atlas size. Degenerate atlas -> 0..1.
    inline void FrameUV(const FrameData& frame, int atlasWidth, int atlasHeight, float& uMin, float& vMin, float& uMax, float& vMax)
    {
        if (atlasWidth <= 0 || atlasHeight <= 0)
        {
            uMin = 0.0f;
            vMin = 0.0f;
            uMax = 1.0f;
            vMax = 1.0f;
            return;
        }
        const float w = static_cast<float>(atlasWidth);
        const float h = static_cast<float>(atlasHeight);
        uMin = static_cast<float>(frame.m_x) / w;
        vMin = static_cast<float>(frame.m_y) / h;
        uMax = static_cast<float>(frame.m_x + frame.m_w) / w;
        vMax = static_cast<float>(frame.m_y + frame.m_h) / h;
    }

    //! Find a tag by name (exact match); nullptr if absent.
    inline const TagData* FindTag(const Document& doc, AZStd::string_view name)
    {
        for (const TagData& tag : doc.m_tags)
        {
            if (tag.m_name == name)
            {
                return &tag;
            }
        }
        return nullptr;
    }

    //! Build the one-cycle playlist of frame indices for a tag, honoring direction.
    //! Forward: from..to. Reverse: to..from. PingPong: from..to then to-1..from+1
    //! (endpoints not duplicated). Indices are clamped to the document's frame range.
    inline AZStd::vector<int> BuildPlaylist(const Document& doc, const TagData& tag)
    {
        AZStd::vector<int> playlist;
        const int count = static_cast<int>(doc.m_frames.size());
        if (count == 0)
        {
            return playlist;
        }
        const int from = AZ::GetClamp(AZ::GetMin(tag.m_from, tag.m_to), 0, count - 1);
        const int to = AZ::GetClamp(AZ::GetMax(tag.m_from, tag.m_to), 0, count - 1);

        if (tag.m_direction == Direction::Reverse)
        {
            for (int i = to; i >= from; --i)
            {
                playlist.push_back(i);
            }
        }
        else
        {
            for (int i = from; i <= to; ++i)
            {
                playlist.push_back(i);
            }
            if (tag.m_direction == Direction::PingPong)
            {
                for (int i = to - 1; i > from; --i)
                {
                    playlist.push_back(i);
                }
            }
        }
        return playlist;
    }

    //! The frame index to show for a tag at elapsedSeconds, honoring each frame's own
    //! duration and the tag direction. Loops when loop is true; otherwise holds the
    //! last playlist frame after the cycle ends. Empty/zero-length tag -> first frame.
    inline int FrameAtTime(const Document& doc, const TagData& tag, float elapsedSeconds, bool loop)
    {
        const AZStd::vector<int> playlist = BuildPlaylist(doc, tag);
        if (playlist.empty())
        {
            return 0;
        }

        float cycle = 0.0f;
        for (const int index : playlist)
        {
            cycle += AZ::GetMax(doc.m_frames[index].m_durationSeconds, 0.0f);
        }
        if (cycle <= 0.0f)
        {
            return playlist.front();
        }

        float t = elapsedSeconds;
        if (loop)
        {
            t = std::fmod(t, cycle);
            if (t < 0.0f)
            {
                t += cycle;
            }
        }
        else if (t >= cycle)
        {
            return playlist.back();
        }
        else if (t < 0.0f)
        {
            return playlist.front();
        }

        float accum = 0.0f;
        for (const int index : playlist)
        {
            accum += AZ::GetMax(doc.m_frames[index].m_durationSeconds, 0.0f);
            if (t < accum)
            {
                return index;
            }
        }
        return playlist.back();
    }
} // namespace Diorama::Aseprite
