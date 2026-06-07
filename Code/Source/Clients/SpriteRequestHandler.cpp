/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAssetUtils.h>
#include <Clients/SpritePresenter.h>
#include <Clients/SpriteRequestHandler.h>

#include <AzCore/Math/MathUtils.h>
#include <AzFramework/Asset/AssetCatalogBus.h>

namespace Diorama
{
    void SpriteRequestHandler::Connect(
        AZ::EntityId entityId, SpriteComponentConfig& config, SpritePresenter& presenter, ChangedCallback onChanged)
    {
        m_config = &config;
        m_presenter = &presenter;
        m_onChanged = AZStd::move(onChanged);
        m_connected = true;
        DioramaSpriteRequestBus::Handler::BusConnect(entityId);
    }

    void SpriteRequestHandler::Disconnect()
    {
        DioramaSpriteRequestBus::Handler::BusDisconnect();
        m_connected = false;
        m_config = nullptr;
        m_presenter = nullptr;
        m_onChanged = {};
    }

    void SpriteRequestHandler::NotifyChanged()
    {
        if (m_onChanged)
        {
            m_onChanged();
        }
    }

    bool SpriteRequestHandler::SetTextureByPath(AZStd::string_view productPath)
    {
        if (m_config == nullptr)
        {
            return false;
        }

        if (productPath.empty())
        {
            m_config->m_texture = {};
            NotifyChanged();
            return true;
        }

        const AZ::Data::AssetId assetId = ResolveStreamingImageAssetId(productPath);
        if (!assetId.IsValid())
        {
            return false;
        }

        m_config->m_texture = AZ::Data::Asset<AZ::RPI::StreamingImageAsset>(assetId, AZ::AzTypeInfo<AZ::RPI::StreamingImageAsset>::Uuid());
        m_config->m_texture.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
        NotifyChanged();
        return true;
    }

    bool SpriteRequestHandler::SetNormalMapByPath(AZStd::string_view productPath)
    {
        if (m_config == nullptr)
        {
            return false;
        }

        if (productPath.empty())
        {
            m_config->m_normalMap = {};
            NotifyChanged();
            return true;
        }

        const AZ::Data::AssetId assetId = ResolveStreamingImageAssetId(productPath);
        if (!assetId.IsValid())
        {
            return false;
        }

        m_config->m_normalMap =
            AZ::Data::Asset<AZ::RPI::StreamingImageAsset>(assetId, AZ::AzTypeInfo<AZ::RPI::StreamingImageAsset>::Uuid());
        m_config->m_normalMap.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
        NotifyChanged();
        return true;
    }

    void SpriteRequestHandler::SetSize(float width, float height)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_size = AZ::Vector2(AZ::GetMax(0.0f, width), AZ::GetMax(0.0f, height));
        NotifyChanged();
    }

    void SpriteRequestHandler::SetPivot(float x, float y)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_pivot = AZ::Vector2(AZ::GetClamp(x, 0.0f, 1.0f), AZ::GetClamp(y, 0.0f, 1.0f));
        NotifyChanged();
    }

    void SpriteRequestHandler::SetTint(float r, float g, float b, float a)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_tint =
            AZ::Color(AZ::GetClamp(r, 0.0f, 1.0f), AZ::GetClamp(g, 0.0f, 1.0f), AZ::GetClamp(b, 0.0f, 1.0f), AZ::GetClamp(a, 0.0f, 1.0f));
        NotifyChanged();
    }

    void SpriteRequestHandler::SetFlash(float r, float g, float b, float amount)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_flashColor = AZ::Color(AZ::GetClamp(r, 0.0f, 1.0f), AZ::GetClamp(g, 0.0f, 1.0f), AZ::GetClamp(b, 0.0f, 1.0f), 1.0f);
        m_config->m_flashAmount = AZ::GetClamp(amount, 0.0f, 1.0f);
        NotifyChanged();
    }

    void SpriteRequestHandler::SetOutline(float r, float g, float b, float thickness)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_outlineColor = AZ::Color(AZ::GetClamp(r, 0.0f, 1.0f), AZ::GetClamp(g, 0.0f, 1.0f), AZ::GetClamp(b, 0.0f, 1.0f), 1.0f);
        m_config->m_outlineThickness = thickness < 0.0f ? 0.0f : thickness;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetEmissive(float r, float g, float b, float intensity)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_emissiveColor = AZ::Color(AZ::GetClamp(r, 0.0f, 1.0f), AZ::GetClamp(g, 0.0f, 1.0f), AZ::GetClamp(b, 0.0f, 1.0f), 1.0f);
        m_config->m_emissiveIntensity = intensity < 0.0f ? 0.0f : intensity;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetBillboard(bool enabled)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_billboard = enabled;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetDoubleSided(bool enabled)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_doubleSided = enabled;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetPointFilter(bool enabled)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_pointFilter = enabled;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetUVRegion(float uMin, float vMin, float uMax, float vMax)
    {
        if (m_config == nullptr)
        {
            return;
        }
        uMin = AZ::GetClamp(uMin, 0.0f, 1.0f);
        vMin = AZ::GetClamp(vMin, 0.0f, 1.0f);
        uMax = AZ::GetClamp(uMax, 0.0f, 1.0f);
        vMax = AZ::GetClamp(vMax, 0.0f, 1.0f);
        // Keep min <= max so the region is well formed regardless of arg order.
        m_config->m_uvMin = AZ::Vector2(AZ::GetMin(uMin, uMax), AZ::GetMin(vMin, vMax));
        m_config->m_uvMax = AZ::Vector2(AZ::GetMax(uMin, uMax), AZ::GetMax(vMin, vMax));
        NotifyChanged();
    }

    void SpriteRequestHandler::SetFlip(bool horizontal, bool vertical)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_flipHorizontal = horizontal;
        m_config->m_flipVertical = vertical;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetTranspose(bool transpose)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_transpose = transpose;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetSortOffset(float sortOffset)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_sortOffset = sortOffset;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetFrameGrid(int columns, int rows, int frameCount)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_frameColumns = AZ::GetMax(1, columns);
        m_config->m_frameRows = AZ::GetMax(1, rows);
        // GetFrameCount clamps to columns * rows at read time; store the request
        // clamped to at least 1 here so the field is sane on its own.
        m_config->m_frameCount = AZ::GetMax(1, frameCount);
        NotifyChanged();
    }

    void SpriteRequestHandler::SetAnimationEnabled(bool enabled)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_animEnabled = enabled;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetPlayback(float framesPerSecond, bool loop)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_framesPerSecond = AZ::GetMax(0.0f, framesPerSecond);
        m_config->m_loop = loop;
        NotifyChanged();
    }

    void SpriteRequestHandler::SetPlaybackSpeed(float speed)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_playbackSpeed = AZ::GetMax(0.0f, speed);
        NotifyChanged();
    }

    void SpriteRequestHandler::SetStartFrame(int frame)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_startFrame = AZ::GetMax(0, frame);
        NotifyChanged();
    }

    void SpriteRequestHandler::PlaySpriteSheet(int columns, int rows, int frameCount, float framesPerSecond, bool loop)
    {
        if (m_config == nullptr)
        {
            return;
        }
        m_config->m_frameColumns = AZ::GetMax(1, columns);
        m_config->m_frameRows = AZ::GetMax(1, rows);
        m_config->m_frameCount = AZ::GetMax(1, frameCount);
        m_config->m_framesPerSecond = AZ::GetMax(0.0f, framesPerSecond);
        m_config->m_loop = loop;
        m_config->m_animEnabled = true;
        NotifyChanged();
    }

    SpriteInfo SpriteRequestHandler::GetSpriteInfo()
    {
        SpriteInfo info;
        if (m_config == nullptr)
        {
            return info;
        }

        const AZ::Data::AssetId textureId = m_config->m_texture.GetId();
        if (textureId.IsValid())
        {
            AZStd::string path;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(path, &AZ::Data::AssetCatalogRequests::GetAssetPathById, textureId);
            info.m_texturePath = path;
        }
        info.m_textureLoaded = m_config->m_texture.IsReady();
        info.m_width = m_config->m_size.GetX();
        info.m_height = m_config->m_size.GetY();
        info.m_pivotX = m_config->m_pivot.GetX();
        info.m_pivotY = m_config->m_pivot.GetY();
        info.m_sortOffset = m_config->m_sortOffset;
        info.m_billboard = m_config->m_billboard;
        info.m_doubleSided = m_config->m_doubleSided;
        info.m_pointFilter = m_config->m_pointFilter;
        info.m_flipHorizontal = m_config->m_flipHorizontal;
        info.m_flipVertical = m_config->m_flipVertical;
        info.m_animEnabled = m_config->m_animEnabled;
        info.m_frameCount = m_config->GetFrameCount();

        // Resolved runtime state from the presenter (drawability + current frame).
        if (m_presenter != nullptr)
        {
            info.m_visible = m_presenter->IsVisible();
            info.m_currentFrame = m_presenter->GetCurrentFrame();
        }
        return info;
    }
} // namespace Diorama
