# Diorama How-To Guides

Step-by-step guides that follow the teaching ladder in
[../examples-outline.md](../examples-outline.md), from a single sprite to a
complete game. Each rung has a written guide and, where applicable, a runnable
example under [../examples](../examples).

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

Run an example in the editor:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/<script>.py
```
