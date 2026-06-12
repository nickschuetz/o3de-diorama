"""
World-space readout: a health bar + marker pinned above a unit (no screen-space UI).

Builds the scene in its own level (Diorama 13-world-space-hud how-to: Docs/howto/13-world-space-hud.md).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/hud_demo.py
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
    print("DIORAMA_HUD: {}".format(msg))


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

LEVEL_NAME = "DioramaWorldHud"

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

    # A world-space readout pinned to a unit: bar background + fill, floating above
    # the character. This is the world-space pattern (a label pinned to a world
    # entity); a screen-corner HUD is LyShine's job, not Diorama's.
    unit, _ = make_entity("Unit", math.Vector3(0.0, -1.0, 1.0), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", unit, "diorama/textures/o3de_mascot.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", unit, 3.2, 4.1)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", unit, True)

    bar_bg, _ = make_entity("HealthBarBack", math.Vector3(0.0, 1.6, 1.0), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", bar_bg, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", bar_bg, 3.4, 0.5)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", bar_bg, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", bar_bg, 0.12, 0.12, 0.14, 0.9)

    bar, _ = make_entity("HealthBarFill", math.Vector3(-0.45, 1.6, 1.05), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", bar, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", bar, 2.3, 0.34)  # ~70% health
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", bar, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", bar, 0.25, 0.9, 0.35, 1.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", bar, 1.0)

    pip, _ = make_entity("Marker", math.Vector3(0.0, 2.3, 1.0), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", pip, "diorama/textures/heart.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", pip, 0.7, 0.7)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", pip, True)

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

    log("World-space readout built in " + LEVEL_NAME)
    log("MANUAL: parent the bar/marker entities under Unit (drag in the Outliner) so")
    log("        they follow it, then drive the fill width from gameplay (SetSize).")
    log("done")


main()
