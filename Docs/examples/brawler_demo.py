"""
Brawler lanes: depth-lane movement + in-lane attacks vs a chasing enemy.

Builds the scene in its own level (Diorama 26-brawler how-to: Docs/howto/26-brawler.md).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/brawler_demo.py
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
    print("DIORAMA_BRAWLER: {}".format(msg))


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

LEVEL_NAME = "DioramaBrawlerDemo"


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
                {"channel": "keyboard_key_alphanumeric_W", "scale": 1.0, "axis": 1},
                {"channel": "keyboard_key_alphanumeric_S", "scale": -1.0, "axis": 1},
            ],
        },
        {"name": "attack", "kind": 0, "bindings": [{"channel": "keyboard_key_alphanumeric_J", "scale": 1.0, "axis": 0}]},
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
    depth = find_type_id("2.5D Depth Body")
    if sprite is None or inp is None or depth is None:
        log("FAIL: components not found (rebuild/enable the gem?)")
        return

    # Street floor strip for the lane illusion.
    floor, _ = make_entity("Street", math.Vector3(0.0, -3.4, 0.5), [sprite])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", floor, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", floor, 30.0, 5.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", floor, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", floor, 0.3, 0.3, 0.35, 1.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", floor, -10.0)

    # Player: lane movement (W/S changes depth) + J attacks in-lane.
    player, _ = make_entity("Player", math.Vector3(-3.0, -1.5, 1.0), [sprite, depth, inp])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", player, "diorama/textures/o3de_mascot.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", player, 2.2, 2.8)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", player, True)

    # Enemy: chases the player across lanes once its Target is set (manual step:
    # an EntityId script property cannot be baked portably into the prefab).
    enemy, _ = make_entity("Enemy", math.Vector3(4.0, -1.5, 1.0), [sprite, depth])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", enemy, "diorama/textures/shmup_enemy.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", enemy, 2.0, 2.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", enemy, True)

    general.idle_wait_frames(40)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
            patch_prefab(LEVEL_NAME, {'Player': ['diorama/examples/brawler/brawler_player.luac'], 'Enemy': ['diorama/examples/brawler/brawler_enemy.luac']}, input_config=INPUT_CONFIG, extra=None)
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Brawler demo built in " + LEVEL_NAME)
    log("MANUAL: (1) Enemy -> Lua Script -> Target = the Player entity (an EntityId")
    log("        property cannot be baked into the prefab portably). (2) Camera ->")
    log("        Be this camera, game mode: A/D walk, W/S change lanes, J attacks")
    log("        when in the same lane.")
    log("done")


main()
