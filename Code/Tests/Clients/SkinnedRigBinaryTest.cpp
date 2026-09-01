/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/containers/vector.h>

#include <Clients/SkinnedRigBinary.h>

namespace Diorama
{
    using namespace DragonBones;

    namespace
    {
        void ExpectVec2Eq(const AZ::Vector2& a, const AZ::Vector2& b)
        {
            EXPECT_FLOAT_EQ(a.GetX(), b.GetX());
            EXPECT_FLOAT_EQ(a.GetY(), b.GetY());
        }

        void ExpectAffineEq(const MeshSkin::Affine2D& a, const MeshSkin::Affine2D& b)
        {
            EXPECT_FLOAT_EQ(a.m_a, b.m_a);
            EXPECT_FLOAT_EQ(a.m_b, b.m_b);
            EXPECT_FLOAT_EQ(a.m_c, b.m_c);
            EXPECT_FLOAT_EQ(a.m_d, b.m_d);
            EXPECT_FLOAT_EQ(a.m_tx, b.m_tx);
            EXPECT_FLOAT_EQ(a.m_ty, b.m_ty);
        }

        void ExpectKeyframeEq(const Keyframe& a, const Keyframe& b)
        {
            EXPECT_FLOAT_EQ(a.m_startTime, b.m_startTime);
            EXPECT_FLOAT_EQ(a.m_duration, b.m_duration);
            ExpectVec2Eq(a.m_value, b.m_value);
            EXPECT_EQ(a.m_tween, b.m_tween);
            for (int i = 0; i < 4; ++i)
            {
                EXPECT_FLOAT_EQ(a.m_curve[i], b.m_curve[i]);
            }
        }

        void ExpectKeyframesEq(const AZStd::vector<Keyframe>& a, const AZStd::vector<Keyframe>& b)
        {
            ASSERT_EQ(a.size(), b.size());
            for (size_t i = 0; i < a.size(); ++i)
            {
                ExpectKeyframeEq(a[i], b[i]);
            }
        }

        void ExpectDocEq(const Document& a, const Document& b)
        {
            EXPECT_EQ(a.m_name, b.m_name);
            EXPECT_EQ(a.m_bonesInOrder, b.m_bonesInOrder);
            ASSERT_EQ(a.m_armatures.size(), b.m_armatures.size());
            for (size_t ai = 0; ai < a.m_armatures.size(); ++ai)
            {
                const Armature& x = a.m_armatures[ai];
                const Armature& y = b.m_armatures[ai];
                EXPECT_EQ(x.m_name, y.m_name);
                EXPECT_FLOAT_EQ(x.m_frameRate, y.m_frameRate);

                ASSERT_EQ(x.m_bones.size(), y.m_bones.size());
                for (size_t i = 0; i < x.m_bones.size(); ++i)
                {
                    const BoneData& p = x.m_bones[i];
                    const BoneData& q = y.m_bones[i];
                    EXPECT_EQ(p.m_name, q.m_name);
                    EXPECT_EQ(p.m_parentIndex, q.m_parentIndex);
                    ExpectAffineEq(p.m_bindLocal, q.m_bindLocal);
                    EXPECT_FLOAT_EQ(p.m_x, q.m_x);
                    EXPECT_FLOAT_EQ(p.m_y, q.m_y);
                    EXPECT_FLOAT_EQ(p.m_skewXDegrees, q.m_skewXDegrees);
                    EXPECT_FLOAT_EQ(p.m_skewYDegrees, q.m_skewYDegrees);
                    EXPECT_FLOAT_EQ(p.m_scaleX, q.m_scaleX);
                    EXPECT_FLOAT_EQ(p.m_scaleY, q.m_scaleY);
                    EXPECT_EQ(p.m_isSurface, q.m_isSurface);
                    EXPECT_EQ(p.m_segmentX, q.m_segmentX);
                    EXPECT_EQ(p.m_segmentY, q.m_segmentY);
                    ASSERT_EQ(p.m_bindControlPoints.size(), q.m_bindControlPoints.size());
                    for (size_t k = 0; k < p.m_bindControlPoints.size(); ++k)
                    {
                        ExpectVec2Eq(p.m_bindControlPoints[k], q.m_bindControlPoints[k]);
                    }
                }

                ASSERT_EQ(x.m_meshes.size(), y.m_meshes.size());
                for (size_t i = 0; i < x.m_meshes.size(); ++i)
                {
                    const SkinnedMesh& p = x.m_meshes[i];
                    const SkinnedMesh& q = y.m_meshes[i];
                    EXPECT_EQ(p.m_slotName, q.m_slotName);
                    EXPECT_EQ(p.m_displayName, q.m_displayName);
                    EXPECT_EQ(p.m_drawOrder, q.m_drawOrder);
                    ASSERT_EQ(p.m_vertices.size(), q.m_vertices.size());
                    for (size_t k = 0; k < p.m_vertices.size(); ++k)
                    {
                        ExpectVec2Eq(p.m_vertices[k].m_bindPos, q.m_vertices[k].m_bindPos);
                        ExpectVec2Eq(p.m_vertices[k].m_uv, q.m_vertices[k].m_uv);
                        ASSERT_EQ(p.m_vertices[k].m_influenceCount, q.m_vertices[k].m_influenceCount);
                        for (int inf = 0; inf < p.m_vertices[k].m_influenceCount; ++inf)
                        {
                            EXPECT_EQ(p.m_vertices[k].m_influences[inf].m_boneIndex, q.m_vertices[k].m_influences[inf].m_boneIndex);
                            EXPECT_FLOAT_EQ(p.m_vertices[k].m_influences[inf].m_weight, q.m_vertices[k].m_influences[inf].m_weight);
                        }
                    }
                    EXPECT_EQ(p.m_indices, q.m_indices);
                    EXPECT_EQ(p.m_boneGlobalIndices, q.m_boneGlobalIndices);
                    ASSERT_EQ(p.m_bindWorld.size(), q.m_bindWorld.size());
                    for (size_t k = 0; k < p.m_bindWorld.size(); ++k)
                    {
                        ExpectAffineEq(p.m_bindWorld[k], q.m_bindWorld[k]);
                    }
                }

                ASSERT_EQ(x.m_surfaceMeshes.size(), y.m_surfaceMeshes.size());
                for (size_t i = 0; i < x.m_surfaceMeshes.size(); ++i)
                {
                    const SurfaceMesh& p = x.m_surfaceMeshes[i];
                    const SurfaceMesh& q = y.m_surfaceMeshes[i];
                    EXPECT_EQ(p.m_slotName, q.m_slotName);
                    EXPECT_EQ(p.m_displayName, q.m_displayName);
                    EXPECT_EQ(p.m_drawOrder, q.m_drawOrder);
                    EXPECT_EQ(p.m_surfaceBoneIndex, q.m_surfaceBoneIndex);
                    ASSERT_EQ(p.m_bindVertices.size(), q.m_bindVertices.size());
                    for (size_t k = 0; k < p.m_bindVertices.size(); ++k)
                    {
                        ExpectVec2Eq(p.m_bindVertices[k], q.m_bindVertices[k]);
                    }
                    ASSERT_EQ(p.m_uvs.size(), q.m_uvs.size());
                    for (size_t k = 0; k < p.m_uvs.size(); ++k)
                    {
                        ExpectVec2Eq(p.m_uvs[k], q.m_uvs[k]);
                    }
                    EXPECT_EQ(p.m_indices, q.m_indices);
                }

                ASSERT_EQ(x.m_animations.size(), y.m_animations.size());
                for (size_t i = 0; i < x.m_animations.size(); ++i)
                {
                    const Animation& p = x.m_animations[i];
                    const Animation& q = y.m_animations[i];
                    EXPECT_EQ(p.m_name, q.m_name);
                    EXPECT_FLOAT_EQ(p.m_durationSeconds, q.m_durationSeconds);
                    EXPECT_EQ(p.m_loop, q.m_loop);
                    EXPECT_EQ(p.m_blendType, q.m_blendType);

                    ASSERT_EQ(p.m_bones.size(), q.m_bones.size());
                    for (size_t k = 0; k < p.m_bones.size(); ++k)
                    {
                        EXPECT_EQ(p.m_bones[k].m_boneName, q.m_bones[k].m_boneName);
                        EXPECT_EQ(p.m_bones[k].m_boneIndex, q.m_bones[k].m_boneIndex);
                        ExpectKeyframesEq(p.m_bones[k].m_translate, q.m_bones[k].m_translate);
                        ExpectKeyframesEq(p.m_bones[k].m_rotate, q.m_bones[k].m_rotate);
                        ExpectKeyframesEq(p.m_bones[k].m_scale, q.m_bones[k].m_scale);
                    }

                    ASSERT_EQ(p.m_deforms.size(), q.m_deforms.size());
                    for (size_t k = 0; k < p.m_deforms.size(); ++k)
                    {
                        EXPECT_EQ(p.m_deforms[k].m_targetName, q.m_deforms[k].m_targetName);
                        EXPECT_EQ(p.m_deforms[k].m_kind, q.m_deforms[k].m_kind);
                        EXPECT_EQ(p.m_deforms[k].m_targetIndex, q.m_deforms[k].m_targetIndex);
                        ASSERT_EQ(p.m_deforms[k].m_frames.size(), q.m_deforms[k].m_frames.size());
                        for (size_t fi = 0; fi < p.m_deforms[k].m_frames.size(); ++fi)
                        {
                            const DeformFrame& df = p.m_deforms[k].m_frames[fi];
                            const DeformFrame& dg = q.m_deforms[k].m_frames[fi];
                            EXPECT_FLOAT_EQ(df.m_startTime, dg.m_startTime);
                            EXPECT_FLOAT_EQ(df.m_duration, dg.m_duration);
                            EXPECT_EQ(df.m_tween, dg.m_tween);
                            EXPECT_EQ(df.m_offset, dg.m_offset);
                            ASSERT_EQ(df.m_rawDeltas.size(), dg.m_rawDeltas.size());
                            for (size_t d = 0; d < df.m_rawDeltas.size(); ++d)
                            {
                                EXPECT_FLOAT_EQ(df.m_rawDeltas[d], dg.m_rawDeltas[d]);
                            }
                        }
                    }

                    ASSERT_EQ(p.m_progress.size(), q.m_progress.size());
                    for (size_t k = 0; k < p.m_progress.size(); ++k)
                    {
                        EXPECT_EQ(p.m_progress[k].m_targetName, q.m_progress[k].m_targetName);
                        EXPECT_EQ(p.m_progress[k].m_targetIndex, q.m_progress[k].m_targetIndex);
                        EXPECT_FLOAT_EQ(p.m_progress[k].m_positionX, q.m_progress[k].m_positionX);
                        ExpectKeyframesEq(p.m_progress[k].m_values, q.m_progress[k].m_values);
                    }

                    ASSERT_EQ(p.m_weights.size(), q.m_weights.size());
                    for (size_t k = 0; k < p.m_weights.size(); ++k)
                    {
                        EXPECT_EQ(p.m_weights[k].m_targetName, q.m_weights[k].m_targetName);
                        EXPECT_EQ(p.m_weights[k].m_targetIndex, q.m_weights[k].m_targetIndex);
                        ExpectKeyframesEq(p.m_weights[k].m_values, q.m_weights[k].m_values);
                    }

                    ASSERT_EQ(p.m_parameters.size(), q.m_parameters.size());
                    for (size_t k = 0; k < p.m_parameters.size(); ++k)
                    {
                        EXPECT_EQ(p.m_parameters[k].m_targetName, q.m_parameters[k].m_targetName);
                        EXPECT_EQ(p.m_parameters[k].m_targetIndex, q.m_parameters[k].m_targetIndex);
                        ExpectKeyframesEq(p.m_parameters[k].m_values, q.m_parameters[k].m_values);
                    }
                }
            }
        }

        //! Build a document that exercises every struct and every field the codec touches:
        //! a regular bone and a surface bone (with control points), a skinned mesh (weighted
        //! vertices + bindWorld) and a surface mesh, and animations spanning bone timelines
        //! (all three tween kinds), a surface deform channel (offset + raw deltas), and the
        //! type-40/41/42 progress/weight/parameter channels including a 1D blend host.
        Document MakeRichDocument()
        {
            Document doc;
            doc.m_name = "puppet";
            doc.m_bonesInOrder = true;

            Armature arm;
            arm.m_name = "body";
            arm.m_frameRate = 24.0f;

            BoneData root;
            root.m_name = "root";
            root.m_parentIndex = -1;
            root.m_bindLocal = MeshSkin::Affine2D{ 1.0f, 0.0f, 0.0f, 1.0f, 5.0f, -3.0f };
            root.m_x = 5.0f;
            root.m_y = -3.0f;
            root.m_skewXDegrees = 10.0f;
            root.m_skewYDegrees = 12.5f;
            root.m_scaleX = 1.25f;
            root.m_scaleY = 0.75f;
            arm.m_bones.push_back(root);

            BoneData surface;
            surface.m_name = "water";
            surface.m_parentIndex = 0;
            surface.m_isSurface = true;
            surface.m_segmentX = 2;
            surface.m_segmentY = 1;
            surface.m_bindControlPoints = { AZ::Vector2(0, 0),  AZ::Vector2(10, 0),  AZ::Vector2(20, 0),
                                            AZ::Vector2(0, 10), AZ::Vector2(10, 10), AZ::Vector2(20, 10) };
            arm.m_bones.push_back(surface);

            SkinnedMesh mesh;
            mesh.m_slotName = "torso";
            mesh.m_displayName = "body/torso";
            mesh.m_drawOrder = 3;
            MeshSkin::SkinnedVertex v0;
            v0.m_bindPos = AZ::Vector2(1.0f, 2.0f);
            v0.m_uv = AZ::Vector2(0.1f, 0.2f);
            v0.m_influenceCount = 2;
            v0.m_influences[0] = MeshSkin::Influence{ 0, 0.7f };
            v0.m_influences[1] = MeshSkin::Influence{ 1, 0.3f };
            MeshSkin::SkinnedVertex v1;
            v1.m_bindPos = AZ::Vector2(3.0f, 4.0f);
            v1.m_uv = AZ::Vector2(0.3f, 0.4f);
            v1.m_influenceCount = 1;
            v1.m_influences[0] = MeshSkin::Influence{ 1, 1.0f };
            mesh.m_vertices = { v0, v1 };
            mesh.m_indices = { 0, 1, 0 };
            mesh.m_boneGlobalIndices = { 0, 1 };
            mesh.m_bindWorld = { MeshSkin::Affine2D::Identity(), MeshSkin::Affine2D{ 0.5f, 0.1f, -0.1f, 0.5f, 7.0f, 8.0f } };
            arm.m_meshes.push_back(mesh);

            SurfaceMesh surfMesh;
            surfMesh.m_slotName = "pond";
            surfMesh.m_displayName = "body/pond";
            surfMesh.m_drawOrder = 1;
            surfMesh.m_surfaceBoneIndex = 1;
            surfMesh.m_bindVertices = { AZ::Vector2(0, 0), AZ::Vector2(20, 0), AZ::Vector2(20, 10), AZ::Vector2(0, 10) };
            surfMesh.m_uvs = { AZ::Vector2(0, 0), AZ::Vector2(1, 0), AZ::Vector2(1, 1), AZ::Vector2(0, 1) };
            surfMesh.m_indices = { 0, 1, 2 };
            arm.m_surfaceMeshes.push_back(surfMesh);

            Animation idle;
            idle.m_name = "idle";
            idle.m_durationSeconds = 1.0f;
            idle.m_loop = true;
            idle.m_blendType = BlendType::None;
            BoneTimeline bt;
            bt.m_boneName = "root";
            bt.m_boneIndex = 0;
            bt.m_translate.push_back(Keyframe{ 0.0f, 0.5f, AZ::Vector2(1.0f, 0.0f), TweenType::Linear, { 0, 0, 1, 1 } });
            bt.m_translate.push_back(Keyframe{ 0.5f, 0.5f, AZ::Vector2(0.0f, 1.0f), TweenType::Curve, { 0.25f, 0.1f, 0.75f, 0.9f } });
            bt.m_rotate.push_back(Keyframe{ 0.0f, 1.0f, AZ::Vector2(15.0f, 15.0f), TweenType::Stepped, { 0, 0, 1, 1 } });
            bt.m_scale.push_back(Keyframe{ 0.0f, 1.0f, AZ::Vector2(1.1f, 0.9f), TweenType::Linear, { 0, 0, 1, 1 } });
            idle.m_bones.push_back(bt);
            DeformTimeline dt;
            dt.m_targetName = "water";
            dt.m_kind = DeformTargetKind::Surface;
            dt.m_targetIndex = 1;
            DeformFrame frame;
            frame.m_startTime = 0.0f;
            frame.m_duration = 1.0f;
            frame.m_tween = TweenType::Linear;
            frame.m_offset = 2;
            frame.m_rawDeltas = { 1.5f, -2.5f, 3.5f, -4.5f };
            dt.m_frames.push_back(frame);
            idle.m_deforms.push_back(dt);
            ProgressTimeline prog;
            prog.m_targetName = "PARAM_flow";
            prog.m_targetIndex = 1;
            prog.m_positionX = 0.0f;
            prog.m_values.push_back(Keyframe{ 0.0f, 1.0f, AZ::Vector2(0.5f, 0.0f), TweenType::Linear, { 0, 0, 1, 1 } });
            idle.m_progress.push_back(prog);
            WeightTimeline wt;
            wt.m_targetName = "PARAM_surge";
            wt.m_targetIndex = 1;
            wt.m_values.push_back(Keyframe{ 0.0f, 1.0f, AZ::Vector2(0.8f, 0.0f), TweenType::Linear, { 0, 0, 1, 1 } });
            idle.m_weights.push_back(wt);
            arm.m_animations.push_back(idle);

            Animation blend;
            blend.m_name = "seastate";
            blend.m_durationSeconds = 2.0f;
            blend.m_loop = false;
            blend.m_blendType = BlendType::Blend1D;
            ProgressTimeline left;
            left.m_targetName = "PARAM_calm";
            left.m_targetIndex = 0;
            left.m_positionX = 0.0f;
            left.m_values.push_back(Keyframe{ 0.0f, 2.0f, AZ::Vector2(1.0f, 0.0f), TweenType::Linear, { 0, 0, 1, 1 } });
            ProgressTimeline right;
            right.m_targetName = "PARAM_choppy";
            right.m_targetIndex = 0;
            right.m_positionX = 1.0f;
            right.m_values.push_back(Keyframe{ 0.0f, 2.0f, AZ::Vector2(1.0f, 0.0f), TweenType::Linear, { 0, 0, 1, 1 } });
            blend.m_progress = { left, right };
            ParameterTimeline pt;
            pt.m_targetName = "seastate";
            pt.m_targetIndex = 1;
            pt.m_values.push_back(Keyframe{ 0.0f, 2.0f, AZ::Vector2(0.5f, 0.0f), TweenType::Linear, { 0, 0, 1, 1 } });
            blend.m_parameters.push_back(pt);
            arm.m_animations.push_back(blend);

            doc.m_armatures.push_back(arm);
            return doc;
        }
    } // namespace

    // ---- round trip -----------------------------------------------------------------

    TEST(SkinnedRigBinaryTest, RichDocumentRoundTrips)
    {
        const Document original = MakeRichDocument();
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Encode(original, bytes);
        EXPECT_GT(bytes.size(), 8u); // at least the header

        Document decoded;
        ASSERT_TRUE(SkinnedRig::Decode(bytes, decoded));
        ExpectDocEq(original, decoded);
    }

    TEST(SkinnedRigBinaryTest, EmptyDocumentRoundTrips)
    {
        const Document original; // default: empty name, no armatures, bonesInOrder true
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Encode(original, bytes);

        Document decoded;
        ASSERT_TRUE(SkinnedRig::Decode(bytes, decoded));
        ExpectDocEq(original, decoded);
    }

    TEST(SkinnedRigBinaryTest, EncodeIsDeterministic)
    {
        const Document doc = MakeRichDocument();
        AZStd::vector<AZ::u8> a;
        AZStd::vector<AZ::u8> b;
        SkinnedRig::Encode(doc, a);
        SkinnedRig::Encode(doc, b);
        EXPECT_EQ(a, b); // stable products keep the asset hash / cache stable
    }

    // ---- malformed / untrusted input ------------------------------------------------

    TEST(SkinnedRigBinaryTest, RejectsEmptyBuffer)
    {
        Document decoded;
        EXPECT_FALSE(SkinnedRig::Decode(nullptr, 0, decoded));
        AZStd::vector<AZ::u8> empty;
        EXPECT_FALSE(SkinnedRig::Decode(empty, decoded));
    }

    TEST(SkinnedRigBinaryTest, RejectsBadMagic)
    {
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Encode(MakeRichDocument(), bytes);
        bytes[0] ^= 0xFF; // corrupt the magic
        Document decoded;
        EXPECT_FALSE(SkinnedRig::Decode(bytes, decoded));
        EXPECT_TRUE(decoded.m_armatures.empty()); // out is cleared on failure
    }

    TEST(SkinnedRigBinaryTest, RejectsWrongVersion)
    {
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Encode(MakeRichDocument(), bytes);
        bytes[4] = 0xEE; // bump the little-endian version's low byte to an unknown value
        Document decoded;
        EXPECT_FALSE(SkinnedRig::Decode(bytes, decoded));
    }

    TEST(SkinnedRigBinaryTest, RejectsTruncation)
    {
        AZStd::vector<AZ::u8> full;
        SkinnedRig::Encode(MakeRichDocument(), full);
        // Every strict prefix (past the header) must fail rather than read past the end.
        for (size_t cut = 8; cut < full.size(); cut += 7)
        {
            AZStd::vector<AZ::u8> truncated(full.begin(), full.begin() + cut);
            Document decoded;
            EXPECT_FALSE(SkinnedRig::Decode(truncated, decoded)) << "cut=" << cut;
        }
    }

    TEST(SkinnedRigBinaryTest, RejectsTrailingBytes)
    {
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Encode(MakeRichDocument(), bytes);
        bytes.push_back(0x00); // extra byte past the encoded document
        Document decoded;
        EXPECT_FALSE(SkinnedRig::Decode(bytes, decoded)); // must consume the whole buffer exactly
    }

    TEST(SkinnedRigBinaryTest, RejectsHugeArmatureCount)
    {
        // Header + document name (empty) + bonesInOrder, then a bogus armature count of 2^30.
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Writer w(bytes);
        w.U32(SkinnedRig::kMagic);
        w.U32(SkinnedRig::kVersion);
        w.Str(AZStd::string{}); // name
        w.U8(1); // bonesInOrder
        w.U32(1u << 30); // armature count far beyond the remaining bytes
        Document decoded;
        EXPECT_FALSE(SkinnedRig::Decode(bytes, decoded)); // no over-allocation, no crash
    }

    TEST(SkinnedRigBinaryTest, RejectsCorruptInfluenceCount)
    {
        // Hand-craft a buffer valid up to a single vertex, then write a corrupt influence
        // count beyond MaxInfluences. The reader must fail on the range check rather than
        // read a wrong number of influence pairs and desync the stream.
        AZStd::vector<AZ::u8> bytes;
        SkinnedRig::Writer w(bytes);
        w.U32(SkinnedRig::kMagic);
        w.U32(SkinnedRig::kVersion);
        w.Str(AZStd::string{}); // document name
        w.U8(1); // bonesInOrder
        w.U32(1); // 1 armature
        w.Str(AZStd::string("a")); // armature name
        w.F32(24.0f); // frame rate
        w.U32(0); // 0 bones
        w.U32(1); // 1 skinned mesh
        w.Str(AZStd::string("s")); // slot name
        w.Str(AZStd::string{}); // display name
        w.I32(0); // draw order
        w.U32(1); // 1 vertex
        w.Vec2(AZ::Vector2(0.0f, 0.0f)); // bindPos
        w.Vec2(AZ::Vector2(0.0f, 0.0f)); // uv
        w.I32(99); // CORRUPT influence count (> MaxInfluences)

        Document decoded;
        EXPECT_FALSE(SkinnedRig::Decode(bytes, decoded));
        EXPECT_TRUE(decoded.m_armatures.empty());
    }

    // ---- the committed example rigs, through the builder's exact path ----------------

    namespace
    {
        //! Read a whole file relative to this gem checkout (located from this source file's
        //! own path), so the test runs against the real committed example rigs with no asset
        //! pipeline. Empty on any failure.
        AZStd::string ReadGemFile(const char* gemRelativePath)
        {
            // <gem>/Code/Tests/Clients/<this file> -> <gem>
            const AZ::IO::Path gemRoot = AZ::IO::Path(__FILE__).ParentPath().ParentPath().ParentPath().ParentPath();
            const AZ::IO::Path full = gemRoot / gemRelativePath;
            AZ::IO::SystemFile file;
            if (!file.Open(full.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY))
            {
                return {};
            }
            AZStd::string contents;
            contents.resize(static_cast<size_t>(file.Length()));
            const AZ::IO::SystemFile::SizeType read = file.Read(contents.size(), contents.data());
            contents.resize(static_cast<size_t>(read));
            return contents;
        }
    } // namespace

    TEST(SkinnedRigBinaryTest, CommittedExampleRigsRoundTripThroughTheBuilderPath)
    {
        // The exact sequence the AssetBuilder runs (parse the armature, bake the atlas UV remap
        // in, encode) followed by the runtime's decode. The decoded document must equal the
        // imported one field for field, for every real rig that ships with the gem: the
        // weighted-mesh seaweed, the surface / FFD water with its type-40/41/42 parameter
        // layer, and the bone-driven puppet.
        const char* rigs[] = { "seaweed", "water", "puppet" };
        for (const char* rig : rigs)
        {
            SCOPED_TRACE(rig);
            const AZStd::string skePath = AZStd::string::format("Assets/Diorama/Examples/Skinned/%s_ske.json", rig);
            const AZStd::string texPath = AZStd::string::format("Assets/Diorama/Examples/Skinned/%s_tex.json", rig);
            const AZStd::string ske = ReadGemFile(skePath.c_str());
            const AZStd::string tex = ReadGemFile(texPath.c_str());
            ASSERT_FALSE(ske.empty()) << "missing committed example rig " << skePath.c_str();
            ASSERT_FALSE(tex.empty()) << "missing committed example atlas " << texPath.c_str();

            Document imported;
            ASSERT_TRUE(ParseDocument(ske, imported));
            Atlas atlas;
            ASSERT_TRUE(ParseAtlas(tex, atlas));
            ApplyAtlasUVs(imported, atlas);
            ASSERT_FALSE(imported.m_armatures.empty());

            AZStd::vector<AZ::u8> bytes;
            SkinnedRig::Encode(imported, bytes);
            Document decoded;
            ASSERT_TRUE(SkinnedRig::Decode(bytes, decoded));
            ExpectDocEq(imported, decoded);
        }
    }
} // namespace Diorama
