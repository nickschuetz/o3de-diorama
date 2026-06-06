# How-To: Parallax and Layers (2.5D)

Teaching ladder rung 5. Builds depth into a Diorama scene: ordering overlapping
sprites into layers, and making background layers scroll slower than the camera
for a 2.5D parallax effect.

## Two separate ideas

**Layering** is draw order: which sprite appears in front of which. **Parallax**
is motion: how fast a layer moves relative to the camera. Diorama handles the
first directly; the second is a small script on top.

## Layering with sort offset

Diorama sprites and tilemaps are transparent, so they draw back-to-front by a
sort key. Each has a **Sort Offset** (`SetSortOffset` on the sprite and tilemap
buses, or the inspector field): larger values draw later, on top.

Group your scene into layers by giving each a sort offset:

| Layer | Sort offset |
| ----- | ----------- |
| Far background | 0 |
| Midground | 5 |
| Foreground / gameplay | 10 |

Sort offset orders sprites without moving them in depth, so you can layer
overlapping content (a HUD-like overlay, a foreground fringe, a background sky)
while keeping everything in the same world plane. Combine with the entity's
actual depth position for true perspective when you want it.

## Parallax with a small script

Parallax makes distant layers move less than near ones as the camera pans,
selling depth. It is camera-relative motion, so it lives in a script rather than
the renderer: [`Assets/Diorama/Scripts/parallax_layer.lua`](../../Assets/Diorama/Scripts/parallax_layer.lua).

Attach it to a layer entity and set:

- **Camera** -- the entity the parallax is measured against (your camera).
- **ParallaxFactor** -- `0` = far background (follows the camera, appears static
  on screen), `1` = foreground (fixed in the world), values between = midground.

Each tick the script offsets the layer by the camera's movement times
`(1 - ParallaxFactor)`, so a factor-0 layer tracks the camera one-to-one (looks
infinitely far away) while a factor-1 layer stays put in the world.

## Runnable example

[`../examples/parallax_layers.py`](../examples/parallax_layers.py) builds a
background, midground, and foreground sprite layer with distinct sort offsets and
tints, and logs how to attach the parallax script per layer:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/parallax_layers.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Script or component

The parallax motion can come from either:

- **`parallax_layer.lua`** (a script component): attach it to each layer, set its
  `Camera` and `ParallaxFactor`. Good for quick setup and tweaking in script.
- **`2D Parallax Layer`** (a gem component): **Add Component -> Diorama -> 2D
  Parallax Layer**, set its `Camera` and `Factor`. The first-class, no-script
  option, with a typed `DioramaParallaxRequestBus` (`SetCamera`, `SetFactor`,
  `SetEnabled`, `GetParallaxInfo`) so an agent or gameplay script can drive it.

Both do the same thing (offset the layer by the camera's movement scaled by
`1 - factor`); pick whichever fits your workflow. The motion is visible at game
time.

## Next

The [side-scroller vertical slice](11-sidescroller.md) puts layering to use: three
parallax background layers behind the gameplay sprites, each on its own layer for a
2.5D sense of depth.
