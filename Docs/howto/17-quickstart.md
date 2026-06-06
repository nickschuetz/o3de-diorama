# How-To: 2.5D Quick-Start (new game in minutes)

The smallest playable Diorama scene: a ground, a player sprite you move with
**WASD / arrows / left stick**, and a framed 2D camera. This is the "new 2.5D
game" starting point. Once it moves, layer on the per-feature how-tos
([05](05-parallax-layers.md) parallax, [07](07-lighting.md) lighting,
[08](08-camera.md) camera follow, [09](09-particles.md) particles,
[10](10-materials.md) materials) one at a time.

Build it (its own level, `DioramaQuickStart`):

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/quickstart_demo.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## What it creates

| Entity | Components | Role |
| --- | --- | --- |
| `Ground` | Sprite | A wide, soft quad on a back sort layer so movement reads against it |
| `Player` | Sprite (mascot) | The thing you move; you add the controller + Input below |
| `QuickCamera` | 2D Camera Controller + Atom Camera | A framed view of the XY plane |

The generator builds the scene and saves the level. It does not attach the Lua
movement script or the Input component, because the editor does not reliably wire
a Lua script asset from Python; you do those two clicks once, by hand.

## Finish the wiring (two components on `Player`)

1. Select **Player** -> **Add Component -> Lua Script**, and set the script to
   `diorama/examples/quickstart/player_move_2d.lua`.
2. Select **Player** -> **Add Component -> Input**, and set its **Input Event
   Bindings** asset to `diorama/input/diorama_move.inputbindings`.
3. Select **QuickCamera** -> **Be this camera** (or right-click -> Be this
   camera).

## Run it

**Enter game mode** (Ctrl+G). Move the player with **WASD**, the **arrow keys**,
or a gamepad **left stick**. Escape leaves game mode.

The generator pre-frames `QuickCamera` for you (it bakes a `-90` X rotation so the
camera looks down `-Z` at the XY plane). If you change the scene layout and the
framing is off, nudge `QuickCamera`'s transform; it only needs to sit back along +Z
and look at the XY plane where the player moves.

## How it works

Two small, reusable pieces do all the work, and neither is new engine code:

- **`diorama/input/diorama_move.inputbindings`** maps keys and the stick to two
  analog events, `move_x` and `move_y`. D / right-arrow / stick-right drive
  `move_x` to `+1`, A / left-arrow / stick-left to `-1`; W / S / up / down drive
  `move_y`. This is a standard O3DE `InputEventBindingsAsset`.
- **`player_move_2d.lua`** connects to those two events through
  `InputEventNotificationBus`, tracks the live axis values (pressed/held set them,
  released clears them), and each tick moves the entity in the XY plane via
  `TransformBus`. Its one tunable property is **Speed** (world units per second).

```lua
function PlayerMove2D:OnTick(deltaTime, scriptTime)
    if self.moveX == 0.0 and self.moveY == 0.0 then return end
    local p = TransformBus.Event.GetWorldTranslation(self.entityId)
    local step = self.speed * deltaTime
    TransformBus.Event.SetWorldTranslation(
        self.entityId, Vector3(p.x + self.moveX * step, p.y + self.moveY * step, p.z))
end
```

Because the controller and bindings are generic, you can reuse them for any
sprite: add the same two components to another entity and it moves too.

## Where to go next

- Make the camera follow the player: give `QuickCamera`'s **2D Camera Controller**
  a target and read [08-camera.md](08-camera.md).
- Add depth: drop in parallax background layers ([05](05-parallax-layers.md)).
- Add atmosphere: a 2D light ([07](07-lighting.md)) and a particle emitter
  ([09](09-particles.md)).
- The full integration of all of these in one scene is the side-scroller
  ([11-sidescroller.md](11-sidescroller.md)), and the cartoon solar-system diorama
  is the layered 2.5D showcase (see the README).
