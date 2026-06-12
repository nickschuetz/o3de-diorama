/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/AnimStateMachine.h>
#include <Clients/SimStateBus.h>
#include <Diorama/DioramaAnimStateMachineBus.h>
#include <Diorama/DioramaSimClockBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

// The pure AnimSM enums live in a dependency-free header, so their reflection type
// info is specialized here (the one place that reflects them for the editor combo boxes).
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::AnimSM::ParamKind, Diorama::AnimSMParamKindTypeId);
    AZ_TYPE_INFO_SPECIALIZE(Diorama::AnimSM::Compare, Diorama::AnimSMCompareTypeId);
} // namespace AZ

namespace Diorama
{
    //! One authored parameter: a named blackboard value transitions can test.
    struct AnimParamData final
    {
        AZ_TYPE_INFO(Diorama::AnimParamData, AnimParamDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        AnimSM::ParamKind m_kind = AnimSM::ParamKind::Bool;
        float m_defaultFloat = 0.0f;
        bool m_defaultBool = false;
    };

    //! One guard on a transition, referencing a parameter by name.
    struct AnimConditionData final
    {
        AZ_TYPE_INFO(Diorama::AnimConditionData, AnimConditionDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_param;
        AnimSM::Compare m_compare = AnimSM::Compare::Greater; //!< Float params only.
        float m_threshold = 0.0f; //!< Float params.
        bool m_expected = true; //!< Bool params.
    };

    //! One transition edge, referencing states by name. An empty "from" is Any State.
    struct AnimTransitionData final
    {
        AZ_TYPE_INFO(Diorama::AnimTransitionData, AnimTransitionDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_from; //!< Source state name; empty = Any State.
        AZStd::string m_to; //!< Destination state name.
        AZStd::vector<AnimConditionData> m_conditions; //!< ANDed guards.
        bool m_hasExitTime = false; //!< Gate on normalized time in the source state.
        float m_exitTime = 1.0f; //!< Fire only once the source clip is this far along [0,1].
    };

    //! One state: a name plus the animation it plays on the target entity. If
    //! m_asepriteTag is set it plays that Aseprite tag; otherwise, if m_frameCount > 0,
    //! it plays a sprite-sheet clip. m_duration drives exit-time normalization.
    struct AnimStateData final
    {
        AZ_TYPE_INFO(Diorama::AnimStateData, AnimStateDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        //! Clip length in seconds used to normalize exit-time transitions; <= 0 treats
        //! the state as instantly finished for exit-time purposes.
        float DurationSeconds() const;

        AZStd::string m_name;

        // Aseprite binding (preferred when set): play this tag on the target's Aseprite component.
        AZStd::string m_asepriteTag;

        // Sprite-sheet binding (used when no Aseprite tag is set and m_frameCount > 0).
        int m_columns = 1;
        int m_rows = 1;
        int m_frameCount = 0;
        float m_fps = 12.0f;
        bool m_loop = true;

        //! Explicit clip duration for exit-time; 0 = derive from frameCount / fps.
        float m_duration = 0.0f;
    };

    //! Configuration for the animation state machine: the parameter blackboard, the
    //! states with their animation bindings, the transitions, the initial state, and
    //! which entity's Sprite/Aseprite to drive (invalid = this entity).
    class DioramaAnimStateMachineConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaAnimStateMachineConfig, DioramaAnimStateMachineConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaAnimStateMachineConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaAnimStateMachineConfig() override = default;

        AZStd::vector<AnimParamData> m_parameters;
        AZStd::vector<AnimStateData> m_states;
        AZStd::vector<AnimTransitionData> m_transitions;
        AZStd::string m_defaultState; //!< Initial state name; empty = first state.
        AZ::EntityId m_target; //!< Entity whose Sprite/Aseprite to drive; invalid = self.
        //! Advance on the 2D Simulation Clock's fixed steps instead of the render
        //! tick (deterministic; falls back to the render tick when no clock runs).
        bool m_useSimClock = false;
    };

    //! Resolve the authored, name-based config into the pure AnimSM definition: the
    //! parameter values (from defaults), the transitions (names -> indices), and the
    //! per-state durations. Names that do not resolve drop their condition/edge so a
    //! malformed graph degrades gracefully rather than crashing. Exposed for unit tests.
    struct AnimStateMachineDefinition
    {
        AZStd::vector<AnimSM::Transition> m_transitions;
        AZStd::vector<float> m_durations; //!< One per state, aligned with config.m_states.
        AnimSM::Runtime m_runtime; //!< Params sized + defaulted; current state unset.
        int m_defaultStateIndex = -1;
    };
    AnimStateMachineDefinition BuildAnimStateMachineDefinition(const DioramaAnimStateMachineConfig& config);

    //! Runtime 2D animation state machine. Each tick it advances the pure AnimSM::Runtime
    //! and, when the state changes, plays the new state's bound animation on the target
    //! (an Aseprite tag or a sprite sheet) and emits OnStateChanged. Driven at runtime
    //! through DioramaAnimStateMachineRequestBus for AI/human parity. It owns no
    //! rendering: it composes with the Sprite/Aseprite components that do.
    class DioramaAnimStateMachineComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaAnimStateMachineRequestBus::Handler
        , protected DioramaSimTickNotificationBus::Handler
        , protected DioramaSimStateParticipantBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaAnimStateMachineComponent, DioramaAnimStateMachineComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DioramaAnimStateMachineComponent() = default;
        explicit DioramaAnimStateMachineComponent(const DioramaAnimStateMachineConfig& config);
        ~DioramaAnimStateMachineComponent() override = default;

        //! Snapshot chunk: graph runtime (current state, time, params) ('ANSM' little-endian).
        static constexpr AZ::u32 AnimStateMachineChunkTag = 0x4D534E41; // 'ANSM' little-endian

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSimTickNotifications (Use Simulation Clock mode)
        void OnSimTick(AZ::s64 frame, float stepSeconds) override;

        // DioramaSimStateParticipantBus (snapshot/restore of the graph runtime)
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;

    private:
        //! Advance the graph by `deltaTime` (render tick or fixed sim step).
        void Advance(float deltaTime);

    protected:
        // DioramaAnimStateMachineRequests
        void SetBool(const AZStd::string& name, bool value) override;
        void SetFloat(const AZStd::string& name, float value) override;
        void SetTrigger(const AZStd::string& name) override;
        void ResetTrigger(const AZStd::string& name) override;
        void Play(const AZStd::string& stateName) override;
        void SetUseSimClock(bool enabled) override;
        AZStd::string GetCurrentState() override;
        bool GetBool(const AZStd::string& name) override;
        float GetFloat(const AZStd::string& name) override;

    private:
        //! Index of a parameter by name, or -1.
        int FindParam(const AZStd::string& name) const;
        //! Index of a state by name, or -1.
        int FindState(const AZStd::string& name) const;
        //! Target entity to drive (m_config.m_target if valid, else this entity).
        AZ::EntityId TargetEntity() const;
        //! Enter a state: play its animation on the target and fire OnStateChanged.
        void EnterState(int stateIndex);

        DioramaAnimStateMachineConfig m_config;
        AnimStateMachineDefinition m_definition;
    };
} // namespace Diorama
