# Diorama How-To Guides

Step-by-step guides that follow the teaching ladder in
[../examples-outline.md](../examples-outline.md), from a single sprite to a
complete game. Each rung has a written guide and, where applicable, a runnable
example under [../examples](../examples).

## Start here

New to the gem? The [2.5D Quick-Start](17-quickstart.md) builds the smallest
playable scene (a player you move with WASD / arrows / left stick, a ground, and
a camera) in minutes, then points at the feature guides to build on it.

## Core ladder

| Rung | Guide | Status |
| ---- | ----- | ------ |
| 1 | [Hello Sprite](01-hello-sprite.md) | Written |
| 2 | [Animated Sprite](02-animated-sprite.md) | Written |
| 3 | [Sprite Atlas](03-sprite-atlas.md) | Written |
| 4 | [Tilemap](04-tilemap.md) | Written |
| 5 | [Parallax and Layers (2.5D)](05-parallax-layers.md) | Written |
| 6 | [Twin-Stick Shooter (capstone)](06-twin-stick.md) | Written |

All rungs of the ladder now have a written guide, from a single sprite to the
complete twin-stick capstone.

## Feature demos

Focused, one-feature-each demos that extend the ladder as new gem features land.
Each has a written guide and a runnable example.

| Feature | Guide | Status |
| ------- | ----- | ------ |
| 2D Dynamic Lighting | [07-lighting.md](07-lighting.md) | Written |
| 2D Camera Controller | [08-camera.md](08-camera.md) | Written |
| 2D Particle Emitter | [09-particles.md](09-particles.md) | Written |
| 2D Materials (flash + outline) | [10-materials.md](10-materials.md) | Written |
| Side-Scroller Slice (integration) | [11-sidescroller.md](11-sidescroller.md) | Written |
| 2D UI / HUD (text + bar + panel) | [13-ui-hud.md](13-ui-hud.md) | Written |
| Make it Glow (post-processing + emissive) | [14-glow.md](14-glow.md) | Written |
| Sound (SFX and music via MiniAudio) | [15-audio.md](15-audio.md) | Written |
| Retro CRT Overlay (scanlines) | [16-crt.md](16-crt.md) | Written |
| Skeletal Cutout Animation (keyframed bone rig) | [18-skeletal.md](18-skeletal.md) | Written |
| Aseprite Animation Import (tags + per-frame timing) | [19-aseprite.md](19-aseprite.md) | Written |

## Tooling

| Topic | Guide | Status |
| ----- | ----- | ------ |
| Record a demo headlessly (GameLauncher + ffmpeg) | [12-recording-demos.md](12-recording-demos.md) | Written |

## Reference

The guides teach by building. For exhaustive, parameter-by-parameter detail and
the design, see:

- [Architecture](../architecture.md): the module split, data model, persistence,
  and render path, with diagrams.
- [Sprite component reference](../reference/sprite-component.md): every Sprite
  parameter in depth.
- [Tilemap component reference](../reference/tilemap-component.md): every Tilemap
  parameter in depth.
- [API reference](../reference/api.md): the typed `DioramaSpriteRequestBus` and
  `DioramaTilemapRequestBus` for scripts and agents.

## Runnable examples

| Example | Script |
| ------- | ------ |
| Hello Sprite | [../examples/hello_sprite.py](../examples/hello_sprite.py) |
| Animated Sprite | [../examples/animated_sprite.py](../examples/animated_sprite.py) |
| Sprite Atlas | [../examples/sprite_atlas.py](../examples/sprite_atlas.py) |
| Tilemap | [../examples/tilemap.py](../examples/tilemap.py) |
| Parallax and Layers | [../examples/parallax_layers.py](../examples/parallax_layers.py) |
| Twin-Stick Shooter | [../../Samples/TwinStick](../../Samples/TwinStick) |
| 2D Dynamic Lighting | [../examples/lighting_demo.py](../examples/lighting_demo.py) |
| 2D Camera Controller | [../examples/camera_demo.py](../examples/camera_demo.py) |
| 2D Particle Emitter | [../examples/particles_demo.py](../examples/particles_demo.py) |
| 2D Materials (flash + outline) | [../examples/materials_demo.py](../examples/materials_demo.py) |
| Side-Scroller Slice (integration) | [../examples/sidescroller_demo.py](../examples/sidescroller_demo.py) |
| 2D UI / HUD | [../examples/ui_hud_demo.py](../examples/ui_hud_demo.py) |
| 2.5D Quick-Start | [../examples/quickstart_demo.py](../examples/quickstart_demo.py) |

Run an example in the editor:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/<script>.py
```
