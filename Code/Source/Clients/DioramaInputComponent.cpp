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
            // Version 2 adds the motion-input layer (direction action + motions);
            // version 3 the sim-clock sampling mode + history ring. New fields default
            // off/empty so an older config keeps its exact behavior.
            serializeContext->Class<DioramaInputConfig>()
                ->Version(3)
                ->Field("actions", &DioramaInputConfig::m_actions)
                ->Field("directionAction", &DioramaInputConfig::m_directionAction)
                ->Field("directionDeadZone", &DioramaInputConfig::m_directionDeadZone)
                ->Field("motions", &DioramaInputConfig::m_motions)
                ->Field("useSimClock", &DioramaInputConfig::m_useSimClock)
                ->Field("historyFrames", &DioramaInputConfig::m_historyFrames);

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
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaInputConfig::m_motions, "Motions", "Recognized motions")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaInputConfig::m_useSimClock,
                        "Use Simulation Clock",
                        "Sample once per fixed simulation step (needs a 2D Simulation Clock) and keep a per-frame history ring")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaInputConfig::m_historyFrames,
                        "History Frames",
                        "Ring capacity in simulation frames (how far back frame queries reach)")
                    ->Attribute(AZ::Edit::Attributes::Min, 2)
                    ->Attribute(AZ::Edit::Attributes::Max, 3600);
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

        // Sim-clock mode: size the history ring (every slot pre-sized to the action
        // count so the steady state never allocates) and listen for fixed steps. The
        // render tick stays connected for the live channel cache; evaluation moves to
        // OnSimTick.
        m_ring.clear();
        if (m_config.m_useSimClock)
        {
            const int capacity = m_config.m_historyFrames < 2 ? 2 : m_config.m_historyFrames;
            m_ring.resize(static_cast<size_t>(capacity));
            for (FrameRecord& record : m_ring)
            {
                record.m_values.assign(m_config.m_actions.size(), InputMap::ActionValue());
            }
            DioramaSimTickNotificationBus::Handler::BusConnect();
        }

        DioramaInputRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();
    }

    void DioramaInputComponent::Deactivate()
    {
        AzFramework::InputChannelEventListener::Disconnect();
        AZ::TickBus::Handler::BusDisconnect();
        if (m_config.m_useSimClock)
        {
            DioramaSimTickNotificationBus::Handler::BusDisconnect();
        }
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
        if (m_config.m_useSimClock)
        {
            // Sim-clock mode: the render tick only keeps the live channel cache warm
            // (OnInputChannelEventFiltered); evaluation happens once per fixed step in
            // OnSimTick so the same frames replay to the same actions.
            return;
        }
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

    // ---- Sim-clock mode (deterministic sim phase C) -----------------------------------

    DioramaInputComponent::FrameRecord* DioramaInputComponent::RecordForFrame(AZ::s64 frame)
    {
        if (m_ring.empty() || frame < 0)
        {
            return nullptr;
        }
        FrameRecord& record = m_ring[static_cast<size_t>(frame % static_cast<AZ::s64>(m_ring.size()))];
        return (record.m_frame == frame) ? &record : nullptr;
    }

    const InputMap::ActionValue* DioramaInputComponent::ValueAtFrame(const AZStd::string& action, AZ::s64 frame)
    {
        const int index = FindAction(action);
        if (index < 0)
        {
            return nullptr;
        }
        FrameRecord* record = RecordForFrame(frame);
        if (record == nullptr || index >= static_cast<int>(record->m_values.size()))
        {
            return nullptr;
        }
        return &record->m_values[static_cast<size_t>(index)];
    }

    void DioramaInputComponent::OnSimTick(AZ::s64 frame, float stepSeconds)
    {
        if (m_ring.empty())
        {
            return;
        }
        FrameRecord& record = m_ring[static_cast<size_t>(frame % static_cast<AZ::s64>(m_ring.size()))];

        // Edges derive from the previous FRAME's record (not whatever m_values held),
        // so they are exact per-step transitions, identical on re-simulation.
        const FrameRecord* prev = RecordForFrame(frame - 1);

        // A frame that already has a record REPLAYS it (recomputing only the edges):
        // that is a re-simulation after a rollback restore, where the ring (plus any
        // injected corrections) is the authoritative input log and the live devices
        // hold the present, not this frame's past. Only a never-recorded frame
        // samples the live channel cache.
        const bool replay = (record.m_frame == frame);
        const AZStd::span<const float> sources(m_sources.data(), m_sources.size());
        for (size_t i = 0; i < m_definition.m_actions.size() && i < record.m_values.size(); ++i)
        {
            const bool prevPressed = (prev != nullptr && i < prev->m_values.size()) ? prev->m_values[i].m_pressed : false;
            if (replay)
            {
                InputMap::ActionValue& v = record.m_values[i];
                v.m_pressedThisFrame = v.m_pressed && !prevPressed;
                v.m_releasedThisFrame = !v.m_pressed && prevPressed;
            }
            else
            {
                record.m_values[i] = InputMap::EvaluateAction(m_definition.m_actions[i], sources, prevPressed);
            }
        }
        record.m_frame = frame;
        record.m_injected = false;

        // The live surface (IsPressed / GetValue / edges) mirrors the newest frame.
        m_values = record.m_values;
        for (size_t i = 0; i < m_values.size() && i < m_config.m_actions.size(); ++i)
        {
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
            // Frame-derived time: exact and identical on every machine and every
            // re-simulation (wall-clock time is neither). Known limit: the direction
            // history is not rewound by a rollback restore, so motion recognition
            // around a rollback converges within one buffer window rather than
            // replaying exactly; games that need exact motion rollback gate motions
            // on button edges (which do replay exactly).
            UpdateMotions(static_cast<float>(frame) * stepSeconds);
        }
    }

    bool DioramaInputComponent::WasPressedAtFrame(const AZStd::string& action, AZ::s64 frame)
    {
        const InputMap::ActionValue* value = ValueAtFrame(action, frame);
        return value != nullptr && value->m_pressed;
    }

    float DioramaInputComponent::GetValueAtFrame(const AZStd::string& action, AZ::s64 frame)
    {
        const InputMap::ActionValue* value = ValueAtFrame(action, frame);
        return value != nullptr ? value->m_x : 0.0f;
    }

    float DioramaInputComponent::GetValueYAtFrame(const AZStd::string& action, AZ::s64 frame)
    {
        const InputMap::ActionValue* value = ValueAtFrame(action, frame);
        return value != nullptr ? value->m_y : 0.0f;
    }

    void DioramaInputComponent::InjectActionState(AZ::s64 frame, const AZStd::string& action, float x, float y, bool pressed)
    {
        if (m_ring.empty() || frame < 0)
        {
            return; // sim-clock mode only
        }
        const int index = FindAction(action);
        if (index < 0)
        {
            return;
        }
        FrameRecord& record = m_ring[static_cast<size_t>(frame % static_cast<AZ::s64>(m_ring.size()))];
        if (record.m_frame != frame || !record.m_injected)
        {
            // Claiming the slot for a new injected frame: start every action from a
            // neutral state so one injected action does not drag along stale values
            // from whatever old frame occupied the slot.
            for (InputMap::ActionValue& v : record.m_values)
            {
                v = InputMap::ActionValue();
            }
            record.m_frame = frame;
            record.m_injected = true;
        }
        InputMap::ActionValue& v = record.m_values[static_cast<size_t>(index)];
        v.m_x = x;
        v.m_y = y;
        v.m_pressed = pressed;
        // Edges are derived when the frame is simulated (OnSimTick), not here.
    }
} // namespace Diorama
