# How-To: World-Space HUD (health bars and icons in the world)

Diorama is **world-space only** (see [VISION.md](../../VISION.md)). It does not
provide a screen-space HUD or UI component: a HUD pinned to the screen corner, and
menus/widgets in general, are **LyShine's** job (O3DE's UI gem). Diorama owns
content that lives **in the world**.

The good news is that the in-world HUD elements a 2.5D game actually wants, a health
bar floating above a unit, an exclamation icon over an objective, a selection ring,
are just **sprites**. No new component is needed: you compose them from the Sprite
component you already know, and they get depth sorting, lighting, and camera framing
for free because they are real world objects.

## Build it

A one-command scene builder assembles this guide's demo in its own level
(a unit with a world-space health bar and marker pinned above it); finish any wiring the editor cannot script (noted in its output), then
enter game mode:

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/hud_demo.py
```

## A floating health bar

A health bar is two sprites parented above the unit:

- a **background** sprite (the empty track), and
- a **fill** sprite drawn on top, shrunk from its left edge by the health fraction.

The trick for the fill is the **pivot**: set the fill sprite's pivot to its left
edge `(0, 0.5)`, then setting its width to `maxWidth * value` shrinks it toward the
left instead of the center, exactly like a gauge.

```lua
-- On the fill sprite entity (a child placed above the unit). The parent offset and
-- the SetBillboard(true) below make the bar hover over the unit and face the camera.
function OnActivate(self)
    -- Pivot at the left edge so the bar empties from the right.
    DioramaSpriteRequestBus.Event.SetPivot(self.entityId, 0.0, 0.5)
    DioramaSpriteRequestBus.Event.SetBillboard(self.entityId, true)
    DioramaSpriteRequestBus.Event.SetTint(self.entityId, 0.2, 0.9, 0.2, 1.0) -- green
    self.maxWidth = 1.0
    self.height = 0.15
end

-- Call when health changes (value is 0..1).
function SetHealth(self, value)
    DioramaSpriteRequestBus.Event.SetSize(self.entityId, self.maxWidth * value, self.height)
    -- Optional: tint from green to red as it drops.
    DioramaSpriteRequestBus.Event.SetTint(self.entityId, 1.0 - value, value, 0.1, 1.0)
end
```

The background track is a second sprite at the same position with a fixed
`SetSize(maxWidth, height)` and a darker tint, on a slightly lower sort offset
(`SetSortOffset`) so the fill draws in front.

## Pinning it above the unit

Make the bar entities **children** of the unit and offset them up in the unit's
local space (e.g. `+Y` above the sprite). Because they are world objects:

- `SetBillboard(true)` keeps them facing the camera in a 2.5D scene,
- depth sorting layers them correctly against the world,
- they move, scale, and cull with the unit automatically.

An icon (exclamation mark, status effect) is the same idea with one sprite: parent
it above the entity, billboard it, swap the texture for the state you want.

## What this does not cover (and what to use instead)

- **Screen-anchored HUD** (a score in the top-left corner, a minimap frame, pause
  menus): that is screen-space UI. Use **LyShine**. Diorama deliberately does not
  overlap it.
- **World-space text** (floating damage numbers, nameplates): a sprite shows a
  texture, not arbitrary glyphs, and the engine font interface is screen-space, so
  in-world text needs a bitmap-font-sprite or a text-to-texture approach. That is a
  possible future Diorama feature; it is not available as a built-in today.

## Verifying

Each bar/icon is a Sprite, so the usual sprite verify loop applies: poll
`GetSpriteInfo` to confirm the texture loaded and the size took effect, no screenshot
required (see [01-hello-sprite.md](01-hello-sprite.md)).
