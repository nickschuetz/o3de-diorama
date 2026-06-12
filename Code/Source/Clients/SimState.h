/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

#include <cstring>

// Pure, header-only binary writer/reader for simulation state snapshots
// (Docs/design/2d-deterministic-sim.md phase B). Explicitly little-endian
// (shift-based encode/decode, no struct memcpy), so a snapshot taken on any
// supported platform reads back identically on any other.
//
// The READER TREATS THE BUFFER AS UNTRUSTED: every read is bounds-checked and
// returns false on truncation instead of reading past the end, and a failed
// reader stays failed (m_ok latches), so a restore path can run a whole parse
// and check one flag. Snapshots are an internal format, not a network protocol,
// but a corrupt or tampered buffer must fail cleanly, never crash.
namespace Diorama::SimState
{
    //! FNV-1a 64-bit over a byte range: the state-hash primitive (desync detection,
    //! CI determinism assertions, agent verify loops). Not cryptographic.
    inline AZ::u64 Fnv1a64(const AZ::u8* data, size_t size)
    {
        AZ::u64 h = 0xCBF29CE484222325ull;
        for (size_t i = 0; i < size; ++i)
        {
            h ^= data[i];
            h *= 0x100000001B3ull;
        }
        return h;
    }

    //! Append-only little-endian writer over a growable byte buffer.
    class Writer
    {
    public:
        explicit Writer(AZStd::vector<AZ::u8>& buffer)
            : m_buffer(buffer)
        {
        }

        void U8(AZ::u8 v)
        {
            m_buffer.push_back(v);
        }
        void U32(AZ::u32 v)
        {
            m_buffer.push_back(static_cast<AZ::u8>(v));
            m_buffer.push_back(static_cast<AZ::u8>(v >> 8));
            m_buffer.push_back(static_cast<AZ::u8>(v >> 16));
            m_buffer.push_back(static_cast<AZ::u8>(v >> 24));
        }
        void U64(AZ::u64 v)
        {
            U32(static_cast<AZ::u32>(v));
            U32(static_cast<AZ::u32>(v >> 32));
        }
        void S64(AZ::s64 v)
        {
            U64(static_cast<AZ::u64>(v));
        }
        void F32(float v)
        {
            AZ::u32 bits;
            std::memcpy(&bits, &v, sizeof(bits)); // bit pattern only; endianness handled by U32
            U32(bits);
        }
        void F64(double v)
        {
            AZ::u64 bits;
            std::memcpy(&bits, &v, sizeof(bits));
            U64(bits);
        }

        //! Begin a length-prefixed chunk: writes the tag and a size placeholder,
        //! returns the placeholder position to pass to EndChunk.
        size_t BeginChunk(AZ::u32 tag)
        {
            U32(tag);
            const size_t sizePos = m_buffer.size();
            U32(0);
            return sizePos;
        }

        //! Patch the chunk's size now that its payload is written.
        void EndChunk(size_t sizePos)
        {
            const AZ::u32 payload = static_cast<AZ::u32>(m_buffer.size() - sizePos - 4);
            m_buffer[sizePos + 0] = static_cast<AZ::u8>(payload);
            m_buffer[sizePos + 1] = static_cast<AZ::u8>(payload >> 8);
            m_buffer[sizePos + 2] = static_cast<AZ::u8>(payload >> 16);
            m_buffer[sizePos + 3] = static_cast<AZ::u8>(payload >> 24);
        }

    private:
        AZStd::vector<AZ::u8>& m_buffer;
    };

    //! Bounds-checked little-endian reader over a byte span. Every accessor returns
    //! false on truncation and latches m_ok false; Ok() reports whether every read
    //! so far succeeded.
    class Reader
    {
    public:
        Reader(const AZ::u8* data, size_t size)
            : m_data(data)
            , m_size(data != nullptr ? size : 0)
        {
        }

        bool Ok() const
        {
            return m_ok;
        }
        size_t Remaining() const
        {
            return m_size - m_pos;
        }

        bool U8(AZ::u8& out)
        {
            if (!Need(1))
            {
                return false;
            }
            out = m_data[m_pos++];
            return true;
        }
        bool U32(AZ::u32& out)
        {
            if (!Need(4))
            {
                return false;
            }
            out = static_cast<AZ::u32>(m_data[m_pos]) | (static_cast<AZ::u32>(m_data[m_pos + 1]) << 8) |
                (static_cast<AZ::u32>(m_data[m_pos + 2]) << 16) | (static_cast<AZ::u32>(m_data[m_pos + 3]) << 24);
            m_pos += 4;
            return true;
        }
        bool U64(AZ::u64& out)
        {
            AZ::u32 lo = 0;
            AZ::u32 hi = 0;
            if (!U32(lo) || !U32(hi))
            {
                return false;
            }
            out = static_cast<AZ::u64>(lo) | (static_cast<AZ::u64>(hi) << 32);
            return true;
        }
        bool S64(AZ::s64& out)
        {
            AZ::u64 raw = 0;
            if (!U64(raw))
            {
                return false;
            }
            out = static_cast<AZ::s64>(raw);
            return true;
        }
        bool F32(float& out)
        {
            AZ::u32 bits = 0;
            if (!U32(bits))
            {
                return false;
            }
            std::memcpy(&out, &bits, sizeof(out));
            return true;
        }
        bool F64(double& out)
        {
            AZ::u64 bits = 0;
            if (!U64(bits))
            {
                return false;
            }
            std::memcpy(&out, &bits, sizeof(out));
            return true;
        }

        //! Read a chunk header (tag + payload size). The payload size is validated
        //! against the remaining bytes, so a hostile size cannot run past the end.
        bool ChunkHeader(AZ::u32& tag, AZ::u32& payloadSize)
        {
            if (!U32(tag) || !U32(payloadSize))
            {
                return false;
            }
            if (payloadSize > Remaining())
            {
                m_ok = false;
                return false;
            }
            return true;
        }

        //! Skip `count` bytes (an unrecognized chunk's payload).
        bool Skip(size_t count)
        {
            if (!Need(count))
            {
                return false;
            }
            m_pos += count;
            return true;
        }

        //! A sub-reader over the next `count` bytes, advancing this reader past them
        //! (hand a chunk's payload to its owner without exposing the rest).
        Reader SubReader(size_t count)
        {
            if (!Need(count))
            {
                return Reader(nullptr, 0);
            }
            Reader sub(m_data + m_pos, count);
            m_pos += count;
            return sub;
        }

    private:
        bool Need(size_t count)
        {
            if (!m_ok || count > m_size - m_pos)
            {
                m_ok = false;
                return false;
            }
            return true;
        }

        const AZ::u8* m_data = nullptr;
        size_t m_size = 0;
        size_t m_pos = 0;
        bool m_ok = true;
    };
} // namespace Diorama::SimState
