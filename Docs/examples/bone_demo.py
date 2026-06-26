"""
Bone-attached hitbox demo: a typed box rides a 2D-skeletal bone.

Builds a Fighter (sprite + 2D Frame-Data Hitboxes) with the box overlay on and TWO
boxes, so the difference is unmistakable:
  * a green Hurtbox with no bone -> stays put on the rig (the v1 static behavior);
  * a red Hitbox whose Bone is "Bone" -> tracks the world position of a descendant
    entity named Bone.
The Bone is a child entity (a bright marker sprite) that trail_mover.lua slides left
and right. In game mode the green box holds while the red box follows the sliding
Bone -- that following is the feature (boxes deform with a cutout rig instead of
sitting at a static facing-mirrored offset).

The box array, bone name, overlay flag, and the mover script are baked into the saved
prefab (live component-array and script-asset edits are unreliable in this build).
After running, be DemoCamera (or Game -> Simulate) and enter game mode.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/bone_demo.py
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

LEVEL_NAME = "DioramaBoneDemo"
BONE_SCRIPT = "diorama/examples/sprite/trail_mover.luac"


def log(msg):
    print("DIORAMA_BONE: {}".format(msg))


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


def make_entity(name, position, type_ids, parent=None):
    parent_id = parent if parent is not None else azlmbr.entity.EntityId()
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", position, parent_id)
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    if type_ids:
        editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    return eid


_next_cid = [992000000001]


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


def patch_prefab(level_name, scripts_by_entity, extra=None):
    try:
        proj = str(azlmbr.paths.projectroot)
    except Exception as e:
        log("could not resolve project root ({}); wire by hand".format(e))
        return
    pf = "{}/Levels/{}/{}.prefab".format(proj, level_name, level_name)
    try:
        with open(pf) as f:
            doc = json.load(f)
    except Exception as e:
        log("could not read prefab ({}); wire by hand".format(e))
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
    if extra is not None:
        extra(doc)

    try:
        with open(pf, "w") as f:
            json.dump(doc, f, indent=4)
        general.open_level_no_prompt(level_name)
        general.idle_wait_frames(20)
        log("Prefab patched: scripts={}.".format(patched))
        if missing:
            log("NOTE: not processed yet (reprocess assets + re-run): {}".format(", ".join(missing)))
    except Exception as e:
        log("prefab patch failed ({}); wire by hand".format(e))


def set_hitbox_boxes(doc):
    """Bake the two boxes + overlay onto the Fighter's hitbox component."""
    for entity in doc.get("Entities", {}).values():
        if (entity.get("Name") or "") != "Fighter":
            continue
        for comp in entity.get("Components", {}).values():
            if "Hitbox" in comp.get("$type", ""):
                cfg = comp.setdefault("Config", {})
                cfg["showOverlay"] = True
                cfg["boxes"] = [
                    # Static reference hurtbox (green), no bone -> stays on the rig.
                    {"kind": 0, "offset": [0.0, 0.0], "halfExtents": [0.7, 1.1],
                     "startFrame": 0, "endFrame": 9999},
                    # Bone-attached hitbox (red) -> follows the descendant named "Bone".
                    {"kind": 1, "offset": [0.0, 0.0], "halfExtents": [0.5, 0.5],
                     "startFrame": 0, "endFrame": 9999, "boneName": "Bone"},
                ]
                return


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    hitbox = find_type_id("2D Frame-Data Hitboxes")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    if sprite is None or hitbox is None:
        log("FAIL: components missing (sprite={}, hitbox={})".format(sprite is not None, hitbox is not None))
        return

    # The Fighter: a sprite + the hitbox rig (boxes baked in by set_hitbox_boxes).
    fighter = make_entity("Fighter", math.Vector3(0.0, 0.0, 1.0), [sprite, hitbox])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", fighter, "diorama/textures/o3de_mascot.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", fighter, 2.4, 3.1)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", fighter, True)

    # The Bone: a CHILD marker the red box rides; trail_mover slides it (wired below).
    bone = make_entity("Bone", math.Vector3(2.0, 1.5, 1.05), [sprite], parent=fighter)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", bone, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", bone, 0.6, 0.6)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", bone, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", bone, 1.0, 0.9, 0.2, 1.0)

    cam_types = ([cam_ctrl] if cam_ctrl is not None else []) + ([atom_cam] if atom_cam is not None else [])
    make_entity("DemoCamera", math.Vector3(0.0, -14.0, 1.5), cam_types)

    general.idle_wait_frames(30)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
        except Exception as e:
            log("save_level raised: {}".format(e))

    patch_prefab(LEVEL_NAME, {"Bone": [BONE_SCRIPT]}, extra=set_hitbox_boxes)

    log("Built Fighter (green static box + red bone-attached box, overlay on) and a")
    log("sliding Bone child in level '{}'.".format(LEVEL_NAME))
    log("Be DemoCamera (or Game -> Simulate) + game mode: the red box tracks the Bone,")
    log("the green box stays put. (Overlay can also be forced with d_dioramaHitboxOverlay 1.)")
    log("done")


main()
