# Design: 2D collision and triggers reachable from gameplay scripts

Status: **shipped.** Gem-native colliders, triggers, contact/trigger events, and
queries (`OverlapCircle`/`OverlapBox`/`Raycast2D`), plus pushbox resolution
(`ComputeBoxPushOut` / `Collision2D::MinimumTranslation`). The names below match what
shipped; see the CHANGELOG and [how-to 21](../howto/21-fighting.md) (pushbox).

## Problem

The twin-stick sample wanted PhysX collisions but could not use them, for two
independent reasons confirmed in the engine source:

1. **Scope.** PhysX exposes its collision/trigger *convenience notification buses*
   (`AutomationCollisionNotificationsBus` / `AutomationTriggerNotificationsBus`,
   declared in `Code/Framework/AzFramework/AzFramework/Physics/Common/PhysicsSimulatedBodyAutomation.h`
   in the 26.05 SDK). Their handler's `Reflect` tags them with
   `AZ::Script::Attributes::Scope = ScopeFlags::Automation`; that `.cpp` is not
   shipped as source in the SDK (the SDK carries it only as a built library), but
   the scope attribute is confirmed on the engine `development` branch and, more
   tellingly, by lived behavior: the twin-stick sample and other gems could not
   receive collisions in launcher Lua. `ScopeFlags` is defined in
   `AzCore/Script/ScriptContextAttributes.h` (verified: `Automation = 1 << 1`,
   `Launcher = 1 << 0`, `Common = Launcher | Automation`), so an `Automation`-only
   binding excludes `Launcher`, and the obvious `CollisionNotificationBus.Connect`
   path a gameplay script reaches for is nil in the GameLauncher. There *is* a
   narrower existing path: on `development` the `SimulatedBody` class itself is
   reflected at `Common` scope with `GetOnCollisionBeginEvent()` / `Persist` / `End`
   getters returning `AZ::Event` sources a launcher script can connect to (the
   `.cpp` is likewise not in the SDK source, so re-verify against a build before
   relying on it). But that path requires the entity to *have a PhysX simulated
   body* in the first place, which is exactly what reason 2 makes fragile.
2. **Authored transform.** A dynamic rigid body initializes at the world origin
   rather than the entity's authored editor transform at simulation start, so even
   placement is fragile for the kind of statically-placed actors a 2D scene has.

What *is* reachable from launcher Lua: **scene queries** (raycast / shapecast /
overlap) via `AzPhysics::SceneInterface::QueryScene`
(`AzFramework/Physics/PhysicsScene.h`, requests in
`AzFramework/Physics/Common/PhysicsSceneQueries.h`), given a `SceneHandle`. And
C++ math helpers `AZ::ShapeIntersection::Overlaps(...)` for Aabb/Sphere/Capsule
(`AzCore/Math/ShapeIntersection.h`), though those are C++-only, not reflected.

## Decision

**Build a gem-native 2D collision system**, and **separately propose the upstream
scope fix** as an engine contribution.

Why gem-native is the primary:

- It is decoupled from PhysX entirely, so it sidesteps both the scope wall and the
  origin-at-sim-start bug.
- It matches how Diorama gameplay already works: transform-driven, no rigid bodies
  (the sample, the AI-native bus model). Collision becomes a query/notify layer
  over transforms, not a simulation we fight.
- 2D collision is cheap and well-understood (circle/box on a plane); a focused
  implementation is small, fast, and fully under our control and verifiable.
- It fits the VISION security/perf criteria: we bound everything and allocate
  nothing per frame.

Why also contribute upstream: reflecting the collision/trigger *convenience* buses
at `ScopeFlags::Common` (so the natural `CollisionNotificationBus.Connect` works in
launcher scripts), and/or shipping a docs sample of the existing `SimulatedBody`
event-getter path, would help *every* O3DE game that does use PhysX bodies, not just
Diorama. That is exactly the "found broken, contribute back" the project commits to.
Note: gem-native collision and the upstream PhysX-scope improvement are
complementary, not redundant: Diorama gameplay is transform-driven (no simulated
bodies), so it wants the gem-native path regardless, while PhysX-body games want the
scope fix. **The upstream change requires explicit go-ahead before filing anything
on o3de/o3de** (per standing rule); it is recorded here as a candidate, not an
action, and tracked as finding #8 in the project's upstream-contribution notes.

## The gem-native system

### Components

`Diorama2DColliderComponent` (runtime + editor twin, sharing one config):

- Shape: `Circle { radius }` or `Box { halfExtents (2D) }`, plus a 2D `offset`.
- `m_isTrigger` (bool): trigger fires enter/exit only, no overlap reporting to the
  contact path.
- `m_layer` (one of N named layers) and `m_collidesWith` (a layer bitmask): the
  standard 2D layer/mask matrix, so "player hearts" hit "haters" but not each other.
- Operates on a **configurable 2D plane** (default the sprite plane). Positions come
  from the entity `TransformBus`; the third axis is ignored for overlap, optionally
  band-limited so different 2.5D height bands do not collide.

### System

A `Diorama2DPhysicsSystem` (a system component / feature-processor-adjacent system),
ticked once per frame:

1. Gather active colliders and their current world positions (from transforms).
2. **Broadphase**: a uniform spatial hash on the 2D plane (cell ~ largest collider),
   reused across frames, no per-frame heap allocation. v1 may start with a simple
   sort-and-sweep on one axis; v2 the hash for scale.
3. **Narrowphase**: circle/circle, circle/box, box/box overlap tests (`AZ::Sphere`
   / `AZ::Aabb` helpers where they fit; SAT for rotated boxes in v2).
4. Diff this frame's overlapping pairs against last frame's to emit begin / stay /
   end. Per-pair state lives in a reused map keyed by an ordered entity-id pair.

### Buses (the whole point: reachable from gameplay scripts)

`Diorama2DCollisionNotificationBus` (EBus addressed by EntityId), reflected with
`ScopeFlags::Common` so launcher Lua, editor, Python, and Script Canvas all see it:

- `OnContactBegin(otherEntityId, contactPoint, normal)`
- `OnContactStay(otherEntityId, contactPoint, normal)`
- `OnContactEnd(otherEntityId)`
- `OnTriggerEnter(otherEntityId)` / `OnTriggerExit(otherEntityId)`

`Diorama2DCollisionRequestBus` (global + per-entity), also `Common`:

- `OverlapCircle(center2d, radius, layerMask) -> EntityId[]`
- `OverlapBox(center2d, halfExtents, layerMask) -> EntityId[]`
- `Raycast2D(origin2d, dir2d, distance, layerMask) -> { entityId, point, normal, t }`
- `SetLayer / SetCollidesWith / SetRadius / SetEnabled` per collider.

These are documented, clamped, scalar-argument verbs in the same AI-native style as
`DioramaSpriteRequestBus`, so an agent can wire collision the same way it wires
sprites. The twin-stick sample's manual distance loop collapses into
`OnContactBegin` plus `OverlapCircle` queries.

## Security and performance

- All counts (colliders, contacts, query results) are bounded and validated; query
  result arrays are capped and the cap is logged if hit (no silent truncation).
- Spatial hash and pair-state containers are reused; the per-frame path does no heap
  allocation, consistent with the render-loop rule.
- Asset/inspector inputs (radius, extents, layer index) are validated and clamped at
  load and on every setter.

## Phasing

1. **v1**: circle colliders + triggers, layer/mask matrix, sort-and-sweep
   broadphase, `OnContact*`/`OnTrigger*` notifications, `OverlapCircle` + `Raycast2D`
   queries, all reflected `Common`. Retrofit the twin-stick sample to it (a strong
   dogfood + a visible simplification of the sample).
2. **v2**: box colliders + SAT, spatial-hash broadphase for scale, `OverlapBox`.
3. **v3**: optional simple collision response helpers (push-out / slide) for users
   who want bodies that stop at walls without writing the resolution themselves.

## Verification plan (needs the monitor / a run)

- Two circle colliders driven together by transforms: `OnContactBegin` fires once on
  touch, `OnContactEnd` once on separation; no duplicate/missed events across frames.
- A trigger zone: `OnTriggerEnter`/`Exit` fire exactly once per crossing.
- `OverlapCircle` and `Raycast2D` from Lua return the expected entities/hit.
- Retrofit twin-stick: hearts befriend haters via `OnContactBegin` and the game plays
  identically to the distance-loop version, with simpler scripts.
- Profile a few hundred colliders: no per-frame allocation, stable frame time.

## Open questions

- The collision plane: RESOLVED. The collider has a configurable plane (XY / XZ /
  YZ); the default is the XY screen plane, which is what the twin-stick moves on
  (confirmed in-game). A band limit for 2.5D height separation on the third axis is
  a possible future refinement.
- `SceneHandle` acquisition from launcher Lua for the *PhysX* scene-query fallback
  (if we ever want gem colliders to also see PhysX geometry): document how to obtain
  it, or keep the two worlds separate in v1.
