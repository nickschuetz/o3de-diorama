/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/AsepriteImport.h>
#include <Diorama/DioramaAsepriteBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

// The pure Aseprite::Direction enum lives in the import core; its reflection type
// info is specialized here (the one place that reflects it).
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::Aseprite::Direction, Diorama::AsepriteDirectionTypeId);
}

namespace Diorama
{
    //! Inspector-facing mirror of Aseprite::FrameData (a frame's atlas rect + duration).
    //! Populated by the editor import; serialized into the config so the runtime needs
    //! no file IO.
    struct AsepriteFrameData final
    {
        AZ_TYPE_INFO(Diorama::AsepriteFrameData, AsepriteFrameDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        int m_x = 0;
        int m_y = 0;
        int m_w = 0;
        int m_h = 0;
        float m_durationSeconds = 0.1f;
    };

    //! Inspector-facing mirror of Aseprite::TagData (a named frame range + direction).
    struct AsepriteTagData final
    {
        AZ_TYPE_INFO(Diorama::AsepriteTagData, AsepriteTagDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        int m_from = 0;
        int m_to = 0;
        Aseprite::Direction m_direction = Aseprite::Direction::Forward;
    };

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
    };

    //! Build the pure Aseprite::Document the timeline functions operate on from a config.
    Aseprite::Document BuildAsepriteDocument(const DioramaAsepriteConfig& config);

    //! Runtime Aseprite animation player. Drives a Diorama Sprite on the same entity:
    //! sets its texture to the imported atlas and its UV region to the current frame
    //! each tick, honoring per-frame durations and tag direction (forward / reverse /
    //! ping-pong). Driven at runtime through DioramaAsepriteRequestBus for AI/human
    //! parity. No third-party runtime; the JSON is imported offline.
    class DioramaAsepriteComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaAsepriteRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaAsepriteComponent, DioramaAsepriteComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DioramaAsepriteComponent() = default;
        explicit DioramaAsepriteComponent(const DioramaAsepriteConfig& config);
        ~DioramaAsepriteComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaAsepriteRequests
        void PlayTag(const AZStd::string& tagName) override;
        void Play() override;
        void Stop() override;
        void SetFrame(int frameIndex) override;
        void SetSpeed(float speed) override;
        void SetLooping(bool looping) override;
        bool IsPlaying() override;
        int GetCurrentFrame() override;

    private:
        //! Push the given frame index onto the sprite as a UV region.
        void ApplyFrame(int frameIndex);

        DioramaAsepriteConfig m_config;
        Aseprite::Document m_doc;
        int m_tagIndex = -1; //!< index into m_doc.m_tags, or -1 for "whole sheet"
        float m_elapsed = 0.0f;
        int m_currentFrame = 0;
        bool m_playing = false;
    };
} // namespace Diorama
