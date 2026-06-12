# How-To: Retro CRT Overlay

Give a 2D scene a retro CRT look with scanlines. Add a **CRT Overlay** component to
any entity (the camera is a natural home); it draws dark horizontal scanlines and an
optional screen darkening over the whole viewport each frame, as screen-space quads
(the same path the HUD uses). No render pass or pipeline change.

## Build it

A one-command scene builder assembles this guide's demo in its own level
(a colorful scene under the CRT scanline overlay); finish any wiring the editor cannot script (noted in its output), then
enter game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/crt_demo.py
```

## Use it

1. Add a **CRT Overlay** component (`Diorama` category) to an entity.
2. Tune it:
   - **Line Spacing** -- pixels between scanlines (smaller = denser).
   - **Line Darkness** -- how dark each scanline is (0..1).
   - **Tint** -- a flat screen darkening for the dimmer CRT feel (0..1).
   - **Enabled** -- toggle the whole effect.

## Drive it from script

`DioramaCRTRequestBus` is reflected `Common`, so a script, Script Canvas, or an agent
toggles and tunes the effect like any other Diorama feature:

```lua
DioramaCRTRequestBus.Event.SetEnabled(self.entityId, true)
DioramaCRTRequestBus.Event.SetScanlineDarkness(self.entityId, 0.45)
DioramaCRTRequestBus.Event.SetScanlineSpacing(self.entityId, 4.0)
```

## Scope

This is the **scanline look** (scanlines + flat darkening) -- cheap and pass-free.
The full warping CRT (barrel curvature, chromatic aberration, phosphor bloom) needs a
fullscreen post-process pass that filters the rendered image; that is a planned
follow-up (a Diorama CRT pass injected into the render pipeline). For glow/bloom and
color grading, combine this with Atom's post-process and the emissive hook (see
[14-glow.md](14-glow.md)).
