# Design: typed interaction boxes with attack payloads

Status: **in progress; phases A (pure core), B (component, OnBoxEvent, pushbox
separation), and C (world-space box overlay) shipped; sample (D) remains.** The
overlay is runtime-only (game mode + launcher), matching the component's existing
no-editor-preview stance. Companion doc:
[2d-deterministic-sim.md](2d-deterministic-sim.md) (fixed-tick sim + rollback-ready
state). Driver: fighting games of the SF6 / GGST class. The shipped frame-data rig
(`DioramaHitboxComponent`, [howto/21-fighting.md](../howto/21-fighting.md)) proved
the shape: boxes live on animation-frame windows, hits resolve once per active
window. This doc deepens it from two box kinds and a bare "who hit whom" event to
the full interaction vocabulary a serious fighter needs.

Names stay functional (pushbox, throwbox, armor), never genre-branded: the same
boxes serve brawlers, platformer combat, and action RPGs.

## Problem

The v1 rig has exactly two kinds (`HitboxFrames::BoxKind`: Hurtbox, Hitbox) and its
events carry only the other entity (`OnHit(target)` / `OnHurt(attacker)`):

1. **No attack data on the box.** Damage, hitstun, blockstun, hitstop, pushback,
   guard height, launch: the game must re-derive all of it from "which move was I
   doing", which is exactly the bookkeeping a frame-data rig exists to remove.
2. **Missing categories.** No pushbox (body collision), throwbox / throwable,
   armor, or proximity (guard-trigger) boxes, so throws, armored moves, and
   proximity guard cannot be expressed on the rig.
3. **No hit-vs-hit story.** Two active hitboxes meeting (clash / priority) is
   unhandled.
4. **Invisible boxes.** No way to see the live rig in the viewport; tuning frame
   data blind is the single biggest authoring pain, and a training-mode box display
   is table stakes in the genre.

## Decision

### 1. Box kinds (pure core extension)

`HitboxFrames::BoxKind` grows to:

| Kind | Role |
| --- | --- |
| `Hurtbox` | vulnerable (as today) |
| `Hitbox` | deals a hit while active (as today) |
| `Pushbox` | body volume; overlapping pushboxes separate positionally |
| `Throwbox` | grabs while active |
| `ThrowableBox` | can be grabbed (throw-invulnerable states simply deactivate it) |
| `ArmorBox` | absorbs hits while active (hyper armor windows) |
| `ProximityBox` | presence signal (proximity guard, spacing AI) |

Each box keeps its v1 frame window `[startFrame, endFrame]`; activation logic is
unchanged and stays pure-tested.

### 2. Hit properties (the payload)

A `HitProperties` struct authored **per box** (meaningful on `Hitbox` /
`Throwbox`), carried by the box and delivered with the event:

- `damage` (float)
- `hitstunFrames`, `blockstunFrames`, `hitstopFrames` (int, sim frames)
- `pushback` (Vector2, applied to the defender; attacker pushback stays game-side)
- `guardHeight` (int enum: 0 any, 1 high, 2 low, 3 unblockable)
- `launch` (Vector2, zero means grounded reaction)
- `priority` (int, clash resolution)
- `customId` (u32, opaque to the gem: move id, sound table key, whatever the game
  wants to key on)

The gem stores and delivers these numbers; it does **not** interpret them (it never
applies damage or stun itself). Interpretation is the game's combat script. That
keeps the boundary clean: primitives in the gem, fighting systems (combo scaling,
meters, cancel tables, character state machines) game-side.

### 3. Interaction matrix (pure core)

A pure `Resolve(kindA, kindB)` decides the outcome; the component fires events
accordingly, with the same once-per-(box, target, window) rule v1 established:

- `Hitbox` vs `Hurtbox` -> **Hit** (as today)
- `Hitbox` vs `Hitbox` (both active) -> equal `priority`: **Clash** to both sides;
  unequal: higher priority scores a **Hit**, lower receives **Beaten**. Trades
  (both connect) need no special case: they are two independent Hit resolutions in
  the same frame, which already works.
- `Hitbox` vs `ArmorBox` -> **Absorbed** (both sides notified; the attacker knows
  it hit armor, the defender's script spends its armor hit)
- `Throwbox` vs `ThrowableBox` -> **Throw**
- `Pushbox` vs `Pushbox` -> not an event: positional separation through the
  existing `ComputeBoxPushOut` path, with an opt-in auto-apply on the component
  (or read `GetPushOut()` and apply game-side)
- `ProximityBox` vs `Hurtbox` -> **Proximity** (signal only; the defender's script
  decides to enter guard)

### 4. Events (additive, non-breaking)

`OnHit` / `OnHurt` keep firing exactly as today. New on the same notification bus:

`OnBoxEvent(event)` where the event struct carries: result (Hit / Clash / Beaten /
Absorbed / Throw / Proximity), attacker and defender entity ids, the box index on
each side, an approximate world contact point (for spark placement), and the
attacker box's `HitProperties`. Reflected read-only to Lua / Script Canvas /
Python (parity + agent verify, like every Diorama Info struct).

### 5. Box overlay (world-space debug draw)

A viewport visualizer for the live rig, color-coded by kind (hurt green, hit red,
push yellow, throw purple, armor blue, proximity gray), drawn as world-space
translucent quads through the existing sprite batch path. Toggled per rig (bus +
Inspector) and globally (console variable). Works in editor and launcher; it is
the training-mode box display and the frame-data tuning loop. World-space drawing
only, so it stays inside the gem's scope line.

### 6. Determinism tie-in

All resolution iterates in stable (sorted) order, and every piece of per-window
state (active flags, already-hit sets, armor spent) participates in
`SaveSimState` / `RestoreSimState` from the companion doc, so a restored frame
re-resolves identically.

## Phases

- **A (S): core.** Kind enum, `HitProperties`, `Resolve` matrix in the pure
  header; unit tests for every cell of the matrix and the window edge cases.
- **B (M): component.** Authoring (Inspector rows per box), runtime resolution and
  the new event, generalized once-per-window bookkeeping, pushbox auto-separation
  toggle, parity verbs, `GetHitboxInfo` extended with per-kind active counts.
- **C (S): overlay.** Per-rig + global toggles, color table, launcher + editor.
- **D (S): sample + docs.** The fighting example moves its damage/hitstun numbers
  onto the boxes, demonstrates armor and throw and a clash, and how-to 21 grows
  the new sections.

## Acceptance criteria

- **Security:** payloads are plain bounded data; no script execution from assets;
  event structs validated like every reflected struct.
- **Performance:** resolution stays O(active boxes) with the scratch-buffer reuse
  the v1 component already has; the overlay costs nothing when off.
- **Efficiency:** v1 rigs keep working untouched (new kinds and payload fields
  default to v1 behavior).
- **Ease of use:** author everything on the component in the Inspector; one event
  carries everything a combat script needs; the overlay makes frame data visible.

## Out of scope

Damage/stun application, combo scaling, meters, cancel tables, character state
machines, throw teching policy, screen-space HUD (LyShine), netcode (companion doc
covers rollback readiness only).
