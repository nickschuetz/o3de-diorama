# Design: Aseprite / sprite-sheet import

Status: **shipped (phases 1, 2a, 2b).** The JSON sprite-sheet importer (editor-time),
the native-binary parser + compositor + atlas packer, AND the AssetBuilder
(`DioramaAsepriteBuilder` -> `.dioramasheet` product, consumed via the component's
asset reference) are all merged and unit-tested. See [how-to 19](../howto/19-aseprite.md).
The detailed phase-2b design is at the
bottom of this doc; the sections above are the original plan and remain the reference.

## Goal

Let artists author in Aseprite (the de-facto indie pixel-art tool) and drop a
`.aseprite` / `.ase` file into the project, getting a ready sprite sheet plus
animation clips with no manual slicing. The `.ase` *file format* is openly
documented, so reading it carries no license constraint (we are not shipping
Aseprite, only importing its open format).

## Key findings (O3DE 26.05)

- A gem adds a source-asset importer by implementing
  `AssetBuilderSDK::AssetBuilderCommandBus::Handler` (`CreateJobs`, `ProcessJob`,
  `ShutDown`) and registering it from a `BuilderComponent` in the tools module
  (pattern: `Gems/OpenParticleSystem/.../Builder/`). The builder declares the source
  extension via `AssetBuilderPattern` and bumps `m_version` to force re-analysis.
- `ProcessJob` emits `JobProduct`s with a stable `m_productSubID`, an asset type, and
  `ProductDependency`s.
- A builder can **delegate image compression** to the existing image pipeline:
  `ImageBuilderRequestBus::ConvertImageObject(...)`
  (`Gems/Atom/Asset/ImageProcessingAtom/.../ImageBuilderComponent.h`) turns our
  composed RGBA into a proper `StreamingImageAsset`, so we never reimplement texture
  compression.
- There is a `TextureAtlas` gem but it is a runtime lookup class with no importer, so
  it does not do this for us; our sprite UV model is independent and fine.

## Approach

A `DioramaAsepriteBuilder` (tools module) that:

1. **Parses the `.aseprite` binary** ourselves (the format is simple: a header, then
   frames, each a list of chunks: layers `0x2004`, cels `0x2005`, tags `0x2018`,
   palette). Cel pixel data uses zlib, which AzCore already provides. We treat the
   file as untrusted and validate every offset and length before reading.
2. **Composites** each frame's visible layers into an RGBA image and **packs** the
   frames into a sheet (v1: a uniform grid, so it drops straight into the existing
   sprite-sheet animation path; v2: a shelf/rect pack for tight atlases).
3. **Emits two products**: (a) the packed sheet, handed to `ConvertImageObject` so it
   becomes a `StreamingImageAsset`; and (b) a small **Diorama sprite-sheet metadata**
   product (grid dimensions or per-frame rects, per-frame durations as fps, and tags
   as named clips) that a Sprite component references. The metadata product carries a
   `ProductDependency` on the streaming image.

## Integration with existing features

- v1's uniform-grid output plugs directly into the current flipbook animation
  (`PlaySpriteSheet`, the grid + fps + loop config), so import works end to end with
  no new runtime code.
- v2's named tags become named clips, which pairs with the clip authoring in
  [Docs/design/2d-editor-authoring.md](2d-editor-authoring.md), and non-uniform frame
  rects pair with free-form atlas slicing there.

## Security and performance

- Every header field, chunk size, frame/layer/cel count, and pixel-data length is
  bounded and validated before use; a malformed file fails the job cleanly rather
  than over-reading (the VISION untrusted-asset criterion, enforced in the builder).
- Compression is delegated to the proven image pipeline; we add no per-frame runtime
  cost (output is an ordinary streaming image + a tiny metadata asset).

## Phasing

1. **v1**: flattened frames -> uniform sheet + grid/fps metadata (works with current
   animation).
2. **v2**: tags -> named clips, per-frame durations, non-uniform rect packing.
3. **v3**: layer and slice import (expose Aseprite slices as named sub-sprites).

## Verification plan

- Import a multi-frame, tagged `.aseprite`: the sheet builds, a Sprite plays the
  frames at the authored fps, and a named tag plays as a clip.
- A truncated/garbage `.ase` fails the job with a clear error and no crash.

## Phase 2b design (detailed): the AssetBuilder

This is the concrete plan for the remaining piece, written before code. Phases 1 + 2a
already give us, all pure and unit-tested:

- `Aseprite::ParseAsepriteBinary(bytes) -> BinarySprite` (`AsepriteBinary.h`),
- `Aseprite::CompositeFrame(sprite, i) -> FrameImage`,
- `Aseprite::PackFramesToGrid(sprite, columns, name) -> PackedAtlas` (atlas RGBA + a
  `Document` of frame rects, durations, and tags).

So 2b is purely the AssetProcessor plumbing around that core.

### 1. The builder and where it lives

A `DioramaAsepriteBuilder` (CreateJobs / ProcessJob / ShutDown) + a `BuilderComponent`
that registers it for the `*.aseprite` and `*.ase` source patterns, following the
`Gems/OpenParticleSystem/.../Builder/` pattern (`BuilderComponent.h` + `ParticleBuilder.h`).

- New source files under `Code/Source/Builders/`. Builders run inside the
  AssetProcessor, which loads the gem's **editor/tools** module, so register the
  `BuilderComponent` from `DioramaEditorSystemComponent` (or add it to the editor
  module's descriptor list). No new runtime cost; the builder is editor/AP-only.
- CMake: the editor/builder target gains `AssetBuilderSDK` (builder + `JobProduct`
  types) and **`Gem::ImageProcessingAtom.Headers`** as dependencies. The latter is the
  header-only API target exposing `ImageProcessing::ImageBuilderRequestBus`; calling an
  EBus needs only its interface header, not the implementation. The handler lives in
  `Gem::ImageProcessingAtom.Editor`, which the AssetProcessor already loads, so we do
  not link the implementation. (Resolved from the SDK; four engine gems depend on
  `.Headers` the same way.)
- Bump the builder's `m_version` to force re-analysis when the logic changes.

### 2. ProcessJob: parse -> pack -> two products

1. Read the source file bytes; `ParseAsepriteBinary`; on failure, fail the job with a
   clear message (no crash) -- the untrusted-asset path is already enforced in the parser.
2. `PackFramesToGrid` to get the atlas RGBA + `Document`. v1 uniform grid; columns =
   `ceil(sqrt(frames))` (squarish) or a fixed width, a tuning detail.
3. **Image product:** build an `ImageProcessingAtom::IImageObjectPtr` from the packed
   RGBA (`R8G8B8A8`) and hand it to `ImageBuilderRequestBus::ConvertImageObject(...)`,
   which returns `JobProduct`s for a normal `StreamingImageAsset` -- we reuse the proven
   texture pipeline and never reimplement compression.
4. **Metadata product:** serialize the `Document` (atlas size, per-frame rects +
   durations, tags) to a small reflected product asset with a `ProductDependency` on
   the streaming image.

### 3. The metadata asset type (resolved open question)

Use a **small dedicated reflected asset** (`DioramaAsepriteSheetAsset`) wrapping the
`Document` data, not a struct embedded on the Sprite config. Rationale: a product asset
can be referenced by `AssetId`, shared across entities, and carry the image dependency;
embedding on the config only suited the editor-time JSON import (which bakes into one
component). The `Document` model is already defined, so this is a thin reflected wrapper.

### 4. Component consumption + parity (the one real runtime addition)

`DioramaAsepriteComponent` currently consumes a config with **embedded** frames/tags
(filled by the editor-time JSON import) plus an atlas texture path. Phase 2b adds an
**asset-reference mode**: an optional `DioramaAsepriteSheetAsset` reference; when set,
the component loads it (and its dependent streaming image) and plays from it instead of
the embedded `Document`. Per the standing **parity** rule, the asset reference is both an
Inspector field (an asset picker) and reachable from the bus (e.g.
`SetSheetByAssetPath(path)`), so an agent can swap sheets exactly as a human can. The
existing embedded-config path stays, so the JSON importer is unaffected.

### 5. Verification plan (AssetProcessor, not unit tests)

The builder is not unit-testable; it is verified end to end against a running AP:

1. Generate a valid `.aseprite` fixture to disk (the test fixture writer already emits
   valid bytes) under a project's `Assets/`.
2. Restart AssetProcessor with the rebuilt gem; watch the AP log for the
   `DioramaAsepriteBuilder` job and a clean success.
3. Confirm the **cache** holds the `StreamingImageAsset` product and the
   `DioramaAsepriteSheetAsset` product with the image as a product dependency.
4. In a level, point a Sprite + Aseprite component at the sheet asset and play a tag
   (this is the on-screen step that needs the editor).
5. Negative: a truncated/garbage `.ase` fails the job cleanly with no crash.

Because 2b is build-green-but-not-headlessly-verified until step 2-3 pass, it lands only
after a real AP run confirms the products, not on compile alone.

## Licensing

The whole path stays dual **Apache-2.0 OR MIT**, matching O3DE:

- The `.aseprite` reader is **our own code written from the openly published file-format
  spec**; we ship no Aseprite source. Implementing a documented format carries no
  license obligation (the Aseprite application's own license does not reach a clean-room
  reader of its format).
- ZLIB inflate reuses `AzCore::ZLib` (the permissive zlib license, already bundled) --
  no new dependency. `AssetBuilderSDK` and ImageProcessingAtom are O3DE (Apache-2.0 OR
  MIT). Every new file carries the SPDX header the lint CI enforces.
- Not allowed: bundling Aseprite source, GPL/again-restricted runtimes, or third-party
  art of unclear provenance. Test fixtures are synthetic bytes generated in-repo, never
  copyrighted art.

## Open questions

- Multi-layer blend-mode fidelity (v3): match Aseprite's normal/multiply/etc. or
  flatten with normal blending in v1/v2 (v1/v2 flatten with normal).
- Frame packing: uniform grid (v1, plugs into the current model) vs. a tight rect/shelf
  pack (later) for sprites with lots of empty space.
