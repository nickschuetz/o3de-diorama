# How-To: 2D Dynamic Lighting

Diorama sprites can be lit by gem-native 2D lights, so a flat 2D scene reacts to
a moving light: a point light brightens nearby sprites in its color and fades
with distance, and a directional light tints the whole layer like a sun. This
guide builds the focused lighting demo from scratch.

If you just want to see it, run the example script and skip to
[Toggling lighting](#toggling-lighting). It builds into its own fresh level
(`DioramaLightingDemo`), independent of any other scene in your project, and
saves it:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/lighting_demo.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## What you build

A dark backdrop with a row of creature sprites and one warm **point light** in
front of them. With lighting on, the sprites near the light glow orange and fall
off with distance; the ones farther away sit at the ambient floor. Attaching the
orbit script sweeps the glow across the scene.

## 1. Place some sprites

Create a few sprites the way [Hello Sprite](01-hello-sprite.md) shows. For the
demo: one large, dark, billboarded backdrop quad (tint roughly `0.22, 0.22,
0.30`, a low sort offset so it draws behind), and a row of smaller creature
sprites in front of it. Any sprites work; lighting modulates whatever is drawn.

## 2. Add a 2D Light

1. Create an entity where you want the light (for a point light, its position is
   the light's position; place it just in front of the sprites).
2. In the Inspector: **Add Component -> Diorama -> 2D Light**.
3. Set the properties:
   - **Type**: `Point` (a localized glow) or `Directional (sun)` (lights the
     whole layer from one direction).
   - **Color**: the light color (e.g. a warm orange).
   - **Intensity**: brightness multiplier (try `2.5`).
   - **Radius** (point lights): how far the light reaches in world units; the
     glow fades to nothing at this distance (try `8`).
   - **Direction** (directional lights): the world-space direction the light
     travels.
   - **Enabled**: turn this one light on or off.

The viewport updates live as you edit, so you can dial in the look immediately.

## 3. Drive it from script (optional)

The light has a typed request bus, `DioramaLightRequestBus`, so a script or an
agent can change it at runtime exactly like the inspector does. From the
demo Python:

```python
diorama.DioramaLightRequestBus(bus.Event, "SetDirectional", light, False)
diorama.DioramaLightRequestBus(bus.Event, "SetColor", light, 1.0, 0.55, 0.15)
diorama.DioramaLightRequestBus(bus.Event, "SetIntensity", light, 2.5)
diorama.DioramaLightRequestBus(bus.Event, "SetRadius", light, 8.0)
info = diorama.DioramaLightRequestBus(bus.Event, "GetLightInfo", light)
```

`GetLightInfo` reads the resolved state back, so a tool can confirm a change took
effect without a screenshot.

## 4. Animate the light

To make the glow move, add a **Lua Script** component to the light entity and set
its script to `diorama/examples/lighting/light_orbit.lua`. It orbits the light
around its starting position each tick (move the entity, the light follows). Its
properties: `OrbitRadius`, `OrbitSpeed` (radians/second), and an optional
`BobAmplitude`. Enter game mode and the glow sweeps across the sprites.

## Toggling lighting

Two independent switches:

- **`Enabled`** on a light (per light): removes just that one light. Other
  enabled lights keep shining.
- **`r_dioramaSpriteLighting`** console var (global master): `0` collapses the
  whole scene back to the flat, unlit look; `1` restores lighting.

A scene with **no active lights** renders exactly like the pre-lighting look:
sprites are full-bright, not dimmed. The ambient floor (set by
`r_dioramaSpriteAmbient`, default `0.35`) only applies once at least one enabled
light exists, so adding the feature never darkens a scene on its own. You opt
into the moody look by placing a light.

## Normal maps (shaped light)

By default lights only brighten sprites by distance (flat). Assign a sprite a
**normal map** and the lights *shape* it: the side facing a light gets brighter
and the highlight tracks a moving light, so flat art reads as 3D. The demo does
this with a shipped spherical normal map (`diorama/textures/sphere_normal.png`),
which makes each flat creature light like a ball:

```python
diorama.DioramaSpriteRequestBus(bus.Event, "SetNormalMapByPath", eid, "diorama/textures/sphere_normal.png")
```

You can also set it in the inspector via the Sprite component's **Normal Map**
field. Normal maps work best on **billboarded** sprites (the lighting uses the
camera's basis to orient the normal). Remove the normal map to compare against the
flat look. Painting normal maps for your own art (e.g. with Laigter or SpriteIlluminator)
is the usual workflow; the shipped sphere map is just a ready demo.

## How it works

The `SpriteFeatureProcessor` gathers every registered `2D Light` once per frame
into the sprite shader's per-draw constants (up to 8 lights, clamped on the CPU).
The shader adds each light's contribution to an ambient base: directional lights
add their full color, point lights add color scaled by a distance falloff. It is
deliberately decoupled from Atom's built-in lights, so it depends on nothing
outside Diorama's own renderer. See
[Docs/design/2d-lighting.md](../design/2d-lighting.md) for the full design.
