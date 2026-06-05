/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/AsepriteBinary.h>

#include <AzCore/Compression/Compression.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/utils.h>

#include <cstring>

namespace Diorama::Aseprite
{
    namespace
    {
        constexpr AZ::u16 FileMagic = 0xA5E0;
        constexpr AZ::u16 FrameMagic = 0xF1FA;
        constexpr AZ::u32 ChunkLayer = 0x2004;
        constexpr AZ::u32 ChunkCel = 0x2005;
        constexpr AZ::u32 ChunkTags = 0x2018;
        constexpr int MaxDimension = 16384; //!< guard against absurd canvas/cel sizes from a bad file

        //! Bounds-checked, little-endian byte reader over an untrusted buffer. Any read
        //! past the end sets a sticky failure flag and returns 0, so the parser can read
        //! optimistically and check Ok() at safe points.
        class Reader
        {
        public:
            Reader(const AZ::u8* data, size_t size)
                : m_data(data)
                , m_size(size)
            {
            }

            bool Ok() const
            {
                return m_ok;
            }
            size_t Pos() const
            {
                return m_pos;
            }
            size_t Size() const
            {
                return m_size;
            }
            const AZ::u8* Ptr() const
            {
                return m_data + m_pos;
            }

            void Seek(size_t pos)
            {
                if (pos > m_size)
                {
                    m_ok = false;
                }
                else
                {
                    m_pos = pos;
                }
            }
            void Skip(size_t count)
            {
                if (m_pos + count > m_size)
                {
                    m_ok = false;
                    m_pos = m_size;
                }
                else
                {
                    m_pos += count;
                }
            }

            AZ::u8 U8()
            {
                if (m_pos + 1 > m_size)
                {
                    m_ok = false;
                    return 0;
                }
                return m_data[m_pos++];
            }
            AZ::u16 U16()
            {
                if (m_pos + 2 > m_size)
                {
                    m_ok = false;
                    m_pos = m_size;
                    return 0;
                }
                const AZ::u16 v = static_cast<AZ::u16>(m_data[m_pos]) | static_cast<AZ::u16>(m_data[m_pos + 1] << 8);
                m_pos += 2;
                return v;
            }
            AZ::s16 S16()
            {
                return static_cast<AZ::s16>(U16());
            }
            AZ::u32 U32()
            {
                if (m_pos + 4 > m_size)
                {
                    m_ok = false;
                    m_pos = m_size;
                    return 0;
                }
                const AZ::u32 v = static_cast<AZ::u32>(m_data[m_pos]) | (static_cast<AZ::u32>(m_data[m_pos + 1]) << 8) |
                    (static_cast<AZ::u32>(m_data[m_pos + 2]) << 16) | (static_cast<AZ::u32>(m_data[m_pos + 3]) << 24);
                m_pos += 4;
                return v;
            }

            bool ReadBytes(AZ::u8* dst, size_t count)
            {
                if (m_pos + count > m_size)
                {
                    m_ok = false;
                    return false;
                }
                ::memcpy(dst, m_data + m_pos, count);
                m_pos += count;
                return true;
            }

            //! Aseprite STRING: u16 length then that many UTF-8 bytes.
            AZStd::string ReadString()
            {
                const AZ::u16 length = U16();
                if (!m_ok || m_pos + length > m_size)
                {
                    m_ok = false;
                    return {};
                }
                AZStd::string out(reinterpret_cast<const char*>(m_data + m_pos), length);
                m_pos += length;
                return out;
            }

        private:
            const AZ::u8* m_data;
            size_t m_size;
            size_t m_pos = 0;
            bool m_ok = true;
        };

        //! Inflate exactly expectedBytes of ZLIB data into dst. Returns false if the
        //! stream does not produce exactly that many bytes.
        bool InflateExact(const AZ::u8* src, AZ::u32 srcLen, AZ::u8* dst, AZ::u32 expectedBytes)
        {
            AZ::ZLib zlib;
            zlib.StartDecompressor();
            AZ::u32 destSize = expectedBytes; // capacity in; updated to remaining capacity out
            zlib.Decompress(src, srcLen, dst, destSize, AZ::ZLib::FT_FINISH);
            zlib.StopDecompressor();
            // destSize is the LEFTOVER destination capacity, so all bytes were produced
            // exactly when it reaches 0.
            return destSize == 0;
        }

        //! Normal alpha-over of src (premultiplied by a) onto dst, both straight RGBA.
        void BlendPixelOver(AZ::u8* dst, const AZ::u8* src, float coverage)
        {
            const float srcA = (static_cast<float>(src[3]) / 255.0f) * coverage;
            if (srcA <= 0.0f)
            {
                return;
            }
            const float dstA = static_cast<float>(dst[3]) / 255.0f;
            const float outA = srcA + dstA * (1.0f - srcA);
            for (int i = 0; i < 3; ++i)
            {
                const float s = static_cast<float>(src[i]) / 255.0f;
                const float d = static_cast<float>(dst[i]) / 255.0f;
                const float o = outA > 0.0f ? (s * srcA + d * dstA * (1.0f - srcA)) / outA : 0.0f;
                dst[i] = static_cast<AZ::u8>(AZ::GetClamp(o, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
            dst[3] = static_cast<AZ::u8>(AZ::GetClamp(outA, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    } // namespace

    bool ParseAsepriteBinary(AZStd::span<const AZ::u8> bytes, BinarySprite& out)
    {
        out = BinarySprite();
        Reader reader(bytes.data(), bytes.size());

        reader.U32(); // file size (not needed; bounds come from the buffer)
        const AZ::u16 magic = reader.U16();
        const AZ::u16 frameCount = reader.U16();
        const AZ::u16 width = reader.U16();
        const AZ::u16 height = reader.U16();
        const AZ::u16 colorDepth = reader.U16();
        reader.Skip(114); // remainder of the 128-byte header
        if (!reader.Ok() || magic != FileMagic)
        {
            return false;
        }
        if (colorDepth != 32) // v1: RGBA only
        {
            return false;
        }
        if (width == 0 || height == 0 || width > MaxDimension || height > MaxDimension)
        {
            return false;
        }
        out.m_width = width;
        out.m_height = height;

        for (AZ::u16 f = 0; f < frameCount; ++f)
        {
            const size_t frameStart = reader.Pos();
            const AZ::u32 bytesInFrame = reader.U32();
            const AZ::u16 frameMagic = reader.U16();
            const AZ::u16 oldChunkCount = reader.U16();
            const AZ::u16 durationMs = reader.U16();
            reader.Skip(2);
            const AZ::u32 newChunkCount = reader.U32();
            if (!reader.Ok() || frameMagic != FrameMagic || bytesInFrame < 16)
            {
                return false;
            }
            const size_t frameEnd = frameStart + bytesInFrame;
            if (frameEnd > bytes.size())
            {
                return false;
            }
            const AZ::u32 chunkCount = newChunkCount != 0 ? newChunkCount : oldChunkCount;

            BinaryFrame frame;
            frame.m_durationSeconds = durationMs > 0 ? static_cast<float>(durationMs) / 1000.0f : 0.1f;

            for (AZ::u32 c = 0; c < chunkCount; ++c)
            {
                const size_t chunkStart = reader.Pos();
                if (chunkStart + 6 > frameEnd)
                {
                    return false;
                }
                const AZ::u32 chunkSize = reader.U32();
                const AZ::u32 chunkType = reader.U16();
                if (chunkSize < 6 || chunkStart + chunkSize > frameEnd)
                {
                    return false;
                }
                const size_t chunkEnd = chunkStart + chunkSize;

                if (chunkType == ChunkLayer)
                {
                    const AZ::u16 flags = reader.U16();
                    reader.U16(); // layer type
                    reader.U16(); // child level
                    reader.U16(); // default width (ignored)
                    reader.U16(); // default height (ignored)
                    const AZ::u16 blendMode = reader.U16();
                    const AZ::u8 opacity = reader.U8();
                    reader.Skip(3); // reserved
                    AZStd::string name = reader.ReadString();
                    if (!reader.Ok())
                    {
                        return false;
                    }
                    BinaryLayer layer;
                    layer.m_name = AZStd::move(name);
                    layer.m_visible = (flags & 0x1) != 0;
                    layer.m_opacity = opacity;
                    layer.m_blendMode = static_cast<int>(blendMode);
                    out.m_layers.push_back(AZStd::move(layer));
                }
                else if (chunkType == ChunkCel)
                {
                    const AZ::u16 layerIndex = reader.U16();
                    const AZ::s16 x = reader.S16();
                    const AZ::s16 y = reader.S16();
                    const AZ::u8 opacity = reader.U8();
                    const AZ::u16 celType = reader.U16();
                    reader.Skip(7); // z-index + reserved
                    if (!reader.Ok())
                    {
                        return false;
                    }

                    BinaryCel cel;
                    cel.m_layerIndex = layerIndex;
                    cel.m_x = x;
                    cel.m_y = y;
                    cel.m_opacity = opacity;

                    if (celType == 0 || celType == 2) // raw or compressed image
                    {
                        const AZ::u16 cw = reader.U16();
                        const AZ::u16 ch = reader.U16();
                        if (!reader.Ok() || cw > MaxDimension || ch > MaxDimension)
                        {
                            return false;
                        }
                        const size_t expected = static_cast<size_t>(cw) * ch * 4;
                        cel.m_width = cw;
                        cel.m_height = ch;
                        cel.m_rgba.resize(expected);
                        if (expected > 0)
                        {
                            if (celType == 0)
                            {
                                if (!reader.ReadBytes(cel.m_rgba.data(), expected))
                                {
                                    return false;
                                }
                            }
                            else
                            {
                                const size_t compLen = chunkEnd - reader.Pos();
                                if (reader.Pos() + compLen > bytes.size() ||
                                    !InflateExact(
                                        reader.Ptr(), static_cast<AZ::u32>(compLen), cel.m_rgba.data(), static_cast<AZ::u32>(expected)))
                                {
                                    return false;
                                }
                                reader.Skip(compLen);
                            }
                        }
                        frame.m_cels.push_back(AZStd::move(cel));
                    }
                    else if (celType == 1) // linked: reuse an earlier frame's cel for this layer
                    {
                        const AZ::u16 linkedFrame = reader.U16();
                        if (!reader.Ok())
                        {
                            return false;
                        }
                        if (linkedFrame < out.m_frames.size())
                        {
                            for (const BinaryCel& source : out.m_frames[linkedFrame].m_cels)
                            {
                                if (source.m_layerIndex == layerIndex)
                                {
                                    BinaryCel linked = source;
                                    linked.m_opacity = opacity;
                                    frame.m_cels.push_back(AZStd::move(linked));
                                    break;
                                }
                            }
                        }
                    }
                    // celType 3 (tilemap) is not supported in v1; the chunk is skipped below.
                }
                else if (chunkType == ChunkTags)
                {
                    const AZ::u16 tagCount = reader.U16();
                    reader.Skip(8); // reserved
                    for (AZ::u16 t = 0; t < tagCount; ++t)
                    {
                        const AZ::u16 from = reader.U16();
                        const AZ::u16 to = reader.U16();
                        const AZ::u8 direction = reader.U8();
                        reader.Skip(12); // repeat + reserved + deprecated rgb + extra (fixed 17-byte prefix total)
                        AZStd::string name = reader.ReadString();
                        if (!reader.Ok())
                        {
                            return false;
                        }
                        TagData tag;
                        tag.m_name = AZStd::move(name);
                        tag.m_from = from;
                        tag.m_to = to;
                        tag.m_direction = direction == 1 ? Direction::Reverse : (direction == 2 ? Direction::PingPong : Direction::Forward);
                        out.m_tags.push_back(AZStd::move(tag));
                    }
                }

                reader.Seek(chunkEnd); // robustly skip any unread/unknown chunk bytes
                if (!reader.Ok())
                {
                    return false;
                }
            }

            out.m_frames.push_back(AZStd::move(frame));
            reader.Seek(frameEnd);
            if (!reader.Ok())
            {
                return false;
            }
        }

        return true;
    }

    FrameImage CompositeFrame(const BinarySprite& sprite, int frameIndex)
    {
        FrameImage image;
        image.m_width = sprite.m_width;
        image.m_height = sprite.m_height;
        image.m_rgba.resize(static_cast<size_t>(sprite.m_width) * sprite.m_height * 4, 0);

        if (frameIndex < 0 || frameIndex >= static_cast<int>(sprite.m_frames.size()))
        {
            return image;
        }
        const BinaryFrame& frame = sprite.m_frames[frameIndex];

        // Composite in layer order (back to front). A cel names its layer; draw it only
        // if that layer is visible. coverage = layer opacity * cel opacity.
        for (int layerIndex = 0; layerIndex < static_cast<int>(sprite.m_layers.size()); ++layerIndex)
        {
            const BinaryLayer& layer = sprite.m_layers[layerIndex];
            if (!layer.m_visible)
            {
                continue;
            }
            for (const BinaryCel& cel : frame.m_cels)
            {
                if (cel.m_layerIndex != layerIndex || cel.m_rgba.empty())
                {
                    continue;
                }
                const float coverage = (static_cast<float>(layer.m_opacity) / 255.0f) * (static_cast<float>(cel.m_opacity) / 255.0f);
                for (int cy = 0; cy < cel.m_height; ++cy)
                {
                    const int dy = cel.m_y + cy;
                    if (dy < 0 || dy >= sprite.m_height)
                    {
                        continue;
                    }
                    for (int cx = 0; cx < cel.m_width; ++cx)
                    {
                        const int dx = cel.m_x + cx;
                        if (dx < 0 || dx >= sprite.m_width)
                        {
                            continue;
                        }
                        const AZ::u8* src = &cel.m_rgba[(static_cast<size_t>(cy) * cel.m_width + cx) * 4];
                        AZ::u8* dst = &image.m_rgba[(static_cast<size_t>(dy) * sprite.m_width + dx) * 4];
                        BlendPixelOver(dst, src, coverage);
                    }
                }
            }
        }
        return image;
    }

    PackedAtlas PackFramesToGrid(const BinarySprite& sprite, int columns, const AZStd::string& imageName)
    {
        PackedAtlas atlas;
        atlas.m_document.m_imageName = imageName;

        const int frameCount = static_cast<int>(sprite.m_frames.size());
        const int cellW = sprite.m_width;
        const int cellH = sprite.m_height;
        if (frameCount == 0 || cellW <= 0 || cellH <= 0)
        {
            return atlas;
        }

        const int cols = AZ::GetMax(columns, 1);
        const int rows = (frameCount + cols - 1) / cols;
        atlas.m_width = cols * cellW;
        atlas.m_height = rows * cellH;
        atlas.m_rgba.resize(static_cast<size_t>(atlas.m_width) * atlas.m_height * 4, 0);

        atlas.m_document.m_atlasWidth = atlas.m_width;
        atlas.m_document.m_atlasHeight = atlas.m_height;
        atlas.m_document.m_frames.reserve(static_cast<size_t>(frameCount));

        for (int i = 0; i < frameCount; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;
            const int originX = col * cellW;
            const int originY = row * cellH;

            const FrameImage frame = CompositeFrame(sprite, i);
            for (int y = 0; y < cellH; ++y)
            {
                const AZ::u8* srcRow = &frame.m_rgba[static_cast<size_t>(y) * cellW * 4];
                AZ::u8* dstRow = &atlas.m_rgba[(static_cast<size_t>(originY + y) * atlas.m_width + originX) * 4];
                ::memcpy(dstRow, srcRow, static_cast<size_t>(cellW) * 4);
            }

            FrameData rect;
            rect.m_x = originX;
            rect.m_y = originY;
            rect.m_w = cellW;
            rect.m_h = cellH;
            rect.m_durationSeconds = sprite.m_frames[i].m_durationSeconds;
            atlas.m_document.m_frames.push_back(rect);
        }

        atlas.m_document.m_tags = sprite.m_tags;
        return atlas;
    }
} // namespace Diorama::Aseprite
