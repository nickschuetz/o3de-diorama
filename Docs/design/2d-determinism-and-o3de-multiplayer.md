# Decision: the 2D Simulation Clock vs. O3DE Multiplayer

Status: **decided (complementary; do not overlap).** This doc exists because the 2D
Simulation Clock and its snapshot/restore look, at a glance, like they might reproduce
O3DE's Multiplayer gem, which would violate the project's hard rule: *do not build a
parallel system to a core O3DE feature; if a planned feature overlaps an engine system,
stop and reconsider* (see [VISION.md](../../VISION.md) boundaries and the
`complementary-not-competing` working rule). We stopped and reconsidered. The verdict is
that they solve **different problems** and must stay in their lanes; this doc draws the
line so future work does not drift across it.

## The question

Diorama ships a **2D Simulation Clock** (fixed-step `OnSimTick`, seeded RNG) and a
**snapshot/restore** layer (`SaveSimState`/`TryRestoreChunk` per component, gathered by the
clock's `CaptureFrame`/`RestoreFrame` into `SaveToSlot`/`RestoreFromSlot`). O3DE's
**Multiplayer gem** also keeps per-frame state history and calls its mechanism "rollback."
Are we duplicating the engine?

## What O3DE Multiplayer actually is

Grounded in the 26.05.0 SDK headers (`/opt/O3DE/26.05.0/Gems/Multiplayer/`) and the O3DE
docs (the SDK ships headers only, so algorithm details are from docs + signatures):

- **Client-server only, server-authoritative.** `MultiplayerAgentType` is `Client` /
  `ClientServer` (listen server) / `DedicatedServer` (`IMultiplayer.h`). There is **no
  peer-to-peer** and **no offline/local mode**; every path assumes a network session with an
  authority.
- **"Rollback" here = client prediction + server backward-reconciliation** (the
  Overwatch/Source lineage). The autonomous client predicts and is **corrected** from
  authoritative state (`LocalPredictionPlayerInputComponent`'s
  `HandleSendClientInputCorrection`, `m_predictiveStateHistory`, `m_inputHistory`); the
  server **rewinds other entities within a spatial volume** to lag-compensate hit detection
  (`INetworkTime::AlterTime(..., ConnectionId)`, `SyncEntitiesToRewindState(rewindVolume)`).
- **Determinism is neither required nor guaranteed.** The model *expects* prediction to
  diverge and corrects it; there is a whole **Desync Audit Trail** for when client and
  server disagree on a networked value. Correction, not lock-step determinism, is the source
  of truth.
- **No network-free state snapshot/restore.** State capture rides `NetBindComponent`
  (`SerializeEntityCorrection`, rewindable network properties in a 128-deep ring keyed by the
  network `HostFrameId`). There is no supported "capture the whole sim to a buffer and
  restore it" call that works without a live session.
- **~30 Hz network tick**, cvar-driven (`cl_InputRateMs=33`, `sv_serverSendRateMs=50`),
  interpolation-smoothed, not a 60 Hz frame-locked simulation.
- **No fighting-game / GGPO evidence.** The reference title is a server-authoritative
  shooter (`o3de-multiplayersample`). Peer-to-peer deterministic rollback ("rollback
  netcode" of the GGPO / Fightcade class) is not a documented or supported use case; it is
  middleware territory.
- **Heavy coupling to participate.** A component must become a codegen'd
  `*.AutoComponent.xml` multiplayer auto-component with a `NetBindComponent`, mark state as
  `IsRewindable`/`IsPredictable` network properties, and route mutation through
  `CreateInput`/`ProcessInput`. You cannot get the rewind machinery without adopting the
  whole client-server session model.

## What the 2D Simulation Clock is

- **Local and network-free.** No session, no server, no connection. It runs single-player,
  in the editor, and headless in CI.
- **Bit-exact deterministic fixed step.** Every gameplay system advances on `OnSimTick` at a
  fixed rate; RNG is seeded and its draw count is part of the snapshot; `GetStateHash` is a
  fingerprint for proving two runs match.
- **Full-state snapshot to a plain buffer.** `CaptureFrame` gathers every enrolled
  participant's chunks (via the Simulation State marker + `SaveSimState`) into a byte buffer;
  `RestoreFrame` puts them back. No network types involved.
- **Its actual jobs, none of which O3DE Multiplayer serves:** training-mode frame advance
  (`StepOnce`), replays, a headless determinism proof that runs in CI on every PR, and being
  the **deterministic substrate a peer-to-peer (GGPO-class) fighting netcode requires**.

## Side by side

| | O3DE Multiplayer | Diorama 2D Simulation Clock |
| --- | --- | --- |
| Topology | Client-server (dedicated / listen) | Local; no network at all |
| "Rollback" means | Client prediction + server lag-comp reconciliation | Local full-state snapshot / restore |
| Determinism | Not required; desyncs corrected + audited | **Required**; bit-exact, hash-verified |
| State capture | `NetBindComponent` rewindable props, session-bound | Plain byte buffer, network-free |
| Tick | ~30 Hz network tick, interpolated | Fixed sim step (e.g. 60 Hz), frame-exact |
| Coupling to use | Auto-component + NetBind + input pipeline | An ordinary `AZ::Component` + a marker |
| Fighting / GGPO | Not supported; middleware territory | Purpose-built substrate for it |

## Decision

**Complementary, not duplicative.** O3DE Multiplayer cannot do what the sim clock does
(local, bit-exact, network-free snapshot for training / replay / CI / P2P-deterministic
substrate), and the sim clock does not do what O3DE Multiplayer does (client-server
transport, prediction, reconciliation, replication). The overlap is only the shared word
"rollback," which names two different techniques. The 2D Simulation Clock and its
snapshot/restore are kept.

## The boundary line (rules going forward)

1. **Diorama never builds the netcode.** No transport, no network prediction/reconciliation,
   no replication, no matchmaking, no session management. That is O3DE Multiplayer's (for
   server-authoritative games) or a GGPO-class middleware's (for P2P fighting).
2. **The sim clock stays local and network-free.** Its value is determinism + snapshot for
   single-player tooling and as a substrate; it must not grow a network dependency.
3. **If a Diorama game wants server-authoritative networked play** (FPS/MMO shape), it uses
   O3DE Multiplayer directly on its own gameplay components. Diorama's 2D components render
   and animate; they do not try to be network auto-components. Diorama does not get in the
   way of, or wrap, the Multiplayer gem.
4. **If a Diorama game wants P2P deterministic fighting netcode**, it pairs the sim clock's
   deterministic sim + snapshot with a rollback middleware: **Diorama provides the state,
   the middleware provides transport and rollback orchestration.**
5. **Terminology hygiene.** In Diorama, prefer "deterministic simulation" and "state
   snapshot / restore" for the local primitive. Reserve "rollback" for the netcode layer we
   do not own, and when we say a component is "rollback-exact" we mean *its state
   snapshots and restores bit-exactly* (a determinism property), not that Diorama ships
   rollback netcode.

## Implication for current work

The sim-clock migration of the gameplay components (sprite, Aseprite, state machine, hitbox
rig, bullet emitter, and the skeletal + skinned character-animation players) is on the right
side of this line: it makes 2D components advance deterministically and expose a snapshot
chunk. That is the substrate, not netcode. It ships. The character-animation migration
([2d-deterministic-sim.md](2d-deterministic-sim.md)) proceeds. What must **not** happen is a
follow-on that adds networking, prediction, or transport to Diorama; at that point the
answer is "use O3DE Multiplayer or a rollback middleware," and this doc is the reference for
saying so.
