"""
Skeletal / bone-animation demo: a little humanoid puppet.

Builds a Puppet with the Diorama **Skinned Sprite (mesh deform)** component pointed at the
generated puppet rig (scripts/gen_puppet_rig.py -> Assets/Diorama/Examples/Skinned/). The rig
is a readable figure - head, torso, jointed arms and legs - built as rigid quads each
weighted fully to one DragonBones bone; the "idle" clip sways the torso, swings the arms in
opposition, and bobs, so the whole character animates. This is the bone-driven counterpart to
the water demo (which shows surface deform).

The component reads the DragonBones "*_ske.json" + "*_tex.json" from the product cache via the
@products@ alias (so it works packaged), and the atlas PNG is a normal texture asset. Those
refs plus the animation name are baked into the saved prefab (live string-property edits are
unreliable in this build).

After running, be DemoCamera (or Game -> Simulate) to watch the idle loop; in edit mode the
animated geometry keeps the viewport live, so it also plays in the editor preview.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/puppet_demo.py
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

LEVEL_NAME = "DioramaPuppetDemo"
SKE_SOURCE = "@products@/diorama/examples/skinned/puppet_ske.json"
TEX_PRODUCT = "diorama/examples/skinned/puppet_tex.png.streamingimage"


def log(msg):
    print("DIORAMA_PUPPET: {}".format(msg))


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


def resolve_texture(product_path):
    aid = azlmbr.asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", product_path, math.Uuid(), False)
    if aid is None or not aid.is_valid():
        return None
    s = aid.to_string()
    guid, _sep, sub_hex = s.partition(":")
    return {"assetId": {"guid": guid, "subId": int(sub_hex, 16) if sub_hex else 0}, "assetHint": product_path}


def bake_config(doc):
    tex = resolve_texture(TEX_PRODUCT)
    for entity in doc.get("Entities", {}).values():
        if (entity.get("Name") or "") != "Puppet":
            continue
        for comp in entity.get("Components", {}).values():
            if "SkinnedSprite" in comp.get("$type", ""):
                cfg = comp.setdefault("Config", {})
                cfg["sourcePath"] = SKE_SOURCE
                cfg["scale"] = 0.02
                cfg["animationName"] = "idle"
                cfg["autoPlay"] = True
                cfg["flipVertical"] = True
                if tex is not None:
                    cfg["texture"] = tex
                else:
                    log("NOTE: puppet texture not found; reprocess assets and re-run")
                return


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
    sprite = find_type_id("Sprite")
    cam_ctrl = find_type_id("2D Camera Controller")
    atom_cam = find_type_id("Camera")
    if skinned is None:
        log("FAIL: Skinned Sprite component not found (gem built + enabled?)")
        return

    # The puppet (config baked below).
    make_entity("Puppet", math.Vector3(0.0, 0.0, 0.0), [skinned])

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
    patch_prefab(LEVEL_NAME, bake_config)

    log("Built an idling Puppet + Backdrop + DemoCamera in '{}'.".format(LEVEL_NAME))
    log("Be DemoCamera to watch the idle loop.")
    log("done")


main()
