/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/ReflectContext.h>

#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace Diorama
{
    //! Shared configuration for a world-space sprite. The same struct is used by
    //! the runtime SpriteComponent and the EditorSpriteComponent so that the
    //! editor can author values and hand an identical configuration to the
    //! runtime component through BuildGameEntity.
    class SpriteComponentConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(SpriteComponentConfig, SpriteComponentConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(SpriteComponentConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        virtual ~SpriteComponentConfig() = default;

        //! Texture drawn on the sprite quad. NoLoad keeps the asset reference
        //! cheap until the component activates and explicitly requests the load.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_texture{ AZ::Data::AssetLoadBehavior::PreLoad };

        //! Optional tangent-space normal map (2D dynamic lighting v1b). When set, the
        //! gem's 2D lights shape the flat art (Lambertian N.L) instead of only
        //! attenuating by distance; when unset, lighting is the flat v1a path. Best
        //! suited to billboarded sprites (the lighting uses the camera basis). Left
        //! NoLoad until the component activates and requests it.
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_normalMap{ AZ::Data::AssetLoadBehavior::PreLoad };

        //! Hit-flash material effect (2D materials v1). m_flashAmount in 0..1 blends
        //! the sprite toward m_flashColor after lighting, for a hit/damage flash;
        //! 0 (default) is no flash, the unchanged look. Drive it from script (e.g.
        //! flash to 1 on a hit, then ease back to 0).
        AZ::Color m_flashColor = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
        float m_flashAmount = 0.0f;

        //! Outline material effect (2D materials v1). m_outlineThickness > 0 draws an
        //! outline of m_outlineColor around the sprite's opaque silhouette (in the
        //! transparent fringe of the quad), for selection/hit highlights. 0 = off.
        //! Thickness is a screen-relative multiplier (uses the UV derivative), so it
        //! stays roughly constant on screen regardless of texture resolution.
        AZ::Color m_outlineColor = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
        float m_outlineThickness = 0.0f;

        //! Emissive material effect (2D post-processing hook). m_emissiveColor scaled
        //! by m_emissiveIntensity is added to the lit color, so an intensity > 1 pushes
        //! the sprite above HDR 1.0 and it blooms through Atom's post-process pass
        //! (add a PostFxLayer + Bloom to the scene). 0 (default) = no emissive.
        AZ::Color m_emissiveColor = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
        float m_emissiveIntensity = 0.0f;

        //! Size of the quad in world units (width, height).
        AZ::Vector2 m_size = AZ::Vector2(1.0f, 1.0f);

        //! Normalized pivot within the quad. (0.5, 0.5) centers the sprite on the
        //! entity origin; (0.5, 0.0) anchors it at the bottom edge.
        AZ::Vector2 m_pivot = AZ::Vector2(0.5f, 0.5f);

        //! Color multiplied into the sampled texture. Alpha drives transparency.
        AZ::Color m_tint = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);

        //! When true the quad always faces the camera. When false it is oriented
        //! by the entity transform (the quad spans the entity local X and Z axes).
        bool m_billboard = false;

        //! When true the sprite is visible from both sides (the renderer also
        //! emits back-facing triangles). When false only the front face draws and
        //! the sprite disappears when viewed from behind. Defaults to true because
        //! a flat 2D sprite reads as a double-sided card; turn it off for effects
        //! that should only be seen from the front. Has no visible effect on a
        //! billboard, which always faces the camera.
        bool m_doubleSided = true;

        //! Use nearest-neighbor (point) texture filtering instead of the default
        //! linear filtering. Turn this on for pixel art so low-resolution textures
        //! stay crisp instead of being bilinearly smoothed (no need to pre-upscale
        //! them in another program). Off (default) keeps linear, which suits
        //! photographic or high-resolution art.
        bool m_pointFilter = false;

        //! Normalized top-left of the texture sub-rectangle to sample. Together
        //! with m_uvMax this selects a region of a texture atlas or sprite sheet.
        //! The default (0,0)-(1,1) samples the whole texture.
        AZ::Vector2 m_uvMin = AZ::Vector2(0.0f, 0.0f);

        //! Normalized bottom-right of the sampled sub-rectangle. See m_uvMin.
        AZ::Vector2 m_uvMax = AZ::Vector2(1.0f, 1.0f);

        //! Mirror the sampled region horizontally (useful for facing direction).
        bool m_flipHorizontal = false;

        //! Mirror the sampled region vertically.
        bool m_flipVertical = false;

        //! Reflect the sampled region across its anti-diagonal (swap the U and V
        //! axes). Applied before the flips, matching Tiled's diagonal-flip flag, so
        //! transpose combined with the two flips yields all four 90-degree rotations
        //! and mirrors of a (square) cell. Best on square sprites/tiles; a non-square
        //! region transposed maps onto the quad rotated.
        bool m_transpose = false;

        //! Additional sort bias for transparent draw ordering. Larger values draw
        //! later (on top). Use this to layer overlapping sprites in 2.5D scenes
        //! without moving them in depth.
        float m_sortOffset = 0.0f;

        //! Afterimage trail: number of fading ghost copies of recent poses drawn
        //! behind the sprite (0 = off). The classic dash / super-activation trail.
        //! Ghosts are captured on m_trailInterval and drawn through the sprite batch
        //! path, behind the live sprite, each fainter than the last.
        int m_trailCount = 0;
        //! Seconds between captured ghost poses (the spacing of the trail).
        float m_trailInterval = 0.05f;
        //! Alpha of the freshest ghost (0..1); older ghosts fall off by m_trailFade.
        float m_trailStartAlpha = 0.5f;
        //! Geometric alpha falloff applied per older ghost (0..1).
        float m_trailFade = 0.6f;
        //! Color the ghosts are drawn in (white shows the sprite's own texture faded).
        AZ::Color m_trailTint = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);

        //! Play frames from a sprite sheet on a timer. When false the sprite is
        //! static and the animation fields below are ignored.
        bool m_animEnabled = false;

        //! Sprite-sheet grid. Frames are laid out left-to-right, top-to-bottom in
        //! a uniform grid of m_frameColumns by m_frameRows cells inside the
        //! [m_uvMin, m_uvMax] region, so animation composes with an atlas sub-rect.
        int m_frameColumns = 1;
        int m_frameRows = 1;

        //! Number of active frames played, allowing a partial last row. Clamped to
        //! [1, m_frameColumns * m_frameRows].
        int m_frameCount = 1;

        //! Playback rate in frames per second.
        float m_framesPerSecond = 12.0f;

        //! Time-scale multiplier for playback (1 = normal). 0 freezes the animation
        //! in place (hit-stop), < 1 is slow motion, > 1 fast-forward. Clamped
        //! non-negative. Mirrors the Aseprite component's speed control.
        float m_playbackSpeed = 1.0f;
        //! Advance the sprite-sheet animation on the 2D Simulation Clock's fixed
        //! steps instead of the render tick (deterministic playback; falls back to
        //! the render tick when no clock is running, so editor previews still play).
        bool m_useSimClock = false;

        //! Loop back to the first frame after the last, or hold the last frame.
        bool m_loop = true;

        //! Frame shown first (and in the editor when not playing). Clamped to range.
        int m_startFrame = 0;

        //! Compute the effective UV coordinates for the four quad corners in
        //! winding order (bottom-left, bottom-right, top-right, top-left),
        //! applying the sub-rectangle and the flip flags. Output layout matches
        //! the renderer's vertex order.
        void GetCornerUVs(float outU[4], float outV[4]) const;

        //! Normalized columns/rows clamped to at least 1.
        int GetFrameColumns() const;
        int GetFrameRows() const;
        //! Active frame count clamped to [1, columns * rows].
        int GetFrameCount() const;

        //! Sub-rectangle of the texture for a given frame index, expressed in the
        //! same normalized space as m_uvMin/m_uvMax. The frame grid lives inside
        //! [m_uvMin, m_uvMax]. frameIndex is clamped to the valid range.
        void GetFrameUVRegion(int frameIndex, AZ::Vector2& outMin, AZ::Vector2& outMax) const;
    };
} // namespace Diorama
