/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/SpriteBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void SpriteInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SpriteInfo>()
                ->Version(1)
                ->Field("texturePath", &SpriteInfo::m_texturePath)
                ->Field("textureLoaded", &SpriteInfo::m_textureLoaded)
                ->Field("visible", &SpriteInfo::m_visible)
                ->Field("width", &SpriteInfo::m_width)
                ->Field("height", &SpriteInfo::m_height)
                ->Field("pivotX", &SpriteInfo::m_pivotX)
                ->Field("pivotY", &SpriteInfo::m_pivotY)
                ->Field("sortOffset", &SpriteInfo::m_sortOffset)
                ->Field("billboard", &SpriteInfo::m_billboard)
                ->Field("doubleSided", &SpriteInfo::m_doubleSided)
                ->Field("pointFilter", &SpriteInfo::m_pointFilter)
                ->Field("flipHorizontal", &SpriteInfo::m_flipHorizontal)
                ->Field("flipVertical", &SpriteInfo::m_flipVertical)
                ->Field("animEnabled", &SpriteInfo::m_animEnabled)
                ->Field("currentFrame", &SpriteInfo::m_currentFrame)
                ->Field("frameCount", &SpriteInfo::m_frameCount)
                ->Field("useSimClock", &SpriteInfo::m_useSimClock);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<SpriteInfo>("DioramaSpriteInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/Sprite")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                // An *Info struct is a returned snapshot, so each field is reflected
                // read-only (a getter, no setter): readable from Lua / Python /
                // Script Canvas, listed once (a setter would add a duplicate node).
                ->Property("texturePath", BehaviorValueGetter(&SpriteInfo::m_texturePath), nullptr)
                ->Property("textureLoaded", BehaviorValueGetter(&SpriteInfo::m_textureLoaded), nullptr)
                ->Property("visible", BehaviorValueGetter(&SpriteInfo::m_visible), nullptr)
                ->Property("width", BehaviorValueGetter(&SpriteInfo::m_width), nullptr)
                ->Property("height", BehaviorValueGetter(&SpriteInfo::m_height), nullptr)
                ->Property("pivotX", BehaviorValueGetter(&SpriteInfo::m_pivotX), nullptr)
                ->Property("pivotY", BehaviorValueGetter(&SpriteInfo::m_pivotY), nullptr)
                ->Property("sortOffset", BehaviorValueGetter(&SpriteInfo::m_sortOffset), nullptr)
                ->Property("billboard", BehaviorValueGetter(&SpriteInfo::m_billboard), nullptr)
                ->Property("doubleSided", BehaviorValueGetter(&SpriteInfo::m_doubleSided), nullptr)
                ->Property("pointFilter", BehaviorValueGetter(&SpriteInfo::m_pointFilter), nullptr)
                ->Property("flipHorizontal", BehaviorValueGetter(&SpriteInfo::m_flipHorizontal), nullptr)
                ->Property("flipVertical", BehaviorValueGetter(&SpriteInfo::m_flipVertical), nullptr)
                ->Property("animEnabled", BehaviorValueGetter(&SpriteInfo::m_animEnabled), nullptr)
                ->Property("currentFrame", BehaviorValueGetter(&SpriteInfo::m_currentFrame), nullptr)
                ->Property("frameCount", BehaviorValueGetter(&SpriteInfo::m_frameCount), nullptr)
                ->Property("useSimClock", BehaviorValueGetter(&SpriteInfo::m_useSimClock), nullptr);
        }
    }

    //! Reflect the agent-facing sprite buses to the BehaviorContext so they are
    //! callable from Python/script with named, documented verbs. Called from the
    //! sprite component's Reflect.
    void ReflectSpriteBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext
            ->EBus<DioramaSpriteRequestBus>("DioramaSpriteRequestBus")
            // Common scope so the bus is exported to the editor Python bindings
            // (azlmbr) as well as runtime script, which is what makes it
            // agent-drivable; without it the bus is reflected but not callable
            // from Python.
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Sprite")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            // Each Event passes per-argument {name, tooltip} metadata. This names
            // and documents every argument in the BehaviorContext, where it
            // surfaces in Script Canvas node pins and is readable by any tool
            // that introspects the context (GetArgumentName / GetArgumentToolTip)
            // to build an agent-facing schema. Note the editor's generated Python
            // stub (azlmbr/diorama.pyi) lists EBus event arguments by type only,
            // so this metadata does not reach that stub; it is the C++ reflection,
            // not the .pyi, that carries the names.
            ->Event(
                "SetTextureByPath",
                &DioramaSpriteRequestBus::Events::SetTextureByPath,
                { { { "productPath",
                      "Texture product path, e.g. 'diorama/textures/hero.png'. Returns false if it does not resolve to an asset." } } })
            ->Event(
                "SetNormalMapByPath",
                &DioramaSpriteRequestBus::Events::SetNormalMapByPath,
                { { { "productPath",
                      "Normal map product path (2D lighting v1b); empty clears it. Returns false if it does not resolve to an asset." } } })
            ->Event(
                "SetSize",
                &DioramaSpriteRequestBus::Events::SetSize,
                { { { "width", "Quad width in world units; negative is clamped to zero." },
                    { "height", "Quad height in world units; negative is clamped to zero." } } })
            ->Event(
                "SetPivot",
                &DioramaSpriteRequestBus::Events::SetPivot,
                { { { "x", "Normalized horizontal pivot, clamped to 0..1 (0.5 is centered)." },
                    { "y", "Normalized vertical pivot, clamped to 0..1 (0.5 is centered)." } } })
            ->Event(
                "SetTint",
                &DioramaSpriteRequestBus::Events::SetTint,
                { { { "r", "Red tint multiplier, clamped to 0..1." },
                    { "g", "Green tint multiplier, clamped to 0..1." },
                    { "b", "Blue tint multiplier, clamped to 0..1." },
                    { "a", "Alpha (opacity) multiplier, clamped to 0..1." } } })
            ->Event(
                "SetFlash",
                &DioramaSpriteRequestBus::Events::SetFlash,
                { { { "r", "Flash red, clamped 0..1." },
                    { "g", "Flash green, clamped 0..1." },
                    { "b", "Flash blue, clamped 0..1." },
                    { "amount", "Blend toward the flash color after lighting, 0..1 (1 on a hit, ease back to 0)." } } })
            ->Event(
                "SetOutline",
                &DioramaSpriteRequestBus::Events::SetOutline,
                { { { "r", "Outline red, clamped 0..1." },
                    { "g", "Outline green, clamped 0..1." },
                    { "b", "Outline blue, clamped 0..1." },
                    { "thickness", "Silhouette outline thickness (0 = off); screen-relative, clamped non-negative." } } })
            ->Event(
                "SetEmissive",
                &DioramaSpriteRequestBus::Events::SetEmissive,
                { { { "r", "Emissive red, clamped 0..1." },
                    { "g", "Emissive green, clamped 0..1." },
                    { "b", "Emissive blue, clamped 0..1." },
                    { "intensity", "Emissive strength (0 = off); > 1 makes the sprite bloom via a PostFxLayer + Bloom." } } })
            ->Event(
                "SetBillboard",
                &DioramaSpriteRequestBus::Events::SetBillboard,
                { { { "enabled", "When true the sprite always faces the camera." } } })
            ->Event(
                "SetDoubleSided",
                &DioramaSpriteRequestBus::Events::SetDoubleSided,
                { { { "enabled", "When true the sprite is visible from both sides; when false it is hidden when viewed from behind." } } })
            ->Event(
                "SetPointFilter",
                &DioramaSpriteRequestBus::Events::SetPointFilter,
                { { { "enabled",
                      "When true, use nearest-neighbor (point) texture filtering so pixel art stays crisp; false (default) is linear. "
                      "Pair with a no-mipmap texture import preset for crisp results at any scale." } } })
            ->Event(
                "SetUVRegion",
                &DioramaSpriteRequestBus::Events::SetUVRegion,
                { { { "uMin", "Left edge of the texture sub-rectangle, normalized and clamped to 0..1." },
                    { "vMin", "Top edge of the texture sub-rectangle, normalized and clamped to 0..1." },
                    { "uMax", "Right edge of the texture sub-rectangle, normalized and clamped to 0..1." },
                    { "vMax", "Bottom edge of the texture sub-rectangle, normalized and clamped to 0..1." } } })
            ->Event(
                "SetFlip",
                &DioramaSpriteRequestBus::Events::SetFlip,
                { { { "horizontal", "Mirror the sampled region left-to-right." },
                    { "vertical", "Mirror the sampled region top-to-bottom." } } })
            ->Event(
                "SetTranspose",
                &DioramaSpriteRequestBus::Events::SetTranspose,
                { { { "transpose", "Reflect across the anti-diagonal (swap U/V); with the flips this gives 90-degree rotations." } } })
            ->Event(
                "SetSortOffset",
                &DioramaSpriteRequestBus::Events::SetSortOffset,
                { { { "sortOffset", "Transparent draw-order bias; larger values draw on top." } } })
            ->Event(
                "SetFrameGrid",
                &DioramaSpriteRequestBus::Events::SetFrameGrid,
                { { { "columns", "Number of columns in the sprite sheet grid." },
                    { "rows", "Number of rows in the sprite sheet grid." },
                    { "frameCount", "Number of frames to play; clamped to columns * rows." } } })
            ->Event(
                "SetAnimationEnabled",
                &DioramaSpriteRequestBus::Events::SetAnimationEnabled,
                { { { "enabled", "Enable or disable sprite-sheet playback." } } })
            ->Event(
                "SetPlayback",
                &DioramaSpriteRequestBus::Events::SetPlayback,
                { { { "framesPerSecond", "Playback rate in frames per second." },
                    { "loop", "When true the clip repeats; when false it holds the last frame." } } })
            ->Event(
                "SetPlaybackSpeed",
                &DioramaSpriteRequestBus::Events::SetPlaybackSpeed,
                { { { "speed", "Time-scale: 1 = normal, 0 = freeze (hit-stop), <1 = slow motion, >1 = fast. Clamped >= 0." } } })
            ->Event(
                "SetStartFrame",
                &DioramaSpriteRequestBus::Events::SetStartFrame,
                { { { "frame", "Frame shown first and while not playing; clamped to range." } } })
            ->Event(
                "SetUseSimClock",
                &DioramaSpriteRequestBus::Events::SetUseSimClock,
                { { { "enabled",
                      "Advance playback on the 2D Simulation Clock's fixed steps instead of the render tick; "
                      "with no clock in the level the render tick still advances." } } })
            ->Event(
                "PlaySpriteSheet",
                &DioramaSpriteRequestBus::Events::PlaySpriteSheet,
                { { { "columns", "Number of columns in the sprite sheet grid." },
                    { "rows", "Number of rows in the sprite sheet grid." },
                    { "frameCount", "Number of frames to play; clamped to columns * rows." },
                    { "framesPerSecond", "Playback rate in frames per second." },
                    { "loop", "When true the clip repeats; when false it holds the last frame." } } })
            ->Event("GetSpriteInfo", &DioramaSpriteRequestBus::Events::GetSpriteInfo);

        behaviorContext->EBus<DioramaSpriteNotificationBus>("DioramaSpriteNotificationBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/Sprite")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Handler<DioramaSpriteNotificationHandler>();
    }
} // namespace Diorama
