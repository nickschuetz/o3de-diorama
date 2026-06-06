# How-To: Start a New 2.5D Game from the Template

The gem ships an O3DE **project template**, `Diorama2DGame`, so you can scaffold a fresh
project with the Diorama gem already enabled and 2.5D starter content in place, in one
command, instead of wiring a project up by hand.

## One-time setup

Register the Diorama gem and the template with your engine (from a clone of this repo):

```bash
o3de register --gem-path <path-to>/o3de-diorama/Code
o3de register --template-path <path-to>/o3de-diorama/Templates/Diorama2DGame
```

(Use the `o3de` CLI that ships with your engine: `scripts\o3de.bat` on Windows,
`scripts/o3de.sh` on Linux. On an RPM SDK install it is the versioned CLI, e.g.
`o3de2605-cli`.)

## Create a project

```bash
o3de create-project --project-path <path-to>/MyGame --template-name Diorama2DGame
```

This produces a complete, buildable O3DE project at `MyGame` with:

- the **Diorama** gem enabled in `project.json` (alongside the standard 2D-friendly
  gems: Atom, LyShine, MiniAudio, PhysX, and the rest),
- a `STARTING.md` first-steps guide written into the project,
- a `Assets/Diorama/Starter/starter_sprite.lua` you can attach to an entity to see a
  world-space sprite driven entirely through `DioramaSpriteRequestBus`,
- the standard `Levels/DefaultLevel` to open and build in.

## Build and run

```bash
cd <path-to>/MyGame
cmake -B build/linux -S . -G "Ninja Multi-Config"
cmake --build build/linux --config profile --target MyGame.GameLauncher Editor
```

Open the Editor, load `Levels/DefaultLevel`, and start adding Diorama components. The
project's `STARTING.md` walks through your first sprite; the gem's other how-tos
(starting with [17-quickstart.md](17-quickstart.md)) cover every feature, and
[reference/api.md](../reference/api.md) is the full script/agent API.

## What the template is (and is not)

The template is the stock O3DE project template plus the Diorama gem and 2.5D starter
assets, so building the generated project requires the Diorama gem to be registered
(the one-time setup above). It is the fast on-ramp: a correctly configured project, not a
finished game. Building out a fully pre-populated demo level inside the template is a
follow-up.
