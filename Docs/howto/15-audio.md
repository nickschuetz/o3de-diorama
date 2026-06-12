# How-To: Sound (SFX and music)

Add sound to a Diorama game. O3DE 26.05 ships the **MiniAudio** gem (a lightweight,
dependency-free audio engine; enabled in this project), with a per-entity playback
component and a typed request bus that is reflected for Lua, Python, and Script
Canvas. So, as with post-processing, Diorama leans on the engine's audio rather than
shipping its own: this guide is the blessed 2D path, and the AI-friendly angle is
that an agent drives the same bus.

> Note: audio output cannot be verified from a headless screen capture. The sound
> asset processing, the components, and the script calls below are real and verified
> to build/load; confirm the audio *plays* by listening in game mode.

## Build it

A one-command scene builder assembles this guide's demo in its own level
(an input-wired soundboard: press 1/2/3 in game mode for the bundled one-shots); finish any wiring the editor cannot script (noted in its output), then
enter game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/audio_demo.py
```

## A sound asset

Drop a `.wav` / `.ogg` / `.mp3` / `.flac` into the project; the asset processor turns
it into a `.miniaudio` product. This gem ships a small set of original generated
SFX under `diorama/audio/` to play with:

| Sound | Use |
| ----- | --- |
| `blip.wav` | a plain tone / UI beep |
| `ui_click.wav` | a short high tick (menu/UI) |
| `pickup.wav` | a bright two-step (coin / collect) |
| `jump.wav` | an upward sweep (jump / launch) |
| `hit.wav` | a low thud (impact / damage) |

## Play a sound

1. Add a **MiniAudio Playback** component to an entity.
2. Set its **Sound** to your asset (e.g. `diorama/audio/blip.wav`). Tick **Looping**
   for music or an ambient loop.
3. Trigger it from script through the typed bus, addressed by the entity id:

```lua
MiniAudioPlaybackRequestBus.Event.Play(self.entityId)
MiniAudioPlaybackRequestBus.Event.Stop(self.entityId)
MiniAudioPlaybackRequestBus.Event.SetLooping(self.entityId, true)
MiniAudioPlaybackRequestBus.Event.SetVolumePercentage(self.entityId, 0.5)
```

The runnable example [`play_sound.lua`](../../Assets/Diorama/Examples/Audio/play_sound.lua)
plays the entity's sound when it activates. Because the bus is reflected `Common`,
the exact same calls work from editor Python, Script Canvas, and an AI agent -- the
same parity as the sprite/HUD buses.

## Music vs SFX vs spatial

- **Music / UI SFX (2D, non-spatial)**: one entity with a looping (music) or
  one-shot (SFX) Playback component. No listener needed for non-spatial playback.
- **Spatial SFX**: add a **MiniAudio Listener** component to the camera entity; the
  Playback component then attenuates with distance and direction. For a top-down or
  side view, place the listener with the camera.
- **Master volume**: the system bus `MiniAudioRequestBus` exposes
  `SetGlobalVolume` / `SetGlobalVolumeInDecibels`.

## Fire-and-forget one-shots

MiniAudio playback is per-entity, which is ideal for music and looping sources but
verbose for the many short SFX a game fires (hits, pickups, footsteps). Diorama adds
a one-call convenience on top: `DioramaAudioRequestBus` (a global bus, reflected
`Common`).

```lua
-- Play a sound once, fire and forget; no entity or component wiring.
DioramaAudioRequestBus.Broadcast.PlayOneShot("diorama/audio/blip.wav", 1.0)
-- Master volume for all audio.
DioramaAudioRequestBus.Broadcast.SetMasterVolume(0.8)
```

`PlayOneShot(productPath, volume)` plays from a small internally managed voice pool
(8 voices, round-robin, so overlapping SFX do not cut each other off). It accepts
either the source name (`diorama/audio/blip.wav`) or the product
(`...wav.miniaudio`). Because the bus is `Common`, a game script, Script Canvas, or
an AI agent all fire SFX with the same one line -- the audio parallel to the sprite
and HUD buses.

Use `PlayOneShot` for transient SFX; use a Playback component (above) for music and
looping/positional sources you start and stop.
