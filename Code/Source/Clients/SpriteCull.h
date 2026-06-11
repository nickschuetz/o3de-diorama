/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Frustum.h>
#include <AzCore/Math/Vector3.h>

#include <cmath>

// Pure, header-only off-screen culling math for the sprite feature processor. The
// processor builds a view frustum once per frame and asks, per sprite, whether the
// sprite's bounding sphere could be on screen; sprites that cannot be are skipped
// from the batch (no vertex packing, no draw). It has no engine/render dependency
// beyond AzCore math, so the visibility decision is unit tested headlessly, the same
// pattern as TilemapCollision.h / SpriteBatchPlan.
namespace Diorama::SpriteCull
{
    //! Conservative bounding-sphere radius for a sprite quad of the given world size
    //! (full width/height) and uniform world scale. The sphere circumscribes the quad
    //! in any orientation (billboards rotate to face the camera each frame), so testing
    //! it never wrongly culls a visible sprite. A non-positive scale is treated as 1.
    inline float BoundingRadius(float width, float height, float worldScale)
    {
        const float scale = worldScale > 0.0f ? worldScale : 1.0f;
        return 0.5f * std::sqrt(width * width + height * height) * scale;
    }

    //! True when the bounding sphere (center, radius) is not entirely outside any of
    //! the view frustum's four SIDE planes (left/right/top/bottom). The near and far
    //! planes are deliberately ignored, so the result does NOT depend on the
    //! projection's reverse-depth convention. This keeps culling conservative: a
    //! sprite is culled only when it sits fully beyond a side plane (clearly off the
    //! left/right/top/bottom of the screen), never wrongly hidden. The cost is that a
    //! sprite directly behind the camera may not be culled (a minor wasted draw, not a
    //! visible error), which the batch already tolerates.
    inline bool IsVisible(const AZ::Frustum& frustum, const AZ::Vector3& center, float radius)
    {
        // Frustum plane normals point inward, so a point inside has a positive signed
        // distance to every plane; the sphere is fully outside a plane when its center
        // is more than `radius` on the negative side.
        const AZ::Frustum::PlaneId sides[4] = {
            AZ::Frustum::Left,
            AZ::Frustum::Right,
            AZ::Frustum::Top,
            AZ::Frustum::Bottom,
        };
        for (const AZ::Frustum::PlaneId id : sides)
        {
            if (frustum.GetPlane(id).GetPointDist(center) < -radius)
            {
                return false;
            }
        }
        return true;
    }
} // namespace Diorama::SpriteCull
