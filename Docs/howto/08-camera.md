# How-To: 2D Camera Controller (follow, deadzone, shake)

The Diorama 2D Camera Controller gives a camera the moves every 2D game needs:
smoothly follow a target, hold a deadzone so small motions do not jitter the
view, lead the target's motion (lookahead), stay inside world bounds, and shake
on impact. It writes only the camera's position, so an authored 2.5D tilt is
preserved. All the feel math is the unit-tested `Camera2D` core.

If you just want to see it, run the example, then follow the in-editor steps it
prints. It builds its own level (`DioramaCameraDemo`), independent of any other
scene:

```bash
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/camera_demo.py
```

> **On Windows:** use `bin\Windows\profile\Default\Editor.exe` and run it on one line
> (or use a backtick `` ` `` for line continuation). See
> [Running the examples](README.md#running-the-examples-windows-or-linux).

## Important: the controller runs at game time

The controller follows and shakes only **in game mode** (it ticks through the
runtime component). In the editor viewport you author it and position the camera;
the motion happens when you enter game mode. This is deliberate, so a follow
camera never fights the editor's own viewport camera.

## What you build

A row of reference posts (so panning is visible), a Target sprite, and a camera
entity carrying an Atom Camera plus the 2D Camera Controller. A small script
patrols the target and periodically kicks the camera with shake.

## 1. Add the controller to a camera

1. Create (or pick) a camera entity that has an Atom **Camera** component.
2. **Add Component -> Diorama -> 2D Camera Controller**.
3. Configure it in the inspector, or drive it from script (below). Key fields:
   - **Target**: the entity to follow.
   - **Plane**: the world plane gameplay moves in (XY screen, XZ ground).
   - **Follow Offset**: added to the target to frame the shot; its out-of-plane
     part is the camera's distance/height.
   - **Smooth Time**: follow smoothing in seconds (0 snaps).
   - **Deadzone Half**: how far the target moves before the camera pans.
   - **Lookahead**: how far the view leads the target's motion.
   - **Use Bounds** / **Bounds Min/Max**: clamp the camera to the playfield.
   - **Max Shake** / **Trauma Decay**: shake strength and how fast it settles.

## 2. Drive it from script

The controller has a typed request bus, `DioramaCamera2DRequestBus`, so a script
or agent can set it up and kick it at runtime. From the demo's `camera_target.lua`:

```lua
DioramaCamera2DRequestBus.Event.SetTarget(camera, self.entityId)
DioramaCamera2DRequestBus.Event.SetFollowOffset(camera, 0.0, 0.0, 28.0)
DioramaCamera2DRequestBus.Event.SetSmoothTime(camera, 0.25)
DioramaCamera2DRequestBus.Event.SetDeadzone(camera, 4.0, 4.0)
DioramaCamera2DRequestBus.Event.SetLookahead(camera, 3.0)
-- On a hit, kick the camera (the gameplay "juice" hook):
DioramaCamera2DRequestBus.Event.AddTrauma(camera, 0.6)
```

`AddTrauma` is the impact hook: call it on a hit and the camera kicks, with shake
= `maxShake * trauma^2`, decaying over time. `GetCameraInfo` reads the resolved
state back for verification.

## 3. Run the demo

After running `camera_demo.py`:

1. Select the **Target** entity, **Add Component -> Lua Script**, set the script
   to `diorama/examples/camera/camera_target.lua`, and set its **Camera** property
   to the **DemoCamera** entity.
2. Select **DemoCamera** and click **Be this camera** (or right-click ->
   Be this camera) so game mode renders from it.
3. **Enter game mode** (Ctrl+G). The camera pans to follow the patrolling target,
   eases with the deadzone and lookahead, and kicks with shake on a timer.

If the framing looks off, adjust DemoCamera's transform so it looks at the XY
plane; the controller preserves the camera's rotation and only drives position.

## How it works

Each tick the runtime component reads the target's transform, computes a desired
position (target + follow offset, plus lookahead), applies the deadzone, smooths
toward it frame-rate-independently, clamps to bounds, then adds the shake offset
*after* follow so shake never fights tracking. It writes only the camera entity's
translation. See [Docs/design/2d-camera.md](../design/2d-camera.md) for the full
design and [Code/Source/Clients/Camera2D.h](../../Code/Source/Clients/Camera2D.h)
for the pure math core.
