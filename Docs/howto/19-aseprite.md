# How-To: Aseprite Animation Import

Bring an [Aseprite](https://www.aseprite.org/) animation into Diorama and play its
named tags ("walk", "attack", "idle") on a sprite. The **Aseprite Animation** component
imports Aseprite's standard *Export Sprite Sheet* output (a PNG atlas + a JSON sidecar)
and plays it by driving a Diorama **Sprite** on the same entity: it points the sprite at
the atlas and sets its UV region to the current frame each tick, honoring each frame's
own duration and the tag's direction (forward, reverse, ping-pong).

This is the format the whole ecosystem consumes, so there is no third-party runtime to
ship: the importer is plain JSON parsing, fully open source (Apache-2.0 OR MIT, matching
O3DE).

## Export from Aseprite

In Aseprite: **File -> Export Sprite Sheet**.

- **Output**: tick **Output File** (the PNG atlas) and **JSON Data** (the sidecar).
- **JSON Data**: either "Hash" or "Array" form works; the importer reads both.
- **Meta**: tick **Tags** so your animation tags come across.
- Leave **Trim** off for v1 (the player maps each frame's full rect to the sprite quad;
  trimmed source offsets are not applied yet).

Copy the exported `.png` and `.json` into your project (for example under
`Assets/Diorama/Textures/`). The Asset Processor will produce the PNG as a streamable
texture; note its product path (for example `diorama/textures/hero.png`).

## Set it up in the editor

1. On an entity that has a Diorama **Sprite** component, add an **Aseprite Animation**
   component (`Diorama` category).
2. Under **Import**, set **Aseprite JSON** to the path of your exported `.json`. On
   change it parses immediately; **Status** reports how many frames and tags it read and
   the atlas size. The **Frames** and **Tags** lists fill in.
3. Set the config's **Atlas texture** to the product path of the exported PNG (the
   matching atlas), for example `diorama/textures/hero.png`.
4. Pick a **Default tag** to play on activate (the first tag is filled in for you), and
   set **Loop**, **Speed**, and **Auto play** as desired.

Enter game mode and the sprite plays the tag.

## Drive it from script

`DioramaAsepriteRequestBus` is reflected `Common`, so a script, Script Canvas, or an
agent plays tags like any other Diorama feature:

```lua
DioramaAsepriteRequestBus.Event.PlayTag(self.entityId, "walk")
DioramaAsepriteRequestBus.Event.SetSpeed(self.entityId, 1.5)

-- Play a one-shot, then check when it has finished:
DioramaAsepriteRequestBus.Event.SetLooping(self.entityId, false)
DioramaAsepriteRequestBus.Event.PlayTag(self.entityId, "attack")
if not DioramaAsepriteRequestBus.Event.IsPlaying(self.entityId) then
    DioramaAsepriteRequestBus.Event.PlayTag(self.entityId, "idle")
end
```

`GetCurrentFrame` and `IsPlaying` let a script sequence animations without a screenshot.

## Play a native `.aseprite` import directly (asset reference)

Drop a `.aseprite` file into your project and the native AssetBuilder produces a
`.dioramasheet` product (the packed atlas + frames + tags). Instead of importing the JSON
by hand, assign that product to the component's **Sheet asset** field. At runtime the
component loads it and plays it -- no JSON step, and the atlas texture comes from the sheet
too. The assigned sheet takes precedence over any inline (JSON-imported) data; leave the
field empty to use the JSON-import path. Either way, playback runs through the exact same
`Document` model and timeline.

## How it works

The import parses the JSON into the component's serialized config (frames = atlas rects
+ durations, tags = named frame ranges + direction), so the **runtime does no file IO** :
it plays straight from serialized data. The parse and the playback timeline
(per-frame durations, ping-pong that does not duplicate the endpoints) are a pure,
unit-tested core (`AsepriteImport.h`). At runtime the component sets the sprite's texture
once, disables the sprite's own frame-grid animation so the two do not fight, and writes
the current frame's UV rect each tick.

## Scope

Two import paths are supported: the standard **non-trimmed sprite-sheet export** (PNG +
JSON, imported in the editor), and the native **`.aseprite` binary** directly (layers,
cels, blend modes, no export step) via the AssetBuilder, which packs a `.dioramasheet`
you reference as above. Both reuse the same `Document` model and timeline, and the binary
format is parsed ourselves (the documented open format) rather than bundling any
third-party runtime, keeping the licensing clean. Trimmed-frame source offsets and slices
are follow-ups.
