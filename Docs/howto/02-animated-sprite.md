# How-To: Animated Sprite

Teaching ladder rung 2. Plays frames from a sprite sheet on the Sprite component.

## What you will build

A sprite that flips through frames of a sheet on a timer, on a loop.

## The animation model

A sprite sheet is a grid of frames in one texture. The Sprite component plays it
with a few fields:

- **Frame grid**: `frameColumns` x `frameRows` cells, read left-to-right then
  top-to-bottom.
- **Frame count**: how many of those cells are actual frames (allows a partial
  last row), clamped to `columns * rows`.
- **Frames per second**: playback rate.
- **Loop**: restart after the last frame, or hold it.
- **Start frame**: the frame shown first (and in the editor when not playing).

Frames live inside the sprite's UV sub-region, so animation composes with an
atlas sub-rectangle and with the horizontal/vertical flips (rung 3). Playback
runs through the shared presenter, so it animates identically at runtime and in
the editor viewport.

## Steps (in the editor)

1. Add a **Sprite** component and set its texture to a sprite sheet.
2. Set the frame grid (columns, rows) and the frame count.
3. Set frames per second and enable looping.
4. Enable animation.

## Driving it from script or an agent

`PlaySpriteSheet` is the one-call convenience that sets the grid, sets playback,
and enables animation together:

```python
# Play a 2x2 sheet as 4 frames at 4 fps, looping.
diorama.DioramaSpriteRequestBus(bus.Event, "PlaySpriteSheet", eid, 2, 2, 4, 4.0, True)
info = diorama.DioramaSpriteRequestBus(bus.Event, "GetSpriteInfo", eid)  # animEnabled, frameCount, currentFrame
```

There are also granular verbs (`SetFrameGrid`, `SetPlayback`, `SetStartFrame`,
`SetAnimationEnabled`) when you want to change one thing at a time.
`GetSpriteInfo` reports `currentFrame`, so you can confirm playback without
watching the viewport.

## Runnable example

[`../examples/animated_sprite.py`](../examples/animated_sprite.py) treats the 2x2
sample atlas as a four-frame sheet:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/animated_sprite.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Next

Rung 3, [Sprite Atlas](03-sprite-atlas.md), shares one texture across many
sprites with UV sub-regions.
