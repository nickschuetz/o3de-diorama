# How-To: 2D Materials (hit-flash)

Diorama's 2D materials add per-sprite shader effects on top of the lit sprite.
The first one is the **hit-flash**: blend a sprite toward a color after lighting,
for the classic damage/hit pop. It is the material backbone the next effects
(outline, dissolve) build on.

If you just want to see it, run the example, attach the script, and enter game
mode. It builds its own level (`DioramaMaterialsDemo`):

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/materials_demo.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Important: the flash animation runs at game time

A constant flash can be set in the inspector (the Sprite component's **Flash
Color** / **Flash Amount** fields), but the *animated* hit-flash is driven by a
script at game time. The demo's `flash_pulse.lua` does this.

## Flash from the inspector

Select a sprite, and on its Sprite component set **Flash Amount** above 0 and pick
a **Flash Color**. The sprite blends toward that color (after lighting). This is a
static flash, useful to preview the look.

## Flash from script (the hit pop)

The real use is transient: on a hit, snap the flash to 1, then ease it back to 0.
Use the sprite's `DioramaSpriteRequestBus`:

```lua
-- On a hit:
DioramaSpriteRequestBus.Event.SetFlash(enemy, 1.0, 1.0, 1.0, 1.0)  -- full white flash
-- Each tick after, ease back:
amount = math.max(0.0, amount - deltaTime / fadeTime)
DioramaSpriteRequestBus.Event.SetFlash(enemy, 1.0, 1.0, 1.0, amount)
```

`flash_pulse.lua` (in the demo) wraps exactly this on a timer. The flash blends in
*after* lighting, so it reads as a bright pop even on a lit or normal-mapped
sprite. Channels and amount are clamped to 0..1.

## 3. Run the demo

After running `materials_demo.py`:

1. Select a **Target** entity, **Add Component -> Lua Script**, set the script to
   `diorama/examples/materials/flash_pulse.lua`.
2. **Enter game mode** (Ctrl+G): the sprite flashes on the timer. Tune the
   script's `FlashR/G/B`, `Interval`, and `FadeTime` properties.

## Outline

The other shipped material is a **silhouette outline** for selection or hit
highlights. Set it from the inspector (the Sprite component's **Outline Color** /
**Outline Thickness**) or from script:

```lua
DioramaSpriteRequestBus.Event.SetOutline(enemy, 0.1, 0.9, 1.0, 1.5)  -- cyan outline
DioramaSpriteRequestBus.Event.SetOutline(enemy, 0.0, 0.0, 0.0, 0.0)  -- clear it
```

The shader draws the outline color in the transparent fringe just outside the
sprite's opaque silhouette (it samples neighbor texels via the UV derivative, so
thickness stays roughly constant on screen). The demo gives its first target a
cyan outline directly from the bus. Outline works on any sprite that has a
transparent margin around its art.

## Afterimage trail (dash / super ghosts)

A sprite can leave a trail of fading ghost copies of its recent poses behind it, the
classic dash and super-activation effect. Author it on the Sprite component's
**Afterimage Trail** group (**Trail Ghosts** = how many ghosts, **Trail Interval** =
seconds between them, **Trail Start Alpha**, **Trail Fade** = falloff per older ghost,
**Trail Tint**) or drive it from script to switch it on for a move and off again:

```lua
-- Turn on a 6-ghost trail spaced 40ms apart, freshest at 0.6 alpha, each older one
-- 0.6x fainter; tint it cyan for a super flash.
DioramaSpriteRequestBus.Event.SetTrail(player, 6, 0.04, 0.6, 0.6)
DioramaSpriteRequestBus.Event.SetTrailTint(player, 0.4, 0.9, 1.0, 1.0)
-- ...later, end the move:
DioramaSpriteRequestBus.Event.SetTrail(player, 0, 0.04, 0.6, 0.6)  -- 0 ghosts = off
```

The presenter captures the pose (transform + the current animation frame) on the
interval and draws each ghost through the sprite batch path, behind the live sprite,
each fainter than the last. It works on static and animated sprites and under the
simulation clock, costs the renderer nothing when off (Trail Ghosts = 0), and stays
world-space. For a fighter, flip it on at the start of a dash or super and off in
recovery.

Demo: `Docs/examples/trails_demo.py` builds a runnable `DioramaTrailsDemo` level (a
mascot that dashes left/right and drags a fading ghost trail); assign
`diorama/examples/sprite/trail_mover.lua` to the Mover and enter game mode.

## Palette recolor (team / alt colors)

The same sprite sheet can wear different color schemes - P1 vs P2, team colors, a
hit-flash variant - without separate art. The shader remaps each pixel's brightness
through a **three-stop color ramp** (a shadow color for the darks, a mid color, a
highlight color for the brights) and blends the sprite toward it by a strength. Author
it on the Sprite component's **Palette Recolor** group (**Palette Strength** = how much
to apply, plus the three ramp colors) or from script:

```lua
-- Switch this fighter to its red P2 scheme.
DioramaSpriteRequestBus.Event.SetPaletteColors(
    p2,
    0.25, 0.02, 0.02,  -- shadow (dark red)
    0.80, 0.15, 0.15,  -- mid    (red)
    1.00, 0.70, 0.60)  -- highlight (warm white)
DioramaSpriteRequestBus.Event.SetPaletteStrength(p2, 1.0)  -- 0 = original colors
```

Because it keys off luminance, it recolors any sprite (it does not need index-encoded
art); pick ramps with enough contrast between the three stops to keep the shape
readable. Strength 0 is off (the sprite keeps its own colors), and the palette is part
of the draw batch key, so two differently-colored copies of the same sheet still batch
efficiently.

Demo: `Docs/examples/palette_demo.py` builds a runnable `DioramaPaletteDemo` level - the
same mascot in four schemes (original, P1 cool, P2 warm, alt green) side by side, so the
luminance ramp is visible at a glance.

## How it works

A flashing sprite carries its flash color + amount in its batch key, so it draws
in its own batch with the flash set as a per-draw constant; the shader does
`lerp(litColor, flashColor, amount)` as the last step. Sprites with amount 0 (the
default) are unaffected and batch together as before. See
[Docs/design/2d-materials.md](../design/2d-materials.md) for the design and the
roadmap to outline/dissolve/additive blend.
