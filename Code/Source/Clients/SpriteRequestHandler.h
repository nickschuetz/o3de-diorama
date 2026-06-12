/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/SpriteBus.h>
#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/std/functional.h>

namespace Diorama
{
    class SpritePresenter;

    //! Shared implementation of DioramaSpriteRequestBus used by both the runtime
    //! SpriteComponent and the editor EditorSpriteComponent, so the verb logic
    //! exists once. It edits the owning component's authoritative config in place,
    //! then calls an "on changed" callback the owner supplies (the runtime pushes
    //! to the presenter; the editor also refreshes the inspector). Resolved query
    //! fields (texture loaded, visible, current frame) come from the presenter the
    //! owner points it at.
    //!
    //! Lives in the runtime client library (no Qt, no AzToolsFramework) so the
    //! editor module reuses it through the shared private object library.
    class SpriteRequestHandler : public DioramaSpriteRequestBus::Handler
    {
    public:
        using ChangedCallback = AZStd::function<void()>;

        //! Begin handling requests for the given entity. config and presenter must
        //! outlive this handler (they are members of the owning component).
        void Connect(AZ::EntityId entityId, SpriteComponentConfig& config, SpritePresenter& presenter, ChangedCallback onChanged);
        void Disconnect();

        // DioramaSpriteRequests
        bool SetTextureByPath(AZStd::string_view productPath) override;
        bool SetNormalMapByPath(AZStd::string_view productPath) override;
        void SetSize(float width, float height) override;
        void SetPivot(float x, float y) override;
        void SetTint(float r, float g, float b, float a) override;
        void SetFlash(float r, float g, float b, float amount) override;
        void SetOutline(float r, float g, float b, float thickness) override;
        void SetEmissive(float r, float g, float b, float intensity) override;
        void SetBillboard(bool enabled) override;
        void SetDoubleSided(bool enabled) override;
        void SetPointFilter(bool enabled) override;
        void SetUVRegion(float uMin, float vMin, float uMax, float vMax) override;
        void SetFlip(bool horizontal, bool vertical) override;
        void SetTranspose(bool transpose) override;
        void SetSortOffset(float sortOffset) override;
        void SetFrameGrid(int columns, int rows, int frameCount) override;
        void SetAnimationEnabled(bool enabled) override;
        void SetPlayback(float framesPerSecond, bool loop) override;
        void SetPlaybackSpeed(float speed) override;
        void SetStartFrame(int frame) override;
        void SetUseSimClock(bool enabled) override;
        void PlaySpriteSheet(int columns, int rows, int frameCount, float framesPerSecond, bool loop) override;
        SpriteInfo GetSpriteInfo() override;

    private:
        //! Notify the owner that the config changed (push to presenter, refresh UI).
        void NotifyChanged();

        SpriteComponentConfig* m_config = nullptr;
        SpritePresenter* m_presenter = nullptr;
        ChangedCallback m_onChanged;
        bool m_connected = false;
    };
} // namespace Diorama
