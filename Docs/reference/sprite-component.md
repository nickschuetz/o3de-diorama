# Reference: Sprite component

The Sprite component draws a single textured quad in world space through Atom.
It is the most basic Diorama renderable: a flat card, sized in world meters,
sampled from a texture (or a sub-rectangle of one), tinted, optionally
billboarded toward the camera, and optionally animated from a sprite sheet.

## The shared configuration

Every parameter on this page lives on one struct, `SpriteComponentConfig`. The
editor component (`EditorSpriteComponent`) authors that struct in the Inspector,
and at build time hands an identical copy to the runtime `SpriteComponent`
through `BuildGameEntity`. There is exactly one set of fields and one set of
semantics, whether you are in the editor or in a running game.

That same configuration is reachable two ways, with identical effect:

- The **editor Inspector**, for a human authoring a scene.
- The typed **`DioramaSpriteRequestBus`**, for a script or an agent driving the
  sprite by entity id.

This is the AI/human parity principle: there is no editor-only knob and no
script-only knob. Every bus verb takes plain scalars (no math types to
construct) and is forgiving (values are validated and clamped, never crash), so
the same change you can make by hand you can also make programmatically and
verify with `GetSpriteInfo`.

The texture asset reference is declared `PreLoad`, so when the component
activates the texture is requested and streamed in before the sprite is
considered ready (see `OnTextureReady` below).

## Parameters

Eighteen serialized parameters, grouped here exactly as the header and the
Inspector group them.

### Texture / Appearance

#### Texture

| | |
| --- | --- |
| Inspector label | `Texture` |
| Config field | `m_texture` (`AZ::Data::Asset<AZ::RPI::StreamingImageAsset>`) |
| Bus setter | `bool SetTextureByPath(AZStd::string_view productPath)` |
| Default | none (unassigned); renders a bundled default texture so the sprite is still visible |
| Load behavior | `PreLoad` |

The texture drawn on the quad. In the Inspector you pick a streaming image
asset; through the bus you assign it by product path (for example
`"diorama/textures/hero.png"`). `SetTextureByPath` returns `false` if the path
does not resolve to an asset, so a script can detect a typo without crashing.

If no texture is assigned, the sprite does not vanish: the feature processor
substitutes a bundled default texture (white, so the per-entity `Tint` still
shows), which keeps a freshly added Sprite visible until you assign artwork.

The asset reference uses the `PreLoad` load behavior: the texture is requested
when the component activates and is streamed in before the sprite reports
itself ready. Use the `OnTextureReady` notification (or poll
`SpriteInfo::m_textureLoaded`) to know when the pixels are actually available
rather than assuming the assignment was instantaneous.

With no texture assigned the quad has nothing to sample and does not draw.

#### Size

| | |
| --- | --- |
| Inspector label | `Size` |
| Config field | `m_size` (`AZ::Vector2`) |
| Bus setter | `void SetSize(float width, float height)` |
| Default | `(1.0, 1.0)` |
| Units | world meters (Inspector suffix ` m`) |
| Range | Inspector `Min` 0.0; bus clamps negative values to zero |

The width and height of the quad in world units. The Inspector shows a ` m`
suffix and enforces a minimum of 0.0. The bus setter clamps negative values to
zero, so a sprite is never given a negative extent.

Size is the world footprint of the card. It is independent of the texture's
pixel resolution and independent of the UV sub-rectangle: a 32x32 atlas cell and
a 256x256 full texture both draw at whatever `Size` you set. Pair `Size` with
`Pivot` to control where that footprint sits relative to the entity origin.

#### Pivot

| | |
| --- | --- |
| Inspector label | `Pivot` |
| Config field | `m_pivot` (`AZ::Vector2`) |
| Bus setter | `void SetPivot(float x, float y)` |
| Default | `(0.5, 0.5)` |
| Range | normalized; bus clamps each component to 0..1 |

The anchor point within the quad, expressed in normalized 0..1 coordinates.
`(0.5, 0.5)` centers the sprite on the entity origin. `(0.5, 0.0)` anchors it at
the bottom edge, which is handy for characters that should stand on the ground
at the entity position. `(0.0, 0.0)` anchors the corner.

The pivot shifts where the `Size` footprint is placed relative to the origin; it
does not change the size or the sampled texture. Changing the pivot moves the
quad, it does not scale it.

#### Tint

| | |
| --- | --- |
| Inspector label | `Tint` |
| Config field | `m_tint` (`AZ::Color`) |
| Bus setter | `void SetTint(float r, float g, float b, float a)` |
| Default | `(1, 1, 1, 1)` (opaque white) |
| Range | each channel clamped to 0..1 (bus) |
| Inspector handler | color swatch |

A color multiplied into the sampled texel. White (the default) leaves the
texture unchanged. Lower RGB to darken or recolor, and use the alpha channel to
control transparency: alpha below 1.0 makes the sprite translucent. Each channel
is clamped to 0..1 by the bus setter.

Because alpha drives transparency, `Tint` interacts with draw ordering. A
translucent sprite is sorted as a transparent draw, where `Sort Offset` (below)
decides what ends up on top. A fully opaque tint avoids the transparent sort
path entirely.

#### Billboard

| | |
| --- | --- |
| Inspector label | `Billboard` |
| Config field | `m_billboard` (`bool`) |
| Bus setter | `void SetBillboard(bool enabled)` |
| Default | `false` |

When `true`, the quad always faces the camera regardless of the entity's
orientation. When `false`, the quad is oriented by the entity transform: it
spans the entity's local X and Z axes.

Billboarding is for sprites that should read as facing the viewer from any
angle (particles, pickups, certain characters). When a sprite is a billboard the
`Double Sided` setting has no visible effect, because a camera-facing quad is
never seen from behind.

#### Double Sided

| | |
| --- | --- |
| Inspector label | `Double Sided` |
| Config field | `m_doubleSided` (`bool`) |
| Bus setter | `void SetDoubleSided(bool enabled)` |
| Default | `true` |

When `true`, the sprite is visible from both sides: the renderer also emits the
back-facing triangles, so the card reads like a printed sheet you can see from
either side. When `false`, only the front face draws and the sprite disappears
when viewed from behind.

The default is `true` because a flat 2D sprite usually wants to behave like a
double-sided card. Turn it off for effects that should only be seen from the
front. As noted above, this setting has no visible effect on a billboard, which
always faces the camera.

#### Point Filter (pixel art)

| | |
| --- | --- |
| Inspector label | `Point Filter (pixel art)` |
| Config field | `m_pointFilter` (`bool`) |
| Bus setter | `void SetPointFilter(bool enabled)` |
| Default | `false` |

When `true`, the sprite samples its texture with nearest-neighbor (point)
filtering instead of the default linear filtering, so low-resolution pixel art
stays crisp and blocky when scaled up instead of being bilinearly blurred. When
`false` (default), linear filtering smooths the texture, which suits photographic
or high-resolution art.

Point filtering is part of the batch key: a point-filtered sprite draws in its
own batch (the sampler is selected per draw), so mixing pixel-art and smooth
sprites in a scene is fine, it just means an extra draw call for each group.

This is the **sampler** half of crisp pixel art. The other half is **mipmaps**:
linear-mipped textures still soften when a sprite is drawn smaller than the
source, because the GPU pulls from a pre-averaged mip. For fully crisp pixel art
at any scale, also import the texture with a **no-mipmap** preset (the built-in
`UserInterface_Lossless` preset works well: uncompressed, no mips, no engine
downscale). See [the pixel-art how-to](../howto/06-pixel-art.md).

### Atlas / UV region

This group is collapsed by default in the Inspector (`AutoExpand` off).

#### UV Min

| | |
| --- | --- |
| Inspector label | `UV Min` |
| Config field | `m_uvMin` (`AZ::Vector2`) |
| Bus setter | `void SetUVRegion(float uMin, float vMin, float uMax, float vMax)` |
| Default | `(0.0, 0.0)` |
| Range | normalized 0..1 (bus clamps each value) |

The top-left corner of the texture sub-rectangle to sample, in normalized
texture coordinates.

#### UV Max

| | |
| --- | --- |
| Inspector label | `UV Max` |
| Config field | `m_uvMax` (`AZ::Vector2`) |
| Bus setter | `void SetUVRegion(float uMin, float vMin, float uMax, float vMax)` |
| Default | `(1.0, 1.0)` |
| Range | normalized 0..1 (bus clamps each value) |

The bottom-right corner of the sampled sub-rectangle. Together with `UV Min`
this selects a region of a texture atlas or sprite sheet. The default
`(0,0)`-`(1,1)` samples the whole texture.

Through the bus, both corners are set in one call, `SetUVRegion`, and each of
the four values is clamped to 0..1. The sub-rectangle is the base region for
everything downstream: the flip flags mirror within it, and the animation frame
grid is laid out *inside* it. That composition is what lets a sprite sheet live
inside one cell of a larger atlas.

#### Flip Horizontal

| | |
| --- | --- |
| Inspector label | `Flip Horizontal` |
| Config field | `m_flipHorizontal` (`bool`) |
| Bus setter | `void SetFlip(bool horizontal, bool vertical)` |
| Default | `false` |

Mirror the sampled region horizontally. This is the usual way to flip a
character to face the other direction without a second texture. Internally the
flip swaps the U coordinates of the sampled corners; it mirrors the *sampling*,
not the quad geometry, so size and pivot are unaffected.

#### Flip Vertical

| | |
| --- | --- |
| Inspector label | `Flip Vertical` |
| Config field | `m_flipVertical` (`bool`) |
| Bus setter | `void SetFlip(bool horizontal, bool vertical)` |
| Default | `false` |

Mirror the sampled region vertically (swaps the V coordinates). The bus setter
`SetFlip` sets both flip flags in one call.

Both flips apply to the active region, which means they also apply correctly to
the current animation frame: flipping a walking sheet mirrors each frame in
place.

### Layering

This group is collapsed by default in the Inspector (`AutoExpand` off).

#### Sort Offset

| | |
| --- | --- |
| Inspector label | `Sort Offset` |
| Config field | `m_sortOffset` (`float`) |
| Bus setter | `void SetSortOffset(float sortOffset)` |
| Default | `0.0` |
| Range | unbounded (positive or negative) |

An additional bias applied to transparent draw ordering. Larger values draw
later, which means on top. Use this to layer overlapping sprites in a 2.5D scene
without physically moving them in depth.

`Sort Offset` only matters for transparent draws (a sprite with alpha below 1.0,
or otherwise on the transparent path). Two overlapping translucent sprites at
the same depth will resolve their order by `Sort Offset`: give the foreground
one a larger value. This is the same draw-order bias the Tilemap component
exposes, so sprites and tilemap layers can be interleaved by sort offset to
build a consistent back-to-front stack.

### Animation

This group is collapsed by default in the Inspector (`AutoExpand` off). When
`Animated` is off, the sprite is static and the remaining animation fields are
ignored.

#### Animated

| | |
| --- | --- |
| Inspector label | `Animated` |
| Config field | `m_animEnabled` (`bool`) |
| Bus setter | `void SetAnimationEnabled(bool enabled)` |
| Default | `false` |

Play frames from a sprite sheet on a timer. When `false` the sprite shows a
single static frame (`Start Frame`) and the grid, rate, loop, and start fields
have no animated effect.

#### Columns

| | |
| --- | --- |
| Inspector label | `Columns` |
| Config field | `m_frameColumns` (`int`) |
| Bus setter | `void SetFrameGrid(int columns, int rows, int frameCount)` |
| Default | `1` |
| Range | Inspector `Min` 1; clamped to at least 1 (`GetFrameColumns`) |

The number of columns in the sprite-sheet grid. Frames are laid out
left-to-right, top-to-bottom in a uniform grid of `Columns` by `Rows` cells.
Values below 1 are treated as 1.

#### Rows

| | |
| --- | --- |
| Inspector label | `Rows` |
| Config field | `m_frameRows` (`int`) |
| Bus setter | `void SetFrameGrid(int columns, int rows, int frameCount)` |
| Default | `1` |
| Range | Inspector `Min` 1; clamped to at least 1 (`GetFrameRows`) |

The number of rows in the grid. Values below 1 are treated as 1.

The grid is laid out *inside* the `[UV Min, UV Max]` region, not the whole
texture: each cell is `(uvMax - uvMin)` divided by columns and rows. This is how
a sprite sheet composes with an atlas sub-rectangle. With the default full-
texture region, the grid spans the whole texture.

#### Frame Count

| | |
| --- | --- |
| Inspector label | `Frame Count` |
| Config field | `m_frameCount` (`int`) |
| Bus setter | `void SetFrameGrid(int columns, int rows, int frameCount)` |
| Default | `1` |
| Range | Inspector `Min` 1; clamped to `[1, Columns * Rows]` (`GetFrameCount`) |

The number of frames actually played. This allows a partial last row: a sheet of
4 columns by 3 rows holds 12 cells, but if the clip is only 10 frames you set
`Frame Count` to 10 and the last two cells are not played. The value is clamped
to `[1, Columns * Rows]`, so it can never exceed the grid capacity or drop below
1.

#### Frames Per Second

| | |
| --- | --- |
| Inspector label | `Frames Per Second` |
| Config field | `m_framesPerSecond` (`float`) |
| Bus setter | `void SetPlayback(float framesPerSecond, bool loop)` |
| Default | `12.0` |
| Units | frames per second (Inspector suffix ` fps`) |
| Range | Inspector `Min` 0.0 |

The playback rate. The Inspector shows a ` fps` suffix and enforces a minimum of
0.0. The bus setter `SetPlayback` sets the rate and the loop flag together.

#### Loop

| | |
| --- | --- |
| Inspector label | `Loop` |
| Config field | `m_loop` (`bool`) |
| Bus setter | `void SetPlayback(float framesPerSecond, bool loop)` |
| Default | `true` |

When `true`, playback wraps from the last active frame back to the first and
continues. When `false`, playback advances to the last frame and holds it; at
that point the sprite raises `OnAnimationFinished` (see below).

#### Start Frame

| | |
| --- | --- |
| Inspector label | `Start Frame` |
| Config field | `m_startFrame` (`int`) |
| Bus setter | `void SetStartFrame(int frame)` |
| Default | `0` |
| Range | Inspector `Min` 0; clamped to the valid frame range |

The frame shown first, and the frame shown in the editor (and at runtime) while
not playing. It is clamped to the valid range of frames. Use it to choose the
resting pose of a static sprite, or the entry frame of a clip.

#### Convenience: PlaySpriteSheet

There is no single Inspector field for this; it is a bus-only convenience that
bundles the common "play this sheet" intent into one call:

```
void PlaySpriteSheet(int columns, int rows, int frameCount, float framesPerSecond, bool loop)
```

It sets the grid (`SetFrameGrid`), sets playback (`SetPlayback`), and enables
animation (`SetAnimationEnabled`) in a single call. The same clamping rules
apply.

## Verifying state (GetSpriteInfo)

The request bus exposes one query, `SpriteInfo GetSpriteInfo()`. It is the
verify-loop payload: it reports what the sprite is *actually* doing (texture
loaded, drawing, current frame), not merely what was requested, so a script or
agent can confirm an action took effect without taking a screenshot. It is safe
to poll.

| Field | Type | Meaning |
| --- | --- | --- |
| `m_texturePath` | `AZStd::string` | Resolved texture product path, empty if none assigned |
| `m_textureLoaded` | `bool` | `true` once the texture asset has streamed in |
| `m_visible` | `bool` | `true` when registered with the feature processor and drawable |
| `m_width` | `float` | Resolved quad width (world meters) |
| `m_height` | `float` | Resolved quad height (world meters) |
| `m_pivotX` | `float` | Resolved pivot X (default 0.5) |
| `m_pivotY` | `float` | Resolved pivot Y (default 0.5) |
| `m_sortOffset` | `float` | Resolved sort offset |
| `m_billboard` | `bool` | Resolved billboard flag |
| `m_doubleSided` | `bool` | Resolved double-sided flag |
| `m_flipHorizontal` | `bool` | Resolved horizontal flip |
| `m_flipVertical` | `bool` | Resolved vertical flip |
| `m_animEnabled` | `bool` | Resolved animation-enabled flag |
| `m_currentFrame` | `int` | The frame currently displayed |
| `m_frameCount` | `int` | The clamped active frame count |

The verify loop is: issue a setter, then read `GetSpriteInfo` and assert the
resolved field matches your intent. Because the resolved values reflect clamping,
this also tells you when an out-of-range request was corrected (for example a
`SetSize(-2, 1)` shows `m_width` of 0).

### Notifications

For event-driven callers that prefer subscribing over polling, the
`DioramaSpriteNotificationBus` (addressed by the sprite entity id) raises:

| Event | When |
| --- | --- |
| `OnTextureReady(const AZStd::string& productPath)` | The texture finished loading, or changed and reloaded |
| `OnVisibilityChanged(bool visible)` | Drawability changed: the sprite became visible or stopped being drawn |
| `OnAnimationFinished()` | A non-looping clip reached and held its last frame |

A `DioramaSpriteNotificationHandler` is reflected to the BehaviorContext so
scripts and agents can subscribe to these events (for example, wait for
`OnTextureReady` before reading pixels, or react to `OnAnimationFinished`
instead of polling `m_currentFrame`).

## See also

- [Architecture](../architecture.md) for the editor/runtime split, the shared
  config, and the batched sprite feature processor.
- [Tilemap component reference](./tilemap-component.md) for the grid-of-cells
  layer that reuses the same render path and the same sort-offset layering.
- [API reference](./api.md) for the full `DioramaSpriteRequestBus` surface.
- How-to guides: [Hello Sprite](../howto/01-hello-sprite.md),
  [Animated Sprite](../howto/02-animated-sprite.md),
  [Sprite Atlas](../howto/03-sprite-atlas.md), and
  [Parallax and Layers](../howto/05-parallax-layers.md).
