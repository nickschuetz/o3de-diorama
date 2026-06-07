# 2D UI / HUD (parity surface)

Status: **v1 shipped** — `DioramaUIComponent` / `DioramaUIRequestBus` + the
`UILayout2D` core + [how-to 13](../howto/13-ui-hud.md).

A screen-space HUD/UI layer that is **at parity with the rest of Diorama**: AI- and
human-drivable through a typed request bus, with an editor twin and demos, the same
model the sprite and tilemap buses follow. This closes the "UI/HUD parity" gap in the
[roadmap](../roadmap.md): LyShine is UI-only and outside Diorama's parity model, so a
game's score, health, and prompts cannot be built the way an agent builds sprites.
This feature makes a HUD a first-class Diorama citizen, not a bolt-on.

## Parity principle

Every Diorama feature exposes one typed, `Common`-scoped request bus that a human,
a Lua script, Script Canvas, or an AI agent all drive identically (see
[../architecture.md](../architecture.md) "AI and human parity"). UI is the same:
`DioramaUIRequestBus` is the only surface, and the editor component is a thin author
twin over the same config. No verb exists that the bus cannot reach.

## Elements (v1)

Each UI element is an entity with a `DioramaUIComponent`, one of:

- **Text**: a string drawn with a font, size, color, and alignment.
- **Bar / gauge**: a filled rectangle whose fill is a 0..1 value (health, cooldown),
  with background and fill colors.
- **Panel / image**: a screen-space sprite quad (a frame, icon, or solid panel),
  reusing the sprite texture path.

## Anchoring and layout (the pure core)

UI is resolved in a **virtual reference resolution** (default 1280x720) and scaled to
the real backbuffer, so a layout holds across window sizes. An element has:

- an **anchor** (`TopLeft`, `Top`, `TopRight`, `Left`, `Center`, `Right`,
  `BottomLeft`, `Bottom`, `BottomRight`),
- a **pixel offset** from that anchor (in reference pixels),
- a **size** (reference pixels), and a **pivot** within its own box.

`UILayout2D.h` is a pure, header-only core (the `Camera2D` / `Particles2D` pattern):
given the reference resolution, the real resolution, an anchor, offset, size, and
pivot, it computes the element's final screen rect in pixels. No Atom or component
dependency, fully unit-tested. The component and bus are thin shells over it.

## Rendering

Drawn in a screen-space overlay after the scene, with no depth test, in element
order:

- **Text** via `AzFramework::FontDrawInterface::DrawScreenAlignedText2d` (backed by
  AtomFont), obtained from `FontQueryInterface::GetDefaultFontDrawInterface()`.
  `GetTextSize` supports alignment and auto-fit.
- **Bars and panels** as screen-space quads. v1 reuses the existing
  `SpriteFeatureProcessor` quad/draw path with a screen-space (orthographic /
  viewport) transform rather than a world transform, so there is no new shader and
  no extra render pass; a dedicated tiny screen-space draw context is the fallback if
  mixing world and screen draws in one feature processor proves awkward.

No per-frame allocation: element state lives in fixed component storage and the draw
reuses scratch buffers, matching the render-loop rule.

## AI-native bus

`DioramaUIRequestBus` (per entity, reflected `Common`), scalar/clamped verbs in the
established style:

- `SetText(string)`, `SetFontSize(px)`, `SetColor(r,g,b,a)`, `SetAlignment(align)`
- `SetValue(t)` (bar fill, clamped 0..1), `SetBarColors(bgRGBA, fillRGBA)`
- `SetImageByPath(path)` (panel), `SetAnchor(anchor)`, `SetOffset(x,y)`,
  `SetSize(w,h)`, `SetVisible(bool)`
- `GetUIInfo()` returns a resolved `UIInfo` (kind, resolved screen rect, current
  text/value, visible) for verification without a screenshot, mirroring
  `GetSpriteInfo`.

So an agent or Script Canvas builds and updates a HUD exactly as it builds sprites.

## Editor twin

`EditorDioramaUIComponent` authors the element kind, anchor, offset, size, and
content in the inspector and exports the runtime component via `BuildGameEntity`,
like the other Diorama editor components (author-only, no tools code in the runtime
module).

## Security and performance

- All values are validated and clamped on load and in every setter (font size, bar
  value, sizes); no asset or script value can size a buffer or read out of range.
- Screen-space draw adds at most one text batch and one quad batch; no extra Atom
  pass and no per-frame heap allocation.

## Phasing

1. **v1**: text, bar/gauge, and panel/image elements; anchoring + virtual-resolution
   scaling; the request bus, editor twin, a HUD demo, and a how-to. Static and
   script-updated (the twin-stick score/health HUD becomes a Diorama HUD).
2. **v2**: layout groups (stack/row containers), nine-slice panels, and tween
   integration (`Assets/Diorama/Scripts/tween.lua`) for animated bars/popups.
3. **v3**: interactive elements (buttons, drag) over the input action-mapping gap,
   once that surface exists.
