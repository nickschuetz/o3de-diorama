# Vision: Diorama in an open, Fedora-native robotics simulation

Status: design / strategy note. No implementation yet. Every technical claim below
is grounded against the actual ROS 2 install and the O3DE ROS 2 Gem on this machine.

## The picture worth building toward

Imagine a roboticist on Fedora who types `dnf install` and, minutes later, has a
complete simulation stack: ROS 2, the O3DE engine, the ROS 2 Gem with PhysX and
sensor models, and Diorama. They load a warehouse, and the robot starts to explore.
As it does, the floor *comes alive*: the costmap paints itself cell by cell under
the robot in real time, the planned path draws itself ahead as a glowing ribbon, the
laser scan rings out across the ground each frame. AprilTags sit on the shelving as
real, flat, sensor-visible textures the simulated camera reads for calibration. None
of it is a debug overlay in a separate window. It is *in the world the robot
perceives*, authored the same way a game artist authors a level, and driven live
from ROS topics.

That is the contribution Diorama can make to robotics: not the simulation itself, but
the **living 2D skin of a 3D simulation**, the part of the world that is flat by
nature and most natural to express as a dynamic, scriptable grid of cells and
sprites. The simulation is 3D and belongs to PhysX and the ROS 2 Gem. Diorama makes
the inherently-2D layer of that world beautiful, dynamic, and, where it matters,
visible to the simulated sensors themselves.

> A clean way to author and live-stream the inherently-2D content of a 3D
> simulation, some of which the simulated sensors can actually see.

## Why now: the open stack is becoming real

This is not hypothetical. The pieces are converging:

- **O3DE's next release theme is simulation and robotics.** The engine is leaning
  directly into this space.
- **A fully open, Fedora-native ROS 2 is shipping as RPMs**, development-only today
  and official-grade soon:
  - **Jazzy** RPMs live in the **`hellaenergy/ros2-jazzy`** COPR: **Fedora 44+ and
    CentOS Stream 10**, **x86_64 and aarch64**, via `ros-jazzy-ros-base` /
    `ros-jazzy-ros-desktop`. Confirmed installed here under `/opt/ros/jazzy`
    (`ros-jazzy-*-1.fc44`).
  - **Lyrical** RPMs (development-only, ahead of the LTS) now live in the
    **`hellaenergy/ros2`** COPR, the staging ground for the next release.
  - Both COPRs are explicitly **development-only**; Open Robotics takes on
    **official** Fedora support starting with **Lyrical Luth (the 2026 LTS)**.
  - The Jazzy COPR already ships **`gazebo_msgs` specifically for the O3DE ROS 2
    Gem**, so O3DE integration is already inside the packaging scope.
- **The O3DE robotics stack (the ROS 2 Gem, Apache-2.0) lives in `o3de-extras`**, a
  clean, separable home next to where a Diorama bridge would naturally sit.

So an entirely `dnf`-installable robotics simulation stack (ROS 2 + O3DE) is buildable
*now* and goes official-grade with Lyrical. Diorama is the optional content layer that
makes the 2D parts of that world come alive.

## The killer fit: live 2D data, rendered in the world

Some robotics data is *intrinsically* 2D, so a 2D representation is the natural form,
not a costume. The flagship case is the occupancy grid / costmap, and it is exactly
the kind of thing nothing else in O3DE does well:

- **`nav_msgs/OccupancyGrid` to a live ground tilemap.** An occupancy grid is a 2D
  array; mobile-robot navigation happens on a plane. O3DE has no first-class way to
  render a **dynamic 2D data grid on a surface that updates per-frame from a stream**,
  with per-cell values. "Just texture a plane" gives you a *static* image; a costmap
  that changes as the robot explores needs a dynamic, per-cell, scriptable grid, which
  is precisely what the Diorama tilemap plus its `SetGridSize` / `Fill` / `SetTile`
  bus already is. This is the lead, and it is a genuine capability gap Diorama fills.
- **`nav_msgs/Path` to line-quad sprites** and **`sensor_msgs/LaserScan`
  (downsampled `PointCloud2`) to ground sprites** are the same idea for other
  intrinsically-2D live streams: a planned path or a scan ring drawn on the floor,
  updating every frame.

Verified present in `/opt/ros/jazzy/include`: `rclcpp`, `nav_msgs`, `geometry_msgs`,
`tf2_ros`, `sensor_msgs` (used via `tf2_ros` to place these in the right frame).

## What else becomes possible: flat content the sensors can see

Beyond the live-data lead, a whole class of flat content belongs *inside the
simulated world* because the robot's own sensors perceive it. An external viewer can
never put content into the world the robot sees; Diorama can:

- **Fiducials (AprilTag / ArUco) as world sprites.** Flat textures in reality; a
  simulated camera sensor reads them directly, so they become real input to a
  perception, calibration, or SLAM test, not just a human overlay.
- **Floor markings and zones** (lanes, charging pads, no-go regions) for AMR /
  warehouse sims: visible content that, paired with the planned 2D trigger zones, can
  also be functional.
- **The sensor-visible subset of `visualization_msgs/MarkerArray`.** Markers meant for
  a *human* are already served by rviz/Foxglove; markers that must appear in a
  *rendered camera feed* share the fiducial justification. This one is gated:
  `visualization_msgs` is not in the minimal COPR set today, so it needs either
  `ros-jazzy-visualization-msgs` added to the COPR (small, permissive BSD) or a
  Diorama-native marker layer first.

## The `diorama-ros2` bridge (sketch)

A **separate optional gem/module**, so Diorama core stays ROS-free and the robotics
story is opt-in:

- Rides the ROS 2 Gem's existing node rather than opening its own ROS context:
  `ROS2Interface::Get()->GetNode()` returns a `std::shared_ptr<rclcpp::Node>`
  (`o3de-extras/Gems/ROS2/Code/Include/ROS2/ROS2Bus.h:38`; the gem's own clock and
  spawner subscribe this way). The bridge creates subscriptions on that node and
  drives the Diorama buses.
- CMake follows the ROS 2 Gem's pattern: `find_package(ROS2 MODULE)` +
  `target_depends_on_ros2_packages(rclcpp nav_msgs sensor_msgs tf2_ros)` (add
  `visualization_msgs` only if/when packaged).
- License: Apache-2.0 (matches the ROS 2 Gem; Diorama core is Apache-2.0 OR MIT).

## Packaging chain: all RPM, all open

```
ros-jazzy-ros-base   (hellaenergy/ros2-jazzy COPR; official with Lyrical later)
  -> o3de            (RPM SDK, COPR)
  -> diorama         (one gem dep Atom_RPI, no bundled third-party, verified)
  -> diorama-ros2    (deps: the ROS 2 Gem + system ros-base
                      [+ visualization-msgs if used])
```

Target matrix mirrors the COPR: Fedora 44+ / CentOS Stream 10, x86_64 + aarch64.
License hygiene is Fedora-clean by construction: Diorama is `Apache-2.0 OR MIT` with
SPDX headers and original/open-licensed assets only, and links nothing it would need
to unbundle. The whole stack is installable, inspectable, and rebuildable end to end.

## Complementary to rviz2, not a substitute

`rviz2` is the standard ROS visualizer: an **external** operator/debug window onto
live topic data that works against real robots, not just sims. It is not packaged on
Fedora *yet* (upstream `ros2/rviz#1708` Ogre / CMake 4.x, `ros2/rviz#1730` Assimp /
GCC), but that gap is temporary and closeable (rviz2 can be packaged when needed, and
Lyrical will carry it). The two are complementary by design: rviz2 draws data *about*
the world in its own window; Diorama draws *inside the simulated world itself*, where
the sim camera and sensors can see it. That distinction is the whole point, and it is
why this is additive to the ecosystem rather than a duplicate of it.

## Where Diorama focuses (and what it gladly leaves to the rest of the stack)

Clarity of scope is what makes the vision credible:

- **The simulation is the ROS 2 Gem's and PhysX's.** Physics, sensor models,
  kinematics, URDF: all theirs. Diorama is the content skin, and it leads by saying so.
- **Diorama's edge is the dynamic, live-streamed, per-cell case.** A static map or a
  fixed marker can already be a textured plane or decal; Diorama earns its place on
  the *changing* content a stream produces.
- **Human-facing debug markers stay with rviz/Foxglove/web dashboards.** Diorama
  takes the sensor-visible subset, which is the part those tools cannot reach.
- **Not a 2D physics sim.** Gridworld / multi-agent RL research uses lightweight
  Python stacks; O3DE is the wrong weight class and Diorama's planned collision is
  scenario-trigger-grade, not a solver.
- **Lead with the robotics tech, let Diorama amplify it.** The engine's robotics
  story leads with ROS 2, sensors, and physics; Diorama makes the world around them
  legible and alive. Diorama itself is still pre-1.0 (beta), which is exactly why a
  clean, opt-in bridge is the right shape.

## The first demo (no monitor needed to start)

`nav2_*` is deliberately out of scope in the COPR today (deferred to Lyrical), so the
first demo is not a full Nav2 stack from `dnf`, and it does not need to be to land. A
small `rclcpp` node (buildable on `ros-base` alone) publishes a synthetic or recorded
`OccupancyGrid` + `Path`, and Diorama visualizes it inside O3DE: a costmap painting
itself onto the floor, a path drawing ahead. Everything is `dnf`-installable; only the
*data source* is a stand-in. When the real stack lands with Lyrical + nav2, swap it in
with no change to the bridge.

## Open decisions

- **Bridge home**: `diorama-ros2` in the Diorama repo (default; keeps core ROS-free)
  vs. proposing it to **o3de-extras** alongside the ROS 2 Gem (needs explicit
  go-ahead before anything is filed there).
- **`visualization_msgs`**: request it in the COPR to enable the sensor-visible
  marker subset, or ship a Diorama-native marker layer first.

## Next steps

1. A focused `diorama-ros2` bridge design doc (subscription-to-bus mapping,
   node-sharing, CMake/subpackage layout), grounded against the ROS 2 Gem source.
2. Prototype the live `OccupancyGrid` subscriber -> tilemap against the installed
   Jazzy + ROS 2 Gem; verify a published grid renders and updates as a ground tilemap.
   That single moving costmap is the whole vision in miniature, and the fastest way to
   make it real.
