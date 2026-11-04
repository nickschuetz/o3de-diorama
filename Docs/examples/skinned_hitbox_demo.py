"""
Skinned-bone hitboxes demo: a frame-data hitbox riding a deforming DragonBones limb.

Builds a Puppet with the Diorama **Skinned Sprite (mesh deform)** component (the generated
humanoid rig, playing its "idle" clip through the compiled rig product) plus the
**2D Frame-Data Hitboxes** component. The punch hitbox names the rig bone `armR_lo`, so it
resolves through the skinned rig's `HasBone` / `GetBoneWorldPosition` verbs and rides the
swinging forearm as the clip deforms the mesh - no child entities, the box follows the
posed bone itself. The body keeps a plain origin-anchored hurtbox, and the training-mode
overlay is on, so both boxes are visible, color-coded, in game mode.

A TrainingDummy sprite with its own hurtbox stands at the edge of the swing: at full
extension the riding hitbox clips it, so OnHit / OnHurt fire on the pair (see
Docs/howto/21-fighting.md "Boxes that ride the skeleton" for wiring a hit response).

The boxes live on the XZ plane: a skinned sprite stands in its entity's X-Z plane
(vertical = world Z), so XZ keeps the bone's swing and height when the world position is
projected onto the box plane.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/skinned_hitbox_demo.py
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

LEVEL_NAME = "DioramaSkinnedHitbox"
SKE_SOURCE = "@products@/diorama/examples/skinned/puppet_ske.json"
RIG_PRODUCT = "diorama/examples/skinned/puppet.dskinrigc"
TEX_PRODUCT = "diorama/examples/skinned/puppet_tex.png.streamingimage"

# Box kinds: 0 hurtbox, 1 hitbox. The punch hitbox rides the rig bone "armR_lo" (the
# right forearm, whose shoulder swings +/-15 degrees in the idle clip); its offset hangs
# the box below the elbow joint, at the hand. Plane XZ: offsets/extents are [x, z].
PUPPET_BOXES = [
    {"kind": 0, "offset": [0.0, 0.0], "halfExtents": [0.85, 2.5], "startFrame": 0, "endFrame": 9999},
    {
        "kind": 1, "boneName": "armR_lo", "offset": [0.0, -0.5], "halfExtents": [0.35, 0.35],
        "startFrame": 0, "endFrame": 9999,
        "hit": {"damage": 5.0, "hitstunFrames": 12, "hitstopFrames": 4},
    },
]
DUMMY_BOXES = [
    {"kind": 0, "offset": [0.0, 0.3], "halfExtents": [0.55, 1.05], "startFrame": 0, "endFrame": 9999},
]


def log(msg):
    print("DIORAMA_SKINNED_HITBOX: {}".format(msg))


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
            waited = 0
            while not now_in(level_name) and waited < 200:
                general.idle_wait_frames(1)
                waited += 1
            if now_in(level_name):
                break
    return now_in(level_name)


def make_entity(name, position, type_ids):
    eid = editor.ToolsApplicationRequestBus(bus.Broadcast, "CreateNewEntityAtPosition", position, azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    if type_ids:
        editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    return eid


def resolve_product(product_path):
    aid = azlmbr.asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", product_path, math.Uuid(), False)
    if aid is None or not aid.is_valid():
        return None
    s = aid.to_string()
    guid, _sep, sub_hex = s.partition(":")
    return {"assetId": {"guid": guid, "subId": int(sub_hex, 16) if sub_hex else 0}, "assetHint": product_path}


def bake_config(doc):
    tex = resolve_product(TEX_PRODUCT)
    if tex is None:
        log("NOTE: puppet texture not found; reprocess assets and re-run")
    rig = resolve_product(RIG_PRODUCT)
    if rig is None:
        log("NOTE: compiled rig product not found; the puppet will use the JSON source path")
    for entity in doc.get("Entities", {}).values():
        name = entity.get("Name") or ""
        if name not in ("Puppet", "TrainingDummy"):
            continue
        for comp in entity.get("Components", {}).values():
            ctype = comp.get("$type", "")
            if name == "Puppet" and "SkinnedSprite" in ctype:
                cfg = comp.setdefault("Config", {})
                if rig is not None:
                    cfg["rigAsset"] = rig
                cfg["sourcePath"] = SKE_SOURCE
                cfg["scale"] = 0.02
                cfg["animationName"] = "idle"
                cfg["autoPlay"] = True
                cfg["flipVertical"] = True
                if tex is not None:
                    cfg["texture"] = tex
            elif "EditorDioramaHitboxComponent" in ctype:
                cfg = comp.setdefault("Config", {})
                cfg["plane"] = 1  # XZ: a standing skinned sprite is vertical in world Z
                cfg["boxes"] = PUPPET_BOXES if name == "Puppet" else DUMMY_BOXES
                cfg["showOverlay"] = True  # training-mode box display, visible in game mode


def patch_prefab(level_name, patch):
    proj = str(azlmbr.paths.projectroot)
    pf = "{}/Levels/{}/{}.prefab".format(proj, level_name, level_name)
    try:
        with open(pf) as f:
            doc = json.load(f)
    except Exception as e:
        log("could not read prefab ({}); wire by hand".format(e))
        return
    patch(doc)
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
    hitbox = find_type_id("2D Frame-Data Hitboxes")
    sprite = find_type_id("Sprite")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    if skinned is None or hitbox is None:
        log("FAIL: components not found (gem built + enabled?)")
        return

    # The puppet and its boxes (config baked below).
    make_entity("Puppet", math.Vector3(0.0, 0.0, 0.0), [skinned, hitbox])

    # A dummy at the edge of the forearm's swing, carrying only a hurtbox.
    dummy = make_entity("TrainingDummy", math.Vector3(1.85, 0.0, 0.0), [sprite, hitbox] if sprite else [hitbox])
    if sprite is not None:
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", dummy, "diorama/textures/shmup_enemy.png")
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", dummy, 1.5, 1.5)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", dummy, True)

    # A soft backdrop so the figures and the overlay read against something.
    if sprite is not None:
        back = make_entity("Backdrop", math.Vector3(0.9, 0.0, -0.2), [sprite])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", back, "diorama/textures/white_sprite.png")
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", back, 12.0, 9.0)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", back, False)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", back, 0.82, 0.85, 0.90, 1.0)

    cam_types = ([cam_ctrl] if cam_ctrl else []) + ([atom_cam] if atom_cam else [])
    make_entity("DemoCamera", math.Vector3(0.9, -8.0, 0.0), cam_types)

    general.idle_wait_frames(30)
    if now_in(LEVEL_NAME):
        try:
            general.save_level()
        except Exception as e:
            log("save raised: {}".format(e))
    patch_prefab(LEVEL_NAME, bake_config)

    log("Built Puppet (bone-riding hitbox) + TrainingDummy + DemoCamera in '{}'.".format(LEVEL_NAME))
    log("Enter game mode: the red hitbox rides the swinging forearm; at full extension")
    log("it clips the dummy's hurtbox. Overlay toggle: d_dioramaHitboxOverlay.")
    log("done")


main()
