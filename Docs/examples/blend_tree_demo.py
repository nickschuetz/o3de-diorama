"""
1D blend tree demo: a skeletal rig blends continuously by a parameter.

Builds a Character (a body sprite + a 2D Skeletal Clip component) with one bone, "Arm"
(a child sprite). The Skeletal Clip carries a Blend tree (1D) of two single-pose clips:
the default clip puts the Arm low (anchor 0), a "raised" clip puts it high (anchor 1).
blend_sweep.lua sweeps the blend parameter 0 -> 1 -> 0, so in game mode the Arm eases
continuously between low and high - the blend, not a switch or a one-shot cross-fade.
A real game would feed a value such as walk speed to SetBlendParam instead.

The skeletal config (tracks, the "raised" clip, the blend tree) and the sweep script are
baked into the saved prefab (nested component arrays and live script-asset assignment do
not stick in edit mode). After running, be DemoCamera (or Game -> Simulate) + game mode.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/blend_tree_demo.py
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

LEVEL_NAME = "DioramaBlendTree"
SWEEP_SCRIPT = "diorama/examples/skeletal/blend_sweep.luac"

# Arm bone local poses: low at blend 0, high at blend 1 (translation only, so it reads
# clearly on a billboard sprite). X = out to the side, Z = up; Y is depth.
ARM_LOW = [1.3, 0.0, -0.7]
ARM_HIGH = [1.3, 0.0, 1.5]


def arm_key(translation):
    return {"time": 0.0, "translation": translation, "rotationDegrees": [0.0, 0.0, 0.0], "uniformScale": 1.0, "ease": 0}


# The Skeletal Clip Config baked onto Character: a default clip (Arm low), a "raised"
# clip (Arm high), and a 1D blend tree anchoring them at 0 and 1.
SKELETAL_CONFIG = {
    "duration": 1.0,
    "looping": True,
    "speed": 1.0,
    "autoPlay": True,
    "tracks": [{"boneName": "Arm", "keys": [arm_key(ARM_LOW)]}],
    "clips": [
        {"name": "raised", "duration": 1.0, "looping": True,
         "tracks": [{"boneName": "Arm", "keys": [arm_key(ARM_HIGH)]}]}
    ],
    "blendTree": [
        {"clipName": "", "anchor": 0.0},
        {"clipName": "raised", "anchor": 1.0},
    ],
}


def log(msg):
    print("DIORAMA_BLENDTREE: {}".format(msg))


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
    eid = editor.ToolsApplicationRequestBus(bus.Broadcast, "CreateNewEntityAtPosition", position, parent_id)
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    if type_ids:
        editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    return eid


_next_cid = [993000000001]


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


def set_skeletal_config(doc):
    """Bake the blend-tree Skeletal Clip Config onto Character."""
    for entity in doc.get("Entities", {}).values():
        if (entity.get("Name") or "") != "Character":
            continue
        for comp in entity.get("Components", {}).values():
            if "Skeletal" in comp.get("$type", ""):
                comp["Config"] = SKELETAL_CONFIG
                return


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite = find_type_id("Sprite")
    skeletal = find_type_id("Skeletal Clip")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    if sprite is None or skeletal is None:
        log("FAIL: components missing (sprite={}, skeletal={})".format(sprite is not None, skeletal is not None))
        return

    # Character: a body sprite + the skeletal clip player (config baked below).
    character = make_entity("Character", math.Vector3(0.0, 0.0, 1.5), [sprite, skeletal])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", character, "diorama/textures/o3de_mascot.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", character, 2.6, 3.3)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", character, True)

    # Arm: the one bone, a child sprite the blend tree positions low vs high.
    arm = make_entity("Arm", math.Vector3(1.3, 0.0, -0.7), [sprite], parent=character)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", arm, "diorama/textures/white_sprite.png")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", arm, 0.6, 1.4)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", arm, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", arm, 1.0, 0.85, 0.2, 1.0)

    cam_types = ([cam_ctrl] if cam_ctrl is not None else []) + ([atom_cam] if atom_cam is not None else [])
    make_entity("DemoCamera", math.Vector3(0.0, -14.0, 1.5), cam_types)

    general.idle_wait_frames(30)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
        except Exception as e:
            log("save_level raised: {}".format(e))

    patch_prefab(LEVEL_NAME, {"Character": [SWEEP_SCRIPT]}, extra=set_skeletal_config)

    log("Built Character + Arm bone with a 1D blend tree (low@0, high@1) in '{}'.".format(LEVEL_NAME))
    log("Be DemoCamera (or Game -> Simulate) and enter game mode: the arm sweeps up and")
    log("down, easing continuously between the two clips as blend_sweep drives SetBlendParam.")
    log("done")


main()
