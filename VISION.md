# Diorama: World-Space 2D and 2.5D for Open 3D Engine

## The gap

O3DE today has no first-class path for world-space 2D. LyShine is screen-space UI, and Atom is a modern 3D PBR renderer. A developer who wants to build a 2D or 2.5D game ends up working against the engine: abusing UI canvases, hand-rolling quads, or bolting sprites onto 3D meshes. There is no shared, idiomatic way to place a sprite or a tilemap in the world.

## The effort

Diorama is a gem that makes O3DE a credible engine for 2D and 2.5D games by adding world-space sprite and tilemap rendering through Atom. It is delivered as a clean, upstreamable gem rather than an engine fork. It does not replace or compete with LyShine. LyShine stays the UI layer; Diorama owns world-space 2D content.

## What 2D and 2.5D mean here

- 2D: flat content composed in the world. Sprites, tilemaps, sprite-sheet animation, orthographic and pixel-perfect cameras.
- 2.5D: those flat elements living inside a real 3D scene. Camera-facing billboards, depth-sorted layers, parallax, and free mixing of sprites with 3D geometry, lighting, physics, and post effects.

The same components serve both. 2.5D is what you get for free by rendering 2D through a 3D renderer instead of maintaining a separate 2D pipeline.

## Why it matters

- It lowers the barrier to entry for indie, mobile, and education users, the audiences that drive engine adoption.
- It reuses Atom's strengths (batching, modern GPU features, post-processing) instead of maintaining a parallel 2D stack.
- A 2D game in O3DE can still get dynamic lights, particles, and a 3D background. That 2.5D sweet spot is something pure-2D engines cannot reach.

## Design priorities

These are treated as acceptance criteria, not aspirations.

- Security: asset-sourced data (tilemap dimensions, indices, texture references) is validated and bounded in builders and at load. Builder inputs are treated as untrusted. No unchecked sizes feed GPU buffers.
- Performance: no per-frame allocations in the render loop, sprites batch into shared dynamic buffers, the path scales from one sprite to a batched feature processor for thousands, and 2.5D depth and transparency are handled without overdraw blowups.
- Efficiency: the runtime dependency surface stays minimal (no Qt, no AzToolsFramework in shipped clients), serialized formats are compact, and product assets load without runtime parsing.
- Ease of use: clear component UI, sensible defaults, documented workflows, and a runnable showcase scene as a worked reference.

## How the architecture serves the goal

- Two-module split: a lightweight runtime client module and a heavy Qt editor module. Shipped games carry only the runtime, so adding 2D costs little at runtime while authors still get real tooling.
- Integration over reinvention: Diorama builds on existing O3DE systems (transforms, prefabs, scripting, physics) rather than inventing parallel ones.
- A rendering path designed to scale: a batched Atom feature processor where sprites and tilemap tiles sharing a texture and sort layer collapse into one draw, with automatic camera-distance depth sorting and soft ground shadows for the 2.5D look.

## Boundaries

- World-space only.
- No dependency on LyShine and no overlap with its component services.
- Complements the engine, does not patch it. Anything found broken or improvable in the engine along the way is contributed back upstream to o3de.

## Success looks like

A developer enables one gem, drops a Sprite or Tilemap component on an entity, and gets a performant 2D or 2.5D game with full access to the 3D world: dynamic lights, particles, post effects, and 3D geometry in the same scene. The path from "one sprite" to "a lit, animated, depth-sorted scene" is short, taught by the how-to ladder and reference docs, and demonstrated by a runnable showcase (the cartoon solar-system diorama).

Concretely, success means:

- **Real and trusted.** The gem builds with its unit tests green on both Linux and Windows, ships an SBOM and a security policy, and stays a clean, upstreamable gem: no engine fork, and no Qt or AzToolsFramework in shipped clients.
- **Every feature reachable two ways.** Each runtime knob is in the Inspector and on a typed request bus, so a human authors in the editor and a script or an AI agent drives the exact same API. That parity is a deliberate bet that a lot of new 2D/2.5D games will be built with AI assistance.
- **Broad enough to earn its place upstream.** Sprites, tilemaps with autotiling, 2.5D depth and parallax, dynamic lighting, particles, post, a 2D camera, collision, and animation (including native Aseprite import) cover what indie, mobile, and education projects actually need, making O3DE a credible 2D/2.5D choice, with fixes found along the way contributed back to the engine.
