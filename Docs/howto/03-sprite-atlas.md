# How-To: Sprite Atlas

**Teaching ladder rung 3.** Draw many sprites that all share one texture atlas,
each showing a different region, and understand why this matters for
performance. Builds on Hello Sprite (rung 1) and Animated Sprite (rung 2); see
the [how-to index](README.md) for the full ladder and which guides exist yet.

## What you will build

Several Sprite entities that reference the *same* texture but each sample a
different rectangle of it through the **Atlas / UV Region** fields. A 2x2 atlas
with four colored cells becomes four separate on-screen sprites, all backed by a
single texture asset.

## Why an atlas

A texture atlas packs many images into one texture. Two benefits:

- **Fewer assets to manage.** One texture, one asset to load, instead of N.
- **Draw-call batching.** Sprites that share a texture can be drawn together.
  Diorama groups sprites by texture (and draw layer) and submits each group as a
  single draw call, so one hundred sprites that share an atlas cost roughly one
  draw instead of one hundred. This is the difference that lets a 2D scene scale
  to thousands of sprites. (See the note on rendering below.)

The trade-off is authoring: you must know each image's rectangle within the
atlas, expressed as normalized UV coordinates in the range 0 to 1.

## UV coordinates in Diorama

A region is two points:

- **UV Min** is the top-left corner of the rectangle to sample.
- **UV Max** is the bottom-right corner.

Both are normalized: `(0, 0)` is the top-left of the whole texture and `(1, 1)`
is the bottom-right. V increases downward, matching image space.

For a uniform grid of `columns` x `rows` cells, the cell at column `c`, row `r`
(both zero-based) is:

```
uvMin = ( c      / columns,  r      / rows )
uvMax = ( (c + 1) / columns, (r + 1) / rows )
```

### Worked example: the 2x2 sample atlas

The gem ships `Assets/Diorama/Textures/atlas_2x2.png`, a 2x2 grid:

```
+-----------+-----------+
|  red      |  green    |   row 0
|  (0,0)    |  (1,0)    |
+-----------+-----------+
|  blue     |  yellow   |   row 1
|  (0,1)    |  (1,1)    |
+-----------+-----------+
        columns ->
```

The four cells map to these regions:

| Cell   | Column, Row | UV Min       | UV Max       |
| ------ | ----------- | ------------ | ------------ |
| Red    | 0, 0        | `(0.0, 0.0)` | `(0.5, 0.5)` |
| Green  | 1, 0        | `(0.5, 0.0)` | `(1.0, 0.5)` |
| Blue   | 0, 1        | `(0.0, 0.5)` | `(0.5, 1.0)` |
| Yellow | 1, 1        | `(0.5, 0.5)` | `(1.0, 1.0)` |

## Steps (in the editor)

1. Open your project with the Diorama gem enabled (see the
   [README](../../README.md) for install and enable steps).
2. Create an entity, add a **Sprite** component, and assign
   `diorama/textures/atlas_2x2.png` to **Texture**.
3. Expand the **Atlas / UV Region** group and set **UV Min** and **UV Max** to
   one cell from the table above. The sprite now shows just that color.
4. Duplicate the entity three times, move each one in the world, and give each a
   different cell. You now have four distinct sprites sharing one texture.

Because all four sprites reference the same texture asset, the renderer draws
them together.

## Runnable example

`Docs/examples/sprite_atlas.py` builds this scene from script. Run it in the
editor:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/sprite_atlas.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

It places four sprites in a row, one per atlas cell, all using
`atlas_2x2.png`. The result is a red, green, blue, and yellow quad side by side.

## How rendering batches these sprites

Diorama renders sprites through an Atom scene feature processor. Each frame it
groups the visible sprites by `(texture, draw layer)` and submits one batched
draw call per group, sharing a single shader resource group. Sprites that use
the same atlas and sit on the same layer therefore collapse into one draw,
which is what makes large sprite counts affordable.

Two consequences for how you author:

- **Share textures to share draws.** Prefer one atlas over many separate
  textures when sprites appear together.
- **Layering splits batches.** Sprites with different **Layering > Sort Offset**
  values draw in separate groups so that 2.5D draw order is preserved. Use
  distinct sort offsets only when you need a specific overlap order, not by
  default, or you give up some batching.

## Next

- **Tilemap** ([rung 4](04-tilemap.md)): render a whole grid of atlas tiles from a
  single asset.
- **Parallax and Layers** ([rung 5](05-parallax-layers.md)): use the sort offset and
  depth to build 2.5D scenes.
