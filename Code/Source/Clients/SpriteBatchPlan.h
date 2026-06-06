/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/sort.h>

namespace Diorama::SpriteBatchPlan
{
    //! The minimal per-sprite key needed to decide batching. A batch is a run of
    //! sprites that share the same texture and sort layer and can therefore be
    //! drawn with one shared draw call and shader resource group.
    struct BatchKey
    {
        //! Texture identity. The full asset id is used (not a folded hash) so
        //! distinct textures can never collide into the same batch. A default
        //! (invalid) id means "no texture"; such items are dropped by Build.
        AZ::Data::AssetId m_textureId;
        //! 2.5D draw-order bias from SpriteComponentConfig::m_sortOffset. Kept as
        //! a float so fractional layers stay distinct (truncating to an integer
        //! would collapse 0.25 and 0.75 into one layer and wrongly merge them).
        float m_sortOffset = 0.0f;
        //! Optional normal-map identity. The normal map is bound per draw (one per
        //! batch), so sprites that differ only in normal map must not share a batch.
        //! Default (invalid) means "no normal map" (the flat v1a lighting path).
        AZ::Data::AssetId m_normalMapId;
        //! Hit-flash material (2D materials v1), bound per draw, so sprites that
        //! differ in flash must not share a batch. Default 0 amount = no flash.
        float m_flashAmount = 0.0f;
        AZ::u32 m_flashColor = 0xFFFFFFFFu;
        //! Outline material (2D materials v1), bound per draw like the flash.
        float m_outlineThickness = 0.0f;
        AZ::u32 m_outlineColor = 0xFFFFFFFFu;
        //! Emissive material (post-processing hook), bound per draw, so sprites that
        //! differ in emissive must not share a batch. Default 0 intensity = none.
        float m_emissiveIntensity = 0.0f;
        AZ::u32 m_emissiveColor = 0xFFFFFFFFu;
        //! Nearest-neighbor (point) texture filtering for crisp pixel art. The sampler
        //! is selected per draw (m_material.w in the shader), so a point-filtered
        //! sprite must not share a batch with a linear-filtered one. Default = linear.
        bool m_pointFilter = false;

        bool operator==(const BatchKey& other) const
        {
            return m_textureId == other.m_textureId && m_sortOffset == other.m_sortOffset && m_normalMapId == other.m_normalMapId &&
                m_flashAmount == other.m_flashAmount && m_flashColor == other.m_flashColor &&
                m_outlineThickness == other.m_outlineThickness && m_outlineColor == other.m_outlineColor &&
                m_emissiveIntensity == other.m_emissiveIntensity && m_emissiveColor == other.m_emissiveColor &&
                m_pointFilter == other.m_pointFilter;
        }
        bool operator!=(const BatchKey& other) const
        {
            return !(*this == other);
        }
    };

    //! An input sprite to be batched, tagged with its original index so the
    //! caller can map back to its full sprite record after sorting.
    struct Item
    {
        BatchKey m_key;
        AZ::u32 m_index = 0; //!< Index into the caller's sprite array.
        //! Squared distance to the camera. Used only when Build is called with
        //! byDepth=true: within a sort layer, sprites are ordered far-to-near so
        //! nearer ones draw on top (the sprite shader does not depth-test).
        float m_depth = 0.0f;
    };

    //! A contiguous run of items (after ordering) that draw together. m_begin and
    //! m_end are offsets into the ordered item list filled by Build.
    struct Batch
    {
        BatchKey m_key;
        AZ::u32 m_begin = 0; //!< First ordered-item offset in the batch.
        AZ::u32 m_end = 0; //!< One past the last ordered-item offset.

        AZ::u32 Count() const
        {
            return m_end - m_begin;
        }
    };

    //! Group sprites into draw batches. Pure function of its inputs, but writes
    //! into caller-owned output buffers (cleared and reused) so it allocates
    //! nothing on the steady-state path when called every frame.
    //!
    //! It copies the drawable items (texture id valid) into outOrdered, sorts
    //! them by (sortOffset, textureId), then collects maximal runs that share a
    //! BatchKey into outBatches. Sprites with an invalid texture id are dropped.
    //!
    //! With byDepth=true the secondary sort key is each item's m_depth (camera
    //! distance), ordered far-to-near within a sort layer, so nearer sprites draw
    //! on top (2.5D painter's order, since the sprite shader does not depth-test).
    //! Cross-layer order still follows m_sortOffset, so a lower-offset ground or
    //! background sprite stays behind regardless of distance.
    //!
    //! Stable ordering: ties keep their original relative order, so sprites that
    //! land in the same batch preserve registration order, which keeps draw
    //! results deterministic frame to frame.
    inline void Build(
        const AZStd::vector<Item>& items, AZStd::vector<Item>& outOrdered, AZStd::vector<Batch>& outBatches, bool byDepth = false)
    {
        outOrdered.clear();
        outBatches.clear();
        outOrdered.reserve(items.size());

        for (const Item& item : items)
        {
            if (item.m_key.m_textureId.IsValid())
            {
                outOrdered.push_back(item);
            }
        }

        AZStd::stable_sort(
            outOrdered.begin(),
            outOrdered.end(),
            [byDepth](const Item& lhs, const Item& rhs)
            {
                if (lhs.m_key.m_sortOffset != rhs.m_key.m_sortOffset)
                {
                    return lhs.m_key.m_sortOffset < rhs.m_key.m_sortOffset;
                }
                if (byDepth && lhs.m_depth != rhs.m_depth)
                {
                    return lhs.m_depth > rhs.m_depth; // farther from camera first -> drawn behind
                }
                if (lhs.m_key.m_textureId != rhs.m_key.m_textureId)
                {
                    return lhs.m_key.m_textureId < rhs.m_key.m_textureId;
                }
                if (lhs.m_key.m_normalMapId != rhs.m_key.m_normalMapId)
                {
                    return lhs.m_key.m_normalMapId < rhs.m_key.m_normalMapId;
                }
                if (lhs.m_key.m_flashAmount != rhs.m_key.m_flashAmount)
                {
                    return lhs.m_key.m_flashAmount < rhs.m_key.m_flashAmount;
                }
                if (lhs.m_key.m_flashColor != rhs.m_key.m_flashColor)
                {
                    return lhs.m_key.m_flashColor < rhs.m_key.m_flashColor;
                }
                if (lhs.m_key.m_outlineThickness != rhs.m_key.m_outlineThickness)
                {
                    return lhs.m_key.m_outlineThickness < rhs.m_key.m_outlineThickness;
                }
                if (lhs.m_key.m_outlineColor != rhs.m_key.m_outlineColor)
                {
                    return lhs.m_key.m_outlineColor < rhs.m_key.m_outlineColor;
                }
                if (lhs.m_key.m_emissiveIntensity != rhs.m_key.m_emissiveIntensity)
                {
                    return lhs.m_key.m_emissiveIntensity < rhs.m_key.m_emissiveIntensity;
                }
                if (lhs.m_key.m_emissiveColor != rhs.m_key.m_emissiveColor)
                {
                    return lhs.m_key.m_emissiveColor < rhs.m_key.m_emissiveColor;
                }
                return lhs.m_key.m_pointFilter < rhs.m_key.m_pointFilter;
            });

        for (AZ::u32 i = 0; i < outOrdered.size();)
        {
            AZ::u32 j = i + 1;
            while (j < outOrdered.size() && outOrdered[j].m_key == outOrdered[i].m_key)
            {
                ++j;
            }
            outBatches.push_back(Batch{ outOrdered[i].m_key, i, j });
            i = j;
        }
    }
} // namespace Diorama::SpriteBatchPlan
