"""
Pixel art A/B: the same low-res sprite with linear vs point filtering.

Builds the scene in its own level (Diorama 06-pixel-art how-to: Docs/howto/06-pixel-art.md).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/pixel_art_demo.py
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


def log(msg):
    print("DIORAMA_PIXEL: {}".format(msg))


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
    outcome = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    comps = outcome.GetValue() if outcome.IsSuccess() else []
    return eid, comps


def set_prop(comp, path, value):
    editor.EditorComponentAPIBus(bus.Broadcast, "SetComponentProperty", comp, path, value)

LEVEL_NAME = "DioramaPixelArt"

def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    if sprite is None:
        log("FAIL: Sprite component not found (rebuild/enable the gem?)")
        return

    # The same low-resolution sprite twice at the same large size: LEFT keeps the
    # default linear filtering (blurry), RIGHT uses the point filter (crisp).
    for name, px, point in (("Linear", -4.5, False), ("Point", 4.5, True)):
        eid, _ = make_entity(name, math.Vector3(px, 0.0, 1.0), [sprite])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, "diorama/textures/pixel_sprite.png")
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 7.0, 7.0)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", eid, True)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetPointFilter", eid, point)

    general.idle_wait_frames(40)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
            pass
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Pixel-art A/B built in " + LEVEL_NAME)
    log("MANUAL: point a camera at the pair: LEFT is linear-filtered (soft), RIGHT is")
    log("        point-filtered (crisp). Pair with a no-mip import preset for best results.")
    log("done")


main()
