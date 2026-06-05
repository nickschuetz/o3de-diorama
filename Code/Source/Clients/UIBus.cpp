/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Diorama/DioramaUIBus.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    void UIInfo::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<UIInfo>()
                ->Version(1)
                ->Field("text", &UIInfo::m_text)
                ->Field("fontSize", &UIInfo::m_fontSize)
                ->Field("visible", &UIInfo::m_visible)
                ->Field("anchor", &UIInfo::m_anchor)
                ->Field("kind", &UIInfo::m_kind)
                ->Field("value", &UIInfo::m_value)
                ->Field("screenX", &UIInfo::m_screenX)
                ->Field("screenY", &UIInfo::m_screenY);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<UIInfo>("DioramaUIInfo")
                ->Attribute(AZ::Script::Attributes::Category, "Diorama/UI")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Property("text", BehaviorValueGetter(&UIInfo::m_text), nullptr)
                ->Property("fontSize", BehaviorValueGetter(&UIInfo::m_fontSize), nullptr)
                ->Property("visible", BehaviorValueGetter(&UIInfo::m_visible), nullptr)
                ->Property("anchor", BehaviorValueGetter(&UIInfo::m_anchor), nullptr)
                ->Property("kind", BehaviorValueGetter(&UIInfo::m_kind), nullptr)
                ->Property("value", BehaviorValueGetter(&UIInfo::m_value), nullptr)
                ->Property("screenX", BehaviorValueGetter(&UIInfo::m_screenX), nullptr)
                ->Property("screenY", BehaviorValueGetter(&UIInfo::m_screenY), nullptr);
        }
    }

    void ReflectUIBuses(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext
            ->EBus<DioramaUIRequestBus>("DioramaUIRequestBus")
            // Common scope so it is callable from editor Python and runtime script.
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Category, "Diorama/UI")
            ->Attribute(AZ::Script::Attributes::Module, "diorama")
            ->Event("SetText", &DioramaUIRequestBus::Events::SetText, { { { "text", "Label text to display." } } })
            ->Event(
                "SetFontSize",
                &DioramaUIRequestBus::Events::SetFontSize,
                { { { "pixels", "Font size in reference pixels; clamped to a sane positive range." } } })
            ->Event(
                "SetColor",
                &DioramaUIRequestBus::Events::SetColor,
                { { { "r", "Red 0..1." }, { "g", "Green 0..1." }, { "b", "Blue 0..1." }, { "a", "Alpha 0..1." } } })
            ->Event(
                "SetAnchor",
                &DioramaUIRequestBus::Events::SetAnchor,
                { { { "anchor", "0 TopLeft,1 Top,2 TopRight,3 Left,4 Center,5 Right,6 BottomLeft,7 Bottom,8 BottomRight." } } })
            ->Event(
                "SetOffset",
                &DioramaUIRequestBus::Events::SetOffset,
                { { { "x", "Offset X from the anchor, in reference pixels." },
                    { "y", "Offset Y from the anchor, in reference pixels (y-down)." } } })
            ->Event(
                "SetSize",
                &DioramaUIRequestBus::Events::SetSize,
                { { { "width", "Element box width in reference pixels (bar/panel)." },
                    { "height", "Element box height in reference pixels (bar/panel)." } } })
            ->Event("SetValue", &DioramaUIRequestBus::Events::SetValue, { { { "value", "Bar fill, clamped 0..1." } } })
            ->Event(
                "SetBackgroundColor",
                &DioramaUIRequestBus::Events::SetBackgroundColor,
                { { { "r", "Red 0..1." }, { "g", "Green 0..1." }, { "b", "Blue 0..1." }, { "a", "Alpha 0..1." } } })
            ->Event(
                "SetVisible", &DioramaUIRequestBus::Events::SetVisible, { { { "visible", "Show (true) or hide (false) the element." } } })
            ->Event("GetUIInfo", &DioramaUIRequestBus::Events::GetUIInfo);
    }
} // namespace Diorama
