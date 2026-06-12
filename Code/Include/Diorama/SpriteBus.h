/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace Diorama
{
    //! Resolved runtime state of a sprite, returned by GetSpriteInfo. This is the
    //! verify-loop payload: it reports what the sprite is actually doing (texture
    //! loaded, drawing, current animation frame), not merely what was requested,
    //! so an agent can confirm an action took effect without a screenshot.
    struct SpriteInfo
    {
        AZ_TYPE_INFO(Diorama::SpriteInfo, SpriteInfoTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_texturePath; //!< Resolved texture product path, empty if none assigned.
        bool m_textureLoaded = false; //!< True once the texture asset has streamed in.
        bool m_visible = false; //!< True when registered with the feature processor and drawable.
        float m_width = 0.0f;
        float m_height = 0.0f;
        float m_pivotX = 0.5f;
        float m_pivotY = 0.5f;
        float m_sortOffset = 0.0f;
        bool m_billboard = false;
        bool m_doubleSided = true;
        bool m_pointFilter = false; //!< Nearest-neighbor texture filtering (pixel art) vs default linear.
        bool m_flipHorizontal = false;
        bool m_flipVertical = false;
        bool m_animEnabled = false;
        int m_currentFrame = 0;
        int m_frameCount = 1;
        bool m_useSimClock = false; //!< Animation advances on the 2D Simulation Clock's fixed steps.
    };

    //! Stable, typed, agent-facing API for driving a single sprite, addressed by
    //! the sprite entity's id. This is the AI-native front door to the sprite:
    //! every verb takes plain scalars (no math types to construct) and is
    //! forgiving (values are validated and clamped, never crash). It is the peer
    //! of the editor inspector over the same configuration; see
    //! Docs/design/ai-sprite-api.md.
    class DioramaSpriteRequests : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaSpriteRequests, DioramaSpriteRequestsTypeId);
        virtual ~DioramaSpriteRequests() = default;

        // Texture / appearance.
        //! Assign the texture by product path (e.g. "diorama/textures/hero.png").
        //! Returns false if the path does not resolve to an asset.
        virtual bool SetTextureByPath(AZStd::string_view productPath) = 0;
        //! Assign the optional normal map by product path (2D lighting v1b); empty
        //! clears it. Returns false if the path does not resolve to an asset. When
        //! set, the gem's 2D lights shape the sprite (best on billboards).
        virtual bool SetNormalMapByPath(AZStd::string_view productPath) = 0;
        //! Quad size in world units; negative values are clamped to zero.
        virtual void SetSize(float width, float height) = 0;
        //! Normalized pivot; clamped to 0..1.
        virtual void SetPivot(float x, float y) = 0;
        //! Tint multiplied into the texture; channels clamped to 0..1.
        virtual void SetTint(float r, float g, float b, float a) = 0;
        //! Hit-flash material (2D materials v1): blend the sprite toward (r,g,b) by
        //! amount (0..1) after lighting. Drive amount to 1 on a hit and ease back to
        //! 0. Channels and amount are clamped to 0..1.
        virtual void SetFlash(float r, float g, float b, float amount) = 0;
        //! Outline material (2D materials v1): draw a silhouette outline of color
        //! (r,g,b) with the given thickness (0 = off). Channels clamped to 0..1,
        //! thickness clamped non-negative.
        virtual void SetOutline(float r, float g, float b, float thickness) = 0;
        //! Emissive material (post-processing hook): add color (r,g,b) scaled by
        //! intensity to the lit result. intensity > 1 pushes the sprite above HDR 1.0
        //! so it blooms through a PostFxLayer + Bloom in the scene. 0 = off. Channels
        //! clamped 0..1, intensity clamped non-negative.
        virtual void SetEmissive(float r, float g, float b, float intensity) = 0;
        //! Always face the camera when true.
        virtual void SetBillboard(bool enabled) = 0;
        //! Visible from both sides when true (default); when false the sprite is
        //! hidden when viewed from behind.
        virtual void SetDoubleSided(bool enabled) = 0;
        //! Nearest-neighbor (point) texture filtering when true, so low-resolution
        //! pixel art stays crisp instead of being blurred; false (default) is linear.
        //! Pair it with a no-mipmap texture import preset for crisp results at any scale.
        virtual void SetPointFilter(bool enabled) = 0;

        // Atlas / UV region.
        //! Normalized texture sub-rectangle; each value clamped to 0..1.
        virtual void SetUVRegion(float uMin, float vMin, float uMax, float vMax) = 0;
        //! Mirror the sampled region horizontally and/or vertically.
        virtual void SetFlip(bool horizontal, bool vertical) = 0;
        //! Reflect the sampled region across its anti-diagonal (swap U/V). Combined
        //! with the flips this gives the four 90-degree rotations (best on square art).
        virtual void SetTranspose(bool transpose) = 0;

        // Layering (2.5D).
        //! Transparent draw-order bias; larger draws on top.
        virtual void SetSortOffset(float sortOffset) = 0;

        // Animation (sprite sheet).
        //! Uniform sheet grid; frameCount is clamped to columns * rows.
        virtual void SetFrameGrid(int columns, int rows, int frameCount) = 0;
        //! Enable or disable sprite-sheet playback.
        virtual void SetAnimationEnabled(bool enabled) = 0;
        //! Playback rate (frames per second) and whether the clip loops.
        virtual void SetPlayback(float framesPerSecond, bool loop) = 0;
        //! Time-scale the animation: 1 = normal, 0 = freeze (hit-stop), <1 = slow
        //! motion, >1 = fast-forward. Clamped non-negative.
        virtual void SetPlaybackSpeed(float speed) = 0;
        //! Frame shown first (and while not playing); clamped to range.
        virtual void SetStartFrame(int frame) = 0;
        //! Advance playback on the 2D Simulation Clock's fixed steps instead of the
        //! render tick (deterministic playback, snapshot/rewind friendly). With no
        //! clock in the level the render tick still advances as before.
        virtual void SetUseSimClock(bool enabled) = 0;
        //! Convenience: set the grid, set playback, and enable animation in one
        //! call (the common "play this sheet" intent).
        virtual void PlaySpriteSheet(int columns, int rows, int frameCount, float framesPerSecond, bool loop) = 0;

        // Query (verify loop).
        //! Resolved runtime state of the sprite. Safe to poll.
        virtual SpriteInfo GetSpriteInfo() = 0;
    };

    using DioramaSpriteRequestBus = AZ::EBus<DioramaSpriteRequests>;

    //! Notifications for event-driven agents that prefer subscribing over polling
    //! GetSpriteInfo. Addressed by the sprite entity's id.
    class DioramaSpriteNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(DioramaSpriteNotifications, DioramaSpriteNotificationsTypeId);
        virtual ~DioramaSpriteNotifications() = default;

        //! The texture finished loading (or changed and reloaded).
        virtual void OnTextureReady(const AZStd::string& productPath)
        {
            AZ_UNUSED(productPath);
        }
        //! Drawability changed: the sprite became visible or stopped being drawn.
        virtual void OnVisibilityChanged(bool visible)
        {
            AZ_UNUSED(visible);
        }
        //! A non-looping animation clip reached and held its last frame.
        virtual void OnAnimationFinished()
        {
        }
        //! The displayed animation frame advanced to frameIndex (sprite-sheet or
        //! Aseprite playback). Fires on every frame change, so games can trigger
        //! frame-exact logic: activate a hitbox, play a sound, open a cancel window.
        //! For Aseprite this is the document frame index; for a sprite sheet it is
        //! the sheet frame index.
        virtual void OnAnimationFrame(int frameIndex)
        {
            AZ_UNUSED(frameIndex);
        }
    };

    using DioramaSpriteNotificationBus = AZ::EBus<DioramaSpriteNotifications>;

    //! BehaviorContext handler so scripts/agents can subscribe to sprite
    //! notifications (e.g. wait for OnTextureReady) rather than poll.
    class DioramaSpriteNotificationHandler
        : public DioramaSpriteNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(
            DioramaSpriteNotificationHandler,
            "{7D519E4F-2C60-4FBD-E41A-803D5F617E93}",
            AZ::SystemAllocator,
            OnTextureReady,
            OnVisibilityChanged,
            OnAnimationFinished,
            OnAnimationFrame);

        void OnTextureReady(const AZStd::string& productPath) override
        {
            Call(FN_OnTextureReady, productPath);
        }
        void OnVisibilityChanged(bool visible) override
        {
            Call(FN_OnVisibilityChanged, visible);
        }
        void OnAnimationFinished() override
        {
            Call(FN_OnAnimationFinished);
        }
        void OnAnimationFrame(int frameIndex) override
        {
            Call(FN_OnAnimationFrame, frameIndex);
        }
    };

    //! Reflect the agent-facing sprite buses (request + notification) to the
    //! BehaviorContext. Called from the sprite component Reflect.
    void ReflectSpriteBuses(AZ::ReflectContext* context);
} // namespace Diorama
