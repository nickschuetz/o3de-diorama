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

    // 2D UI/HUD TypeIds (screen-space HUD elements at parity with the sprite bus)
    inline constexpr const char* DioramaUIConfigTypeId = "{E807B991-372D-40F3-BF59-FD11AB9816C7}";
    inline constexpr const char* DioramaUIComponentTypeId = "{CFAC0BA7-08B4-4F78-BE65-D958A0780108}";
    inline constexpr const char* EditorDioramaUIComponentTypeId = "{159DE9A8-A1BE-4FEC-842B-B15AD8239D88}";
    inline constexpr const char* DioramaUIRequestsTypeId = "{761615B1-CB60-41BD-AD90-BCBFC0163C5B}";
    inline constexpr const char* UIInfoTypeId = "{E78E6C16-4C66-43BB-975C-3EA5721A3182}";

    // Audio convenience TypeIds (fire-and-forget one-shot over MiniAudio)
    inline constexpr const char* DioramaAudioRequestsTypeId = "{51D41A62-96D1-4A9A-A9E6-4C8D3F7D62F1}";

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

    // AI-facing 2D collision API TypeIds
    inline constexpr const char* Diorama2DColliderRequestsTypeId = "{908C687A-9799-4BD9-9E82-3368D3BAEA03}";
    inline constexpr const char* Diorama2DCollisionNotificationsTypeId = "{3E9FB4C7-C577-450B-8CCA-9CEDF0964E06}";
    inline constexpr const char* Diorama2DCollisionRequestsTypeId = "{3CB7F919-1FE3-4718-B20F-5FDDF34F9B7A}";
    inline constexpr const char* Collider2DInfoTypeId = "{49F6D44E-02A1-4703-AEBB-51E14C72BD33}";
    inline constexpr const char* Raycast2DResultTypeId = "{4FCCC3DC-E65B-499C-9E85-F6014F4571AC}";
} // namespace Diorama
