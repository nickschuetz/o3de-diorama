/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaAnimStateMachineComponent.h>

#include <Diorama/DioramaAsepriteBus.h>
#include <Diorama/SpriteBus.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    float AnimStateData::DurationSeconds() const
    {
        if (m_duration > 0.0f)
        {
            return m_duration;
        }
        // Derive from the sprite-sheet clip when no explicit duration is set.
        if (m_frameCount > 0 && m_fps > 0.0f)
        {
            return static_cast<float>(m_frameCount) / m_fps;
        }
        return 0.0f;
    }

    void AnimParamData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AnimParamData>()
                ->Version(1)
                ->Field("name", &AnimParamData::m_name)
                ->Field("kind", &AnimParamData::m_kind)
                ->Field("defaultFloat", &AnimParamData::m_defaultFloat)
                ->Field("defaultBool", &AnimParamData::m_defaultBool);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<AnimParamData>("Parameter", "A named blackboard value transitions can test")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &AnimParamData::m_name, "Name", "Parameter name (referenced by conditions)")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &AnimParamData::m_kind, "Kind", "Bool, Float, or one-shot Trigger")
                    ->EnumAttribute(AnimSM::ParamKind::Bool, "Bool")
                    ->EnumAttribute(AnimSM::ParamKind::Float, "Float")
                    ->EnumAttribute(AnimSM::ParamKind::Trigger, "Trigger")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &AnimParamData::m_defaultFloat, "Default (float)", "Initial value for a Float")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox, &AnimParamData::m_defaultBool, "Default (bool)", "Initial value for a Bool");
            }
        }
    }

    void AnimConditionData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AnimConditionData>()
                ->Version(1)
                ->Field("param", &AnimConditionData::m_param)
                ->Field("compare", &AnimConditionData::m_compare)
                ->Field("threshold", &AnimConditionData::m_threshold)
                ->Field("expected", &AnimConditionData::m_expected);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<AnimConditionData>("Condition", "A single guard on a transition")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimConditionData::m_param, "Parameter", "Name of the parameter to test")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &AnimConditionData::m_compare, "Compare", "Comparison (Float parameters)")
                    ->EnumAttribute(AnimSM::Compare::Greater, ">")
                    ->EnumAttribute(AnimSM::Compare::Less, "<")
                    ->EnumAttribute(AnimSM::Compare::GreaterEqual, ">=")
                    ->EnumAttribute(AnimSM::Compare::LessEqual, "<=")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &AnimConditionData::m_threshold, "Threshold", "Value a Float is compared against")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox, &AnimConditionData::m_expected, "Expected", "Required value of a Bool parameter");
            }
        }
    }

    void AnimTransitionData::Reflect(AZ::ReflectContext* context)
    {
        AnimConditionData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AnimTransitionData>()
                ->Version(1)
                ->Field("from", &AnimTransitionData::m_from)
                ->Field("to", &AnimTransitionData::m_to)
                ->Field("conditions", &AnimTransitionData::m_conditions)
                ->Field("hasExitTime", &AnimTransitionData::m_hasExitTime)
                ->Field("exitTime", &AnimTransitionData::m_exitTime);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<AnimTransitionData>("Transition", "An edge between states, guarded by conditions")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &AnimTransitionData::m_from, "From", "Source state name (empty = Any State)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimTransitionData::m_to, "To", "Destination state name")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimTransitionData::m_conditions, "Conditions", "All must hold to fire")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox, &AnimTransitionData::m_hasExitTime, "Has exit time", "Gate on clip progress")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider, &AnimTransitionData::m_exitTime, "Exit time", "Fraction of the source clip [0,1]")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f);
            }
        }
    }

    void AnimStateData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AnimStateData>()
                ->Version(1)
                ->Field("name", &AnimStateData::m_name)
                ->Field("asepriteTag", &AnimStateData::m_asepriteTag)
                ->Field("columns", &AnimStateData::m_columns)
                ->Field("rows", &AnimStateData::m_rows)
                ->Field("frameCount", &AnimStateData::m_frameCount)
                ->Field("fps", &AnimStateData::m_fps)
                ->Field("loop", &AnimStateData::m_loop)
                ->Field("duration", &AnimStateData::m_duration);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<AnimStateData>("State", "A node that plays one animation clip")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimStateData::m_name, "Name", "State name (referenced by transitions)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AnimStateData::m_asepriteTag,
                        "Aseprite tag",
                        "Tag to play on the target's Aseprite component (takes priority when set)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimStateData::m_columns, "Columns", "Sprite-sheet columns")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimStateData::m_rows, "Rows", "Sprite-sheet rows")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &AnimStateData::m_frameCount, "Frame count", "Frames in the clip (0 = no sheet)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AnimStateData::m_fps, "FPS", "Sprite-sheet playback rate")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &AnimStateData::m_loop, "Loop", "Wrap the sprite-sheet clip")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AnimStateData::m_duration,
                        "Duration override",
                        "Clip length for exit-time; 0 = derive from frames / fps")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
            }
        }
    }

    void DioramaAnimStateMachineConfig::Reflect(AZ::ReflectContext* context)
    {
        AnimParamData::Reflect(context);
        AnimStateData::Reflect(context);
        AnimTransitionData::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaAnimStateMachineConfig>()
                ->Version(1)
                ->Field("parameters", &DioramaAnimStateMachineConfig::m_parameters)
                ->Field("states", &DioramaAnimStateMachineConfig::m_states)
                ->Field("transitions", &DioramaAnimStateMachineConfig::m_transitions)
                ->Field("defaultState", &DioramaAnimStateMachineConfig::m_defaultState)
                ->Field("target", &DioramaAnimStateMachineConfig::m_target)
                ->Field("useSimClock", &DioramaAnimStateMachineConfig::m_useSimClock);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<DioramaAnimStateMachineConfig>(
                        "Anim State Machine Config", "A parameter-driven animation graph over Sprite/Aseprite clips")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaAnimStateMachineConfig::m_parameters, "Parameters", "The blackboard")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaAnimStateMachineConfig::m_states, "States", "Animation nodes")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &DioramaAnimStateMachineConfig::m_transitions, "Transitions", "Edges between states")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaAnimStateMachineConfig::m_defaultState,
                        "Default state",
                        "Initial state name (empty = first state)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaAnimStateMachineConfig::m_target,
                        "Target entity",
                        "Entity whose Sprite/Aseprite to drive (empty = this entity)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox,
                        &DioramaAnimStateMachineConfig::m_useSimClock,
                        "Use Simulation Clock",
                        "Evaluate the graph on the 2D Simulation Clock's fixed steps instead of the render tick. "
                        "With no clock in the level, falls back to the render tick.");
            }
        }
    }

    AnimStateMachineDefinition BuildAnimStateMachineDefinition(const DioramaAnimStateMachineConfig& config)
    {
        AnimStateMachineDefinition definition;

        const auto findParam = [&config](const AZStd::string& name) -> int
        {
            for (size_t i = 0; i < config.m_parameters.size(); ++i)
            {
                if (config.m_parameters[i].m_name == name)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };
        const auto findState = [&config](const AZStd::string& name) -> int
        {
            for (size_t i = 0; i < config.m_states.size(); ++i)
            {
                if (config.m_states[i].m_name == name)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };

        // Parameter values, seeded from the authored defaults. Triggers always start clear.
        definition.m_runtime.m_params.reserve(config.m_parameters.size());
        for (const AnimParamData& param : config.m_parameters)
        {
            AnimSM::ParamValue value;
            value.m_kind = param.m_kind;
            value.m_float = param.m_defaultFloat;
            value.m_bool = (param.m_kind == AnimSM::ParamKind::Trigger) ? false : param.m_defaultBool;
            definition.m_runtime.m_params.push_back(value);
        }

        // Per-state clip durations for exit-time normalization.
        definition.m_durations.reserve(config.m_states.size());
        for (const AnimStateData& state : config.m_states)
        {
            definition.m_durations.push_back(state.DurationSeconds());
        }

        // Resolve name-based edges into index-based pure transitions, dropping any that
        // reference an unknown state so a malformed graph degrades instead of crashing.
        definition.m_transitions.reserve(config.m_transitions.size());
        for (const AnimTransitionData& data : config.m_transitions)
        {
            AnimSM::Transition transition;
            transition.m_from = data.m_from.empty() ? -1 : findState(data.m_from);
            transition.m_to = findState(data.m_to);
            if (transition.m_to < 0)
            {
                continue; // unknown destination -> drop the edge
            }
            if (!data.m_from.empty() && transition.m_from < 0)
            {
                continue; // named-but-unknown source -> drop the edge
            }

            for (const AnimConditionData& condition : data.m_conditions)
            {
                const int paramIndex = findParam(condition.m_param);
                if (paramIndex < 0)
                {
                    continue; // unknown parameter -> drop just this condition
                }
                AnimSM::Condition pure;
                pure.m_param = paramIndex;
                pure.m_compare = condition.m_compare;
                pure.m_threshold = condition.m_threshold;
                pure.m_expected = condition.m_expected;
                transition.m_conditions.push_back(pure);
            }

            transition.m_hasExitTime = data.m_hasExitTime;
            transition.m_exitTime = AZ::GetClamp(data.m_exitTime, 0.0f, 1.0f);
            definition.m_transitions.push_back(transition);
        }

        // Initial state: the named default, else the first state.
        if (config.m_defaultState.empty())
        {
            definition.m_defaultStateIndex = config.m_states.empty() ? -1 : 0;
        }
        else
        {
            definition.m_defaultStateIndex = findState(config.m_defaultState);
            if (definition.m_defaultStateIndex < 0 && !config.m_states.empty())
            {
                definition.m_defaultStateIndex = 0;
            }
        }

        return definition;
    }

    DioramaAnimStateMachineComponent::DioramaAnimStateMachineComponent(const DioramaAnimStateMachineConfig& config)
        : m_config(config)
    {
    }

    void DioramaAnimStateMachineComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaAnimStateMachineConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaAnimStateMachineComponent, AZ::Component>()->Version(1)->Field(
                "Config", &DioramaAnimStateMachineComponent::m_config);
        }

        ReflectAnimStateMachineBuses(context);
    }

    void DioramaAnimStateMachineComponent::Activate()
    {
        m_definition = BuildAnimStateMachineDefinition(m_config);

        DioramaAnimStateMachineRequestBus::Handler::BusConnect(GetEntityId());
        DioramaSimStateParticipantBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        if (m_config.m_useSimClock)
        {
            DioramaSimTickNotificationBus::Handler::BusConnect();
        }

        // Enter the initial state (from = none) so the rig is posed and OnStateChanged fires.
        if (m_definition.m_defaultStateIndex >= 0)
        {
            EnterState(m_definition.m_defaultStateIndex);
        }
    }

    void DioramaAnimStateMachineComponent::Deactivate()
    {
        DioramaSimTickNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        DioramaSimStateParticipantBus::Handler::BusDisconnect();
        DioramaAnimStateMachineRequestBus::Handler::BusDisconnect();
    }

    void DioramaAnimStateMachineComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        // Use Simulation Clock mode: a running clock owns the advance (OnSimTick).
        if (m_config.m_useSimClock && DioramaSimClockRequestBus::HasHandlers())
        {
            return;
        }
        Advance(deltaTime);
    }

    void DioramaAnimStateMachineComponent::OnSimTick([[maybe_unused]] AZ::s64 frame, float stepSeconds)
    {
        Advance(stepSeconds);
    }

    void DioramaAnimStateMachineComponent::Advance(float deltaTime)
    {
        const int fromIndex = m_definition.m_runtime.m_current;
        const int destination = AnimSM::Step(
            m_definition.m_runtime,
            AZStd::span<const AnimSM::Transition>(m_definition.m_transitions.data(), m_definition.m_transitions.size()),
            AZStd::span<const float>(m_definition.m_durations.data(), m_definition.m_durations.size()),
            deltaTime);

        if (destination >= 0)
        {
            // Step already moved the runtime; play the new clip and fire the notification.
            const AZStd::string fromName = (fromIndex >= 0 && fromIndex < static_cast<int>(m_config.m_states.size()))
                ? m_config.m_states[fromIndex].m_name
                : AZStd::string();
            const AZStd::string& toName = m_config.m_states[destination].m_name;

            const AnimStateData& state = m_config.m_states[destination];
            const AZ::EntityId target = TargetEntity();
            if (!state.m_asepriteTag.empty())
            {
                DioramaAsepriteRequestBus::Event(target, &DioramaAsepriteRequests::PlayTag, state.m_asepriteTag);
            }
            else if (state.m_frameCount > 0)
            {
                DioramaSpriteRequestBus::Event(
                    target,
                    &DioramaSpriteRequests::PlaySpriteSheet,
                    state.m_columns,
                    state.m_rows,
                    state.m_frameCount,
                    state.m_fps,
                    state.m_loop);
            }

            DioramaAnimStateMachineNotificationBus::Event(
                GetEntityId(), &DioramaAnimStateMachineNotifications::OnStateChanged, fromName, toName);
        }
    }

    void DioramaAnimStateMachineComponent::EnterState(int stateIndex)
    {
        if (stateIndex < 0 || stateIndex >= static_cast<int>(m_config.m_states.size()))
        {
            return;
        }

        const int fromIndex = m_definition.m_runtime.m_current;
        AnimSM::ForceState(m_definition.m_runtime, stateIndex);

        const AZStd::string fromName = (fromIndex >= 0 && fromIndex < static_cast<int>(m_config.m_states.size()))
            ? m_config.m_states[fromIndex].m_name
            : AZStd::string();
        const AnimStateData& state = m_config.m_states[stateIndex];
        const AZ::EntityId target = TargetEntity();
        if (!state.m_asepriteTag.empty())
        {
            DioramaAsepriteRequestBus::Event(target, &DioramaAsepriteRequests::PlayTag, state.m_asepriteTag);
        }
        else if (state.m_frameCount > 0)
        {
            DioramaSpriteRequestBus::Event(
                target,
                &DioramaSpriteRequests::PlaySpriteSheet,
                state.m_columns,
                state.m_rows,
                state.m_frameCount,
                state.m_fps,
                state.m_loop);
        }

        DioramaAnimStateMachineNotificationBus::Event(
            GetEntityId(), &DioramaAnimStateMachineNotifications::OnStateChanged, fromName, state.m_name);
    }

    int DioramaAnimStateMachineComponent::FindParam(const AZStd::string& name) const
    {
        for (size_t i = 0; i < m_config.m_parameters.size(); ++i)
        {
            if (m_config.m_parameters[i].m_name == name)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int DioramaAnimStateMachineComponent::FindState(const AZStd::string& name) const
    {
        for (size_t i = 0; i < m_config.m_states.size(); ++i)
        {
            if (m_config.m_states[i].m_name == name)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    AZ::EntityId DioramaAnimStateMachineComponent::TargetEntity() const
    {
        return m_config.m_target.IsValid() ? m_config.m_target : GetEntityId();
    }

    void DioramaAnimStateMachineComponent::SetBool(const AZStd::string& name, bool value)
    {
        AnimSM::SetBool(m_definition.m_runtime, FindParam(name), value);
    }

    void DioramaAnimStateMachineComponent::SetFloat(const AZStd::string& name, float value)
    {
        AnimSM::SetFloat(m_definition.m_runtime, FindParam(name), value);
    }

    void DioramaAnimStateMachineComponent::SetTrigger(const AZStd::string& name)
    {
        AnimSM::SetBool(m_definition.m_runtime, FindParam(name), true);
    }

    void DioramaAnimStateMachineComponent::ResetTrigger(const AZStd::string& name)
    {
        AnimSM::SetBool(m_definition.m_runtime, FindParam(name), false);
    }

    void DioramaAnimStateMachineComponent::Play(const AZStd::string& stateName)
    {
        EnterState(FindState(stateName));
    }

    void DioramaAnimStateMachineComponent::SetUseSimClock(bool enabled)
    {
        m_config.m_useSimClock = enabled;
        if (enabled)
        {
            if (!DioramaSimTickNotificationBus::Handler::BusIsConnected())
            {
                DioramaSimTickNotificationBus::Handler::BusConnect();
            }
        }
        else
        {
            DioramaSimTickNotificationBus::Handler::BusDisconnect();
        }
    }

    AZStd::string DioramaAnimStateMachineComponent::GetCurrentState()
    {
        const int current = m_definition.m_runtime.m_current;
        if (current >= 0 && current < static_cast<int>(m_config.m_states.size()))
        {
            return m_config.m_states[current].m_name;
        }
        return AZStd::string();
    }

    bool DioramaAnimStateMachineComponent::GetBool(const AZStd::string& name)
    {
        const int index = FindParam(name);
        if (index >= 0 && index < static_cast<int>(m_definition.m_runtime.m_params.size()))
        {
            return m_definition.m_runtime.m_params[index].m_bool;
        }
        return false;
    }

    float DioramaAnimStateMachineComponent::GetFloat(const AZStd::string& name)
    {
        const int index = FindParam(name);
        if (index >= 0 && index < static_cast<int>(m_definition.m_runtime.m_params.size()))
        {
            return m_definition.m_runtime.m_params[index].m_float;
        }
        return 0.0f;
    }

    // ---- Snapshot / restore -----------------------------------------------------------
    // Chunk payload: current state index (s64), seconds in state (f32), param count
    // (u32), then per parameter its float (f32) and bool (u8) values. Param kinds are
    // config, not runtime state. Restore applies indices silently (no EnterState): the
    // bound animation's own chunk restores the visual playback, and re-entering would
    // restart the clip and re-fire OnStateChanged mid-rollback.

    void DioramaAnimStateMachineComponent::SaveSimState(SimState::Writer& writer)
    {
        const AnimSM::Runtime& runtime = m_definition.m_runtime;
        const size_t sizePos = writer.BeginChunk(AnimStateMachineChunkTag);
        writer.S64(runtime.m_current);
        writer.F32(runtime.m_time);
        writer.U32(static_cast<AZ::u32>(runtime.m_params.size()));
        for (const AnimSM::ParamValue& param : runtime.m_params)
        {
            writer.F32(param.m_float);
            writer.U8(param.m_bool ? 1 : 0);
        }
        writer.EndChunk(sizePos);
    }

    bool DioramaAnimStateMachineComponent::TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload)
    {
        if (tag != AnimStateMachineChunkTag)
        {
            return false;
        }
        AZ::s64 current = -1;
        float time = 0.0f;
        AZ::u32 paramCount = 0;
        if (!payload.S64(current) || !payload.F32(time) || !payload.U32(paramCount))
        {
            return false;
        }
        AnimSM::Runtime& runtime = m_definition.m_runtime;
        if (paramCount != runtime.m_params.size())
        {
            // The graph was re-authored since this image was taken: apply the scalar
            // state and keep the current (default) parameter values rather than
            // misalign values across a different parameter set.
            runtime.m_current = static_cast<int>(current);
            runtime.m_time = time;
            return true;
        }
        // Parse into scratch first so a truncated payload cannot half-apply.
        AZStd::vector<AnimSM::ParamValue> restored = runtime.m_params;
        for (AZ::u32 i = 0; i < paramCount; ++i)
        {
            AZ::u8 boolValue = 0;
            if (!payload.F32(restored[i].m_float) || !payload.U8(boolValue))
            {
                return false;
            }
            restored[i].m_bool = boolValue != 0;
        }
        runtime.m_current = static_cast<int>(current);
        runtime.m_time = time;
        runtime.m_params = AZStd::move(restored);
        return true;
    }
} // namespace Diorama
