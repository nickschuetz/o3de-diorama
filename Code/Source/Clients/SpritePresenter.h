/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/SpriteAnimation.h>
#include <Diorama/DioramaSimClockBus.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/vector.h>

namespace Diorama
{
    class SpriteFeatureProcessor;

    //! Shared helper that connects a sprite (runtime or editor) to the gem's
    //! SpriteRenderer. It registers a draw handle, tracks the owning entity's
    //! world transform, requests the texture load, and pushes updates to the
    //! renderer. Both SpriteComponent (runtime) and EditorSpriteComponent embed
    //! one of these so the sprite draws identically in game and in the editor
    //! viewport, with no duplicated bus logic.
    //!
    //! This type lives in the runtime client code (no Qt, no AzToolsFramework),
    //! so the editor module reuses it through the shared private object library.
    class SpritePresenter final
        : private AZ::TransformNotificationBus::Handler
        , private AZ::Data::AssetBus::MultiHandler
        , private AZ::TickBus::Handler
        , private DioramaSimTickNotificationBus::Handler
    {
    public:
        AZ_TYPE_INFO(Diorama::SpritePresenter, "{6F2E9B8A-2D7C-4C1E-9D3A-7E5B0C4F1A22}");

        SpritePresenter() = default;
        ~SpritePresenter();

        //! Begin presenting the sprite for the given entity with the given config.
        //! Safe to call from a component Activate().
        void Connect(AZ::EntityId entityId, const SpriteComponentConfig& config);

        //! Stop presenting and release the renderer handle. Safe from Deactivate().
        void Disconnect();

        //! Replace the configuration (for example after an editor property edit)
        //! and push the change to the renderer, reloading the texture if it
        //! changed.
        void SetConfig(const SpriteComponentConfig& config);

        //! Resolved runtime state for the AI query API (GetSpriteInfo).
        //! True when the sprite is registered with a feature processor and its
        //! texture is ready, i.e. it is actually being drawn.
        bool IsVisible() const;
        //! Current animation frame (0 when not animating).
        int GetCurrentFrame() const;

    private:
        // AZ::TransformNotificationBus::Handler
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        // AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSimTickNotifications (Use Simulation Clock mode)
        void OnSimTick(AZ::s64 frame, float stepSeconds) override;

    public:
        //! Frame-state access for the snapshot/restore contract (frame, timer,
        //! finished); restore re-pushes and refires nothing (state-only).
        SpriteAnimation::FrameState GetFrameState() const
        {
            return m_frameState;
        }
        void SetFrameState(const SpriteAnimation::FrameState& state);

    private:
        //! Advance the sprite-sheet animation by `deltaSeconds` (shared by the
        //! render tick and the fixed sim tick) and push/notify on frame changes.
        void AdvanceAnimation(float deltaSeconds);

        void QueueTextureLoad();
        void Push();
        //! Build the config to draw this frame: the base config, or (when animating)
        //! a copy with the current frame's UV region. Shared by Push and trail capture.
        SpriteComponentConfig BuildFrameConfig() const;

        //! True when an afterimage trail is configured (one or more ghosts).
        bool TrailEnabled() const;
        //! Advance the afterimage trail by `deltaSeconds`: capture a ghost pose on the
        //! interval and redraw the ghost quads with their fading alpha. Acquires the
        //! ghost handles on first use and releases them when the trail is turned off.
        void UpdateTrail(float deltaSeconds);
        //! Release the trail's renderer handles and clear its captured poses.
        void ReleaseTrail();

        //! Connect or disconnect the tick bus to match current needs: a sprite
        //! ticks if it is animating, or if it has not yet acquired a feature
        //! processor (so it can keep retrying until its scene exists).
        void RefreshTickConnection();
        //! Connect or disconnect the fixed sim tick bus to match the config's
        //! Use Simulation Clock flag (a live SetUseSimClock lands here via SetConfig).
        void RefreshSimClockConnection();
        //! Reset playback to the configured start frame.
        void ResetAnimation();

        //! Try to resolve the entity's scene feature processor and acquire a draw
        //! handle. Safe to call repeatedly; a no-op once a handle is held. Returns
        //! true once the sprite is registered. The scene may not exist yet at
        //! Activate (level load / editor ordering), so this is retried per tick
        //! until it succeeds.
        bool TryAcquireFeatureProcessor();

        //! True while the animation clip needs per-frame stepping.
        bool NeedsAnimationTick() const;

        AZ::EntityId m_entityId;
        SpriteComponentConfig m_config;
        AZ::Transform m_worldTransform = AZ::Transform::CreateIdentity();
        SpriteAnimation::FrameState m_frameState;
        //! Scene feature processor that owns this sprite's draw data. Resolved
        //! lazily (see TryAcquireFeatureProcessor) and held as a non-owning
        //! pointer for the presenter's connected lifetime.
        //!
        //! KNOWN LIMITATION: this assumes the scene (and its feature processor)
        //! outlives the sprite component, which holds for normal entity/level
        //! teardown order (components deactivate before their scene is destroyed).
        //! If a scene were torn down while a sprite stayed connected, this would
        //! dangle. A fully robust fix would observe SceneNotificationBus /
        //! feature-processor lifetime and null this out; deferred until a use case
        //! actually exercises mid-lifetime scene destruction.
        SpriteFeatureProcessor* m_featureProcessor = nullptr;
        AZ::u32 m_handle = 0; // SpriteFeatureProcessor::InvalidHandle
        bool m_connected = false;

        // Afterimage trail: one renderer handle per ghost plus a ring of recent poses
        // (transform + frame config), captured on the configured interval and drawn
        // with a fading alpha behind the live sprite. Held only while a trail is
        // configured, so a sprite with no trail costs the renderer nothing.
        struct TrailGhost
        {
            AZ::Transform m_transform = AZ::Transform::CreateIdentity();
            SpriteComponentConfig m_config;
        };
        AZStd::vector<AZ::u32> m_trailHandles;
        AZStd::vector<TrailGhost> m_trailGhosts; //!< front = freshest capture
        float m_trailAccumulator = 0.0f;
    };
} // namespace Diorama
