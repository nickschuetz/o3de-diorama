/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SpriteFeatureProcessor.h>
#include <Clients/SpritePresenter.h>
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

        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AZ::Data::AssetBus::MultiHandler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

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

        // Static sprite: send the configuration as-is, no copy.
        if (!m_config.m_animEnabled)
        {
            m_featureProcessor->UpdateSprite(m_handle, m_worldTransform, m_config);
            return;
        }

        // Animated sprite: override the UV region with the current frame's cell.
        // Flips still apply on top through the renderer's GetCornerUVs.
        SpriteComponentConfig frameConfig = m_config;
        m_config.GetFrameUVRegion(m_frameState.m_frame, frameConfig.m_uvMin, frameConfig.m_uvMax);
        m_featureProcessor->UpdateSprite(m_handle, m_worldTransform, frameConfig);
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

        if (!NeedsAnimationTick())
        {
            return;
        }

        const int previousFrame = m_frameState.m_frame;
        const bool wasFinished = m_frameState.m_finished;
        // Time-scale the step (0 = hit-stop freeze, <1 = slow motion).
        const float scaledDelta = deltaTime * (m_config.m_playbackSpeed < 0.0f ? 0.0f : m_config.m_playbackSpeed);
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

    bool SpritePresenter::NeedsAnimationTick() const
    {
        return m_config.m_animEnabled && m_config.GetFrameCount() > 1 && m_config.m_framesPerSecond > 0.0f;
    }

    void SpritePresenter::RefreshTickConnection()
    {
        // Tick while animating, or while still waiting to acquire a feature
        // processor (so OnTick can retry the scene lookup).
        const bool shouldTick = m_connected && (NeedsAnimationTick() || m_handle == 0);
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
