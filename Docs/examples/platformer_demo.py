"""
Platformer: ground-follow body, ramp, and a one-way ledge.

Builds the scene in its own level (Diorama 25-platformer how-to: Docs/howto/25-platformer.md).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/platformer_demo.py
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
    print("DIORAMA_PLATFORMER: {}".format(msg))


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

LEVEL_NAME = "DioramaPlatformerDemo"


_next_cid = [990000000001]


def cid():
    _next_cid[0] += 1
    return _next_cid[0]


def resolve_script(product_path):
    """Resolve a compiled-script (.luac) product to a prefab asset-ref dict. A .luac
    GUID is assigned by the AssetProcessor (not path-derived), so it must be resolved
    at runtime; returns None if not processed yet."""
    aid = azlmbr.asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", product_path, math.Uuid(), False)
    s = aid.to_string() if hasattr(aid, "to_string") else ""
    guid, _sep, sub_hex = s.partition(":")
    if not guid.startswith("{") or guid == "{00000000-0000-0000-0000-000000000000}":
        return None
    return {"assetId": {"guid": guid, "subId": int(sub_hex, 16) if sub_hex else 0}, "assetHint": product_path}


def script_component(ref):
    return {"$type": "ScriptEditorComponent", "Id": cid(),
            "ScriptComponent": {"Properties": {"Properties": []}, "Script": ref},
            "ScriptAsset": ref}


def patch_prefab(level_name, scripts_by_entity, input_config=None, extra=None):
    """Bake what a build script cannot set live into the saved prefab, then reopen:
    Lua script assets per entity, the nested input action map, and any demo-specific
    tweak (the `extra(doc)` hook)."""
    try:
        proj = str(azlmbr.paths.projectroot)
    except Exception as e:
        log("could not resolve project root ({}); finish the wiring by hand".format(e))
        return
    pf = "{}/Levels/{}/{}.prefab".format(proj, level_name, level_name)
    try:
        with open(pf) as f:
            doc = json.load(f)
    except Exception as e:
        log("could not read prefab ({}); finish the wiring by hand".format(e))
        return

    refs = {}
    missing = []
    for ename, products in scripts_by_entity.items():
        refs[ename] = []
        for product in products:
            ref = resolve_script(product)
            if ref is None:
                missing.append(product)
            refs[ename].append(ref)

    patched_scripts, patched_input = 0, 0
    for entity in doc.get("Entities", {}).values():
        name = entity.get("Name") or ""
        comps = entity.get("Components", {})
        if input_config is not None:
            for comp in comps.values():
                if "EditorDioramaInputComponent" in comp.get("$type", ""):
                    comp["Config"] = input_config
                    patched_input += 1
        for i, ref in enumerate(refs.get(name, [])):
            if ref is None:
                continue
            comps["Script{}_{}".format(i, name)] = script_component(ref)
            patched_scripts += 1
    if extra is not None:
        extra(doc)

    try:
        with open(pf, "w") as f:
            json.dump(doc, f, indent=4)
        general.open_level_no_prompt(level_name)
        general.idle_wait_frames(20)
        log("Prefab patched: scripts={}, input={}.".format(patched_scripts, patched_input))
        if missing:
            log("NOTE: not yet processed (reprocess assets, re-run, or wire by hand): {}".format(", ".join(missing)))
    except Exception as e:
        log("prefab patch failed ({}); finish the wiring by hand".format(e))

INPUT_CONFIG = {
    "actions": [
        {
            "name": "move", "kind": 2, "deadZone": 0.2, "pressThreshold": 0.2,
            "bindings": [
                {"channel": "keyboard_key_alphanumeric_D", "scale": 1.0, "axis": 0},
                {"channel": "keyboard_key_alphanumeric_A", "scale": -1.0, "axis": 0},
            ],
        },
        {"name": "jump", "kind": 0, "bindings": [{"channel": "keyboard_key_edit_space", "scale": 1.0, "axis": 0}]},
    ]
}


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    inp = find_type_id("2D Input Actions")
    collider = find_type_id("2D Collider")
    if sprite is None or inp is None or collider is None:
        log("FAIL: components not found (rebuild/enable the gem?)")
        return

    # Visual ground + ramp + a floating one-way platform. The WALKABLE surfaces are
    # authored by platformer_ground.lua (AddGroundSegment) on the Ground entity, so
    # the sprites here are the picture and the script is the truth.
    ground, _ = make_entity("Ground", math.Vector3(0.0, -4.4, 1.0), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", ground, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", ground, 26.0, 1.2)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", ground, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", ground, 0.35, 0.45, 0.35, 1.0)

    ledge, lcomp = make_entity("OneWayLedge", math.Vector3(4.0, -1.6, 1.0), [sprite, collider])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", ledge, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", ledge, 4.0, 0.5)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", ledge, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", ledge, 0.6, 0.55, 0.3, 1.0)
    diorama.Diorama2DColliderRequestBus(bus.Event, "SetBox", ledge, 2.0, 0.25)
    diorama.Diorama2DColliderRequestBus(bus.Event, "SetOneWay", ledge, True)

    # The body: sprite + collider + input + the ground-follow mover.
    player, _ = make_entity("Player", math.Vector3(-6.0, -3.0, 1.0), [sprite, collider, inp])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", player, "diorama/textures/o3de_mascot.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", player, 1.6, 2.05)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", player, True)

    general.idle_wait_frames(40)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
            patch_prefab(LEVEL_NAME, {'Ground': ['diorama/examples/sidescroller/platformer_ground.luac'], 'Player': ['diorama/examples/sidescroller/platformer_body.luac']}, input_config=INPUT_CONFIG, extra=None)
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Platformer demo built in " + LEVEL_NAME)
    log("MANUAL: point a camera at the scene, enter game mode: A/D run, Space jumps;")
    log("        the ramp ground-follows and the yellow ledge is one-way (jump up")
    log("        through it, land on top).")
    log("done")


main()
