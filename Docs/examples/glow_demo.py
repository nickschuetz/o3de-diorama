"""
Glow / bloom: a 2D Look post profile + three sprites at rising emissive intensity.

Builds the scene in its own level (Diorama 14-glow how-to: Docs/howto/14-glow.md).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/glow_demo.py
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
    print("DIORAMA_GLOW: {}".format(msg))


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

LEVEL_NAME = "DioramaGlow"

def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    look = find_type_id("2D Look")
    if sprite is None or look is None:
        log("FAIL: components not found (rebuild/enable the gem?)")
        return

    # The post profile: one 2D Look drives Atom bloom + vignette for the scene.
    look_e, _ = make_entity("Look", math.Vector3(0.0, 0.0, 0.0), [look])
    diorama.DioramaLookRequestBus(bus.Event, "SetBloomIntensity", look_e, 1.2)
    diorama.DioramaLookRequestBus(bus.Event, "SetVignetteIntensity", look_e, 0.25)

    # A dim backdrop so the glow reads, plus three mascots at rising emissive
    # intensity: 0 (lit only), 1.5 (gentle glow), 4 (hot bloom).
    back, _ = make_entity("Backdrop", math.Vector3(0.0, 0.0, 0.0), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", back, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", back, 40.0, 22.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", back, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", back, 0.05, 0.05, 0.08, 1.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", back, -10.0)

    for i, (px, glow) in enumerate(((-7.0, 0.0), (0.0, 1.5), (7.0, 4.0))):
        eid, _ = make_entity("Mascot{}".format(i + 1), math.Vector3(px, 0.0, 1.0), [sprite])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, "diorama/textures/o3de_mascot.png")
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 4.0, 5.1)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", eid, True)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetEmissive", eid, 1.0, 0.85, 0.4, glow)

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

    log("Glow scene built in " + LEVEL_NAME)
    log("MANUAL: point a camera at the mascots (Be this camera), enter game mode: the")
    log("        right mascot blooms hot, the middle glows gently, the left is lit only.")
    log("done")


main()
