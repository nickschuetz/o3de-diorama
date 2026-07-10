"""
Skinned-character rollback demo: a humanoid puppet whose pose rewinds exactly.

Builds a Puppet with the Diorama **Skinned Sprite (mesh deform)** component (the same
generated rig as puppet_demo.py) plus a **Simulation State** marker, a **2D Simulation
Clock** (free-running), and the puppet_rollback.lua controller. In game mode the script
self-drives a three-beat loop with no input: raise the arms and SaveToSlot, drop them
(the sim diverges), then RestoreFromSlot so the arms snap back to the saved pose. The
per-bone pose overrides ride the clock's rollback snapshot, so the restore is exact:
rollback-exact skinned character animation, on screen.

The skinned config (ske/tex refs, animation) and the Lua script asset are baked into the
saved prefab, because live string/asset-property edits are unreliable in this build (the
same reason puppet_demo.py and rewind_demo.py bake). The clock is set to run (not paused)
so OnSimTick fires and the script advances on its own.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \\
    --project-path=/path/to/YourProject \\
    --runpython /path/to/o3de-diorama/Docs/examples/puppet_rollback_demo.py
"""
import json

import azlmbr
import azlmbr.asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math
import azlmbr.paths

diorama = azlmbr.diorama

LEVEL_NAME = "DioramaPuppetRollback"
SKE_SOURCE = "@products@/diorama/examples/skinned/puppet_ske.json"
TEX_PRODUCT = "diorama/examples/skinned/puppet_tex.png.streamingimage"
SCRIPT_PRODUCT = "diorama/examples/skinned/puppet_rollback.luac"


def log(msg):
    print("DIORAMA_PUPPET_ROLLBACK: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


def now_in(name):
    return general.get_current_level_name() == name


def open_or_create_level(level_name):
    general.idle_enable(True)
    booted = 0
    while general.get_current_level_name() in ("", "Untitled") and booted < 600:
        general.idle_wait_frames(1)
        booted += 1
    general.idle_wait_frames(30)
    try:
        general.open_level_no_prompt(level_name)
    except Exception as e:
        log("open raised: {}".format(e))
    waited = 0
    while not now_in(level_name) and waited < 200:
        general.idle_wait_frames(1)
        waited += 1
    if not now_in(level_name):
        for template in ("Default_Level", "Empty", "Basic"):
            try:
                general.create_level_no_prompt(template, level_name, 128, 1, 512, False)
            except Exception as e:
                log("create('{}') raised: {}".format(template, e))
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
    eid = editor.ToolsApplicationRequestBus(bus.Broadcast, "CreateNewEntityAtPosition", position, azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    if type_ids:
        editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    return eid


def resolve_ref(product_path):
    """Resolve a product path to a prefab asset-ref dict, or None if not processed yet.
    Product GUIDs are assigned by the AssetProcessor (not path-derived), so they must be
    resolved at runtime."""
    aid = azlmbr.asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", product_path, math.Uuid(), False)
    if aid is None or not aid.is_valid():
        return None
    s = aid.to_string()
    guid, _sep, sub_hex = s.partition(":")
    if not guid.startswith("{") or guid == "{00000000-0000-0000-0000-000000000000}":
        return None
    return {"assetId": {"guid": guid, "subId": int(sub_hex, 16) if sub_hex else 0}, "assetHint": product_path}


_next_cid = [990000000001]


def cid():
    _next_cid[0] += 1
    return _next_cid[0]


def bake(doc):
    """Bake what the build script cannot set live: the puppet's skinned config, the
    clock's run state, and the rollback Lua script on the puppet."""
    tex = resolve_ref(TEX_PRODUCT)
    script = resolve_ref(SCRIPT_PRODUCT)
    for entity in doc.get("Entities", {}).values():
        name = entity.get("Name") or ""
        comps = entity.get("Components", {})
        if name == "Puppet":
            for comp in comps.values():
                if "SkinnedSprite" in comp.get("$type", ""):
                    cfg = comp.setdefault("Config", {})
                    cfg["sourcePath"] = SKE_SOURCE
                    cfg["scale"] = 0.02
                    cfg["animationName"] = "idle"
                    cfg["autoPlay"] = True
                    cfg["flipVertical"] = True
                    cfg["useSimClock"] = True
                    if tex is not None:
                        cfg["texture"] = tex
                    else:
                        log("NOTE: puppet texture not found; reprocess assets and re-run")
            if script is not None:
                comps["Script0_Puppet"] = {
                    "$type": "ScriptEditorComponent", "Id": cid(),
                    "ScriptComponent": {"Properties": {"Properties": []}, "Script": script},
                    "ScriptAsset": script,
                }
            else:
                log("NOTE: puppet_rollback.luac not processed yet; reprocess assets and re-run, or add the Lua Script component by hand")
        elif name == "SimClock":
            for comp in comps.values():
                if "SimClock" in comp.get("$type", ""):
                    comp.setdefault("Config", {})["StartPaused"] = False


def patch_prefab(level_name):
    proj = str(azlmbr.paths.projectroot)
    pf = "{}/Levels/{}/{}.prefab".format(proj, level_name, level_name)
    try:
        with open(pf) as f:
            doc = json.load(f)
    except Exception as e:
        log("could not read prefab ({}); wire by hand".format(e))
        return
    bake(doc)
    try:
        with open(pf, "w") as f:
            json.dump(doc, f, indent=4)
        general.open_level_no_prompt(level_name)
        general.idle_wait_frames(20)
        log("Prefab patched.")
    except Exception as e:
        log("prefab patch failed ({}); wire by hand".format(e))


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open/create '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    skinned = find_type_id("Skinned Sprite (mesh deform)")
    marker = find_type_id("Simulation State")
    clock = find_type_id("2D Simulation Clock")
    sprite = find_type_id("Sprite")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    if skinned is None or marker is None or clock is None:
        log("FAIL: Skinned Sprite / Simulation State / 2D Simulation Clock not found (gem built + enabled?)")
        return

    # The puppet: skinned rig + the snapshot marker that enrolls it in frame capture.
    make_entity("Puppet", math.Vector3(0.0, 0.0, 0.0), [skinned, marker])

    # The deterministic heartbeat + snapshot slots (baked to run, not paused).
    make_entity("SimClock", math.Vector3(0.0, 0.0, 0.0), [clock])

    # A soft backdrop so the figure reads against something.
    if sprite is not None:
        back = make_entity("Backdrop", math.Vector3(0.0, 0.0, -0.2), [sprite])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", back, "diorama/textures/white_sprite.png")
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", back, 12.0, 10.0)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", back, False)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", back, 0.82, 0.85, 0.90, 1.0)

    cam_types = ([cam_ctrl] if cam_ctrl else []) + ([atom_cam] if atom_cam else [])
    make_entity("DemoCamera", math.Vector3(0.0, -8.0, -1.5), cam_types)

    general.idle_wait_frames(30)
    if now_in(LEVEL_NAME):
        try:
            general.save_level()
        except Exception as e:
            log("save raised: {}".format(e))
    patch_prefab(LEVEL_NAME)

    log("Built a Puppet + SimClock + Backdrop + DemoCamera in '{}'.".format(LEVEL_NAME))
    log("MANUAL: be DemoCamera, then enter game mode (Ctrl+G). The arms cycle raise ->")
    log("        SaveToSlot -> drop -> RestoreFromSlot (snap back). The Console prints each")
    log("        beat. No input needed.")
    log("done")


main()
