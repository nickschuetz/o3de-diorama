"""
Fighting: two frame-data hitbox rigs + a quarter-circle special.

Builds the scene in its own level (Diorama 21-fighting how-to: Docs/howto/21-fighting.md).

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/fighting_demo.py
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
    print("DIORAMA_FIGHTING: {}".format(msg))


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

LEVEL_NAME = "DioramaFightingDemo"


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
            "name": "direction", "kind": 2, "deadZone": 0.2, "pressThreshold": 0.2,
            "bindings": [
                {"channel": "keyboard_key_alphanumeric_D", "scale": 1.0, "axis": 0},
                {"channel": "keyboard_key_alphanumeric_A", "scale": -1.0, "axis": 0},
                {"channel": "keyboard_key_alphanumeric_W", "scale": 1.0, "axis": 1},
                {"channel": "keyboard_key_alphanumeric_S", "scale": -1.0, "axis": 1},
            ],
        },
        {"name": "punch", "kind": 0, "bindings": [{"channel": "keyboard_key_alphanumeric_J", "scale": 1.0, "axis": 0}]},
    ],
    "directionAction": "direction",
    "directionDeadZone": 0.4,
    "motions": [{"name": "qcf", "sequence": "236", "windowSeconds": 0.4}],
}


def hitbox_rig(doc):
    """Bake a typed-box frame-data rig into both fighters and turn the training-mode
    overlay on so the boxes are visible in game mode. Box kinds: 0 hurtbox, 1 hitbox,
    2 pushbox, 3 throwbox, 4 throwable, 5 armor, 6 proximity. The punch hitbox carries
    an attack payload (damage, hitstun/hitstop frames, pushback, clash priority) the
    rig delivers on OnBoxEvent; box_combat.lua reads it. The body is always a hurtbox
    + a throwable + a pushbox (on its own push layer); a proximity box reaches ahead;
    a punch hitbox is live on frames 2-4 and a throwbox on frames 6-8; an armor window
    covers frames 10-14 (a hyper-armor move)."""
    boxes = [
        {"kind": 0, "offset": [0.0, 0.0], "halfExtents": [0.7, 1.2], "startFrame": 0, "endFrame": 9999},
        {"kind": 4, "offset": [0.0, 0.0], "halfExtents": [0.7, 1.2], "startFrame": 0, "endFrame": 9999},
        {"kind": 2, "offset": [0.0, 0.0], "halfExtents": [0.6, 1.1], "startFrame": 0, "endFrame": 9999},
        {"kind": 6, "offset": [1.6, 0.4], "halfExtents": [0.9, 0.6], "startFrame": 0, "endFrame": 9999},
        {
            "kind": 1, "offset": [1.1, 0.4], "halfExtents": [0.5, 0.3], "startFrame": 2, "endFrame": 4,
            "hit": {
                "damage": 9.0, "hitstunFrames": 16, "hitstopFrames": 6,
                "pushback": [2.5, 0.0], "guardHeight": 1, "priority": 1, "customId": 1001,
            },
        },
        {
            "kind": 3, "offset": [0.9, 0.0], "halfExtents": [0.5, 0.6], "startFrame": 6, "endFrame": 8,
            "hit": {"damage": 14.0, "hitstunFrames": 22, "hitstopFrames": 8, "customId": 2001},
        },
        {"kind": 5, "offset": [0.0, 0.4], "halfExtents": [0.8, 1.0], "startFrame": 10, "endFrame": 14},
    ]
    for entity in doc.get("Entities", {}).values():
        if entity.get("Name") in ("FighterOne", "FighterTwo"):
            for comp in entity.get("Components", {}).values():
                if "EditorDioramaHitboxComponent" in comp.get("$type", ""):
                    cfg = comp.setdefault("Config", {})
                    cfg["boxes"] = boxes
                    cfg["pushLayer"] = 512      # 0x0200, distinct from the hurt layer (0x0100)
                    cfg["showOverlay"] = True   # training-mode box display, visible in game mode


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    inp = find_type_id("2D Input Actions")
    hitbox = find_type_id("2D Frame-Data Hitboxes")
    if sprite is None or inp is None or hitbox is None:
        log("FAIL: components not found (rebuild/enable the gem?)")
        return

    # Two fighters with the same rig; FighterOne carries the input map (with the
    # quarter-circle motion) and the motion_special script.
    p1, _ = make_entity("FighterOne", math.Vector3(-2.5, -1.0, 1.0), [sprite, hitbox, inp])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", p1, "diorama/textures/o3de_mascot.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", p1, 2.4, 3.1)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", p1, True)

    p2, _ = make_entity("FighterTwo", math.Vector3(2.5, -1.0, 1.0), [sprite, hitbox])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", p2, "diorama/textures/shmup_enemy.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", p2, 2.2, 2.2)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", p2, True)
    diorama.DioramaHitboxRequestBus(bus.Event, "SetFacing", p2, -1)

    general.idle_wait_frames(40)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
            patch_prefab(
                LEVEL_NAME,
                {
                    'FighterOne': [
                        'diorama/examples/fighting/motion_special.luac',
                        'diorama/examples/fighting/box_combat.luac',
                    ],
                    'FighterTwo': ['diorama/examples/fighting/box_combat.luac'],
                },
                input_config=INPUT_CONFIG,
                extra=hitbox_rig)
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Fighting demo built in " + LEVEL_NAME)
    log("MANUAL: camera -> Be this camera, then enter game mode. Each fighter carries a")
    log("        typed-box rig with the overlay ON, so you see the boxes color-coded by")
    log("        kind (hurt green, hit red, push yellow, throw purple, armor blue,")
    log("        proximity gray). The punch hitbox (frames 2-4) carries a damage payload")
    log("        box_combat.lua reads on OnBoxEvent; a throwbox (6-8) and an armor window")
    log("        (10-14) demonstrate the other kinds. Tap 236 (S, S+D, D) then J for the")
    log("        qcf special. To toggle the overlay from the console: d_dioramaHitboxOverlay 1.")
    log("done")


main()
