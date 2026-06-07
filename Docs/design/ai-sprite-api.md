# Design: DioramaSpriteRequestBus (AI-native sprite API)

Status: **shipped and extended.** The base bus matches this spec; later verbs
(`SetNormalMapByPath`, `SetFlash`, `SetOutline`, `SetEmissive`, `SetPointFilter`,
`SetTranspose`, `SetPlaybackSpeed`) and the `OnAnimationFrame` notification were added
on top. See [reference/api.md](../reference/api.md). Date: 2026-05-30.

## Motivation

Diorama is meant to be AI-friendly as a primary goal, not an afterthought: an AI
agent should drive sprites to build 2D and 2.5D games with minimal friction. The
existing O3DE AI tooling (o3de-mcp, the AiCompanion gem) was retrofitted onto an
engine and gems that were not designed for AI, and it shows:

- Components are driven through `EditorComponentAPIBus.SetComponentProperty` with
  brittle pipe-separated EditContext path strings (`Config|Group|Field`). It is
  schema-less, has no introspection, and a typo silently no-ops. This cost real
  time even for a human during development.
- The tooling hand-maintains a catalog of 100+ component names and a fallback
  list because the engine does not let an agent reliably discover components.
- There is a C++ shim purely to unwrap `GenericComponentWrapper` so setting
  `Asset<T>` properties does not crash.

Diorama does not have to inherit those workarounds. It can be the first
AI-native O3DE gem: drivable by raw `azlmbr`, o3de-mcp, or the companion with no
tool-side special-casing. This document specifies the API that makes that true.

## Principles

1. Typed verbs, no property-path strings. An agent never constructs an O3DE math
   type or guesses a nested EditContext path.
2. Forgiving. Setters validate and clamp; they do not crash on bad input.
3. Self-describing. Full BehaviorContext reflection with named args and
   descriptions, so a reflection dump is the documentation and no external
   catalog entry is needed.
4. Verifiable. A query verb returns resolved runtime state so the agent closes
   its own loop without screenshots.
5. One config, two faces, both first-class. The EditContext inspector (for
   humans) and this bus (for agents) are two faces of the same
   `SpriteComponentConfig`. They are peers: neither is built on the other and
   neither is degraded to serve the other. AI-friendliness must not cost any
   quality in the human authoring experience, and vice versa. The bus uses
   scalar args so an LLM never builds a math type, but the inspector keeps its
   rich handlers (color swatch, vector widgets, grouped sections, min/max/suffix,
   live preview); no AI plumbing leaks into the human UI. Human-only authoring
   polish (gizmos, in-viewport editing, preview affordances) is encouraged; the
   parity invariant forbids capability gaps, not extra polish on either side.

## Addressing model

`DioramaSpriteRequestBus` is addressed by `EntityId` (an `AZ::ComponentBus`,
`AddressPolicy::ById`, `HandlerPolicy::Single`), like `TransformBus`. The agent
already holds an `EntityId` from entity creation, this matches o3de-mcp's
entity-centric tools, and it avoids inventing a parallel sprite-handle namespace.

Both `SpriteComponent` (runtime) and `EditorSpriteComponent` connect the bus at
their entity's address, so the same calls work in game and in the editor.

## Request bus

All setters route through the existing `SpritePresenter::SetConfig` path, so
changes apply live and reuse the code already verified this session. Scalars are
used instead of `Vector2`/`Color` so an LLM never builds a math type.

```cpp
class DioramaSpriteRequests : public AZ::ComponentBus
{
public:
    // Texture / appearance
    virtual bool SetTextureByPath(AZStd::string_view productPath) = 0; // false if unresolved
    virtual void SetSize(float width, float height) = 0;              // clamped >= 0
    virtual void SetPivot(float x, float y) = 0;                      // clamped 0..1
    virtual void SetTint(float r, float g, float b, float a) = 0;     // clamped 0..1
    virtual void SetBillboard(bool enabled) = 0;
    virtual void SetDoubleSided(bool enabled) = 0;

    // Atlas / UV region
    virtual void SetUVRegion(float uMin, float vMin, float uMax, float vMax) = 0; // clamped 0..1
    virtual void SetFlip(bool horizontal, bool vertical) = 0;

    // Layering (2.5D)
    virtual void SetSortOffset(float sortOffset) = 0;

    // Animation (sprite sheet)
    virtual void SetFrameGrid(int columns, int rows, int frameCount) = 0; // frameCount clamped to cols*rows
    virtual void SetAnimationEnabled(bool enabled) = 0;
    virtual void SetPlayback(float framesPerSecond, bool loop) = 0;
    virtual void SetStartFrame(int frame) = 0;
    // Convenience: grid + playback + enable in one call (the create_player-style one-shot).
    virtual void PlaySpriteSheet(int columns, int rows, int frameCount, float fps, bool loop) = 0;

    // Query (verify loop)
    virtual SpriteInfo GetSpriteInfo() = 0;
};
using DioramaSpriteRequestBus = AZ::EBus<DioramaSpriteRequests>;
```

Design decisions (chosen): granular setters plus a few one-shot convenience
verbs (not a broad multi-arg `ConfigureSprite`); setters return `void` except
`SetTextureByPath` which returns `bool` (the one operation that can fail to
resolve), with `GetSpriteInfo` as the verification mechanism.

## Query payload

A reflected struct exposing resolved state, not just what was set. The fields
`m_textureLoaded` and `m_visible` are exactly what could not be queried during
development (forcing screenshot-based verification).

```cpp
struct SpriteInfo
{
    AZStd::string m_texturePath;     // resolved product path, "" if none
    bool  m_textureLoaded = false;   // asset actually streamed in
    bool  m_visible = false;         // registered with the feature processor and drawable
    float m_width = 0.0f, m_height = 0.0f;
    float m_pivotX = 0.5f, m_pivotY = 0.5f;
    float m_sortOffset = 0.0f;
    bool  m_billboard = false;
    bool  m_doubleSided = true;
    bool  m_flipHorizontal = false, m_flipVertical = false;
    bool  m_animEnabled = false;
    int   m_currentFrame = 0;
    int   m_frameCount = 1;
};
```

## Notification bus

Event-driven agents can subscribe instead of polling `GetSpriteInfo`.
Addressed by `EntityId` to match the request bus.

```cpp
class DioramaSpriteNotifications : public AZ::ComponentBus
{
public:
    // Fired when the sprite's texture finishes loading (or changes and reloads).
    virtual void OnTextureReady(const AZStd::string& productPath) {}
    // Fired when drawability changes (became visible / stopped being drawn).
    virtual void OnVisibilityChanged(bool visible) {}
    // Fired when an animation clip completes (non-looping clips only).
    virtual void OnAnimationFinished() {}
};
using DioramaSpriteNotificationBus = AZ::EBus<DioramaSpriteNotifications>;
```

These map onto signals the presenter/feature processor already produce:
`OnTextureReady` from the existing `AssetBus::OnAssetReady` path,
`OnVisibilityChanged` when the feature processor first draws or drops the sprite,
`OnAnimationFinished` from the existing `SpriteAnimation::FrameState::m_finished`.

## BehaviorContext reflection

Every event is reflected with named args and a one-line description, plus a
`Category` of "Diorama" so introspection groups it. The struct is reflected as a
behavior class with named properties so query results are readable from script.

```cpp
behaviorContext->EBus<DioramaSpriteRequestBus>("DioramaSpriteRequestBus")
    ->Attribute(AZ::Script::Attributes::Category, "Diorama")
    ->Event("SetTextureByPath", &DioramaSpriteRequests::SetTextureByPath,
            {{ {"productPath", "Texture product path, e.g. diorama/textures/hero.png"} }})
    ->Event("PlaySpriteSheet", &DioramaSpriteRequests::PlaySpriteSheet,
            {{ {"columns","Sheet columns"},{"rows","Sheet rows"},{"frameCount","Frames played"},
               {"fps","Playback rate"},{"loop","Loop or hold last frame"} }})
    // ... all verbs ...
    ->Event("GetSpriteInfo", &DioramaSpriteRequests::GetSpriteInfo);

behaviorContext->Class<SpriteInfo>("DioramaSpriteInfo")
    ->Property("texturePath", BehaviorValueProperty(&SpriteInfo::m_texturePath))
    ->Property("textureLoaded", BehaviorValueProperty(&SpriteInfo::m_textureLoaded))
    ->Property("visible", BehaviorValueProperty(&SpriteInfo::m_visible))
    // ... all fields ...
```

## How existing work inherits this

This bus is a typed front door to the chokepoint the gem already funnels every
change through (`SpritePresenter::SetConfig` -> `Push` -> feature processor). It
adds no parallel state and rewrites nothing.

| Existing component | Role under the AI bus |
| --- | --- |
| `SpriteComponentConfig` | Backing store; each verb is a typed setter onto its existing fields. |
| `SpritePresenter::SetConfig` | The live-update path every verb calls; editor preview keeps working. |
| `SpriteFeatureProcessor` (+ batch plan, dirty-tracking) | Unchanged; renders the config and supplies `GetSpriteInfo().m_visible`. |
| `SpriteAnimation` / frame grid | `SetFrameGrid` / `PlaySpriteSheet` are typed wrappers over its fields. |
| Double-sided toggle | `SetDoubleSided` -> the existing field. |
| Render-job deadlock fix | Why the bus is safe to hammer without freezing the editor. |
| Unit tests | Still the math source of truth; new tests cover verb -> config mapping. |

## Invariant: config-field parity (definition of done)

Parity cuts both ways, so neither face falls behind the other:

- Every field a human can set in the inspector must also be settable through this
  bus and reported by `SpriteInfo` (the agent is never behind the human).
- Every capability exposed through this bus must have a first-class inspector
  representation (the human is never behind the agent).

From now on a new sprite feature is not "done" until it is exposed through both
faces in the same change, each at full quality for its audience (rich widgets and
live preview for the inspector; typed, described verbs for the bus). A unit test
asserts the verb set and `SpriteInfo` cover the config, so adding a field without
an AI verb fails CI; the human-side half is enforced by review. This invariant
forbids capability gaps, not extra polish: human-only authoring niceties (gizmos,
in-viewport editing) and bus-only conveniences (one-shot combos like
`PlaySpriteSheet`) are both fine.

## Creation note

The bus is by-EntityId, so the component must already exist. Adding a component
is an entity/editor operation; agents can use o3de-mcp's generic
`add_component("Sprite")`. Optionally, a thin editor-module
`DioramaSpriteToolsRequestBus::AddSpriteToEntity(entityId)` can spare agents from
resolving component type ids; this stays in the editor module so the runtime
client keeps no tools dependency. Deferred unless needed.

## Agent usage, before and after

Before (retrofit path; brittle, silent failure, no confirmation):

```python
set_component_property(eid, "Sprite", "Config|Atlas / UV Region|UV Min", "0.5,0.5")
```

After:

```python
import azlmbr.bus as bus
import azlmbr.diorama as diorama
diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, "diorama/textures/atlas_2x2.png")
diorama.DioramaSpriteRequestBus(bus.Event, "PlaySpriteSheet", eid, 4, 4, 16, 12.0, True)
info = diorama.DioramaSpriteRequestBus(bus.Event, "GetSpriteInfo", eid)
assert info.visible  # closes the loop, no screenshot
```

## Implementation plan

1. Add `SpriteInfo` (reflected) and the two bus interfaces in `Code/Include` /
   `Code/Source/Clients`.
2. Implement the handlers on `SpriteComponent` (runtime) and reuse on
   `EditorSpriteComponent`; route setters through `SpritePresenter::SetConfig`.
3. Surface `m_visible` / `m_textureLoaded` from the presenter/feature processor.
4. BehaviorContext reflection of both buses and `SpriteInfo`.
5. Tests: verb -> config mapping, clamping, `GetSpriteInfo` correctness, and the
   field-parity invariant.
6. A runnable `Docs/examples` script driving a sprite purely through the bus,
   doubling as an LLM few-shot example.
