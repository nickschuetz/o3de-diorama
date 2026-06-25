/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SpriteFeatureProcessor.h>
#include <Clients/SpritePresenter.h>
#include <Clients/SpriteTrail.h>
#include <Diorama/SpriteBus.h>

#include <Atom/RPI.Public/Scene.h>

namespace Diorama
{
    SpritePresenter::~SpritePresenter()
    {
        Disconnect();
    }

    void SpritePresenter::Connect(AZ::EntityId entityId, const SpriteComponentConfig& config)
    {
        if (m_connected)
        {
            Disconnect();
        }

        m_entityId = entityId;
        m_config = config;
        ResetAnimation();

        m_worldTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(m_worldTransform, m_entityId, &AZ::TransformBus::Events::GetWorldTM);

        m_connected = true;
        RefreshSimClockConnection();

        // Resolve the feature processor now if the scene already exists. If it
        // does not yet (common during level load / editor entity creation),
        // RefreshTickConnection keeps the tick bus connected so OnTick can retry
        // until the scene appears, rather than leaving the sprite invisible.
        TryAcquireFeatureProcessor();

        QueueTextureLoad();
        AZ::TransformNotificationBus::Handler::BusConnect(m_entityId);

        RefreshTickConnection();
        Push();
    }

    bool SpritePresenter::TryAcquireFeatureProcessor()
    {
        if (m_handle != 0)
        {
            return true;
        }

        AZ::RPI::Scene* scene = AZ::RPI::Scene::GetSceneForEntityId(m_entityId);
        if (scene == nullptr)
        {
            return false;
        }

        m_featureProcessor = scene->GetFeatureProcessor<SpriteFeatureProcessor>();
        if (m_featureProcessor == nullptr)
        {
            // Enable on demand the first time a sprite appears in this scene.
            m_featureProcessor = scene->EnableFeatureProcessor<SpriteFeatureProcessor>();
        }
        if (m_featureProcessor == nullptr)
        {
            return false;
        }

        m_handle = m_featureProcessor->AcquireSprite();
        return m_handle != 0;
    }

    void SpritePresenter::Disconnect()
    {
        if (!m_connected)
        {
            return;
        }
        DioramaSimTickNotificationBus::Handler::BusDisconnect();

        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AZ::Data::AssetBus::MultiHandler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        ReleaseTrail();
        if (m_featureProcessor != nullptr && m_handle != 0)
        {
            m_featureProcessor->ReleaseSprite(m_handle);
        }
        m_featureProcessor = nullptr;
        m_handle = 0;

        m_connected = false;
    }

    void SpritePresenter::SetConfig(const SpriteComponentConfig& config)
    {
        const bool textureChanged = config.m_texture.GetId() != m_config.m_texture.GetId();
        const bool normalChanged = config.m_normalMap.GetId() != m_config.m_normalMap.GetId();
        m_config = config;

        if (textureChanged || normalChanged)
        {
            AZ::Data::AssetBus::MultiHandler::BusDisconnect();
            QueueTextureLoad();
        }

        // An edit may have changed the grid or playback, so restart the clip and
        // re-evaluate whether this sprite needs to tick.
        ResetAnimation();
        RefreshTickConnection();
        RefreshSimClockConnection();
        Push();
    }

    bool SpritePresenter::IsVisible() const
    {
        // Drawn when connected to a feature processor with a handle and a ready
        // texture. Mirrors the visibility the feature processor applies.
        return m_connected && m_featureProcessor != nullptr && m_handle != 0 && m_config.m_texture.IsReady();
    }

    int SpritePresenter::GetCurrentFrame() const
    {
        return m_frameState.m_frame;
    }

    void SpritePresenter::QueueTextureLoad()
    {
        if (m_config.m_texture.GetId().IsValid())
        {
            m_config.m_texture.QueueLoad();
            AZ::Data::AssetBus::MultiHandler::BusConnect(m_config.m_texture.GetId());
        }
        // Also stream the optional normal map (2D lighting v1b); the feature
        // processor binds it once ready (it polls IsReady, but we re-push on ready
        // so its copy of the config sees the loaded asset).
        if (m_config.m_normalMap.GetId().IsValid())
        {
            m_config.m_normalMap.QueueLoad();
            AZ::Data::AssetBus::MultiHandler::BusConnect(m_config.m_normalMap.GetId());
        }
    }

    void SpritePresenter::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_worldTransform = world;
        Push();
    }

    void SpritePresenter::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        bool changed = false;
        if (asset.GetId() == m_config.m_texture.GetId())
        {
            m_config.m_texture = asset;
            changed = true;
        }
        if (asset.GetId() == m_config.m_normalMap.GetId())
        {
            m_config.m_normalMap = asset;
            changed = true;
        }
        if (changed)
        {
            Push();
        }
    }

    void SpritePresenter::Push()
    {
        if (!m_connected || m_featureProcessor == nullptr || m_handle == 0)
        {
            return;
        }

        m_featureProcessor->UpdateSprite(m_handle, m_worldTransform, BuildFrameConfig());
    }

    SpriteComponentConfig SpritePresenter::BuildFrameConfig() const
    {
        // Static sprite: the configuration as-is. Animated: override the UV region
        // with the current frame's cell (flips still apply through GetCornerUVs).
        if (!m_config.m_animEnabled)
        {
            return m_config;
        }
        SpriteComponentConfig frameConfig = m_config;
        m_config.GetFrameUVRegion(m_frameState.m_frame, frameConfig.m_uvMin, frameConfig.m_uvMax);
        return frameConfig;
    }

    void SpritePresenter::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        // If the scene was not ready at Connect, keep retrying until the feature
        // processor is available, then push the initial state and re-evaluate
        // whether we still need to tick.
        if (m_handle == 0)
        {
            if (TryAcquireFeatureProcessor())
            {
                Push();
                RefreshTickConnection();
            }
            return;
        }

        // Advance the animation unless a running simulation clock owns it (then
        // OnSimTick does). A non-animating sprite skips this but still updates its
        // afterimage trail below, so a static sprite can leave a dash trail.
        const bool simOwnsAdvance = m_config.m_useSimClock && DioramaSimClockRequestBus::HasHandlers();
        if (NeedsAnimationTick() && !simOwnsAdvance)
        {
            AdvanceAnimation(deltaTime);
        }

        // The trail is purely visual (not snapshotted), so it follows the render tick
        // at render rate regardless of sim-clock mode.
        UpdateTrail(deltaTime);
    }

    void SpritePresenter::RefreshSimClockConnection()
    {
        if (m_connected && m_config.m_useSimClock)
        {
            if (!DioramaSimTickNotificationBus::Handler::BusIsConnected())
            {
                DioramaSimTickNotificationBus::Handler::BusConnect();
            }
        }
        else
        {
            DioramaSimTickNotificationBus::Handler::BusDisconnect();
        }
    }

    void SpritePresenter::OnSimTick([[maybe_unused]] AZ::s64 frame, float stepSeconds)
    {
        // No draw-handle guard here: the deterministic advance (and the
        // OnAnimationFrame events that drive hitbox windows) must not depend on
        // renderer availability. Push() inside AdvanceAnimation no-ops safely.
        if (!NeedsAnimationTick())
        {
            return;
        }
        AdvanceAnimation(stepSeconds);
    }

    void SpritePresenter::AdvanceAnimation(float deltaSeconds)
    {
        const int previousFrame = m_frameState.m_frame;
        const bool wasFinished = m_frameState.m_finished;
        // Time-scale the step (0 = hit-stop freeze, <1 = slow motion).
        const float scaledDelta = deltaSeconds * (m_config.m_playbackSpeed < 0.0f ? 0.0f : m_config.m_playbackSpeed);
        m_frameState =
            SpriteAnimation::Advance(m_frameState, scaledDelta, m_config.m_framesPerSecond, m_config.GetFrameCount(), m_config.m_loop);

        if (m_frameState.m_frame != previousFrame)
        {
            Push();
            // Frame-exact hook for gameplay (hitbox activation, sfx, cancel windows).
            DioramaSpriteNotificationBus::Event(m_entityId, &DioramaSpriteNotifications::OnAnimationFrame, m_frameState.m_frame);
        }
        if (m_frameState.m_finished && !wasFinished)
        {
            DioramaSpriteNotificationBus::Event(m_entityId, &DioramaSpriteNotifications::OnAnimationFinished);
        }
    }

    void SpritePresenter::SetFrameState(const SpriteAnimation::FrameState& state)
    {
        m_frameState = state;
        Push(); // show the restored frame immediately
    }

    bool SpritePresenter::NeedsAnimationTick() const
    {
        return m_config.m_animEnabled && m_config.GetFrameCount() > 1 && m_config.m_framesPerSecond > 0.0f;
    }

    void SpritePresenter::RefreshTickConnection()
    {
        // Tick while animating, while a trail needs capturing, or while still waiting
        // to acquire a feature processor (so OnTick can retry the scene lookup).
        const bool shouldTick = m_connected && (NeedsAnimationTick() || TrailEnabled() || m_handle == 0);
        const bool isTicking = AZ::TickBus::Handler::BusIsConnected();

        if (shouldTick && !isTicking)
        {
            AZ::TickBus::Handler::BusConnect();
        }
        else if (!shouldTick && isTicking)
        {
            AZ::TickBus::Handler::BusDisconnect();
        }
    }

    bool SpritePresenter::TrailEnabled() const
    {
        return m_config.m_trailCount > 0;
    }

    void SpritePresenter::UpdateTrail(float deltaSeconds)
    {
        if (!TrailEnabled() || m_handle == 0 || m_featureProcessor == nullptr)
        {
            ReleaseTrail();
            return;
        }

        // Acquire one ghost handle per configured ghost (re-acquire if the count
        // changed at runtime).
        const size_t ghostCount = static_cast<size_t>(m_config.m_trailCount);
        if (m_trailHandles.size() != ghostCount)
        {
            ReleaseTrail();
            m_trailHandles.reserve(ghostCount);
            for (size_t i = 0; i < ghostCount; ++i)
            {
                m_trailHandles.push_back(m_featureProcessor->AcquireSprite());
            }
        }

        // Capture the current pose on the configured interval (catch-up capped at the
        // ghost count, since older captures fall off the ring anyway).
        const int captures =
            SpriteTrail::CapturesDue(m_trailAccumulator, deltaSeconds, m_config.m_trailInterval, static_cast<int>(ghostCount));
        for (int c = 0; c < captures; ++c)
        {
            TrailGhost ghost;
            ghost.m_transform = m_worldTransform;
            ghost.m_config = BuildFrameConfig();
            m_trailGhosts.insert(m_trailGhosts.begin(), AZStd::move(ghost));
        }
        if (m_trailGhosts.size() > ghostCount)
        {
            m_trailGhosts.resize(ghostCount);
        }

        // Draw each ghost behind the live sprite with its fading alpha; hide handles
        // with no captured pose yet (early in the trail's life).
        for (size_t i = 0; i < m_trailHandles.size(); ++i)
        {
            if (m_trailHandles[i] == 0)
            {
                continue;
            }
            if (i >= m_trailGhosts.size())
            {
                SpriteComponentConfig hidden;
                hidden.m_size = AZ::Vector2(0.0f, 0.0f);
                m_featureProcessor->UpdateSprite(m_trailHandles[i], AZ::Transform::CreateIdentity(), hidden);
                continue;
            }
            SpriteComponentConfig ghostConfig = m_trailGhosts[i].m_config;
            const float alpha = m_config.m_trailTint.GetA() *
                SpriteTrail::GhostAlpha(m_config.m_trailStartAlpha, m_config.m_trailFade, static_cast<int>(i));
            ghostConfig.m_tint = AZ::Color(m_config.m_trailTint.GetR(), m_config.m_trailTint.GetG(), m_config.m_trailTint.GetB(), alpha);
            // Ghosts never cast their own afterimages or carry a flash/outline.
            ghostConfig.m_trailCount = 0;
            ghostConfig.m_flashAmount = 0.0f;
            ghostConfig.m_outlineThickness = 0.0f;
            // Sit just behind the live sprite so the trail reads under it.
            ghostConfig.m_sortOffset = m_config.m_sortOffset - 0.01f * static_cast<float>(i + 1);
            m_featureProcessor->UpdateSprite(m_trailHandles[i], m_trailGhosts[i].m_transform, ghostConfig);
        }
    }

    void SpritePresenter::ReleaseTrail()
    {
        if (m_featureProcessor != nullptr)
        {
            for (AZ::u32 handle : m_trailHandles)
            {
                if (handle != 0)
                {
                    m_featureProcessor->ReleaseSprite(handle);
                }
            }
        }
        m_trailHandles.clear();
        m_trailGhosts.clear();
        m_trailAccumulator = 0.0f;
    }

    void SpritePresenter::ResetAnimation()
    {
        const int count = m_config.GetFrameCount();
        int start = m_config.m_startFrame;
        if (start < 0)
        {
            start = 0;
        }
        else if (start >= count)
        {
            start = count - 1;
        }

        m_frameState = SpriteAnimation::FrameState{ start, 0.0f, false };
    }
} // namespace Diorama
