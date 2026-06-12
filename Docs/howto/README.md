# Diorama How-To Guides

Step-by-step guides that follow the teaching ladder in
[../examples-outline.md](../examples-outline.md), from a single sprite to a
complete game. Each rung has a written guide and, where applicable, a runnable
example under [../examples](../examples).

## Running the examples (Windows or Linux)

Most guides have a runnable Python example you launch through the Editor. The
commands are shown for Linux; on Windows substitute the platform bits. The shape
is the same:

```bash
# Linux
<engine>/bin/Linux/profile/Default/Editor \
  --project-path /path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/<example>.py
```

```powershell
# Windows (one line; or use a backtick ` for continuation in PowerShell)
& "C:\O3DE\26.05\bin\Windows\profile\Default\Editor.exe" --project-path C:\path\to\YourProject --runpython C:\path\to\o3de-diorama\Docs\examples\<example>.py
```

Substitutions when a guide shows a Linux command:

| Linux | Windows |
| --- | --- |
| `bin/Linux/profile/Default/Editor` | `bin\Windows\profile\Default\Editor.exe` |
| `scripts/o3de.sh` | `scripts\o3de.bat` |
| `scripts/*.sh` (capture helpers) | `scripts\*.ps1` |
| `\` line continuation | one line, or a backtick `` ` `` in PowerShell |
| `/` path separators | `\` |

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

The ladder's guides cover sprites through 2.5D layering. An early twin-stick
shooter sample exists but is not yet polished, so it is kept out of the published
tree for now.

## Feature demos

Focused, one-feature-each demos that extend the ladder as new gem features land.
Each has a written guide and a runnable example.

| Feature | Guide | Status |
| ------- | ----- | ------ |
| 2D Dynamic Lighting | [07-lighting.md](07-lighting.md) | Written |
| 2D Camera Controller | [08-camera.md](08-camera.md) | Written |
| 2D Particle Emitter | [09-particles.md](09-particles.md) | Written |
| 2D Materials (flash + outline) | [10-materials.md](10-materials.md) | Written |
| Crisp Pixel Art (point filter + no mipmaps) | [06-pixel-art.md](06-pixel-art.md) | Written |
| Side-Scroller Slice (integration) | [11-sidescroller.md](11-sidescroller.md) | Written |
| World-Space HUD (health bars/icons via sprites) | [13-world-space-hud.md](13-world-space-hud.md) | Written |
| Make it Glow (post-processing + emissive) | [14-glow.md](14-glow.md) | Written |
| Sound (SFX and music via MiniAudio) | [15-audio.md](15-audio.md) | Written |
| Retro CRT Overlay (scanlines) | [16-crt.md](16-crt.md) | Written |
| Skeletal Cutout Animation (keyframed bone rig) | [18-skeletal.md](18-skeletal.md) | Written |
| Aseprite Animation Import (tags + per-frame timing) | [19-aseprite.md](19-aseprite.md) | Written |
| Input Action Mapping (rebindable named actions) | [23-input-actions.md](23-input-actions.md) | Written |
| Animation State Machine (parameter-driven clip switching) | [22-anim-state-machine.md](22-anim-state-machine.md) | Written |
| Fighting building blocks (frame events, hitboxes, versus camera) | [21-fighting.md](21-fighting.md) | Written |
| Bullet patterns (danmaku / shmup emitter) | [24-bullet-patterns.md](24-bullet-patterns.md) | Written |
| Side-scroller platforming (one-way platforms + ramps) | [25-platformer.md](25-platformer.md) | Written |
| 2.5D brawler (depth lanes + depth-aware combat) | [26-brawler.md](26-brawler.md) | Written |
| Grid intelligence (FOV, movement range, A* pathfinding) | [27-grid-intelligence.md](27-grid-intelligence.md) | Written |
| Day/night cycle (time-of-day lighting) | [28-day-night.md](28-day-night.md) | Written |

## Tooling

| Topic | Guide | Status |
| ----- | ----- | ------ |
| Start a New 2.5D Game from the template | [20-template.md](20-template.md) | Written |
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
- [API reference](../reference/api.md): the typed request buses for every feature
  (sprite, tilemap, camera, lighting, particles, parallax, collision, UI, audio,
  post-processing, CRT, skeletal, Aseprite, input actions, animation state machine)
  for scripts and agents.

## Runnable examples

| Example | Script |
| ------- | ------ |
| Hello Sprite | [../examples/hello_sprite.py](../examples/hello_sprite.py) |
| Animated Sprite | [../examples/animated_sprite.py](../examples/animated_sprite.py) |
| Sprite Atlas | [../examples/sprite_atlas.py](../examples/sprite_atlas.py) |
| Tilemap | [../examples/tilemap.py](../examples/tilemap.py) |
| Parallax and Layers | [../examples/parallax_layers.py](../examples/parallax_layers.py) |
| 2D Dynamic Lighting | [../examples/lighting_demo.py](../examples/lighting_demo.py) |
| 2D Camera Controller | [../examples/camera_demo.py](../examples/camera_demo.py) |
| 2D Particle Emitter | [../examples/particles_demo.py](../examples/particles_demo.py) |
| 2D Materials (flash + outline) | [../examples/materials_demo.py](../examples/materials_demo.py) |
| Side-Scroller Slice (integration) | [../examples/sidescroller_demo.py](../examples/sidescroller_demo.py) |
| 2.5D Quick-Start | [../examples/quickstart_demo.py](../examples/quickstart_demo.py) |
| Day/Night Cycle | [../examples/daynight_demo.py](../examples/daynight_demo.py) |

Run an example in the editor:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/<script>.py
```
