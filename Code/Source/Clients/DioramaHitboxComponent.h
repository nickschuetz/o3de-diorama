/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/Collider2DComponent.h> // CollisionPlane
#include <Clients/HitboxFrames.h>
#include <Clients/HitboxRigBus.h>
#include <Clients/SimStateBus.h>
#include <Diorama/DioramaHitboxBus.h>
#include <Diorama/DioramaSimClockBus.h>
#include <Diorama/SpriteBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>

// The pure HitboxFrames types live in a dependency-free header, so their
// reflection type info is specialized here (the one place that reflects them).
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::HitboxFrames::BoxKind, "{4B5C6D7E-8F90-41A2-B3C4-D5E6F7081929}");
    AZ_TYPE_INFO_SPECIALIZE(Diorama::HitboxFrames::GuardHeight, Diorama::DioramaGuardHeightTypeId);
    AZ_TYPE_INFO_SPECIALIZE(Diorama::HitboxFrames::HitProperties, Diorama::DioramaHitPropertiesTypeId);
} // namespace AZ

namespace Diorama
{
    //! Reflect the pure HitProperties payload (serialize + Inspector rows + read-only
    //! script properties). Called from the hitbox config's Reflect.
    void ReflectHitProperties(AZ::ReflectContext* context);

    //! One authored box on the rig: its kind, in-plane offset and half extents, and
    //! the inclusive animation-frame window it is live on. Mirrors the pure
    //! HitboxFrames::Box, adding only what the editor needs to author it.
    struct DioramaHitboxData final
    {
        AZ_TYPE_INFO(Diorama::DioramaHitboxData, DioramaHitboxDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        HitboxFrames::BoxKind m_kind = HitboxFrames::BoxKind::Hurtbox;
        AZ::Vector2 m_offset = AZ::Vector2(0.0f, 0.0f);
        AZ::Vector2 m_halfExtents = AZ::Vector2(0.5f, 0.5f);
        int m_startFrame = 0;
        int m_endFrame = 0;
        HitboxFrames::HitProperties m_hit; //!< attack payload (Hitbox / Throwbox)
    };

    //! Configuration for the frame-data hitbox rig. The boxes are authored once; which
    //! are live is decided each frame by the current animation frame.
    class DioramaHitboxConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaHitboxConfig, DioramaHitboxConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaHitboxConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaHitboxConfig() override = default;

        //! World plane the boxes live in (default XY, the screen plane).
        CollisionPlane m_plane = CollisionPlane::XY;
        //! Category bit(s) this rig's receiving boxes (hurt / armor / throwable)
        //! register on (queryable geometry).
        AZ::u32 m_hurtLayer = 0x0100;
        //! Layer mask this rig's attacking boxes test against. Defaults to the hurt
        //! layer so two fighters with the stock config strike each other's hurtboxes.
        AZ::u32 m_targetMask = 0x0100;
        //! Category bit(s) this rig's pushboxes register on, and the mask its own
        //! pushbox separation tests against.
        AZ::u32 m_pushLayer = 0x0200;
        //! +1 faces +X, -1 mirrors every box's X offset.
        int m_facing = 1;
        //! The authored boxes.
        AZStd::vector<DioramaHitboxData> m_boxes;
        //! Advance on the 2D Simulation Clock's fixed steps instead of the render
        //! tick (deterministic; falls back to the render tick when no clock runs).
        bool m_useSimClock = false;
        //! Apply half the computed pushbox push-out to this entity each evaluation
        //! (overlapping pairs split the separation and converge). Off by default:
        //! the push-out is only reported through GetHitboxInfo.
        bool m_autoSeparate = false;
    };

    //! Runtime frame-data hitbox component. Each frame it reads the animation frame
    //! (from OnAnimationFrame, or a manual SetFrame), decides which boxes are live via
    //! the pure HitboxFrames core, registers the live hurtboxes as static collision
    //! geometry, and queries the live hitboxes against the target layer. A new overlap
    //! fires OnHit on the attacker and OnHurt on the target, once per active window.
    //! This is the component form of the documented per-frame-hitbox Lua pattern,
    //! generalized to a whole rig of boxes with per-box frame data.
    class DioramaHitboxComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaSpriteNotificationBus::Handler
        , protected DioramaHitboxRequestBus::Handler
        , protected DioramaSimStateParticipantBus::Handler
        , protected DioramaSimTickNotificationBus::Handler
        , protected DioramaHitboxRigBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaHitboxComponent, DioramaHitboxComponentTypeId);

        //! Chunk tag for the rig's frame/facing/per-box window state.
        static constexpr AZ::u32 HitboxChunkTag = 0x584F4248; // 'HBOX' little-endian

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        DioramaHitboxComponent() = default;
        explicit DioramaHitboxComponent(const DioramaHitboxConfig& config);
        ~DioramaHitboxComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaSimTickNotifications (Use Simulation Clock mode)
        void OnSimTick(AZ::s64 frame, float stepSeconds) override;

        // DioramaSpriteNotifications
        void OnAnimationFrame(int frameIndex) override;

        // DioramaHitboxRequests
        void SetFacing(int facing) override;
        int GetFacing() override;
        void SetFrame(int frame) override;
        void SetUseSimClock(bool enabled) override;
        void SetAutoSeparate(bool enabled) override;
        DioramaHitboxInfo GetHitboxInfo() override;

        // DioramaHitboxRigBus (the registry attacking rigs pair against)
        AZ::EntityId GetRigEntity() const override;
        AZ::u32 GetRigReceiveLayer() const override;
        const AZStd::vector<ActiveRigBox>& GetRigActiveBoxes() const override;

        // DioramaSimStateParticipants (rollback snapshot: frame, facing, and each
        // box's active-window bookkeeping, so a restored swing re-resolves hits
        // exactly as the original did)
        void SaveSimState(SimState::Writer& writer) override;
        bool TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload) override;

    private:
        //! Project the entity's world translation onto the configured collision plane.
        AZ::Vector2 PlaneCenter() const;
        //! In-plane center of a box, mirrored by facing.
        AZ::Vector2 BoxCenter(const DioramaHitboxData& box, const AZ::Vector2& origin) const;
        //! Rebuild the active-box snapshot and static geometry, then resolve typed
        //! interactions for `frame`: legacy collider hits, the rig-pair matrix
        //! (hit / clash / armor / throw / proximity), and pushbox separation.
        void Evaluate();
        //! First of `theirs` with the given kind overlapping `mine` (authoring order,
        //! so the choice is deterministic), or nullptr.
        const ActiveRigBox* FindOverlapping(
            const ActiveRigBox& mine, const AZStd::vector<ActiveRigBox>& theirs, HitboxFrames::BoxKind kind) const;
        //! Fill and deliver one DioramaBoxEvent to both parties. Roles are explicit
        //! so a hit-vs-hit contest fires one event per side (each side as attacker
        //! with its own box result and payload).
        void FireBoxEvent(
            HitboxFrames::BoxResult result,
            AZ::EntityId attacker,
            const ActiveRigBox& attackerBox,
            AZ::EntityId defender,
            const ActiveRigBox& defenderBox,
            const HitboxFrames::HitProperties& payload);
        //! Move this entity by an in-plane delta (pushbox auto-separation).
        void ApplyPlaneTranslation(const AZ::Vector2& delta);

        DioramaHitboxConfig m_config;
        int m_frame = 0;
        AZ::Vector3 m_worldTranslation = AZ::Vector3::CreateZero();
        AZ::Vector2 m_pushOut = AZ::Vector2(0.0f, 0.0f); //!< last evaluation's pushbox push-out

        //! Per-box state, aligned to m_config.m_boxes. Tracks whether the box was live
        //! last evaluation (to reset on a fresh activation), which targets its current
        //! activation has already struck/grabbed/signalled (one event per opponent per
        //! window), and which opponents it has already contested hit-vs-hit (clash and
        //! strike stay independent so trades work).
        struct BoxState
        {
            bool m_wasActive = false;
            AZStd::unordered_set<AZ::EntityId> m_alreadyHit;
            AZStd::unordered_set<AZ::EntityId> m_alreadyContested;
        };
        AZStd::vector<BoxState> m_boxState;

        // Reused per-evaluation so steady state allocates nothing.
        AZStd::vector<Collision2D::Collider> m_staticScratch; //!< hurt + push static geometry
        AZStd::vector<ActiveRigBox> m_activeBoxScratch; //!< published rig snapshot
        AZStd::vector<AZStd::pair<AZ::EntityId, const DioramaHitboxRigs*>> m_rigScratch; //!< sorted other rigs
        int m_activeHitboxes = 0;
        int m_activeHurtboxes = 0;
        int m_activePushboxes = 0;
        int m_activeThrowboxes = 0;
        int m_activeThrowableBoxes = 0;
        int m_activeArmorBoxes = 0;
        int m_activeProximityBoxes = 0;
    };
} // namespace Diorama
