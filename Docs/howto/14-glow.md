# How-To: Make it Glow (post-processing)

Give a 2D/2.5D scene a modern look -- bloom, color grading, vignette -- using Atom's
post-process stack, plus the Diorama **emissive** hook that makes specific sprites
glow. The key finding: Atom already does post-processing, and Diorama's transparent
sprites are in the image it post-processes, so this is mostly enabling + one gem
feature (emissive), not new render passes.

## Turn on post-processing (Atom, no code)

Atom ships the components; enabling whole-scene bloom is one entity:

1. Add an entity, add a **PostFX Layer** component (sets it as a post-process volume;
   `Priority` orders overlapping layers, `Overrideable Factor` blends it in).
2. On the same entity add a **Bloom** component. That is it -- bright (HDR > 1.0)
   pixels now bloom.
3. Optionally add **Vignette**, **Look Modification** (3-D LUT color grading), or
   **HDR Color Grading** to the same entity to art-direct the whole scene.

Bloom keys on HDR brightness (default threshold ~1.0). A normally lit sprite sits
around 1.0 and barely blooms; to make a sprite *deliberately* glow, push it above
1.0 with emissive.

## One component: the 2D Look (Diorama)

If you would rather not hand-add and tune PostFX Layer + Bloom + Vignette, add a
single **2D Look** component (from the Diorama category) to any entity, e.g. the
camera. It drives Atom's stock `PostProcessFeatureProcessor` with 2D-friendly
defaults (bloom threshold `1.0`, knee `0.5`, intensity `0.5`, plus a gentle
`0.3` vignette), so you get the look from one component and it still composes with
any other post content in the scene. It uses the exact same engine bloom/vignette
as the manual path above; it is just a tuned, one-stop front door. Removing the
component cleanly removes its post settings.

Because it is a real component, it is also drivable at runtime for AI/human parity
through `DioramaLookRequestBus` (same convention as every other Diorama bus):

```lua
DioramaLookRequestBus.Event.SetBloomIntensity(entityId, 0.8)
DioramaLookRequestBus.Event.SetBloomThreshold(entityId, 1.0)
DioramaLookRequestBus.Event.SetVignetteIntensity(entityId, 0.4)
DioramaLookRequestBus.Event.SetBloomEnabled(entityId, true)
```

```python
import azlmbr.bus as bus
import azlmbr.diorama as diorama

diorama.DioramaLookRequestBus(bus.Event, "SetBloomIntensity", entityId, 0.8)
```

The component is the convenience layer; the emissive hook below is still what makes
an individual sprite glow.

## The emissive hook (Diorama)

A Diorama sprite can write above HDR 1.0 so it blooms, via an emissive term added
after lighting. Drive it from the sprite bus, like any other material verb:

```lua
-- Make a pickup glow cyan; intensity > 1 pushes it into the bloom range.
DioramaSpriteRequestBus.Event.SetEmissive(pickup, 0.2, 0.9, 1.0, 3.0)
DioramaSpriteRequestBus.Event.SetEmissive(pickup, 0.0, 0.0, 0.0, 0.0) -- back to normal
```

`SetEmissive(r, g, b, intensity)`: the color times `intensity` is added to the lit
result. `intensity` 0 (default) is off; values above 1 make the sprite glow and
bloom. Emissive is part of the per-sprite material set (with flash and outline), so
it batches and costs nothing when unused.

Without a Bloom component in the scene an emissive sprite simply renders brighter
(over-bright); add PostFX Layer + Bloom and it blooms.

## What Diorama does and does not do

Diorama does **not** reimplement bloom/grading/vignette -- Atom's are good and
compose with the rest of the scene. Diorama's contribution is the 2D-friendly
emissive hook (so sprites can opt into the glow) and this guidance. A retro
CRT/scanline pass is the one effect Atom does not ship; it is a separate planned
component (see [design/2d-post-processing.md](../design/2d-post-processing.md)).
