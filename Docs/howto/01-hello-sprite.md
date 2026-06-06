# How-To: Hello Sprite

Teaching ladder rung 1. The smallest Diorama scene: a single world-space sprite.

## What you will build

One entity with a Sprite component, showing a texture in the world.

## The Sprite component

Diorama's `Sprite` draws a textured quad in world space (not screen space like a
UI image). It has a texture, a size in world units, a pivot, a tint, and the
2.5D options covered in later rungs (atlas sub-regions, flips, sort offset,
sprite-sheet animation). Everything has a forgiving default, so a sprite with
just a texture and size already draws.

## Steps (in the editor)

1. Create an entity.
2. Add the **Sprite** component (Add Component -> Diorama -> Sprite).
3. Set its **Texture** to an image (for example `sample_sprite.png`).
4. Set the **Size** to how large it should be in world units.

The same sprite component works at runtime and in the editor viewport, so it
appears as soon as the texture loads, without entering game mode.

## Driving it from script or an agent

Every inspector field has a typed verb on `DioramaSpriteRequestBus`, so a script
(or an AI agent) builds the same sprite without poking property-path strings:

```python
diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, "diorama/textures/sample_sprite.png")
diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 2.0, 2.0)
info = diorama.DioramaSpriteRequestBus(bus.Event, "GetSpriteInfo", eid)  # confirm it took
```

`GetSpriteInfo` returns the resolved state (texture path, whether it loaded,
whether it is drawing), so you can confirm the sprite without a screenshot.

## Runnable example

[`../examples/hello_sprite.py`](../examples/hello_sprite.py):

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/hello_sprite.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Next

Rung 2, [Animated Sprite](02-animated-sprite.md), plays frames from a sprite
sheet on the same component.
