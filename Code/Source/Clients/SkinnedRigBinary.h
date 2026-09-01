/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DragonBonesImport.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <cstring>

// Compact binary encode/decode for a fully-imported DragonBones armature document
// (the ".dskinrigc" product asset). The DragonBones importer (DragonBonesImport.h)
// parses the source "*_ske.json" + "*_tex.json" and applies the atlas UV remap; the
// AssetBuilder then Encode()s the resulting Document into this format, and the runtime
// AssetHandler Decode()s it back with NO JSON parsing at load time (the VISION
// efficiency goal: "product assets load without runtime parsing").
//
// The format is deliberately dumb and explicit:
//  - little-endian throughout (memcpy of the float bit pattern), so a product baked on
//    one host loads identically on Windows / Linux / mobile (all LE in practice); no
//    reflection tags, so it stays compact.
//  - Decode treats the product as UNTRUSTED input (asset-sourced data per the security
//    priority): every read is bounds-checked against the remaining bytes, every
//    length/count is capped and validated against a per-element minimum size before it
//    can drive an allocation, and any short/oversized read fails the whole decode
//    (out is left cleared) rather than reading past the buffer.
//
// Pure (no engine, no file IO): Encode/Decode operate on in-memory Document and byte
// buffers, so the round-trip is unit tested headlessly. The engine-touching glue (the
// AssetData subclass, its handler, and the builder) lives elsewhere.
namespace Diorama::SkinnedRig
{
    //! Product magic ("DSKR") and format version. Bump the version on any wire change;
    //! Decode rejects an unknown magic or a version it does not understand.
    inline constexpr AZ::u32 kMagic = 0x524B5344u; // 'D','S','K','R' as little-endian bytes 44 53 4B 52
    inline constexpr AZ::u32 kVersion = 1u;

    //! Hard cap on any single length/count read from the product, so a corrupt or hostile
    //! file cannot request a multi-gigabyte allocation. Real rigs are far below this.
    inline constexpr AZ::u32 kMaxCount = 1u << 24;

    //! Appends little-endian primitives to a growing byte buffer. No bounds concerns on
    //! the write side (the buffer grows), so this stays trivial.
    class Writer
    {
    public:
        explicit Writer(AZStd::vector<AZ::u8>& out)
            : m_out(out)
        {
        }

        void U8(AZ::u8 v)
        {
            m_out.push_back(v);
        }
        void U16(AZ::u16 v)
        {
            m_out.push_back(static_cast<AZ::u8>(v & 0xFF));
            m_out.push_back(static_cast<AZ::u8>((v >> 8) & 0xFF));
        }
        void U32(AZ::u32 v)
        {
            m_out.push_back(static_cast<AZ::u8>(v & 0xFF));
            m_out.push_back(static_cast<AZ::u8>((v >> 8) & 0xFF));
            m_out.push_back(static_cast<AZ::u8>((v >> 16) & 0xFF));
            m_out.push_back(static_cast<AZ::u8>((v >> 24) & 0xFF));
        }
        void I32(AZ::s32 v)
        {
            U32(static_cast<AZ::u32>(v));
        }
        void F32(float v)
        {
            AZ::u32 bits = 0;
            static_assert(sizeof(bits) == sizeof(v), "float is not 32-bit");
            std::memcpy(&bits, &v, sizeof(bits));
            U32(bits);
        }
        void Str(const AZStd::string& s)
        {
            U32(static_cast<AZ::u32>(s.size()));
            m_out.insert(m_out.end(), s.begin(), s.end());
        }
        void Vec2(const AZ::Vector2& v)
        {
            F32(v.GetX());
            F32(v.GetY());
        }

    private:
        AZStd::vector<AZ::u8>& m_out;
    };

    //! Bounds-checked little-endian reader over the product bytes. Every accessor first
    //! checks that enough bytes remain; a failure latches m_ok false and every later read
    //! returns a zero/empty value, so a malformed file drops out early instead of
    //! over-reading. Counts are validated against the remaining byte budget so a bogus
    //! length cannot drive an out-of-bounds loop or an oversized reserve.
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
        bool AtEnd() const
        {
            return m_pos == m_size;
        }
        //! Latch a decode failure explicitly (e.g. a value out of its valid range). Every
        //! later read then returns zero/empty and the whole Decode fails.
        void Fail()
        {
            m_ok = false;
        }

        AZ::u8 U8()
        {
            if (!Take(1))
            {
                return 0;
            }
            return m_data[m_pos - 1];
        }
        AZ::u16 U16()
        {
            if (!Take(2))
            {
                return 0;
            }
            const size_t i = m_pos - 2;
            return static_cast<AZ::u16>(m_data[i]) | static_cast<AZ::u16>(m_data[i + 1] << 8);
        }
        AZ::u32 U32()
        {
            if (!Take(4))
            {
                return 0;
            }
            const size_t i = m_pos - 4;
            return static_cast<AZ::u32>(m_data[i]) | (static_cast<AZ::u32>(m_data[i + 1]) << 8) |
                (static_cast<AZ::u32>(m_data[i + 2]) << 16) | (static_cast<AZ::u32>(m_data[i + 3]) << 24);
        }
        AZ::s32 I32()
        {
            return static_cast<AZ::s32>(U32());
        }
        float F32()
        {
            const AZ::u32 bits = U32();
            float v = 0.0f;
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }
        AZStd::string Str()
        {
            const AZ::u32 n = U32();
            if (!m_ok || n > kMaxCount || !Have(n))
            {
                m_ok = false;
                return {};
            }
            AZStd::string s(reinterpret_cast<const char*>(m_data + m_pos), n);
            m_pos += n;
            return s;
        }
        AZ::Vector2 Vec2()
        {
            const float x = F32();
            const float y = F32();
            return AZ::Vector2(x, y);
        }

        //! Read a vector length and validate it: capped at kMaxCount, and no larger than
        //! the remaining bytes can supply given each element's minimum size, so the caller
        //! can safely reserve() and loop exactly n times. Returns 0 (and latches failure)
        //! on a bad count.
        AZ::u32 Count(size_t minElementBytes)
        {
            const AZ::u32 n = U32();
            if (!m_ok || n > kMaxCount)
            {
                m_ok = false;
                return 0;
            }
            const size_t minBytes = minElementBytes == 0 ? 1 : minElementBytes;
            if (static_cast<size_t>(n) > (m_size - m_pos) / minBytes)
            {
                m_ok = false;
                return 0;
            }
            return n;
        }

    private:
        bool Have(size_t n) const
        {
            return m_pos + n <= m_size;
        }
        bool Take(size_t n)
        {
            if (!m_ok || !Have(n))
            {
                m_ok = false;
                return false;
            }
            m_pos += n;
            return true;
        }

        const AZ::u8* m_data = nullptr;
        size_t m_size = 0;
        size_t m_pos = 0;
        bool m_ok = true;
    };

    // ---- element writers -------------------------------------------------------------

    inline void WriteAffine(Writer& w, const MeshSkin::Affine2D& a)
    {
        w.F32(a.m_a);
        w.F32(a.m_b);
        w.F32(a.m_c);
        w.F32(a.m_d);
        w.F32(a.m_tx);
        w.F32(a.m_ty);
    }

    inline void WriteVertex(Writer& w, const MeshSkin::SkinnedVertex& v)
    {
        w.Vec2(v.m_bindPos);
        w.Vec2(v.m_uv);
        const int count = v.m_influenceCount < 0
            ? 0
            : (v.m_influenceCount > static_cast<int>(MeshSkin::MaxInfluences) ? static_cast<int>(MeshSkin::MaxInfluences)
                                                                              : v.m_influenceCount);
        w.I32(count);
        for (int i = 0; i < count; ++i)
        {
            w.I32(v.m_influences[i].m_boneIndex);
            w.F32(v.m_influences[i].m_weight);
        }
    }

    inline void WriteCurve(Writer& w, const float curve[4])
    {
        w.F32(curve[0]);
        w.F32(curve[1]);
        w.F32(curve[2]);
        w.F32(curve[3]);
    }

    inline void WriteKeyframe(Writer& w, const DragonBones::Keyframe& k)
    {
        w.F32(k.m_startTime);
        w.F32(k.m_duration);
        w.Vec2(k.m_value);
        w.U8(static_cast<AZ::u8>(k.m_tween));
        WriteCurve(w, k.m_curve);
    }

    template<typename T, typename ElemWriter>
    inline void WriteVector(Writer& w, const AZStd::vector<T>& v, ElemWriter elem)
    {
        w.U32(static_cast<AZ::u32>(v.size()));
        for (const T& e : v)
        {
            elem(w, e);
        }
    }

    inline void WriteKeyframes(Writer& w, const AZStd::vector<DragonBones::Keyframe>& v)
    {
        WriteVector(
            w,
            v,
            [](Writer& ww, const DragonBones::Keyframe& k)
            {
                WriteKeyframe(ww, k);
            });
    }

    inline void WriteBone(Writer& w, const DragonBones::BoneData& b)
    {
        w.Str(b.m_name);
        w.I32(b.m_parentIndex);
        WriteAffine(w, b.m_bindLocal);
        w.F32(b.m_x);
        w.F32(b.m_y);
        w.F32(b.m_skewXDegrees);
        w.F32(b.m_skewYDegrees);
        w.F32(b.m_scaleX);
        w.F32(b.m_scaleY);
        w.U8(b.m_isSurface ? 1 : 0);
        w.I32(b.m_segmentX);
        w.I32(b.m_segmentY);
        WriteVector(
            w,
            b.m_bindControlPoints,
            [](Writer& ww, const AZ::Vector2& p)
            {
                ww.Vec2(p);
            });
    }

    inline void WriteSkinnedMesh(Writer& w, const DragonBones::SkinnedMesh& m)
    {
        w.Str(m.m_slotName);
        w.Str(m.m_displayName);
        w.I32(m.m_drawOrder);
        WriteVector(
            w,
            m.m_vertices,
            [](Writer& ww, const MeshSkin::SkinnedVertex& v)
            {
                WriteVertex(ww, v);
            });
        WriteVector(
            w,
            m.m_indices,
            [](Writer& ww, const AZ::u16& i)
            {
                ww.U16(i);
            });
        WriteVector(
            w,
            m.m_boneGlobalIndices,
            [](Writer& ww, const int& i)
            {
                ww.I32(i);
            });
        WriteVector(
            w,
            m.m_bindWorld,
            [](Writer& ww, const MeshSkin::Affine2D& a)
            {
                WriteAffine(ww, a);
            });
    }

    inline void WriteSurfaceMesh(Writer& w, const DragonBones::SurfaceMesh& m)
    {
        w.Str(m.m_slotName);
        w.Str(m.m_displayName);
        w.I32(m.m_drawOrder);
        w.I32(m.m_surfaceBoneIndex);
        WriteVector(
            w,
            m.m_bindVertices,
            [](Writer& ww, const AZ::Vector2& p)
            {
                ww.Vec2(p);
            });
        WriteVector(
            w,
            m.m_uvs,
            [](Writer& ww, const AZ::Vector2& p)
            {
                ww.Vec2(p);
            });
        WriteVector(
            w,
            m.m_indices,
            [](Writer& ww, const AZ::u16& i)
            {
                ww.U16(i);
            });
    }

    inline void WriteBoneTimeline(Writer& w, const DragonBones::BoneTimeline& t)
    {
        w.Str(t.m_boneName);
        w.I32(t.m_boneIndex);
        WriteKeyframes(w, t.m_translate);
        WriteKeyframes(w, t.m_rotate);
        WriteKeyframes(w, t.m_scale);
    }

    inline void WriteDeformFrame(Writer& w, const DragonBones::DeformFrame& f)
    {
        w.F32(f.m_startTime);
        w.F32(f.m_duration);
        w.U8(static_cast<AZ::u8>(f.m_tween));
        WriteCurve(w, f.m_curve);
        w.I32(f.m_offset);
        WriteVector(
            w,
            f.m_rawDeltas,
            [](Writer& ww, const float& d)
            {
                ww.F32(d);
            });
    }

    inline void WriteDeformTimeline(Writer& w, const DragonBones::DeformTimeline& t)
    {
        w.Str(t.m_targetName);
        w.U8(static_cast<AZ::u8>(t.m_kind));
        w.I32(t.m_targetIndex);
        WriteVector(
            w,
            t.m_frames,
            [](Writer& ww, const DragonBones::DeformFrame& f)
            {
                WriteDeformFrame(ww, f);
            });
    }

    inline void WriteProgressTimeline(Writer& w, const DragonBones::ProgressTimeline& t)
    {
        w.Str(t.m_targetName);
        w.I32(t.m_targetIndex);
        w.F32(t.m_positionX);
        WriteKeyframes(w, t.m_values);
    }

    inline void WriteNamedValueTimeline(
        Writer& w, const AZStd::string& name, int targetIndex, const AZStd::vector<DragonBones::Keyframe>& values)
    {
        w.Str(name);
        w.I32(targetIndex);
        WriteKeyframes(w, values);
    }

    inline void WriteAnimation(Writer& w, const DragonBones::Animation& a)
    {
        w.Str(a.m_name);
        w.F32(a.m_durationSeconds);
        w.U8(a.m_loop ? 1 : 0);
        w.U8(static_cast<AZ::u8>(a.m_blendType));
        WriteVector(
            w,
            a.m_bones,
            [](Writer& ww, const DragonBones::BoneTimeline& t)
            {
                WriteBoneTimeline(ww, t);
            });
        WriteVector(
            w,
            a.m_deforms,
            [](Writer& ww, const DragonBones::DeformTimeline& t)
            {
                WriteDeformTimeline(ww, t);
            });
        WriteVector(
            w,
            a.m_progress,
            [](Writer& ww, const DragonBones::ProgressTimeline& t)
            {
                WriteProgressTimeline(ww, t);
            });
        WriteVector(
            w,
            a.m_weights,
            [](Writer& ww, const DragonBones::WeightTimeline& t)
            {
                WriteNamedValueTimeline(ww, t.m_targetName, t.m_targetIndex, t.m_values);
            });
        WriteVector(
            w,
            a.m_parameters,
            [](Writer& ww, const DragonBones::ParameterTimeline& t)
            {
                WriteNamedValueTimeline(ww, t.m_targetName, t.m_targetIndex, t.m_values);
            });
    }

    inline void WriteArmature(Writer& w, const DragonBones::Armature& a)
    {
        w.Str(a.m_name);
        w.F32(a.m_frameRate);
        WriteVector(
            w,
            a.m_bones,
            [](Writer& ww, const DragonBones::BoneData& b)
            {
                WriteBone(ww, b);
            });
        WriteVector(
            w,
            a.m_meshes,
            [](Writer& ww, const DragonBones::SkinnedMesh& m)
            {
                WriteSkinnedMesh(ww, m);
            });
        WriteVector(
            w,
            a.m_surfaceMeshes,
            [](Writer& ww, const DragonBones::SurfaceMesh& m)
            {
                WriteSurfaceMesh(ww, m);
            });
        WriteVector(
            w,
            a.m_animations,
            [](Writer& ww, const DragonBones::Animation& an)
            {
                WriteAnimation(ww, an);
            });
    }

    //! Encode a fully-imported document (post atlas-UV remap) into the product byte layout.
    inline void Encode(const DragonBones::Document& doc, AZStd::vector<AZ::u8>& out)
    {
        out.clear();
        Writer w(out);
        w.U32(kMagic);
        w.U32(kVersion);
        w.Str(doc.m_name);
        w.U8(doc.m_bonesInOrder ? 1 : 0);
        WriteVector(
            w,
            doc.m_armatures,
            [](Writer& ww, const DragonBones::Armature& a)
            {
                WriteArmature(ww, a);
            });
    }

    // ---- element readers -------------------------------------------------------------

    inline MeshSkin::Affine2D ReadAffine(Reader& r)
    {
        MeshSkin::Affine2D a;
        a.m_a = r.F32();
        a.m_b = r.F32();
        a.m_c = r.F32();
        a.m_d = r.F32();
        a.m_tx = r.F32();
        a.m_ty = r.F32();
        return a;
    }

    inline void ReadVertex(Reader& r, MeshSkin::SkinnedVertex& v)
    {
        v.m_bindPos = r.Vec2();
        v.m_uv = r.Vec2();
        const int count = r.I32();
        if (!r.Ok())
        {
            return;
        }
        // The writer always clamps the count into [0, MaxInfluences], so a value outside
        // that range is corruption: fail rather than desync the stream by reading a
        // wrong number of influence pairs.
        if (count < 0 || count > static_cast<int>(MeshSkin::MaxInfluences))
        {
            r.Fail();
            return;
        }
        v.m_influenceCount = count;
        for (int i = 0; i < count; ++i)
        {
            v.m_influences[i].m_boneIndex = r.I32();
            v.m_influences[i].m_weight = r.F32();
        }
    }

    inline void ReadCurve(Reader& r, float curve[4])
    {
        curve[0] = r.F32();
        curve[1] = r.F32();
        curve[2] = r.F32();
        curve[3] = r.F32();
    }

    inline void ReadKeyframe(Reader& r, DragonBones::Keyframe& k)
    {
        k.m_startTime = r.F32();
        k.m_duration = r.F32();
        k.m_value = r.Vec2();
        k.m_tween = static_cast<DragonBones::TweenType>(r.U8());
        ReadCurve(r, k.m_curve);
    }

    //! Read a length-prefixed vector. `minElementBytes` is the smallest number of bytes a
    //! single element can occupy (fixed fields, with strings/sub-vectors empty), used to
    //! bound the count so a corrupt length cannot drive an oversized reserve or loop.
    template<typename T, typename ElemReader>
    inline void ReadVector(Reader& r, size_t minElementBytes, AZStd::vector<T>& out, ElemReader elem)
    {
        out.clear();
        const AZ::u32 n = r.Count(minElementBytes);
        if (!r.Ok())
        {
            return;
        }
        out.reserve(n);
        for (AZ::u32 i = 0; i < n && r.Ok(); ++i)
        {
            T value{};
            elem(r, value);
            out.push_back(AZStd::move(value));
        }
    }

    inline void ReadKeyframes(Reader& r, AZStd::vector<DragonBones::Keyframe>& out)
    {
        // Keyframe fixed size: 2 f32 + Vec2 + u8 + 4 f32 = 8 + 8 + 1 + 16 = 33 bytes.
        ReadVector(
            r,
            33,
            out,
            [](Reader& rr, DragonBones::Keyframe& k)
            {
                ReadKeyframe(rr, k);
            });
    }

    inline void ReadBone(Reader& r, DragonBones::BoneData& b)
    {
        b.m_name = r.Str();
        b.m_parentIndex = r.I32();
        b.m_bindLocal = ReadAffine(r);
        b.m_x = r.F32();
        b.m_y = r.F32();
        b.m_skewXDegrees = r.F32();
        b.m_skewYDegrees = r.F32();
        b.m_scaleX = r.F32();
        b.m_scaleY = r.F32();
        b.m_isSurface = r.U8() != 0;
        b.m_segmentX = r.I32();
        b.m_segmentY = r.I32();
        ReadVector(
            r,
            8,
            b.m_bindControlPoints,
            [](Reader& rr, AZ::Vector2& p)
            {
                p = rr.Vec2();
            });
    }

    inline void ReadSkinnedMesh(Reader& r, DragonBones::SkinnedMesh& m)
    {
        m.m_slotName = r.Str();
        m.m_displayName = r.Str();
        m.m_drawOrder = r.I32();
        // SkinnedVertex fixed minimum: Vec2 + Vec2 + i32(count) = 8 + 8 + 4 = 20 bytes.
        ReadVector(
            r,
            20,
            m.m_vertices,
            [](Reader& rr, MeshSkin::SkinnedVertex& v)
            {
                ReadVertex(rr, v);
            });
        ReadVector(
            r,
            2,
            m.m_indices,
            [](Reader& rr, AZ::u16& i)
            {
                i = rr.U16();
            });
        ReadVector(
            r,
            4,
            m.m_boneGlobalIndices,
            [](Reader& rr, int& i)
            {
                i = rr.I32();
            });
        ReadVector(
            r,
            24,
            m.m_bindWorld,
            [](Reader& rr, MeshSkin::Affine2D& a)
            {
                a = ReadAffine(rr);
            });
    }

    inline void ReadSurfaceMesh(Reader& r, DragonBones::SurfaceMesh& m)
    {
        m.m_slotName = r.Str();
        m.m_displayName = r.Str();
        m.m_drawOrder = r.I32();
        m.m_surfaceBoneIndex = r.I32();
        ReadVector(
            r,
            8,
            m.m_bindVertices,
            [](Reader& rr, AZ::Vector2& p)
            {
                p = rr.Vec2();
            });
        ReadVector(
            r,
            8,
            m.m_uvs,
            [](Reader& rr, AZ::Vector2& p)
            {
                p = rr.Vec2();
            });
        ReadVector(
            r,
            2,
            m.m_indices,
            [](Reader& rr, AZ::u16& i)
            {
                i = rr.U16();
            });
    }

    inline void ReadBoneTimeline(Reader& r, DragonBones::BoneTimeline& t)
    {
        t.m_boneName = r.Str();
        t.m_boneIndex = r.I32();
        ReadKeyframes(r, t.m_translate);
        ReadKeyframes(r, t.m_rotate);
        ReadKeyframes(r, t.m_scale);
    }

    inline void ReadDeformFrame(Reader& r, DragonBones::DeformFrame& f)
    {
        f.m_startTime = r.F32();
        f.m_duration = r.F32();
        f.m_tween = static_cast<DragonBones::TweenType>(r.U8());
        ReadCurve(r, f.m_curve);
        f.m_offset = r.I32();
        ReadVector(
            r,
            4,
            f.m_rawDeltas,
            [](Reader& rr, float& d)
            {
                d = rr.F32();
            });
    }

    inline void ReadDeformTimeline(Reader& r, DragonBones::DeformTimeline& t)
    {
        t.m_targetName = r.Str();
        t.m_kind = static_cast<DragonBones::DeformTargetKind>(r.U8());
        t.m_targetIndex = r.I32();
        // DeformFrame fixed minimum: 2 f32 + u8 + 4 f32 + i32 + count = 8 + 1 + 16 + 4 + 4 = 33.
        ReadVector(
            r,
            33,
            t.m_frames,
            [](Reader& rr, DragonBones::DeformFrame& f)
            {
                ReadDeformFrame(rr, f);
            });
    }

    inline void ReadProgressTimeline(Reader& r, DragonBones::ProgressTimeline& t)
    {
        t.m_targetName = r.Str();
        t.m_targetIndex = r.I32();
        t.m_positionX = r.F32();
        ReadKeyframes(r, t.m_values);
    }

    inline void ReadWeightTimeline(Reader& r, DragonBones::WeightTimeline& t)
    {
        t.m_targetName = r.Str();
        t.m_targetIndex = r.I32();
        ReadKeyframes(r, t.m_values);
    }

    inline void ReadParameterTimeline(Reader& r, DragonBones::ParameterTimeline& t)
    {
        t.m_targetName = r.Str();
        t.m_targetIndex = r.I32();
        ReadKeyframes(r, t.m_values);
    }

    inline void ReadAnimation(Reader& r, DragonBones::Animation& a)
    {
        a.m_name = r.Str();
        a.m_durationSeconds = r.F32();
        a.m_loop = r.U8() != 0;
        a.m_blendType = static_cast<DragonBones::BlendType>(r.U8());
        // BoneTimeline fixed minimum: str(4) + i32 + 3 vector counts = 4 + 4 + 12 = 20.
        ReadVector(
            r,
            20,
            a.m_bones,
            [](Reader& rr, DragonBones::BoneTimeline& t)
            {
                ReadBoneTimeline(rr, t);
            });
        ReadVector(
            r,
            13,
            a.m_deforms,
            [](Reader& rr, DragonBones::DeformTimeline& t)
            {
                ReadDeformTimeline(rr, t);
            });
        ReadVector(
            r,
            12,
            a.m_progress,
            [](Reader& rr, DragonBones::ProgressTimeline& t)
            {
                ReadProgressTimeline(rr, t);
            });
        ReadVector(
            r,
            12,
            a.m_weights,
            [](Reader& rr, DragonBones::WeightTimeline& t)
            {
                ReadWeightTimeline(rr, t);
            });
        ReadVector(
            r,
            12,
            a.m_parameters,
            [](Reader& rr, DragonBones::ParameterTimeline& t)
            {
                ReadParameterTimeline(rr, t);
            });
    }

    inline void ReadArmature(Reader& r, DragonBones::Armature& a)
    {
        a.m_name = r.Str();
        a.m_frameRate = r.F32();
        // BoneData fixed minimum: str(4) + i32 + affine(24) + 6 f32(24) + u8 + 2 i32(8) + count(4) = 69.
        ReadVector(
            r,
            69,
            a.m_bones,
            [](Reader& rr, DragonBones::BoneData& b)
            {
                ReadBone(rr, b);
            });
        ReadVector(
            r,
            19,
            a.m_meshes,
            [](Reader& rr, DragonBones::SkinnedMesh& m)
            {
                ReadSkinnedMesh(rr, m);
            });
        ReadVector(
            r,
            24,
            a.m_surfaceMeshes,
            [](Reader& rr, DragonBones::SurfaceMesh& m)
            {
                ReadSurfaceMesh(rr, m);
            });
        ReadVector(
            r,
            15,
            a.m_animations,
            [](Reader& rr, DragonBones::Animation& an)
            {
                ReadAnimation(rr, an);
            });
    }

    //! Decode a product byte buffer back into a Document. Returns false (and clears out) on
    //! a bad magic/version or any short, oversized, or malformed read. On success the whole
    //! buffer is consumed; trailing bytes are treated as corruption and fail the decode.
    inline bool Decode(const AZ::u8* data, size_t size, DragonBones::Document& out)
    {
        out = DragonBones::Document{};
        Reader r(data, size);
        const AZ::u32 magic = r.U32();
        const AZ::u32 version = r.U32();
        if (!r.Ok() || magic != kMagic || version != kVersion)
        {
            out = DragonBones::Document{};
            return false;
        }
        out.m_name = r.Str();
        out.m_bonesInOrder = r.U8() != 0;
        ReadVector(
            r,
            12,
            out.m_armatures,
            [](Reader& rr, DragonBones::Armature& a)
            {
                ReadArmature(rr, a);
            });
        if (!r.Ok() || !r.AtEnd())
        {
            out = DragonBones::Document{};
            return false;
        }
        return true;
    }

    //! Convenience overload for a byte vector.
    inline bool Decode(const AZStd::vector<AZ::u8>& bytes, DragonBones::Document& out)
    {
        return Decode(bytes.data(), bytes.size(), out);
    }
} // namespace Diorama::SkinnedRig
