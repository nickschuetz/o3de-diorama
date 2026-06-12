"""
Shmup dogfood: a small vertical shoot-em-up built to exercise the gem end to end.

Builds the playable scene in its own level:
- a player fighter (Sprite + 2D Input Actions + 2D Bullet Emitter as the gun + 2D
  Collider + the player_ship.lua behaviour),
- a parallax starfield (dim star sprites at a back sort),
- a stand-in enemy the player's shots can hit,
- a camera looking down -Z at the XY play plane.

The player ship moves with the "move" action and fires with "fire"; its gun is the
danmaku bullet emitter reused (one bolt, aimed up), configured by the script at runtime.

This is the dogfood from the gem's own roadmap: assemble the shipped features into a
real game, find the integration bugs unit tests miss, and produce a shareable showcase.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/DioramaSandbox \
    --runpython /path/to/o3de-diorama/Docs/examples/shmup_demo.py

Two manual clicks after it builds (same as anim_input_demo, the editor cannot set a
script asset or activate a camera from script):
  1. Player -> Lua Script -> Script = diorama/examples/shmup/player_ship.lua
  2. ShmupCamera -> Be this camera, then Ctrl+G. Move with WASD/arrows, hold Space to fire.
"""
import json

import azlmbr
import azlmbr.asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

diorama = azlmbr.diorama

LEVEL_NAME = "DioramaShmup"
PLAYER_TEXTURE = "diorama/textures/shmup_player.png"
ENEMY_TEXTURE = "diorama/textures/shmup_enemy.png"
STAR_TEXTURE = "diorama/textures/shmup_star.png"
SHOT_PRODUCT = "diorama/textures/shmup_shot.png.streamingimage"
LUA_SCRIPT = "diorama/examples/shmup/player_ship.lua"

# Enum fields serialize as ints: InputMap::ActionKind Button=0, Axis1D=1, Axis2D=2;
# Axis X=0, Y=1.
INPUT_CONFIG = {
    "actions": [
        {
            "name": "move",
            "kind": 2,
            "deadZone": 0.2,
            "pressThreshold": 0.2,
            "bindings": [
                {"channel": "keyboard_key_alphanumeric_D", "scale": 1.0, "axis": 0},
                {"channel": "keyboard_key_alphanumeric_A", "scale": -1.0, "axis": 0},
                {"channel": "keyboard_key_alphanumeric_W", "scale": 1.0, "axis": 1},
                {"channel": "keyboard_key_alphanumeric_S", "scale": -1.0, "axis": 1},
                {"channel": "keyboard_key_navigation_arrow_right", "scale": 1.0, "axis": 0},
                {"channel": "keyboard_key_navigation_arrow_left", "scale": -1.0, "axis": 0},
                {"channel": "keyboard_key_navigation_arrow_up", "scale": 1.0, "axis": 1},
                {"channel": "keyboard_key_navigation_arrow_down", "scale": -1.0, "axis": 1},
                {"channel": "gamepad_thumbstick_l_x", "scale": 1.0, "axis": 0},
                {"channel": "gamepad_thumbstick_l_y", "scale": 1.0, "axis": 1},
            ],
        },
        {
            "name": "fire",
            "kind": 0,
            "bindings": [
                {"channel": "keyboard_key_edit_space", "scale": 1.0, "axis": 0},
                {"channel": "gamepad_button_a", "scale": 1.0, "axis": 0},
            ],
        },
    ]
}


def log(msg):
    print("DIORAMA_SHMUP: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


def open_or_create_level(level_name):
    general.idle_enable(True)
    booted = 0
    while general.get_current_level_name() in ("", "Untitled") and booted < 600:
        general.idle_wait_frames(1)
        booted += 1
    general.idle_wait_frames(30)

    def now_in(name):
        return general.get_current_level_name() == name

    try:
        general.open_level_no_prompt(level_name)
    except Exception as e:
        log("open_level raised: {}".format(e))
    waited = 0
    while not now_in(level_name) and waited < 200:
        general.idle_wait_frames(1)
        waited += 1

    if not now_in(level_name):
        for template in ("Default_Level", "Empty", "Basic"):
            try:
                general.create_level_no_prompt(template, level_name, 128, 1, 512, False)
            except Exception as e:
                log("create_level('{}') raised: {}".format(template, e))
                continue
            waited = 0
            while not now_in(level_name) and waited < 400:
                general.idle_wait_frames(1)
                waited += 1
            if now_in(level_name):
                break

    general.idle_wait_frames(30)
    return now_in(level_name)


def make_entity(name, position, type_ids):
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", position, azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    out = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    comps = out.GetValue() if out.IsSuccess() else []
    return eid, comps


def set_prop(comp, path, value):
    editor.EditorComponentAPIBus(bus.Broadcast, "SetComponentProperty", comp, path, value)


def patch_prefab():
    """Bake the input config + camera rotation into the saved prefab, then reopen. The
    nested action list cannot be set through the component API, and a runtime transform
    set does not persist; the same approach anim_input_demo.py / quickstart_demo.py use."""
    try:
        proj = str(azlmbr.paths.projectroot)
    except Exception as e:
        log("could not resolve project root ({}); set the input config by hand".format(e))
        return
    pf = "{}/Levels/{}/{}.prefab".format(proj, LEVEL_NAME, LEVEL_NAME)
    try:
        with open(pf) as f:
            doc = json.load(f)
    except Exception as e:
        log("could not read prefab ({}); set the input config by hand".format(e))
        return

    # The gun is authored on the component (a single bolt aimed up, autofire), NOT from
    # the script: a sibling component's request bus may not be connected during the
    # script's OnActivate, so SetPattern/SetAim from there would be silently lost.
    gun = {"pattern": 1, "count": 1, "speed": 18.0, "fireRate": 8.0, "aimDegrees": 90.0,
           "spreadDegrees": 0.0, "bulletLifetime": 3.0, "bulletRadius": 0.3, "fireOnActivate": True,
           "muzzleOffset": [0.0, 0.9]}  # fire from the ship's nose, not its center
    inp, cam, emit = 0, 0, 0
    for entity in doc.get("Entities", {}).values():
        name = entity.get("Name")
        for comp in entity.get("Components", {}).values():
            ctype = comp.get("$type", "")
            if "EditorDioramaInputComponent" in ctype:
                comp["Config"] = INPUT_CONFIG
                inp += 1
            elif "DioramaBulletEmitterComponent" in ctype:
                comp.setdefault("Config", {}).update(gun)
                emit += 1
            elif name == "ShmupCamera" and "TransformComponent" in ctype:
                comp.setdefault("Transform Data", {})["Rotate"] = [-90.0, 0.0, 0.0]
                cam += 1
    try:
        with open(pf, "w") as f:
            json.dump(doc, f, indent=4)
        general.open_level_no_prompt(LEVEL_NAME)
        general.idle_wait_frames(20)
        log("Prefab patched: input config={}, gun={}, camera={}.".format(inp, emit, cam))
    except Exception as e:
        log("prefab patch failed ({}); set the input config by hand".format(e))


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    inp = find_type_id("2D Input Actions")
    emitter = find_type_id("2D Bullet Emitter")
    collider = find_type_id("2D Collider")
    lua = find_type_id("Lua Script")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    missing = [n for n, t in (("Sprite", sprite), ("2D Input Actions", inp),
                              ("2D Bullet Emitter", emitter), ("2D Collider", collider)) if t is None]
    if missing:
        log("FAIL: components not found (rebuild/enable the gem?): {}".format(", ".join(missing)))
        return

    # Parallax starfield: scatter dim stars at a back sort so the field reads as space.
    stars = [(-6, 4), (-3, 6), (1, 5), (4, 7), (6, 3), (-5, -2), (2, -4), (5, -6), (-2, 2), (0, 8)]
    for i, (sx, sy) in enumerate(stars):
        eid, _ = make_entity("Star{}".format(i), math.Vector3(float(sx), float(sy), 0.5), [sprite])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, STAR_TEXTURE)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 0.5, 0.5)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", eid, True)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", eid, 0.7, 0.8, 1.0, 0.8)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", eid, -10.0)

    # Stand-in enemy the player's shots can hit (default collision layer 0x0001).
    enemy, _ = make_entity("Enemy", math.Vector3(0.0, 4.0, 1.0), [sprite, collider])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", enemy, ENEMY_TEXTURE)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", enemy, 1.8, 1.8)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", enemy, True)

    # Player: sprite + input + the gun (bullet emitter) + collider + the behaviour script.
    ptypes = [sprite, inp, emitter, collider]
    if lua is not None:
        ptypes.append(lua)
    player, pc = make_entity("Player", math.Vector3(0.0, -3.0, 1.0), ptypes)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", player, PLAYER_TEXTURE)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", player, 1.6, 1.6)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", player, True)
    # Give the gun a bolt texture (the emitter is component index 2). Other gun params
    # (aim/count/speed/rate) the script sets at runtime over the bus.
    shot_id = azlmbr.asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", SHOT_PRODUCT, math.Uuid(), False)
    if len(pc) >= 3:
        set_prop(pc[2], "Config|Texture", shot_id)

    # Camera looking down -Z at the XY play plane (rotation baked in patch_prefab).
    cam_types = ([cam_ctrl] if cam_ctrl is not None else []) + ([atom_cam] if atom_cam is not None else [])
    make_entity("ShmupCamera", math.Vector3(0.0, 0.0, 14.0), cam_types)

    general.idle_wait_frames(40)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
            patch_prefab()
        except Exception as e:
            log("save_level raised: {}".format(e))

    log("Shmup scene built in level '{}'.".format(LEVEL_NAME))
    log("MANUAL: (1) Player -> Lua Script -> Script = {}".format(LUA_SCRIPT))
    log("MANUAL: (2) ShmupCamera -> Be this camera, then Ctrl+G.")
    log("EXPECT: WASD/arrows fly the ship; hold Space to fire bolts up; bolts hit the enemy")
    log("        (Console logs the hit). Stars sit behind as the field.")
    log("done")


main()
