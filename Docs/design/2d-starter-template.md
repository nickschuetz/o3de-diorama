# Design: instant-start "New 2.5D Game" template and robustness pass

Status: **shipped**: the `Diorama2DGame` project template (`Templates/Diorama2DGame`)
+ [how-to 20](../howto/20-template.md).

## Goal

Make the cold start near zero: a user should go from nothing to a running 2.5D
scene in one step, either by creating a project from a Diorama template or by
enabling the gem and loading a starter scene. First impressions are the point of
the alpha, and "enable gem, see nothing, read ten docs" is the impression to avoid.

## Key findings (O3DE 26.05)

- **A gem can ship project templates**: `gem.json` carries a `"templates"` array of
  relative paths, discovered by `get_gem_templates` (`scripts/o3de/o3de/manifest.py`).
  A template is a folder with `template.json` (metadata + `copyFiles`, token
  substitution like `${Name}`) and a `Template/` tree (`project.json`, `Levels/`,
  `Registry/`, CMake). So `o3de create-project --template <DioramaTemplate>` is
  available to us.
- **Gems ship sample content as prefabs/levels**: e.g.
  `Gems/MotionMatching/Assets/Levels/*.prefab`. A gem can ship a `.prefab` that is
  instantiated into any level.
- There is **no `project_default_level` field**; a level loads by convention or via a
  bootstrap setting (the `Registry` `"LoadLevel"` setreg the sample already uses).

## Approach

Two complementary deliverables, plus a robustness pass:

1. **A shipped starter prefab/level (for existing projects).** A
   `DioramaStarter.prefab` in the gem assets: a tilted 2.5D camera, a floor tilemap,
   a sprite or two, input, and the camera-follow script
   ([Docs/design/2d-camera.md](2d-camera.md)). Document the one-liner to load it.
   This is the fastest path for someone who already has a project and just enabled
   the gem.
2. **A `Diorama 2.5D Game` project template (for new projects).** Referenced from
   `gem.json` `"templates"`, so `o3de create-project --template Diorama2DGame`
   produces a project with the gem enabled, the starter level as the default, the
   right `Registry` (CVar defaults, load-level), and a minimal CMake. From zero to a
   running 2.5D scene in one command.
3. **Robustness pass (the second half of this item).** Audit that "enable gem -> add
   a Sprite -> it just works" with sensible defaults and no manual asset wiring:
   - The feature processor already enables on demand (`TryAcquireFeatureProcessor`),
     and depth-sort/shadows default on; keep those.
   - Ship a default sprite texture so a fresh Sprite is visible immediately rather
     than showing the magenta fallback.
   - Sensible component defaults (a good 2.5D camera pitch, a sane sprite size/sort).
   - Confirm the cold paths (scene-not-ready-at-activation retry, async texture load)
     are covered, which the presenter already handles.

## Security and performance

- Template and prefab content are static assets validated by the normal asset
  pipeline; no runtime parsing or unchecked input.
- The starter scene is intentionally small (one camera, one floor, a couple of
  sprites) so it loads instantly and reads as "clean," not "demo soup."

## Phasing

1. **v1**: the `DioramaStarter.prefab` + a "load it" how-to + the default-texture and
   default-value robustness fixes (the highest impact for the least work).
2. **v2**: the full `Diorama 2.5D Game` project template wired through `gem.json`.
3. **v3**: an editor "Create Diorama Scene" menu/action (tools module) that
   instantiates the starter prefab into the current level with one click.

## Verification plan (needs the editor / a run)

- Create a project from the template: open it and a 2.5D scene renders and runs with
  no manual setup.
- In a blank project, enable the gem and load the starter prefab: same result.
- Drop a bare Sprite component on a new entity: it shows the default texture
  immediately, not the fallback, at a sensible size.

## Open questions

- Whether the template carries the full twin-stick sample or a minimal "camera +
  floor + sprite" scene; lean minimal for the template (fast, clean) and keep the
  twin-stick as the separate worked sample.
- Default-texture choice: ship a small neutral sprite (the existing white sprite or a
  simple mascot) as the gem's built-in default.
