# Design: 2D post-processing (bloom, color grading, vignette, retro)

Status: in progress (Tier-1 roadmap item). The emissive hook (deliverable 2) and the
teaching how-to (deliverable 1) have shipped; see [howto/14-glow.md](../howto/14-glow.md).
The v3 convenience component (the "Diorama Look": one component applying tuned bloom +
vignette via Atom's PostProcessFeatureProcessor, drivable through DioramaLookRequestBus)
has shipped (DioramaLookComponent + EditorDioramaLookComponent). The CRT scanline
overlay shipped as an AuxGeom overlay (DioramaCRTComponent); the true warping CRT
fullscreen pass (deliverable 3) is still pending.

## Goal

Give a 2D/2.5D scene a modern, art-directed look: bloom on glowing sprites,
color grading/tone, vignette, and an optional CRT/scanline retro filter. The
guiding finding: **Atom already does all of this, and our sprites already feed
it**, so most of the work is teaching and a small emissive hook, not new passes.

## Key findings (Atom 26.05)

- Atom ships the components users need: `PostFxLayer`, `Bloom`, `Vignette`,
  `LookModification` (3D-LUT color grading), `ExposureControl`, `DisplayMapper`
  (tone mapping). They are driven by `PostProcessFeatureProcessor`
  (`Gems/AtomLyIntegration/CommonFeatures/.../PostProcess/...`,
  `Gems/Atom/Feature/Common/.../PostProcess/...`).
- Enablement is simple: a `PostFxLayer` on an entity (with `Priority` and
  `OverrideFactor`) applies globally; add a `Shape` + `ShapeWeightModifier` to make
  it a local volume. Minimum whole-scene bloom = one entity with `PostFxLayer` +
  `Bloom`.
- **Pipeline order**: ... -> OpaquePass -> **TransparentPass (our sprites, HDR)** ->
  **PostProcessPass (bloom, vignette, grading)** -> DisplayMapper -> swapchain.
  So our transparent sprites are in the image that gets bloomed and graded. No
  pipeline change needed; the default pipeline already post-processes them.
- **Bloom keys on HDR brightness** (default threshold 1.0, soft knee 0.5). Any
  sprite pixel written `> 1.0` blooms. That is the bridge to the lighting feature:
  an **emissive channel** that pushes a sprite's color above 1.0 makes it glow.

## Decision

Do not reinvent Atom post. Deliver three small things instead:

1. **Teach it.** A how-to ("Make it glow: post-processing a Diorama scene") that
   shows adding `PostFxLayer` + `Bloom` (+ optional `Vignette` / `LookModification`)
   and tuning the bloom threshold for sprite glow. Near-zero code, high payoff.
2. **The emissive hook.** Add an emissive term to the sprite shader/material (an
   emissive tint/strength, optionally an emissive map) so a sprite can deliberately
   write `> 1.0` and bloom. This is v3 of the lighting design
   ([Docs/design/2d-lighting.md](2d-lighting.md)) and the per-sprite materials work
   ([Docs/design/2d-materials.md](2d-materials.md)); cross-linked, built once.
3. **Optional CRT/scanline pass.** Ship a `DioramaCRTComponent` that injects a
   fullscreen compute pass (pattern: Atom's `Vignette.pass` ComputePass) into the
   pipeline at runtime via the pass system (`PassRequestBus` / `RenderPipeline`
   pass insertion), with scanline density, curvature, and chromatic-offset
   parameters. No engine fork: the pass and shader ship as gem assets and are added
   to the running pipeline, removable when the component deactivates.

## What this is not

We are not adding our own bloom/grading/vignette. Those exist and are good; a
parallel implementation would be wasted work and would not compose with other
content in the scene. Our value is the 2D-friendly emissive hook, the guidance, and
the one effect Atom does not ship (a deliberately retro CRT look).

## Security and performance

- The CRT pass is a single fullscreen compute dispatch; it adds one pass only when
  the component is present, and removes it on deactivate (no leaked passes).
- CRT parameters are clamped in the component before reaching the shader CB.
- Emissive is just a shader term; no new buffers, no batching impact.

## Phasing

1. **v1**: the how-to + verify that emissive sprites bloom with a stock
   `PostFxLayer` + `Bloom` (proves the integration end to end).
2. **v2**: the emissive channel in the sprite material (shared with materials +
   lighting work).
3. **v3**: the `DioramaCRTComponent` fullscreen pass; optionally a "Diorama Look"
   convenience component that sets sensible 2D post defaults (a tuned bloom +
   gentle vignette) so users get a good look from one component.

## Verification plan (needs the monitor)

- Stock `PostFxLayer` + `Bloom` on an entity: a bright/emissive sprite blooms; a
  normal sprite does not. Tune threshold and confirm the knee softens the edge.
- `Vignette` + `LookModification` (a test LUT): scene darkens at edges / grades.
- `DioramaCRTComponent`: scanlines + curvature appear; removing the component
  cleanly restores the image (pass removed, no artifacts).
