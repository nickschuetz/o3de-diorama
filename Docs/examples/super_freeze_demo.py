"""
Super-freeze demo: the 2D Simulation Clock's FreezeFor verb, no input needed.

Builds, in its own level, a 2D Simulation Clock plus one bullet emitter that fires a
ring of bullets and carries auto_super_freeze.lua. The emitter's "Use Simulation
Clock" is on, so its bullets advance on the sim clock; the script slides the emitter
on the RENDER tick and every couple of seconds calls FreezeFor. During the freeze the
clock stops ticking, so every bullet in flight hangs frozen in mid-air while the
emitter keeps gliding -- the cinematic super-freeze, proven without any keypress.

The Lua script is wired by patching the saved prefab (the same trick danmaku_demo.py
uses), because live script-asset assignment crashes this engine build. So after this
runs you only need to enter game mode.

Setup after running:
  1. Select DemoCamera -> Be this camera (or just use Game -> Simulate).
  2. Enter game mode (Ctrl+G). Bullets stream out, freeze for ~1 s on a cycle while
     the emitter keeps sliding, then resume.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/super_freeze_demo.py
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

LEVEL_NAME = "DioramaSuperFreeze"
EMITTER_SCRIPT = "diorama/examples/fighting/auto_super_freeze.luac"


def log(msg):
    print("DIORAMA_SUPERFREEZE: {}".format(msg))


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


_next_cid = [991000000001]


def cid():
    _next_cid[0] += 1
    return _next_cid[0]


def resolve_script(product_path):
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


def patch_prefab(level_name, scripts_by_entity):
    try:
        proj = str(azlmbr.paths.projectroot)
    except Exception as e:
        log("could not resolve project root ({}); assign the script by hand".format(e))
        return
    pf = "{}/Levels/{}/{}.prefab".format(proj, level_name, level_name)
    try:
        with open(pf) as f:
            doc = json.load(f)
    except Exception as e:
        log("could not read prefab ({}); assign the script by hand".format(e))
        return

    refs, missing = {}, []
    for ename, products in scripts_by_entity.items():
        refs[ename] = []
        for product in products:
            ref = resolve_script(product)
            if ref is None:
                missing.append(product)
            refs[ename].append(ref)

    patched = 0
    for entity in doc.get("Entities", {}).values():
        name = entity.get("Name") or ""
        comps = entity.get("Components", {})
        for i, ref in enumerate(refs.get(name, [])):
            if ref is None:
                continue
            comps["Script{}_{}".format(i, name)] = script_component(ref)
            patched += 1

    try:
        with open(pf, "w") as f:
            json.dump(doc, f, indent=4)
        general.open_level_no_prompt(level_name)
        general.idle_wait_frames(20)
        log("Prefab patched: scripts={}.".format(patched))
        if missing:
            log("NOTE: not processed yet (reprocess assets + re-run): {}".format(", ".join(missing)))
    except Exception as e:
        log("prefab patch failed ({}); assign the script by hand".format(e))


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    emitter_type = find_type_id("2D Bullet Emitter")
    clock_type = find_type_id("2D Simulation Clock")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    if sprite is None or emitter_type is None or clock_type is None:
        log("FAIL: components missing (sprite={}, emitter={}, clock={})".format(
            sprite is not None, emitter_type is not None, clock_type is not None))
        return

    # The simulation clock: one per level, the heartbeat FreezeFor suppresses.
    make_entity("SimClock", math.Vector3(0.0, 0.0, 0.0), [clock_type])

    # The emitter: a visible launcher sprite + the bullet emitter (on the sim clock,
    # so its bullets freeze) + auto_super_freeze.lua (wired below via the prefab).
    emitter, comps = make_entity("Emitter", math.Vector3(0.0, 0.0, 1.0), [sprite, emitter_type])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", emitter, "diorama/textures/shmup_enemy.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", emitter, 2.2, 2.2)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", emitter, True)
    shot = azlmbr.asset.AssetCatalogRequestBus(
        bus.Broadcast, "GetAssetIdByPath", "diorama/textures/shmup_bullet.png.streamingimage", math.Uuid(), False)
    # Set the emitter config on the SERIALIZED component, not via the request bus:
    # in edit mode the entity carries the editor component, which does not handle the
    # runtime DioramaBulletRequestBus, so bus calls here no-op and never persist. The
    # critical one is Use Simulation Clock -- without it the bullets run on the render
    # tick and FreezeFor cannot touch them.
    if len(comps) >= 2:
        emitter_comp = comps[1]
        set_prop(emitter_comp, "Config|Texture", shot)
        set_prop(emitter_comp, "Config|Pattern", 0)  # 0 = ring
        set_prop(emitter_comp, "Config|Count", 18)
        set_prop(emitter_comp, "Config|Speed", 4.0)
        set_prop(emitter_comp, "Config|Use Simulation Clock", True)

    cam_types = ([cam_ctrl] if cam_ctrl is not None else []) + ([atom_cam] if atom_cam is not None else [])
    make_entity("DemoCamera", math.Vector3(0.0, -16.0, 1.5), cam_types)

    general.idle_wait_frames(30)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
        except Exception as e:
            log("save_level raised: {}".format(e))

    # Wire the script into the saved prefab and reopen (live assignment crashes).
    patch_prefab(LEVEL_NAME, {"Emitter": [EMITTER_SCRIPT]})

    log("Built SimClock + Emitter (ring, sim-clock on) in level '{}'.".format(LEVEL_NAME))
    log("Be DemoCamera (or Game -> Simulate) and enter game mode: bullets stream,")
    log("freeze ~1 s on a cycle while the emitter keeps sliding, then resume.")
    log("done")


main()
