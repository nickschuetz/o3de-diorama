"""
Afterimage-trail demo: fading ghost copies of a moving sprite.

Builds one sprite in its own level with an afterimage trail configured straight off
the bus (DioramaSpriteRequestBus.SetTrail + SetTrailTint): 10 ghosts captured every
0.04 s, the freshest at 0.6 alpha and each older one scaled by 0.85, tinted cool.
The trail only spreads when the sprite MOVES, so the Mover entity also gets a Lua
Script component; assign trail_mover.lua to it and enter game mode -- the mascot
slides left/right and drags a fading dash trail behind it.

Programmatic Lua-script-asset assignment crashes this engine build, so the script
is the one manual step (matching the other demos):
  1. Select the Mover entity -> its Lua Script component -> set Script to
     diorama/examples/sprite/trail_mover.lua
  2. Select DemoCamera -> Be this camera, then enter game mode (Ctrl+G).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/trails_demo.py
"""
import azlmbr
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

diorama = azlmbr.diorama

LEVEL_NAME = "DioramaTrailsDemo"
SPRITE_TEXTURE = "diorama/textures/o3de_mascot.png"


def log(msg):
    print("DIORAMA_TRAILS: {}".format(msg))


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
    if not now_in(level_name):
        try:
            general.open_level_no_prompt(level_name)
        except Exception as e:
            log("reopen raised: {}".format(e))
        waited = 0
        while not now_in(level_name) and waited < 200:
            general.idle_wait_frames(1)
            waited += 1
    return now_in(level_name)


def make_entity(name, pos, type_ids):
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", pos, azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    if type_ids:
        editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    return eid


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create the demo level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite_type = find_type_id("Sprite")
    if sprite_type is None:
        log("FAIL: Diorama Sprite component not found (is the Diorama gem enabled?)")
        return
    lua_type = find_type_id("Lua Script")
    cam_ctrl_type = find_type_id("2D Camera Controller")
    atom_camera_type = find_type_id("Camera")

    mtypes = [sprite_type] + ([lua_type] if lua_type is not None else [])
    if lua_type is None:
        log("WARN: 'Lua Script' component not found; add it to Mover by hand.")
    mover = make_entity("Mover", math.Vector3(0.0, 0.0, 1.0), mtypes)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", mover, SPRITE_TEXTURE)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", mover, 3.0, 3.85)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", mover, True)
    # 10 ghosts, captured every 0.04 s, freshest at 0.6 alpha, each older x0.85.
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTrail", mover, 10, 0.04, 0.6, 0.85)
    # Cool cyan-white tint so the ghosts read as a dash streak.
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTrailTint", mover, 0.6, 0.9, 1.0, 1.0)

    cam_types = ([cam_ctrl_type] if cam_ctrl_type is not None else []) + \
        ([atom_camera_type] if atom_camera_type is not None else [])
    make_entity("DemoCamera", math.Vector3(0.0, -16.0, 1.5), cam_types)

    general.idle_wait_frames(30)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Built the Mover sprite (trail = 10 ghosts) in level '{}'.".format(LEVEL_NAME))
    log("MANUAL: set Mover's Lua Script -> diorama/examples/sprite/trail_mover.lua,")
    log("then DemoCamera -> Be this camera -> enter game mode. The mascot dashes and trails.")
    log("done")


main()
