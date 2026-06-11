/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaInputComponent.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Channels/InputChannel.h>

namespace Diorama
{
    void DioramaInputBindingData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaInputBindingData>()
                ->Version(1)
                ->Field("channel", &DioramaInputBindingData::m_channel)
                ->Field("scale", &DioramaInputBindingData::m_scale)
                ->Field("axis", &DioramaInputBindingData::m_axis);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaInputBindingData>("Binding", "One input channel feeding an action")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaInputBindingData::m_channel,
                        "Channel",
                        "Input channel name, e.g. keyboard_key_edit_W or gamepad_thumbstick_l_x")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaInputBindingData::m_scale,
                        "Scale",
                        "Multiplier (use -1 for the negative side of an axis)")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &DioramaInputBindingData::m_axis, "Axis", "Which axis of a 2D action")
                    ->EnumAttribute(InputMap::Axis::X, "X")
                    ->EnumAttribute(InputMap::Axis::Y, "Y");
            }
        }
    }

    void DioramaInputActionData::Reflect(AZ::ReflectContext* context)
    {
        DioramaInputBindingData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaInputActionData>()
                ->Version(1)
                ->Field("name", &DioramaInputActionData::m_name)
                ->Field("kind", &DioramaInputActionData::m_kind)
                ->Field("deadZone", &DioramaInputActionData::m_deadZone)
                ->Field("pressThreshold", &DioramaInputActionData::m_pressThreshold)
                ->Field("bindings", &DioramaInputActionData::m_bindings);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaInputActionData>("Action", "A named, rebindable gameplay action")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaInputActionData::m_name, "Name", "Action name (read by scripts)")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &DioramaInputActionData::m_kind, "Kind", "Button, 1D axis, or 2D vector")
                    ->EnumAttribute(InputMap::ActionKind::Button, "Button")
                    ->EnumAttribute(InputMap::ActionKind::Axis1D, "Axis 1D")
                    ->EnumAttribute(InputMap::ActionKind::Axis2D, "Axis 2D")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider, &DioramaInputActionData::m_deadZone, "Dead zone", "Axis dead-zone magnitude")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 0.999f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaInputActionData::m_pressThreshold,
                        "Press threshold",
                        "Magnitude at/above which the action counts as pressed")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaInputActionData::m_bindings, "Bindings", "Inputs feeding this action");
            }
        }
    }

    void DioramaMotionData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaMotionData>()
                ->Version(1)
                ->Field("name", &DioramaMotionData::m_name)
                ->Field("sequence", &DioramaMotionData::m_sequence)
                ->Field("windowSeconds", &DioramaMotionData::m_windowSeconds);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaMotionData>("Motion", "A recognized directional motion")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaMotionData::m_name, "Name", "Motion name (read by scripts)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaMotionData::m_sequence,
                        "Sequence",
                        "Numpad notation, e.g. 236 (QCF), 623 (DP), 41236 (HCF)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaMotionData::m_windowSeconds,
                        "Window",
                        "Seconds within which every step must have been entered")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.05f)
                    ->Attribute(AZ::Edit::Attributes::Max, 2.0f);
            }
        }
    }

    void DioramaInputConfig::Reflect(AZ::ReflectContext* context)
    {
        DioramaInputActionData::Reflect(context);
        DioramaMotionData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            // Version 2 adds the motion-input layer (direction action + motions); the
            // new fields default to empty so a v1 config keeps its exact behavior.
            serializeContext->Class<DioramaInputConfig>()
                ->Version(2)
                ->Field("actions", &DioramaInputConfig::m_actions)
                ->Field("directionAction", &DioramaInputConfig::m_directionAction)
                ->Field("directionDeadZone", &DioramaInputConfig::m_directionDeadZone)
                ->Field("motions", &DioramaInputConfig::m_motions);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaInputConfig>("Input Config", "Named, rebindable input actions")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaInputConfig::m_actions, "Actions", "The action list")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaInputConfig::m_directionAction,
                        "Direction action",
                        "Axis2D action quantized into a numpad direction (empty disables motions)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &DioramaInputConfig::m_directionDeadZone,
                        "Direction dead zone",
                        "Stick magnitude below which the direction reads as centered")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 0.99f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaInputConfig::m_motions, "Motions", "Recognized motions");
            }
        }
    }

    InputActionMapDefinition BuildInputActionMapDefinition(const DioramaInputConfig& config)
    {
        InputActionMapDefinition definition;

        // De-duplicate channel names into a source list; a binding's source is its index.
        const auto sourceIndex = [&definition](const AZStd::string& channel) -> int
        {
            for (size_t i = 0; i < definition.m_sourceChannels.size(); ++i)
            {
                if (definition.m_sourceChannels[i] == channel)
                {
                    return static_cast<int>(i);
                }
            }
            definition.m_sourceChannels.push_back(channel);
            return static_cast<int>(definition.m_sourceChannels.size() - 1);
        };

        definition.m_actions.reserve(config.m_actions.size());
        for (const DioramaInputActionData& actionData : config.m_actions)
        {
            InputMap::Action action;
            action.m_kind = actionData.m_kind;
            action.m_deadZone = actionData.m_deadZone;
            action.m_pressThreshold = actionData.m_pressThreshold;
            action.m_bindings.reserve(actionData.m_bindings.size());
            for (const DioramaInputBindingData& bindingData : actionData.m_bindings)
            {
                if (bindingData.m_channel.empty())
                {
                    continue; // a binding with no channel contributes nothing
                }
                InputMap::Binding binding;
                binding.m_source = sourceIndex(bindingData.m_channel);
                binding.m_scale = bindingData.m_scale;
                binding.m_axis = bindingData.m_axis;
                action.m_bindings.push_back(binding);
            }
            definition.m_actions.push_back(action);
        }

        return definition;
    }

    DioramaInputComponent::DioramaInputComponent()
        : AzFramework::InputChannelEventListener(AzFramework::InputChannelEventListener::GetPriorityDefault(), false)
    {
    }

    DioramaInputComponent::DioramaInputComponent(const DioramaInputConfig& config)
        : AzFramework::InputChannelEventListener(AzFramework::InputChannelEventListener::GetPriorityDefault(), false)
        , m_config(config)
    {
    }

    void DioramaInputComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaInputConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaInputComponent, AZ::Component>()->Version(1)->Field("Config", &DioramaInputComponent::m_config);
        }

        ReflectInputBuses(context);
    }

    void DioramaInputComponent::Activate()
    {
        m_definition = BuildInputActionMapDefinition(m_config);
        m_sources.assign(m_definition.m_sourceChannels.size(), 0.0f);
        m_values.assign(m_config.m_actions.size(), InputMap::ActionValue());

        // Parse each motion's numpad string into a direction sequence once, here, so the
        // per-tick path only compares pre-parsed data. Non-digit characters are ignored.
        m_motionSequences.clear();
        m_motionSequences.reserve(m_config.m_motions.size());
        for (const DioramaMotionData& motion : m_config.m_motions)
        {
            AZStd::vector<MotionInput::Direction> sequence;
            for (const char c : motion.m_sequence)
            {
                if (c >= '1' && c <= '9')
                {
                    sequence.push_back(static_cast<MotionInput::Direction>(c - '0'));
                }
            }
            m_motionSequences.push_back(AZStd::move(sequence));
        }
        m_motionMatched.assign(m_config.m_motions.size(), false);
        m_directionHistory.clear();
        m_directionActionIndex = m_config.m_directionAction.empty() ? -1 : FindAction(m_config.m_directionAction);

        DioramaInputRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();
    }

    void DioramaInputComponent::Deactivate()
    {
        AzFramework::InputChannelEventListener::Disconnect();
        AZ::TickBus::Handler::BusDisconnect();
        DioramaInputRequestBus::Handler::BusDisconnect();
    }

    bool DioramaInputComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        // Cache the value of any channel we bound; ignore the rest. Linear scan over a
        // small, per-component channel list (a handful of names), not a global lookup.
        const char* name = inputChannel.GetInputChannelId().GetName();
        for (size_t i = 0; i < m_definition.m_sourceChannels.size(); ++i)
        {
            if (m_definition.m_sourceChannels[i] == name)
            {
                m_sources[i] = inputChannel.GetValue();
                break;
            }
        }
        return false; // never consume: gameplay and other listeners still see the input
    }

    void DioramaInputComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint time)
    {
        const AZStd::span<const float> sources(m_sources.data(), m_sources.size());
        for (size_t i = 0; i < m_definition.m_actions.size() && i < m_values.size(); ++i)
        {
            const bool prevPressed = m_values[i].m_pressed;
            m_values[i] = InputMap::EvaluateAction(m_definition.m_actions[i], sources, prevPressed);

            if (m_values[i].m_pressedThisFrame)
            {
                DioramaInputNotificationBus::Event(
                    GetEntityId(), &DioramaInputNotifications::OnActionPressed, m_config.m_actions[i].m_name);
            }
            if (m_values[i].m_releasedThisFrame)
            {
                DioramaInputNotificationBus::Event(
                    GetEntityId(), &DioramaInputNotifications::OnActionReleased, m_config.m_actions[i].m_name);
            }
        }

        if (m_directionActionIndex >= 0)
        {
            UpdateMotions(static_cast<float>(time.GetSeconds()));
        }
    }

    void DioramaInputComponent::UpdateMotions(float nowSeconds)
    {
        // Quantize the current directional input and record a sample only when the
        // direction changes, so the history holds the sequence of held directions (not
        // one entry per frame). Cap the history so a long session never grows it without
        // bound; the window check in MotionInput::Matches ignores anything older anyway.
        const InputMap::ActionValue& dir = m_values[m_directionActionIndex];
        const MotionInput::Direction current = MotionInput::DirectionFromAxes(dir.m_x, dir.m_y, m_config.m_directionDeadZone);
        if (m_directionHistory.empty() || m_directionHistory.back().m_direction != current)
        {
            m_directionHistory.push_back(MotionInput::Sample{ current, nowSeconds });
            constexpr size_t MaxHistory = 24;
            if (m_directionHistory.size() > MaxHistory)
            {
                m_directionHistory.erase(m_directionHistory.begin(), m_directionHistory.begin() + (m_directionHistory.size() - MaxHistory));
            }
        }

        const AZStd::span<const MotionInput::Sample> history(m_directionHistory.data(), m_directionHistory.size());
        for (size_t i = 0; i < m_config.m_motions.size(); ++i)
        {
            const AZStd::span<const MotionInput::Direction> sequence(m_motionSequences[i].data(), m_motionSequences[i].size());
            const bool matched = MotionInput::Matches(history, sequence, m_config.m_motions[i].m_windowSeconds, nowSeconds);
            if (matched && !m_motionMatched[i])
            {
                DioramaInputNotificationBus::Event(
                    GetEntityId(), &DioramaInputNotifications::OnMotionPerformed, m_config.m_motions[i].m_name);
            }
            m_motionMatched[i] = matched;
        }
    }

    int DioramaInputComponent::FindAction(const AZStd::string& name) const
    {
        for (size_t i = 0; i < m_config.m_actions.size(); ++i)
        {
            if (m_config.m_actions[i].m_name == name)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool DioramaInputComponent::IsPressed(const AZStd::string& action)
    {
        const int index = FindAction(action);
        return (index >= 0 && index < static_cast<int>(m_values.size())) ? m_values[index].m_pressed : false;
    }

    bool DioramaInputComponent::WasPressedThisFrame(const AZStd::string& action)
    {
        const int index = FindAction(action);
        return (index >= 0 && index < static_cast<int>(m_values.size())) ? m_values[index].m_pressedThisFrame : false;
    }

    bool DioramaInputComponent::WasReleasedThisFrame(const AZStd::string& action)
    {
        const int index = FindAction(action);
        return (index >= 0 && index < static_cast<int>(m_values.size())) ? m_values[index].m_releasedThisFrame : false;
    }

    float DioramaInputComponent::GetValue(const AZStd::string& action)
    {
        const int index = FindAction(action);
        return (index >= 0 && index < static_cast<int>(m_values.size())) ? m_values[index].m_x : 0.0f;
    }

    float DioramaInputComponent::GetValueY(const AZStd::string& action)
    {
        const int index = FindAction(action);
        return (index >= 0 && index < static_cast<int>(m_values.size())) ? m_values[index].m_y : 0.0f;
    }

    int DioramaInputComponent::FindMotion(const AZStd::string& name) const
    {
        for (size_t i = 0; i < m_config.m_motions.size(); ++i)
        {
            if (m_config.m_motions[i].m_name == name)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool DioramaInputComponent::WasMotionPerformed(const AZStd::string& motion)
    {
        const int index = FindMotion(motion);
        return (index >= 0 && index < static_cast<int>(m_motionMatched.size())) ? m_motionMatched[index] : false;
    }
} // namespace Diorama
