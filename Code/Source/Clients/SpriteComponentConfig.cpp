/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/SpriteComponentConfig.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utils.h>

namespace Diorama
{
    void SpriteComponentConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SpriteComponentConfig, AZ::ComponentConfig>()
                ->Version(11)
                ->Field("Texture", &SpriteComponentConfig::m_texture)
                ->Field("NormalMap", &SpriteComponentConfig::m_normalMap)
                ->Field("FlashColor", &SpriteComponentConfig::m_flashColor)
                ->Field("FlashAmount", &SpriteComponentConfig::m_flashAmount)
                ->Field("OutlineColor", &SpriteComponentConfig::m_outlineColor)
                ->Field("OutlineThickness", &SpriteComponentConfig::m_outlineThickness)
                ->Field("EmissiveColor", &SpriteComponentConfig::m_emissiveColor)
                ->Field("EmissiveIntensity", &SpriteComponentConfig::m_emissiveIntensity)
                ->Field("Size", &SpriteComponentConfig::m_size)
                ->Field("Pivot", &SpriteComponentConfig::m_pivot)
                ->Field("Tint", &SpriteComponentConfig::m_tint)
                ->Field("Billboard", &SpriteComponentConfig::m_billboard)
                ->Field("DoubleSided", &SpriteComponentConfig::m_doubleSided)
                ->Field("PointFilter", &SpriteComponentConfig::m_pointFilter)
                ->Field("UvMin", &SpriteComponentConfig::m_uvMin)
                ->Field("UvMax", &SpriteComponentConfig::m_uvMax)
                ->Field("FlipHorizontal", &SpriteComponentConfig::m_flipHorizontal)
                ->Field("FlipVertical", &SpriteComponentConfig::m_flipVertical)
                ->Field("Transpose", &SpriteComponentConfig::m_transpose)
                ->Field("SortOffset", &SpriteComponentConfig::m_sortOffset)
                ->Field("TrailCount", &SpriteComponentConfig::m_trailCount)
                ->Field("TrailInterval", &SpriteComponentConfig::m_trailInterval)
                ->Field("TrailStartAlpha", &SpriteComponentConfig::m_trailStartAlpha)
                ->Field("TrailFade", &SpriteComponentConfig::m_trailFade)
                ->Field("TrailTint", &SpriteComponentConfig::m_trailTint)
                ->Field("AnimEnabled", &SpriteComponentConfig::m_animEnabled)
                ->Field("FrameColumns", &SpriteComponentConfig::m_frameColumns)
                ->Field("FrameRows", &SpriteComponentConfig::m_frameRows)
                ->Field("FrameCount", &SpriteComponentConfig::m_frameCount)
                ->Field("FramesPerSecond", &SpriteComponentConfig::m_framesPerSecond)
                ->Field("PlaybackSpeed", &SpriteComponentConfig::m_playbackSpeed)
                ->Field("UseSimClock", &SpriteComponentConfig::m_useSimClock)
                ->Field("Loop", &SpriteComponentConfig::m_loop)
                ->Field("StartFrame", &SpriteComponentConfig::m_startFrame);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<SpriteComponentConfig>("Sprite Configuration", "Settings for a world-space sprite")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_texture, "Texture", "Texture drawn on the sprite quad")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_normalMap,
                        "Normal Map",
                        "Optional tangent-space normal map; when set, 2D lights shape the art (billboards)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color,
                        &SpriteComponentConfig::m_flashColor,
                        "Flash Color",
                        "Color blended in by the hit-flash effect")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &SpriteComponentConfig::m_flashAmount,
                        "Flash Amount",
                        "0 = no flash; 1 = full flash color (drive on a hit)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color,
                        &SpriteComponentConfig::m_outlineColor,
                        "Outline Color",
                        "Color of the silhouette outline")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_outlineThickness,
                        "Outline Thickness",
                        "0 = no outline; larger draws a thicker silhouette outline")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::SoftMax, 4.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color,
                        &SpriteComponentConfig::m_emissiveColor,
                        "Emissive Color",
                        "Color the sprite emits (matches the SetEmissive bus verb)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_emissiveIntensity,
                        "Emissive Intensity",
                        "0 = none; values above 1 push the sprite past HDR 1.0 so it blooms with Atom post (see how-to 14-glow)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::SoftMax, 8.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_size, "Size", "Quad size in world units (width, height)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Suffix, " m")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_pivot,
                        "Pivot",
                        "Normalized pivot within the quad (0.5, 0.5 is centered)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color,
                        &SpriteComponentConfig::m_tint,
                        "Tint",
                        "Color multiplied into the texture; alpha controls transparency")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_billboard, "Billboard", "Always face the camera")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_doubleSided,
                        "Double Sided",
                        "Visible from both sides; turn off to hide the sprite when viewed from behind")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_pointFilter,
                        "Point Filter (pixel art)",
                        "Nearest-neighbor sampling so low-res pixel art stays crisp instead of being blurred. "
                        "Pair it with a no-mipmap texture import preset (e.g. UserInterface_Lossless) for crisp results at any scale")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Atlas / UV Region")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_uvMin,
                        "UV Min",
                        "Top-left of the sampled texture sub-rectangle (0-1)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_uvMax,
                        "UV Max",
                        "Bottom-right of the sampled texture sub-rectangle (0-1)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_flipHorizontal,
                        "Flip Horizontal",
                        "Mirror the sampled region horizontally")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_flipVertical,
                        "Flip Vertical",
                        "Mirror the sampled region vertically")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_transpose,
                        "Transpose",
                        "Reflect across the anti-diagonal (swap U/V); with the flips this gives 90-degree rotations")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Layering")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_sortOffset,
                        "Sort Offset",
                        "Transparent draw-order bias; larger draws on top")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Afterimage Trail")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_trailCount,
                        "Trail Ghosts",
                        "Number of fading ghost copies drawn behind the sprite (0 = off); the dash / super trail")
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_trailInterval,
                        "Trail Interval",
                        "Seconds between captured ghost poses (the spacing of the trail)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &SpriteComponentConfig::m_trailStartAlpha,
                        "Trail Start Alpha",
                        "Alpha of the freshest ghost")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &SpriteComponentConfig::m_trailFade,
                        "Trail Fade",
                        "Geometric alpha falloff per older ghost")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Color,
                        &SpriteComponentConfig::m_trailTint,
                        "Trail Tint",
                        "Color the ghosts are drawn in (white shows the sprite faded)")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Animation")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_animEnabled,
                        "Animated",
                        "Play frames from a sprite sheet on a timer")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_frameColumns, "Columns", "Sprite-sheet columns")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_frameRows, "Rows", "Sprite-sheet rows")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_frameCount,
                        "Frame Count",
                        "Number of frames played (allows a partial last row)")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_framesPerSecond, "Frames Per Second", "Playback rate")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_playbackSpeed,
                        "Playback Speed",
                        "Time-scale multiplier (1 = normal, 0 = freeze / hit-stop, <1 = slow motion)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Suffix, " fps")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SpriteComponentConfig::m_useSimClock,
                        "Use Simulation Clock",
                        "Advance the animation on the 2D Simulation Clock's fixed steps (deterministic); falls back to the render tick "
                        "when no clock runs")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_loop, "Loop", "Loop or hold the last frame")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SpriteComponentConfig::m_startFrame, "Start Frame", "Frame shown first")
                    ->Attribute(AZ::Edit::Attributes::Min, 0);
            }
        }
    }

    void SpriteComponentConfig::GetCornerUVs(float outU[4], float outV[4]) const
    {
        // Corner order matches SpriteRenderer's vertices and indices:
        // 0 bottom-left, 1 bottom-right, 2 top-right, 3 top-left. Texture V increases
        // downward, so the bottom corners use vMax.
        const float uMin = m_uvMin.GetX();
        const float uMax = m_uvMax.GetX();
        const float vMin = m_uvMin.GetY();
        const float vMax = m_uvMax.GetY();

        outU[0] = uMin;
        outV[0] = vMax; // bottom-left
        outU[1] = uMax;
        outV[1] = vMax; // bottom-right
        outU[2] = uMax;
        outV[2] = vMin; // top-right
        outU[3] = uMin;
        outV[3] = vMin; // top-left

        // Orientation as corner-UV permutations, in Tiled's order: anti-diagonal
        // (transpose) first, then horizontal, then vertical. For flip-only cases this
        // is identical to swapping the U / V ranges; transpose adds the remaining
        // square symmetries (so transpose + a flip is a 90-degree rotation).
        const auto swapCorners = [&](int a, int b)
        {
            AZStd::swap(outU[a], outU[b]);
            AZStd::swap(outV[a], outV[b]);
        };
        if (m_transpose)
        {
            swapCorners(1, 3); // reflect across the bottom-left -> top-right diagonal
        }
        if (m_flipHorizontal)
        {
            swapCorners(0, 1);
            swapCorners(3, 2);
        }
        if (m_flipVertical)
        {
            swapCorners(0, 3);
            swapCorners(1, 2);
        }
    }

    int SpriteComponentConfig::GetFrameColumns() const
    {
        return m_frameColumns < 1 ? 1 : m_frameColumns;
    }

    int SpriteComponentConfig::GetFrameRows() const
    {
        return m_frameRows < 1 ? 1 : m_frameRows;
    }

    int SpriteComponentConfig::GetFrameCount() const
    {
        const int maxFrames = GetFrameColumns() * GetFrameRows();
        if (m_frameCount < 1)
        {
            return 1;
        }
        return m_frameCount > maxFrames ? maxFrames : m_frameCount;
    }

    void SpriteComponentConfig::GetFrameUVRegion(int frameIndex, AZ::Vector2& outMin, AZ::Vector2& outMax) const
    {
        const int columns = GetFrameColumns();
        const int rows = GetFrameRows();
        const int count = GetFrameCount();

        // Clamp the requested frame into the valid range.
        if (frameIndex < 0)
        {
            frameIndex = 0;
        }
        else if (frameIndex >= count)
        {
            frameIndex = count - 1;
        }

        // Frames fill the grid left-to-right, top-to-bottom, inside the base region.
        const int column = frameIndex % columns;
        const int row = frameIndex / columns;

        const float cellWidth = (m_uvMax.GetX() - m_uvMin.GetX()) / static_cast<float>(columns);
        const float cellHeight = (m_uvMax.GetY() - m_uvMin.GetY()) / static_cast<float>(rows);

        outMin.Set(m_uvMin.GetX() + cellWidth * column, m_uvMin.GetY() + cellHeight * row);
        outMax.Set(m_uvMin.GetX() + cellWidth * (column + 1), m_uvMin.GetY() + cellHeight * (row + 1));
    }
} // namespace Diorama
