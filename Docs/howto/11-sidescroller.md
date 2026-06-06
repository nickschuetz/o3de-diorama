# How-To: Side-Scroller Vertical Slice

A short side-scroll scene that composes the gem's 2.5D features at once: parallax
background layers, a follow camera, 2D point-light torches, an ember particle
emitter, and an outlined player that walks the level. It is a portfolio sample and
an integration check of the lighting / camera / particles / parallax / materials
features.

Build it (own level, `DioramaSideScroller`):

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/sidescroller_demo.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## What it composes

- **Parallax**: three background quads (`BgFar`/`BgMid`/`BgNear`) with `2D Parallax
  Layer` components at increasing factors, so they scroll at their depths as the
  camera pans. Each references the `SideCamera`.
- **Camera**: `SideCamera` has the `2D Camera Controller`; the walker script makes
  it follow the player and hold vertically (a side-scroll feel).
- **Lighting**: two `Torch_*` point lights the player walks through (it brightens
  as it nears each).
- **Particles**: an `Embers` emitter rising at the first torch.
- **Materials**: the `Player` has a cyan outline.

## Run it

The motion runs at game time, so after running the script:

1. Select **Player**, **Add Component -> Lua Script**, set the script to
   `diorama/examples/sidescroller/walker.lua`. Optionally set its **Camera**
   property to the **SideCamera** entity; if you leave it unset the walker falls
   back to the active camera, so wiring it is not required.
2. Select **SideCamera** -> **Be this camera**.
3. **Enter game mode** (Ctrl+G): the player walks rightward, the camera follows,
   the parallax layers scroll at their depths, and the player passes through the
   torch light and embers, then loops back.

Tune the walker's `Speed` / `LoopDistance` / `SpinSpeed` (a slow vertical-axis
"coin" spin as the player walks; `0` disables it), the parallax `Factor` per
layer, and the torch lights to taste. The spin needs the `Player` sprite to be
**non-billboard** and **double-sided** so it shows its back mid-turn (a billboard
always faces the camera and would ignore the rotation).

## How it works

Nothing here is new engine code -- it is pure composition of the shipped feature
components (`2D Parallax Layer`, `2D Camera Controller`, `2D Light`, `2D Particle
Emitter`, the Sprite material fields) wired together, with one small `walker.lua`
driving the player and configuring the camera through `DioramaCamera2DRequestBus`.
That is the point: the features stack. See the per-feature how-tos
([07](07-lighting.md)-[10](10-materials.md)) for each piece on its own.
