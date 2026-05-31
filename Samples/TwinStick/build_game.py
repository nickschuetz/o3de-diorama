"""
Assemble the whole twin-stick shooter scene: the capstone build.

This is the flagship "an agent builds a 2.5D game" script. It creates the arena
(a Diorama tilemap floor), the player, a top-down camera, the game/HUD
controller, and the wave spawner, configuring every Diorama part through the
typed buses. The standard-O3DE asset wiring that an agent cannot set cleanly yet
(Lua script assets, the input bindings, the spawnable prefab references, and the
LyShine canvas) is logged as the short manual checklist; see
Samples/TwinStick/README.md for the full spec.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Samples/TwinStick/build_game.py
"""
import math as pymath

import azlmbr
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

diorama = azlmbr.diorama

ATLAS = "diorama/textures/atlas_2x2.png"
PLAYER_TEXTURE = "diorama/textures/sample_sprite.png"


def log(msg):
    print("DIORAMA_TWINSTICK: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


def make_entity(name, x, y, z):
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", math.Vector3(x, y, z), azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    return eid


def add_component(eid, display_name):
    type_id = find_type_id(display_name)
    if type_id is None:
        log("WARN: component type not found: {}".format(display_name))
        return False
    editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, [type_id])
    return True


def open_default_level():
    general.idle_enable(True)
    try:
        general.open_level_no_prompt("DefaultLevel")
    except Exception as e:
        log("open_level raised: {}".format(e))
    waited = 0
    while general.get_current_level_name() in ("", "Untitled") and waited < 400:
        general.idle_wait_frames(1)
        waited += 1
    general.idle_wait_frames(20)


def build_arena():
    eid = make_entity("Arena", 0.0, 0.0, 0.0)
    add_component(eid, "Tilemap")
    columns, rows = 16, 16
    diorama.DioramaTilemapRequestBus(bus.Event, "SetAtlasByPath", eid, ATLAS)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetAtlasGrid", eid, 2, 2)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetGridSize", eid, columns, rows)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetTileSize", eid, 1.0, 1.0)
    # Checkerboard floor (atlas cells 0 and 3), blue border (cell 2).
    for row in range(rows):
        for col in range(columns):
            border = row == 0 or row == rows - 1 or col == 0 or col == columns - 1
            tile = 2 if border else (0 if (row + col) % 2 == 0 else 3)
            diorama.DioramaTilemapRequestBus(bus.Event, "SetTile", eid, col, row, tile)
    # Lay the grid flat in the world XY plane (its cells span local X/Z).
    components.TransformBus(bus.Event, "SetLocalRotation", eid, math.Vector3(-pymath.pi / 2.0, 0.0, 0.0))
    info = diorama.DioramaTilemapRequestBus(bus.Event, "GetTilemapInfo", eid)
    log("arena: {}x{} tiles, filled={}".format(info.columns, info.rows, info.filledTileCount))
    return eid


def build_player():
    eid = make_entity("Player", 0.0, 0.0, 0.5)
    add_component(eid, "Sprite")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, PLAYER_TEXTURE)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 1.0, 1.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", eid, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", eid, 10.0)
    add_component(eid, "PhysX Dynamic Rigid Body")
    add_component(eid, "PhysX Primitive Collider")
    add_component(eid, "Lua Script")
    # Component display name is "Input" (StartingPointInput), not "Input to Event
    # Bindings". build_player.py wires the script/bindings/tag in full; this
    # capstone leaves the asset refs to the README checklist.
    add_component(eid, "Input")
    add_component(eid, "Tag")
    return eid


def build_camera():
    # Top-down: above the arena looking straight down (-Z).
    eid = make_entity("GameCamera", 0.0, 0.0, 22.0)
    add_component(eid, "Camera")
    components.TransformBus(bus.Event, "SetLocalRotation", eid, math.Vector3(-pymath.pi / 2.0, 0.0, 0.0))
    return eid


def build_controller():
    eid = make_entity("Game", 0.0, 0.0, 0.0)
    add_component(eid, "Lua Script")
    add_component(eid, "Tag")
    return eid


def build_spawner():
    eid = make_entity("Spawner", 0.0, 0.0, 0.0)
    add_component(eid, "Lua Script")
    return eid


def main():
    log("start: building the twin-stick scene")
    open_default_level()
    if find_type_id("Sprite") is None or find_type_id("Tilemap") is None:
        log("FAIL: Diorama components not found (is the Diorama gem enabled?)")
        return

    build_arena()
    build_player()
    build_camera()
    build_controller()
    build_spawner()

    log("Diorama visuals are configured. Finish the standard O3DE wiring (README):")
    log("  - Player: Lua = twin_stick_player.lua; Input = twin_stick.inputbindings;")
    log("    Tag = Player; rigid body gravity off; ProjectilePrefab = projectile prefab.")
    log("  - Game: Lua = twin_stick_game.lua; Tag = Game; HudCanvas = hud.uicanvas.")
    log("  - Spawner: Lua = twin_stick_spawner.lua; EnemyPrefab = enemy prefab.")
    log("  - Save Enemy/Projectile entities as spawnable prefabs (build_enemy/build_projectile).")
    log("  - Make GameCamera the active view.")
    log("done")


main()
