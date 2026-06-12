/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/InputActionMap.h>
#include <Clients/MotionInput.h>
#include <Diorama/DioramaInputBus.h>
#include <Diorama/DioramaSimClockBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>

// The pure InputMap enums live in a dependency-free header, so their reflection
// type info is specialized here (the one place that reflects them for the editor).
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::InputMap::ActionKind, Diorama::InputActionKindTypeId);
    AZ_TYPE_INFO_SPECIALIZE(Diorama::InputMap::Axis, Diorama::InputAxisTypeId);
} // namespace AZ

namespace Diorama
{
    //! One authored binding: an input channel name (e.g. "keyboard_key_edit_W" or
    //! "gamepad_thumbstick_l_x") scaled into an action, with an axis selector for 2D.
    struct DioramaInputBindingData final
    {
        AZ_TYPE_INFO(Diorama::DioramaInputBindingData, DioramaInputBindingDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_channel;
        float m_scale = 1.0f;
        InputMap::Axis m_axis = InputMap::Axis::X;
    };

    //! One authored action: a name, what it produces, and its bindings.
    struct DioramaInputActionData final
    {
        AZ_TYPE_INFO(Diorama::DioramaInputActionData, DioramaInputActionDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        InputMap::ActionKind m_kind = InputMap::ActionKind::Button;
        float m_deadZone = 0.0f;
        float m_pressThreshold = 0.5f;
        AZStd::vector<DioramaInputBindingData> m_bindings;
    };

    //! One authored motion (e.g. quarter-circle-forward) recognized from the recent
    //! direction history. The sequence is numpad notation (the fighting-game standard:
    //! "236" is down, down-forward, forward); the window is how recently every step
    //! must have been entered, in seconds.
    struct DioramaMotionData final
    {
        AZ_TYPE_INFO(Diorama::DioramaMotionData, DioramaMotionDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        AZStd::string m_sequence = "236";
        float m_windowSeconds = 0.4f;
    };

    //! Configuration for the input action-mapping surface: the list of named actions,
    //! plus the optional motion-input layer (a directional action to quantize and the
    //! motions to recognize from its history).
    class DioramaInputConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaInputConfig, DioramaInputConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaInputConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaInputConfig() override = default;

        AZStd::vector<DioramaInputActionData> m_actions;

        //! Name of the Axis2D action whose value is quantized into a numpad direction
        //! each frame (empty disables motion recognition). Up must be +Y, forward +X.
        AZStd::string m_directionAction;
        //! Stick magnitude below which the direction reads as centered (neutral).
        float m_directionDeadZone = 0.4f;
        //! Motions recognized from the direction history.
        AZStd::vector<DioramaMotionData> m_motions;

        //! Sample input once per fixed simulation step (the 2D Simulation Clock's
        //! OnSimTick) instead of per render tick, recording each frame's action states
        //! into a ring so the recent history is queryable and injectable by frame
        //! (deterministic sim phase C). Requires a 2D Simulation Clock in the level.
        bool m_useSimClock = false;
        //! Ring capacity in simulation frames (how far back WasPressedAtFrame reads
        //! and how far ahead InjectActionState writes). 120 at 60 steps/s = 2 seconds.
        int m_historyFrames = 120;
    };

    //! Resolve the authored, channel-name-based config into the pure InputMap actions
    //! plus the de-duplicated list of channel names each action's bindings reference
    //! (binding source indices point into that list). Exposed for unit tests.
    struct InputActionMapDefinition
    {
        AZStd::vector<InputMap::Action> m_actions; //!< Aligned with config.m_actions.
        AZStd::vector<AZStd::string> m_sourceChannels; //!< Distinct channel names, source index = position.
    };
    InputActionMapDefinition BuildInputActionMapDefinition(const DioramaInputConfig& config);

    //! Runtime input action-mapping surface. Listens to the live input channels, caches
    //! the values of the channels it cares about, and each tick evaluates every action
    //! through the pure InputMap core, exposing named values/edges over
    //! DioramaInputRequestBus and firing press/release notifications. This is the
    //! rebindable layer between raw input and gameplay, at parity with the rest of
    //! Diorama (a human authors actions in the Inspector; a script/agent reads them).
    class DioramaInputComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaInputRequestBus::Handler
        , protected DioramaSimTickNotificationBus::Handler
        , public AzFramework::InputChannelEventListener
    {
    public:
        AZ_COMPONENT(Diorama::DioramaInputComponent, DioramaInputComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DioramaInputComponent();
        explicit DioramaInputComponent(const DioramaInputConfig& config);
        ~DioramaInputComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        // DioramaInputRequests
        bool IsPressed(const AZStd::string& action) override;
        bool WasPressedThisFrame(const AZStd::string& action) override;
        bool WasReleasedThisFrame(const AZStd::string& action) override;
        float GetValue(const AZStd::string& action) override;
        float GetValueY(const AZStd::string& action) override;
        bool WasMotionPerformed(const AZStd::string& motion) override;
        bool WasPressedAtFrame(const AZStd::string& action, AZ::s64 frame) override;
        float GetValueAtFrame(const AZStd::string& action, AZ::s64 frame) override;
        float GetValueYAtFrame(const AZStd::string& action, AZ::s64 frame) override;
        void InjectActionState(AZ::s64 frame, const AZStd::string& action, float x, float y, bool pressed) override;

        // DioramaSimTickNotifications (sim-clock mode: one input sample per fixed step)
        void OnSimTick(AZ::s64 frame, float stepSeconds) override;

    private:
        //! Index of an action by name, or -1.
        int FindAction(const AZStd::string& name) const;
        //! Index of a motion by name, or -1.
        int FindMotion(const AZStd::string& name) const;
        //! Quantize the direction action, append a sample on change, and re-evaluate
        //! every motion, firing OnMotionPerformed on a fresh match. Called each tick.
        void UpdateMotions(float nowSeconds);

        DioramaInputConfig m_config;
        InputActionMapDefinition m_definition;
        AZStd::vector<float> m_sources; //!< Live value per source channel (aligned to m_definition.m_sourceChannels).
        AZStd::vector<InputMap::ActionValue> m_values; //!< Evaluated state per action (aligned to m_config.m_actions).

        //! Parsed numpad sequences (aligned to m_config.m_motions); built on Activate.
        AZStd::vector<AZStd::vector<MotionInput::Direction>> m_motionSequences;
        AZStd::vector<MotionInput::Sample> m_directionHistory; //!< Recent directions, oldest first.
        AZStd::vector<bool> m_motionMatched; //!< Per-motion match state last tick (edge detection).
        int m_directionActionIndex = -1; //!< Cached index of m_config.m_directionAction, or -1.

        //! One simulation frame's recorded action states (sim-clock mode). A slot is
        //! valid for frame f only when m_frame == f (the ring reuses slots).
        struct FrameRecord
        {
            AZ::s64 m_frame = -1;
            bool m_injected = false; //!< Pre-written by InjectActionState; do not sample over it.
            AZStd::vector<InputMap::ActionValue> m_values; //!< Aligned to m_config.m_actions.
        };
        //! The input ring (sim-clock mode): m_ring[frame % capacity]. Deliberately NOT
        //! part of the snapshot/restore image: rollback restores state and then
        //! re-simulates USING this history (plus injected corrections), so the ring
        //! must survive a restore (the input log lives outside the state).
        AZStd::vector<FrameRecord> m_ring;
        //! Ring slot for a frame, or nullptr when that frame's record is absent.
        FrameRecord* RecordForFrame(AZ::s64 frame);
        const InputMap::ActionValue* ValueAtFrame(const AZStd::string& action, AZ::s64 frame);
    };
} // namespace Diorama
