/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

namespace Diorama
{
    // System Component TypeIds
    inline constexpr const char* DioramaSystemComponentTypeId = "{9C66DF50-122D-489A-BD6F-922C79E4D477}";
    inline constexpr const char* DioramaEditorSystemComponentTypeId = "{F05FD104-BC6E-473D-A66D-B00715DE2B5D}";

    // Sprite Component TypeIds
    inline constexpr const char* SpriteComponentConfigTypeId = "{1B3A0B8E-7C2A-4E2E-9E4B-1B7C9D9C2A11}";
    inline constexpr const char* SpriteComponentTypeId = "{2C4B1C9F-8D3B-4F3F-AF5C-2C8DAEAD3B22}";
    inline constexpr const char* EditorSpriteComponentTypeId = "{3D5C2DA0-9E4C-4040-B06D-3D9EBFBE4C33}";

    // Module derived classes TypeIds
    inline constexpr const char* DioramaModuleInterfaceTypeId = "{2A9A9C17-E822-40F4-9353-B0C151D85B3F}";
    inline constexpr const char* DioramaModuleTypeId = "{6B412366-4C21-4209-8078-186713A9C391}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* DioramaEditorModuleTypeId = DioramaModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* DioramaRequestsTypeId = "{EDABD455-BA50-46E7-8D02-2528F511B15F}";

    // AI-facing sprite API TypeIds (DioramaSpriteRequestBus / NotificationBus / SpriteInfo)
    inline constexpr const char* DioramaSpriteRequestsTypeId = "{4A2E6B1C-9F3D-4C8A-B1E7-5D0A2C3F4B60}";
    inline constexpr const char* DioramaSpriteNotificationsTypeId = "{5B3F7C2D-0A4E-4D9B-C2F8-6E1B3D4F5C71}";
    inline constexpr const char* SpriteInfoTypeId = "{6C408D3E-1B5F-4EAC-D309-7F2C4E506D82}";

    // Tilemap Component TypeIds (teaching ladder rung 4)
    inline constexpr const char* TilemapComponentConfigTypeId = "{EAD6109D-2DAF-4F05-884C-C5867AF0214B}";
    inline constexpr const char* TilemapComponentTypeId = "{161C3B81-803E-41D3-B095-2E9E3F3BD32E}";
    inline constexpr const char* EditorTilemapComponentTypeId = "{A706DF64-A627-4CF9-9F95-4CB88D779E0C}";

    // AI-facing tilemap API TypeIds (DioramaTilemapRequestBus / TilemapInfo)
    inline constexpr const char* DioramaTilemapRequestsTypeId = "{7FA050C4-146C-4837-802B-CE3187BA6B3A}";
    inline constexpr const char* TilemapInfoTypeId = "{B966AD30-B56F-4F1B-B25D-3DB243F1B16C}";

    // Custom autotile rule entry (mask -> display offset)
    inline constexpr const char* TilemapAutotileRuleDataTypeId = "{E5576879-8A9B-44C5-D6D7-E8F90A1B2C19}";

    // Animated tile entry (a painted tile index -> a cycling sequence of atlas frames)
    inline constexpr const char* TilemapAnimatedTileDataTypeId = "{F6687980-9BAC-4D6E-E7E8-F9A10B2C3D2A}";

    // Dedicated tilemap asset + builder (.dioramatilemap source -> product asset)
    inline constexpr const char* DioramaTilemapAssetTypeId = "{1C7E2D90-4A6B-4C8D-9E0F-1A2B3C4D5E60}";
    inline constexpr const char* DioramaTilemapLayerDataTypeId = "{2D8F3EA1-5B7C-4D9E-8F10-2B3C4D5E6F71}";

    // 2D collision TypeIds (gem-native colliders + contact/trigger + queries)
    inline constexpr const char* Collider2DConfigTypeId = "{C10840E6-93A2-4243-BF5B-A4DB135AB7EA}";
    inline constexpr const char* Collider2DComponentTypeId = "{A9037410-345D-43AC-80BE-E3FCD4EE5CDF}";
    inline constexpr const char* EditorCollider2DComponentTypeId = "{D779B83E-01A4-42E2-BB82-18ED5BF33AB5}";
    inline constexpr const char* Collision2DSystemComponentTypeId = "{ADF0673F-459E-4A05-BCE0-F02A7C31A166}";

    // 2D dynamic lighting TypeIds (gem-native light components gathered by the FP)
    inline constexpr const char* DioramaLightConfigTypeId = "{F8A96840-C2CE-4367-BE1C-EF3B0D1024AB}";
    inline constexpr const char* DioramaLightComponentTypeId = "{5FE91F03-CEC3-4367-B05A-1F35E4A12B58}";
    inline constexpr const char* EditorDioramaLightComponentTypeId = "{0781B582-E88E-4EEF-BD26-3BB7F7EA25CC}";
    inline constexpr const char* DioramaLightRequestsTypeId = "{2D3D5429-D3C6-4769-80D8-424478630B23}";
    inline constexpr const char* DioramaLightInfoTypeId = "{7A1C4B9E-3D52-4F8A-9C16-2E8B5A3D7F40}";

    // 2D camera TypeIds (gem-native follow/deadzone/bounds/lookahead/shake camera
    // controller; drives a camera entity's transform via the Camera2D math core)
    inline constexpr const char* DioramaCamera2DConfigTypeId = "{1E6B9D24-7C3F-4A85-9B2E-0C4D8F5A1B36}";
    inline constexpr const char* DioramaCamera2DComponentTypeId = "{2F7CAE35-8D40-4B96-AC3F-1D5E9061C247}";
    inline constexpr const char* EditorDioramaCamera2DComponentTypeId = "{3A8DBF46-9E51-4CA7-BD40-2E6FA172D358}";
    inline constexpr const char* DioramaCamera2DRequestsTypeId = "{4B9EC057-AF62-4DB8-CE51-3F70B283E469}";
    inline constexpr const char* Camera2DInfoTypeId = "{5CAFD168-B073-4EC9-DF62-4081C394F57A}";

    // 2D particle emitter TypeIds (pooled emitter rendered through the sprite batch)
    inline constexpr const char* DioramaParticleConfigTypeId = "{6DB0E279-C184-4FDA-E073-5192D4A5061B}";
    inline constexpr const char* DioramaParticleEmitterComponentTypeId = "{7EC1F38A-D295-40EB-F184-62A3E5B6172C}";
    inline constexpr const char* EditorDioramaParticleEmitterComponentTypeId = "{8FD2A49B-E3A6-41FC-A295-73B4F6C7283D}";
    inline constexpr const char* DioramaParticleRequestsTypeId = "{9AE3B5AC-F4B7-42AD-B3A6-84C5A7D8394E}";
    inline constexpr const char* ParticleInfoTypeId = "{ABF4C6BD-A5C8-43BE-C4B7-95D6B8E94A5F}";

    // 2D parallax TypeIds (camera-relative layer offset for 2.5D depth)
    inline constexpr const char* DioramaParallaxConfigTypeId = "{BC05D7CE-B6D9-43BF-C5C8-A6E7C9FA5B60}";
    inline constexpr const char* DioramaParallaxComponentTypeId = "{CD16E8DF-C7EA-44C0-D6D9-B7F8DA0B6C71}";
    inline constexpr const char* EditorDioramaParallaxComponentTypeId = "{DE27F9E0-D8FB-45D1-E7EA-C809EB1C7D82}";
    inline constexpr const char* DioramaParallaxRequestsTypeId = "{EF380AF1-E90C-46E2-F8FB-D910FC2D8E93}";
    inline constexpr const char* ParallaxInfoTypeId = "{F0491B02-FA1D-47F3-A90C-EA21AD3E9FA4}";

    // Audio convenience TypeIds (fire-and-forget one-shot over MiniAudio)
    inline constexpr const char* DioramaAudioRequestsTypeId = "{51D41A62-96D1-4A9A-A9E6-4C8D3F7D62F1}";

    // Tilemap paint tool (editor-only): component mode + mode<->component bus
    inline constexpr const char* EditorTilemapPaintComponentModeTypeId = "{2A7F9B0C-1D3E-4F5A-8B6C-7D8E9F0A1B2C}";
    inline constexpr const char* TilemapPaintEditorRequestsTypeId = "{3B8A0C1D-2E4F-4A6B-9C7D-8E9F0A1B2C3D}";

    // 2D post-processing "look" profile TypeIds (drives Atom bloom + vignette)
    inline constexpr const char* DioramaLookConfigTypeId = "{6F2B1A4C-3D5E-4A8B-9C7D-1E2F3A4B5C6D}";
    inline constexpr const char* DioramaLookComponentTypeId = "{7A3C2B5D-4E6F-4B9C-8D1E-2F3A4B5C6D7E}";
    inline constexpr const char* EditorDioramaLookComponentTypeId = "{8B4D3C6E-5F70-4CAD-9E2F-3A4B5C6D7E8F}";
    inline constexpr const char* DioramaLookRequestsTypeId = "{9C5E4D7F-6081-4DBE-AF3A-4B5C6D7E8F90}";

    // CRT scanline overlay TypeIds (retro screen effect)
    inline constexpr const char* DioramaCRTConfigTypeId = "{DAAA3172-91E6-4521-BFA5-E33F98267758}";
    inline constexpr const char* DioramaCRTComponentTypeId = "{C9606627-3FE6-42EA-AD87-EF46FF171D27}";
    inline constexpr const char* EditorDioramaCRTComponentTypeId = "{B89BDCCD-A677-4BB4-940B-59BEE2550D9F}";
    inline constexpr const char* DioramaCRTRequestsTypeId = "{0D176FF2-664E-48FB-AE3C-91D045A95B44}";

    // Skeletal cutout clip player TypeIds (transform-hierarchy keyframe animation)
    inline constexpr const char* DioramaSkeletalClipConfigTypeId = "{A1E2C3D4-5F60-4A7B-8C9D-0E1F2A3B4C5D}";
    inline constexpr const char* DioramaSkeletalClipComponentTypeId = "{B2F3D4E5-6071-4B8C-9DAE-1F2A3B4C5D6E}";
    inline constexpr const char* EditorDioramaSkeletalClipComponentTypeId = "{C3041506-7182-4C9D-AEBF-2A3B4C5D6E7F}";
    inline constexpr const char* DioramaSkeletalRequestsTypeId = "{D4152617-8293-4DAE-BFC0-3B4C5D6E7F80}";
    inline constexpr const char* SkeletalKeyframeDataTypeId = "{E5263728-93A4-4EBF-90D1-4C5D6E7F8091}";
    inline constexpr const char* SkeletalBoneTrackDataTypeId = "{F6374839-A4B5-4FC0-A1E2-5D6E7F809102}";
    inline constexpr const char* SkeletalEaseTypeId = "{0748594A-B5C6-40D1-B2F3-6E7F80910213}";
    // Named alternative clip in the cross-fade library (v2 animation depth)
    inline constexpr const char* SkeletalNamedClipDataTypeId = "{18596A5B-C6D7-40E2-A3B4-7F8091021324}";
    // Aseprite import TypeIds (sprite-sheet JSON -> frames + named tag animations)
    inline constexpr const char* DioramaAsepriteConfigTypeId = "{1A2B3C4D-5E6F-4071-8192-A3B4C5D6E7F8}";
    inline constexpr const char* DioramaAsepriteComponentTypeId = "{2B3C4D5E-6F70-4182-92A3-B4C5D6E7F809}";
    inline constexpr const char* EditorDioramaAsepriteComponentTypeId = "{3C4D5E6F-7081-4293-A3B4-C5D6E7F8091A}";
    inline constexpr const char* DioramaAsepriteRequestsTypeId = "{4D5E6F70-8192-43A4-B4C5-D6E7F8091A2B}";
    inline constexpr const char* AsepriteFrameDataTypeId = "{5E6F7081-92A3-44B5-C5D6-E7F8091A2B3C}";
    inline constexpr const char* AsepriteTagDataTypeId = "{6F708192-A3B4-45C6-D6E7-F8091A2B3C4D}";
    inline constexpr const char* AsepriteDirectionTypeId = "{708192A3-B4C5-46D7-E7F8-091A2B3C4D5E}";
    inline constexpr const char* DioramaAsepriteSheetAssetTypeId = "{8192A3B4-C5D6-47E8-F809-1A2B3C4D5E6F}";

    // Input action-mapping TypeIds (rebindable named actions over input channels)
    inline constexpr const char* DioramaInputConfigTypeId = "{5D6E7F80-9102-4A3B-8C4D-5E6F70819300}";
    inline constexpr const char* DioramaInputComponentTypeId = "{6E7F8091-0213-4B4C-9D5E-6F708192A401}";
    inline constexpr const char* EditorDioramaInputComponentTypeId = "{7F809102-1324-4C5D-AE6F-708192A3B502}";
    inline constexpr const char* DioramaInputRequestsTypeId = "{80910213-2435-4D6E-BF70-8192A3B4C603}";
    inline constexpr const char* DioramaInputNotificationsTypeId = "{91021324-3546-4E7F-A081-92A3B4C5D704}";
    inline constexpr const char* DioramaInputActionDataTypeId = "{A1132435-4657-4F80-B192-A3B4C5D6E805}";
    inline constexpr const char* DioramaInputBindingDataTypeId = "{B2243546-5768-4091-A2A3-B4C5D6E7F906}";
    inline constexpr const char* InputActionKindTypeId = "{C3354657-6879-41A2-B3B4-C5D6E7F80A07}";
    inline constexpr const char* InputAxisTypeId = "{D4465768-798A-42B3-C4C5-D6E7F8091B08}";
    inline constexpr const char* DioramaMotionDataTypeId = "{E5566879-8A9B-43C4-D5D6-E7F8091C2C09}";
    // 2D animation state machine TypeIds (parameter-driven graph over the existing
    // Sprite/Aseprite animation players)
    inline constexpr const char* DioramaAnimStateMachineConfigTypeId = "{A2B1C0D9-1E2F-4A3B-8C4D-5E6F70819201}";
    inline constexpr const char* DioramaAnimStateMachineComponentTypeId = "{B3C2D1E0-2F30-4B4C-9D5E-6F708192A302}";
    inline constexpr const char* EditorDioramaAnimStateMachineComponentTypeId = "{C4D3E2F1-3041-4C5D-AE6F-708192A3B403}";
    inline constexpr const char* DioramaAnimStateMachineRequestsTypeId = "{D5E4F302-4152-4D6E-BF70-8192A3B4C504}";
    inline constexpr const char* DioramaAnimStateMachineNotificationsTypeId = "{E6F50413-5263-4E7F-A081-92A3B4C5D605}";
    inline constexpr const char* AnimStateDataTypeId = "{F7061524-6374-4F80-B192-A3B4C5D6E706}";
    inline constexpr const char* AnimParamDataTypeId = "{08172635-7485-4091-A2A3-B4C5D6E7F807}";
    inline constexpr const char* AnimConditionDataTypeId = "{19283746-8596-41A2-B3B4-C5D6E7F80908}";
    inline constexpr const char* AnimTransitionDataTypeId = "{2A394857-96A7-42B3-C4C5-D6E7F8091A09}";
    inline constexpr const char* AnimSMParamKindTypeId = "{3B4A5968-A7B8-43C4-D5D6-E7F8091A2B0A}";
    inline constexpr const char* AnimSMCompareTypeId = "{4C5B6A79-B8C9-44D5-E6E7-F8091A2B3C0B}";

    // AI-facing 2D collision API TypeIds
    inline constexpr const char* Diorama2DColliderRequestsTypeId = "{908C687A-9799-4BD9-9E82-3368D3BAEA03}";
    inline constexpr const char* Diorama2DCollisionNotificationsTypeId = "{3E9FB4C7-C577-450B-8CCA-9CEDF0964E06}";
    inline constexpr const char* Diorama2DCollisionRequestsTypeId = "{3CB7F919-1FE3-4718-B20F-5FDDF34F9B7A}";
    inline constexpr const char* Collider2DInfoTypeId = "{49F6D44E-02A1-4703-AEBB-51E14C72BD33}";
    inline constexpr const char* Raycast2DResultTypeId = "{4FCCC3DC-E65B-499C-9E85-F6014F4571AC}";
    inline constexpr const char* GroundProbe2DResultTypeId = "{5A1D7E2B-3C4F-4D60-9E81-2B3C4D5E6F71}";

    // Frame-data hitbox/hurtbox TypeIds (animation-frame-gated boxes over Collision2D)
    inline constexpr const char* DioramaHitboxConfigTypeId = "{1A2B3C4D-5E6F-4071-8293-A4B5C6D7E8F1}";
    inline constexpr const char* DioramaHitboxComponentTypeId = "{2B3C4D5E-6F70-4182-93A4-B5C6D7E8F1A2}";
    inline constexpr const char* EditorDioramaHitboxComponentTypeId = "{3C4D5E6F-7081-4293-A4B5-C6D7E8F1A2B3}";
    inline constexpr const char* DioramaHitboxRequestsTypeId = "{4D5E6F70-8192-43A4-B5C6-D7E8F1A2B3C4}";
    inline constexpr const char* DioramaHitboxNotificationsTypeId = "{5E6F7081-92A3-44B5-C6D7-E8F1A2B3C4D5}";
    inline constexpr const char* DioramaHitboxDataTypeId = "{6F708192-A3B4-45C6-D7E8-F1A2B3C4D5E6}";
    inline constexpr const char* DioramaHitboxInfoTypeId = "{92A3B4C5-D6E7-48F1-A2B3-C4D5E6F70819}";

    // Bullet-pattern (danmaku) emitter TypeIds (pooled pattern emission over Particles2D + Collision2D)
    inline constexpr const char* DioramaBulletConfigTypeId = "{A3B4C5D6-E7F8-4192-93A4-B5C6D7E8F1A3}";
    inline constexpr const char* DioramaBulletEmitterComponentTypeId = "{B4C5D6E7-F809-4203-A4B5-C6D7E8F1A2B4}";
    inline constexpr const char* EditorDioramaBulletEmitterComponentTypeId = "{C5D6E7F8-0912-4314-B5C6-D7E8F1A2B3C5}";
    inline constexpr const char* DioramaBulletRequestsTypeId = "{D6E7F809-1023-4425-C6D7-E8F1A2B3C4D6}";
    inline constexpr const char* DioramaBulletNotificationsTypeId = "{E7F80912-1134-4536-D7E8-F1A2B3C4D5E7}";
    inline constexpr const char* DioramaBulletInfoTypeId = "{F8091223-1245-4647-E8F1-A2B3C4D5E6F8}";

    // 2.5D brawler depth-lane TypeIds (depth-driven sprite lift / sort over the DepthLane core)
    inline constexpr const char* DioramaDepthBodyConfigTypeId = "{1C2D3E4F-5061-4273-8495-A6B7C8D9E0F2}";
    inline constexpr const char* DioramaDepthBodyComponentTypeId = "{2D3E4F50-6172-4384-95A6-B7C8D9E0F1A3}";
    inline constexpr const char* EditorDioramaDepthBodyComponentTypeId = "{3E4F5061-7283-4495-A6B7-C8D9E0F1A2B4}";
    inline constexpr const char* DioramaDepthBodyRequestsTypeId = "{4F506172-8394-45A6-B7C8-D9E0F1A2B3C5}";
    inline constexpr const char* DioramaDepthBodyInfoTypeId = "{50617283-94A5-46B7-C8D9-E0F1A2B3C4D6}";

    // Day/night cycle TypeIds (time-of-day clock driving a Diorama light over the DayNightCycle core)
    inline constexpr const char* DioramaDayNightConfigTypeId = "{61728394-A5B6-47C8-D9E0-F1A2B3C4D5E7}";
    inline constexpr const char* DioramaDayNightComponentTypeId = "{728394A5-B6C7-48D9-E0F1-A2B3C4D5E6F8}";
    inline constexpr const char* EditorDioramaDayNightComponentTypeId = "{8394A5B6-C7D8-49E0-F1A2-B3C4D5E6F7A9}";
    inline constexpr const char* DioramaDayNightRequestsTypeId = "{94A5B6C7-D8E9-40F1-A2B3-C4D5E6F7A8BA}";
    inline constexpr const char* DioramaDayNightInfoTypeId = "{A5B6C7D8-E9F0-41A2-B3C4-D5E6F7A8B9CB}";
} // namespace Diorama
