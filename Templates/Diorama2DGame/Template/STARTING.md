# Getting started with ${Name}

This project was created from the **Diorama 2.5D Game** template, so the
[Diorama](https://github.com/nickschuetz/o3de-diorama) gem is already enabled: a
world-space 2D/2.5D toolkit (sprites, tilemaps, a 2D camera, lighting, particles,
parallax, collision, a HUD, post-processing, plus skeletal and Aseprite animation),
each drivable from script through a typed request bus.

## Build and run

From the engine, build the editor and launcher for this project (see the O3DE docs for
your platform). In short:

```bash
cmake -B build/linux -S . -G "Ninja Multi-Config"
cmake --build build/linux --config profile --target ${Name}.GameLauncher Editor
```

Open the Editor, load `Levels/DefaultLevel`, and you have an empty 2.5D scene to build
in.

## Your first sprite

1. Create an entity.
2. Add a **Sprite** component (the `Diorama` category in Add Component).
3. Point its **Texture** at an image you have imported (any PNG under `Assets/`), and
   set the size.

That is the whole loop: a Diorama Sprite is a textured quad in world space that sorts
by depth, so 2D art lives in the same 3D scene as everything else.

### Drive it from script

The same thing from Lua, via the request bus (a Lua Script component on the entity):

```lua
DioramaSpriteRequestBus.Event.SetTextureByPath(self.entityId, "yourgame/textures/hero.png")
DioramaSpriteRequestBus.Event.SetSize(self.entityId, 1.0, 1.0)
DioramaSpriteRequestBus.Event.SetBillboard(self.entityId, true)
```

`Assets/Diorama/Starter/starter_sprite.lua` is a ready-made version of this: attach it to
an entity that has a Sprite component, set its **Texture Path** property, and enter game
mode.

## Where to go next

Every Diorama feature works the same way (a component you add in the Inspector and a
request bus you call from script or an agent). The gem ships step-by-step how-tos:

- 2.5D Quick-Start, animated sprites, sprite atlases
- Tilemaps (with an in-editor paint tool), parallax layers
- 2D camera (follow / deadzone / shake), lighting, particles
- UI / HUD, audio, post-processing glow, retro CRT overlay
- Skeletal cutout animation, Aseprite sprite-sheet import

See the gem's `Docs/howto/` (start with `17-quickstart.md`) and the programmatic API
reference in `Docs/reference/api.md`.
