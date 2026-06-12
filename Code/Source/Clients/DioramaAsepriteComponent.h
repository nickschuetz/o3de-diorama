/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/AsepriteImport.h>
#include <Clients/AsepriteSheetData.h>
#include <Clients/DioramaAsepriteSheetAsset.h>
#include <Clients/SimStateBus.h>
#include <Diorama/DioramaAsepriteBus.h>
#include <Diorama/DioramaSimClockBus.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace Diorama
{
    //! Configuration for the Aseprite animation player. The frames/tags/atlas size are
    //! filled by importing an Aseprite "Export Sprite Sheet" JSON (see the editor twin);
    //! the atlas texture path and playback knobs are authored normally.
    class DioramaAsepriteConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaAsepriteConfig, DioramaAsepriteConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaAsepriteConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaAsepriteConfig() override = default;

        //! Product path of the atlas PNG (the image Aseprite exported next to the JSON).
        AZStd::string m_atlasTexturePath;
        //! Atlas pixel size, used to turn frame rects into UVs. Filled by import.
        int m_atlasWidth = 0;
        int m_atlasHeight = 0;
        //! Frames in document order. Filled by import.
        AZStd::vector<AsepriteFrameData> m_frames;
        //! Named animations. Filled by import.
        AZStd::vector<AsepriteTagData> m_tags;
        //! Tag to play on activate (empty -> the first tag, or whole sheet if none).
        AZStd::string m_defaultTag;
        //! Playback rate multiplier; negative reverses.
        float m_speed = 1.0f;
        //! Loop the default tag (true) or hold its last frame (false).
        bool m_looping = true;
        //! Begin playing on activate.
        bool m_autoPlay = true;
        //! Advance on the 2D Simulation Clock's fixed steps instead of the render
        //! tick (deterministic; falls back to the render tick when no clock runs).
        bool m_useSimClock = false;
        //! Optional: a .dioramasheet product (emitted by the native .aseprite
        //! AssetBuilder). When set, its frames/tags/atlas are loaded at runtime and
        //! take precedence over the inline (JSON-imported) data above -- assigning the
        //! product is all that is needed, no manual JSON import.
        AZ::Data::Asset<DioramaAsepriteSheetAsset> m_sheet;
    };

    //! Build the pure Aseprite::Document the timeline functions operate on from a config.
    Aseprite::Document BuildAsepriteDocument(const DioramaAsepriteConfig& config);
    //! Same, from a loaded sheet product asset (the runtime asset-reference path).
    Aseprite::Document BuildAsepriteDocument(const DioramaAsepriteSheetAsset& sheet);

    //! Runtime Aseprite animation player. Drives a Diorama Sprite on the same entity:
    //! sets its texture to the imported atlas and its UV region to the current frame
    //! each tick, honoring per-frame durations and tag direction (forward / reverse /
    //! ping-pong). Driven at runtime through DioramaAsepriteRequestBus for AI/human
    //! parity. No third-party runtime; the JSON is imported offline.
    class DioramaAsepriteComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected AZ::Data::AssetBus::Handler
        , protected DioramaAsepriteRequestBus::Handler
        , protected DioramaSimTickNotificationBus::Handler
        , protected DioramaSimStateParticipantBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaAsepriteComponent, DioramaAsepriteComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DioramaAsepriteComponent() = default;
        explicit DioramaAsepriteComponent(const DioramaAsepriteConfig& config);
        ~DioramaAsepriteComponent() override = default;

        //! Snapshot chunk: Aseprite playback position ('ASEP' little-endian).
        static constexpr AZ::u32 AsepriteChunkTag = 0x50455341; // 'ASEP' little-endian

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSimTickNotifications (Use Simulation Clock mode)
        void OnSimTick(AZ::s64 frame, float stepSeconds) override;

        // DioramaSimStateParticipantBus (snapshot/restore of playback position)
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;

    private:
        //! Advance playback by `deltaTime` seconds (render tick or fixed sim step).
        void Advance(float deltaTime);

    protected:
        // AZ::Data::AssetBus (asset-reference mode: load the .dioramasheet, then play)
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // DioramaAsepriteRequests
        void PlayTag(const AZStd::string& tagName) override;
        void Play() override;
        void Stop() override;
        void SetFrame(int frameIndex) override;
        void SetSpeed(float speed) override;
        void SetLooping(bool looping) override;
        void SetUseSimClock(bool enabled) override;
        bool IsPlaying() override;
        int GetCurrentFrame() override;

    private:
        //! Push the given frame index onto the sprite as a UV region.
        void ApplyFrame(int frameIndex);
        //! Take over the sprite and start playback from the current m_doc (shared by
        //! the inline path and the asset-ready path).
        void BeginPlayback();

        DioramaAsepriteConfig m_config;
        Aseprite::Document m_doc;
        int m_tagIndex = -1; //!< index into m_doc.m_tags, or -1 for "whole sheet"
        float m_elapsed = 0.0f;
        int m_currentFrame = 0;
        bool m_playing = false;
        bool m_ticking = false; //!< guards a single TickBus connect across both paths
    };
} // namespace Diorama
