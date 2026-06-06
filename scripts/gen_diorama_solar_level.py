#!/usr/bin/env python3
"""
Author the DioramaSolarSystem level prefab offline: the cartoon solar-system
diorama (view from a planet's surface), built to actually SHOWCASE the gem:

  - world-space sprites + 2.5D depth sort (sky < bodies < ridge < ground < props)
  - billboarding
  - 2D dynamic lighting: a directional "sun" 2D Light raking a normal-mapped,
    layered terrain so the ground reads as 3D rolling hills (not a flat disk)
  - glow/bloom: a 2D Look (Atom bloom + vignette) on the camera, plus emissive
    sun / crystals / comet / glowing flora pushed into the bloom range
  - drop shadows under the billboarded props (r_dioramaSpriteShadows)

Captured headless with capture_level_noap.sh (pass the lighting cvars via
CAP_EXTRA). Motion features (animation, parallax, camera pan) are layered on in a
follow-up pass for the video.
"""
import hashlib, json, os
from PIL import Image

PROJ = os.path.expanduser("~/PROJECTS/DioramaSandbox")
GEMTX = os.path.expanduser("~/PROJECTS/o3de-diorama/Assets/Diorama/Textures")
LEVEL = "DioramaSolarSystem"
HW = 8.2; FW = 2 * HW; FH = FW * 9 / 16    # ortho frame 16.4 x 9.225


def guid(p):
    h = hashlib.sha1(p.lower().replace("\\", "/").encode()).digest()
    b = bytearray(h[:16]); b[6] = (b[6] & 0xF) | 0x50; b[8] = (b[8] & 0x3F) | 0x80
    s = b.hex().upper()
    return "{%s-%s-%s-%s-%s}" % (s[0:8], s[8:12], s[12:16], s[16:20], s[20:32])


def tex(name):
    src = "diorama/textures/%s.png" % name
    return {"assetId": {"guid": guid(src), "subId": 1000}, "assetHint": src + ".streamingimage"}


def aspect(name):
    im = Image.open("%s/%s.png" % (GEMTX, name)); return im.width / im.height


_cid = [992000000000]
def cid():
    _cid[0] += 1; return _cid[0]


CONTAINER = "Entity_[992000000000]"
CAMERA_EID = "Entity_[770077007700]"   # the capture camera's prefab alias (referenced by parallax)
entities, order = {}, []


def sprite(label, texname, x, y, h, sort, tint=(1, 1, 1, 1), billboard=True, nmap=None, emissive=None):
    """One world-space sprite. nmap = normal-map texture name (enables shaped
    lighting); emissive = (r, g, b, intensity) to make it glow + bloom."""
    w = h * aspect(texname)
    cfg = {"Texture": tex(texname), "Size": [w, h], "Billboard": billboard,
           "Tint": list(tint), "SortOffset": float(sort)}
    if nmap:
        cfg["NormalMap"] = tex(nmap)
    if emissive:
        cfg["EmissiveColor"] = [emissive[0], emissive[1], emissive[2], 1.0]
        cfg["EmissiveIntensity"] = float(emissive[3])
    eid = "Entity_[%d]" % cid()
    entities[eid] = {
        "Id": eid, "Name": label,
        "Components": {
            "T": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent", "Id": cid(),
                  "Parent Entity": CONTAINER, "Transform Data": {"Translate": [x, y, 1.0]}},
            "S": {"$type": "Diorama::EditorSpriteComponent", "Id": cid(), "Config": cfg},
        },
    }
    order.append(eid)
    return eid


def sref(name):
    """Asset ref for a solar example script (GUID is from the source .lua path)."""
    g = guid("diorama/examples/solar/%s.lua" % name)
    return {"assetId": {"guid": g, "subId": 1}, "assetHint": "diorama/examples/solar/%s.luac" % name}


def attach_light(eid, ltype, color, intensity, radius):
    """Add a Diorama 2D light to an existing entity. ltype 0=Directional, 1=Point."""
    entities[eid]["Components"]["Light"] = {
        "$type": "Diorama::EditorDioramaLightComponent", "Id": cid(),
        "Config": {"type": ltype, "color": list(color), "intensity": float(intensity),
                   "radius": float(radius), "enabled": True},
    }


def attach_particles(eid, cfg):
    """Add a 2D particle emitter component to an existing entity."""
    entities[eid]["Components"]["Particles"] = {
        "$type": "Diorama::EditorParticleEmitterComponent", "Id": cid(), "Config": cfg,
    }


def attach_parallax(eid, factor):
    """Offset a layer relative to the camera so it separates from the fixed-in-world
    foreground during the pan. factor 0 = far (tracks camera), 1 = fixed in world.
    The camera EntityId is the prefab alias (same form as a TransformComponent
    'Parent Entity' reference)."""
    entities[eid]["Components"]["Parallax"] = {
        "$type": "Diorama::EditorDioramaParallaxComponent", "Id": cid(),
        "Config": {"camera": CAMERA_EID, "factor": float(factor), "enabled": True},
    }


def attach_anim(eid, name, props):
    """Add a Lua ScriptComponent (a motion driver) to an existing entity. props is
    a list of (name, value) number pairs pushed into the component."""
    sa = sref(name)
    plist = [{"$type": "AzFramework::ScriptPropertyNumber", "id": i + 1, "name": k, "value": float(v)}
             for i, (k, v) in enumerate(props)]
    entities[eid]["Components"]["Anim_" + name] = {
        "$type": "ScriptEditorComponent", "Id": cid(),
        "ScriptComponent": {"Properties": {"Properties": plist}, "Script": sa},
        "ScriptAsset": sa,
    }


# --- sky backdrop (farthest). Parallax keeps it nearly screen-fixed during the
# pan, so it needs only a little margin over the frame. ---
sky_eid = sprite("Sky", "cartoon_sky", 0.0, 0.6, FH + 1.5, -100)
attach_parallax(sky_eid, 0.1)   # nearly tracks the camera -> reads as the far backdrop

# --- bodies in the sky, by APPARENT size from Earth's surface ---
# The Moon is by far the closest object, so it dominates; the Sun is the only
# other large disc (~same angular size). Every planet is millions of km farther,
# so they read as small distant discs/points sorted BEHIND the Moon.
# (name, x, y, height, sort)  -- lower sort = farther back
BODIES = [
    ("sun", -5.8, 1.0, 2.9, -58),      # low on the horizon (setting); largest disc, bigger than the Moon
    ("moon", 3.6, 2.5, 1.9, -40),      # closest body, dominant; right side of frame
    ("mercury", -7.4, 3.9, 0.15, -58), ("venus", -4.0, 4.1, 0.30, -58),
    ("mars", 0.6, 4.05, 0.22, -58), ("jupiter", 2.5, 3.9, 0.55, -58),
    ("saturn", 4.9, 4.05, 0.5, -58), ("uranus", 6.3, 3.45, 0.30, -58),
    ("neptune", 7.4, 4.05, 0.27, -58), ("pluto", 8.0, 3.3, 0.12, -58),
    ("comet", 1.3, 3.0, 0.45, -52), ("asteroid", -3.1, 2.0, 0.26, -52),
]
# the Moon "emits light" -> soft white emissive glow, like the sun/comet
# Moon emissive kept low so the rabbit maria + craters stay visible (a high
# uniform emissive add flattens the surface contrast); bloom still gives a halo.
EMISSIVE_BODY = {"sun": (1.0, 0.62, 0.22, 2.0), "comet": (0.5, 0.85, 1.0, 1.8),
                 "moon": (0.8, 0.85, 1.0, 0.45)}
body_eid = {}
for name, x, y, h, sort in BODIES:
    body_eid[name] = sprite(name.capitalize(), "cartoon_" + name, x, y, h, sort, emissive=EMISSIVE_BODY.get(name))
# the comet slowly streaks left-to-right (nucleus leading, tail flowing behind) and wraps
attach_anim(body_eid["comet"], "drift", [("Vx", 0.7), ("Vy", 0.0), ("MinX", -9.5), ("MaxX", 9.5)])
# a cyan particle fountain out the comet's back (emits leftward, direction 180) so
# the tail is alive; the emitter rides the comet entity as it drifts
attach_particles(body_eid["comet"], {
    "texture": tex("cartoon_spark"), "maxParticles": 110, "rate": 55.0, "emitOnActivate": True,
    "lifetimeMin": 0.7, "lifetimeMax": 1.5, "speedMin": 1.4, "speedMax": 3.0,
    "directionDegrees": 180.0, "spreadDegrees": 30.0, "gravity": [0.0, 0.0], "drag": 0.6,
    "startSize": 0.22, "endSize": 0.0,
    "startColor": [0.7, 0.92, 1.0, 1.0], "endColor": [0.3, 0.6, 1.0, 0.0], "sortOffset": -53.0,
})
# distant bodies lag the foreground (parallax). The comet is excluded: it already
# moves via drift, which writes the transform every tick and would fight parallax.
for nm in body_eid:
    if nm == "comet":
        continue
    attach_parallax(body_eid[nm], 0.2 if nm == "moon" else 0.12)
# subtle breathing glow on the sun and moon (emissive only -> composes with parallax)
attach_anim(body_eid["sun"], "pulse_emissive",
            [("R", 1.0), ("G", 0.62), ("B", 0.22), ("Min", 1.7), ("Max", 2.3), ("Speed", 0.8), ("Phase", 0.0)])
attach_anim(body_eid["moon"], "pulse_emissive",
            [("R", 0.8), ("G", 0.85), ("B", 1.0), ("Min", 0.36), ("Max", 0.58), ("Speed", 0.6), ("Phase", 2.0)])
# a few twinkling stars: bright glints that pulse (emissive + bloom) and parallax far
GLINTS = [(-3.2, 3.7, 0.16, 0.0), (1.8, 4.2, 0.13, 1.1), (5.6, 3.8, 0.15, 2.1),
          (-6.3, 3.0, 0.12, 0.7), (0.4, 2.5, 0.12, 1.7), (7.2, 3.2, 0.13, 2.6)]
for i, (gx, gy, gs, ph) in enumerate(GLINTS):
    ge = sprite("Glint%d" % i, "cartoon_spark", gx, gy, gs, -90, emissive=(1.0, 1.0, 1.0, 1.0))
    attach_anim(ge, "pulse_emissive",
                [("R", 1.0), ("G", 1.0), ("B", 0.95), ("Min", 0.1), ("Max", 1.9), ("Speed", 2.8 + 0.4 * i), ("Phase", ph)])
    attach_parallax(ge, 0.1)

# --- layered terrain (atmospheric depth) + dynamic lighting via normal maps ---
# far hazy ridge peeking above the foreground horizon
ridge_eid = sprite("RidgeFar", "terrain_far", 0.0, 1.1, 5.0, -20, nmap="terrain_far_n")   # wide enough to cover the pan
attach_parallax(ridge_eid, 0.5)   # mid layer: lags less than the sky, more than the ground
# foreground convex ground (transparent above its horizon -> sky/ridge show through)
sprite("Ground", "terrain_fore", 0.0, -1.7, 11.0, 0, nmap="terrain_fore_n")

# --- hero foreground props: a gnarled oak, one purple crystal, one glowing
# mushroom, one left-leaning rock. y raised so each base sits ON the grass. ---
# (texname, x, y, height, emissive)
PROPS = [
    ("oak", -2.2, 0.1, 3.5, None),
    ("rock", -5.0, -1.4, 1.4, None),
    ("campfire", -3.6, -2.9, 1.4, None),                    # foreground pit; lit only by its own point light (no self-emissive box)
    ("mushroom", 4.0, -1.0, 0.85, (1.0, 0.86, 0.6, 1.6)),   # warm glowing cluster, farther up the field, smaller, right
]
prop_eid = {}
for name, x, y, h, em in PROPS:
    label = name.replace("_", " ").title().replace(" ", "")
    prop_eid[name] = sprite(label, "cartoon_" + name, x, y, h, 10, emissive=em)
# the FLAME is a wide particle fountain of warm sprites rising from the pit (the
# fastest read as embers); a flickering warm POINT light at the pit makes the
# firelight dance on the rock + ground. No self-emissive on the pit sprite, so its
# rectangular quad never blooms into a box.
attach_particles(prop_eid["campfire"], {
    "texture": tex("cartoon_spark"), "maxParticles": 380, "rate": 195.0, "emitOnActivate": True,
    "lifetimeMin": 0.4, "lifetimeMax": 1.0, "speedMin": 0.5, "speedMax": 1.7,
    "directionDegrees": 90.0, "spreadDegrees": 82.0, "gravity": [0.0, 0.8], "drag": 1.0,
    "startSize": 1.0, "endSize": 0.12,
    "startColor": [1.0, 0.82, 0.35, 1.0], "endColor": [1.0, 0.18, 0.0, 0.0], "sortOffset": 11.0,
})
attach_light(prop_eid["campfire"], 1, [1.0, 0.55, 0.2, 1.0], 3.2, 1.5)   # small firelight pool hugging the pit
attach_anim(prop_eid["campfire"], "flicker_light",
            [("Base", 3.2), ("Amp", 1.5), ("Speed", 9.0)])
# the mushroom cluster keeps its gentle warm breathing glow
attach_anim(prop_eid["mushroom"], "pulse_emissive",
            [("R", 1.0), ("G", 0.86), ("B", 0.6), ("Min", 0.9), ("Max", 1.9), ("Speed", 1.1), ("Phase", 1.6)])

# --- directional "sun" 2D Light raking in from the upper-left (where the sun is) ---
LIGHT = "Entity_[880088008800]"
entities[LIGHT] = {
    "Id": LIGHT, "Name": "SunLight",
    "Components": {
        "T": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent", "Id": cid(),
              "Parent Entity": CONTAINER, "Transform Data": {"Translate": [-7.0, 1.6, 2.0]}},
        "L": {"$type": "Diorama::EditorDioramaLightComponent", "Id": cid(),
              "Config": {"type": 0, "color": [1.0, 0.78, 0.5, 1.0], "intensity": 1.6,
                         "direction": [0.8, -0.5, -0.55], "enabled": True}},
    },
}
order.append(LIGHT)

# --- capture camera (orthographic, gently panning) + 2D Look (bloom + vignette) ---
CAM = CAMERA_EID
SCRIPT = sref("solar_camera")
entities[CAM] = {
    "Id": CAM, "Name": "CaptureCam",
    "Components": {
        "EditorCameraComponent": {"$type": "{CA11DA46-29FF-4083-B5F6-E02C3A8C3A3D} EditorCameraComponent", "Id": cid()},
        "EditorOnlyEntityComponent": {"$type": "EditorOnlyEntityComponent", "Id": cid()},
        "Look": {"$type": "Diorama::EditorDioramaLookComponent", "Id": cid(),
                 "Config": {"bloomEnabled": True, "bloomThreshold": 1.0, "bloomKnee": 0.5,
                            "bloomIntensity": 0.6, "vignetteEnabled": True, "vignetteIntensity": 0.18}},
        "TransformComponent": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent", "Id": cid(),
                               "Parent Entity": CONTAINER,
                               "Transform Data": {"Translate": [0.0, 0.0, 30.0], "Rotate": [-90.0, 0.0, 0.0]}},
        "ActiveCamScript": {"$type": "ScriptEditorComponent", "Id": cid(),
            "ScriptComponent": {"Properties": {"Properties": [
                {"$type": "AzFramework::ScriptPropertyBoolean", "id": 1, "name": "Orthographic", "value": True},
                {"$type": "AzFramework::ScriptPropertyNumber", "id": 2, "name": "OrthographicHalfWidth", "value": HW},
                {"$type": "AzFramework::ScriptPropertyNumber", "id": 3, "name": "PanRange", "value": 1.5},
                {"$type": "AzFramework::ScriptPropertyNumber", "id": 4, "name": "PanSpeed", "value": 0.22},
            ]}, "Script": SCRIPT}, "ScriptAsset": SCRIPT},
    },
}
order.append(CAM)

container = {
    "Id": CONTAINER, "Name": "Level",
    "Components": {
        "EditorEntitySortComponent": {"$type": "EditorEntitySortComponent", "Id": cid(), "Child Entity Order": order},
        "EditorPrefabComponent": {"$type": "EditorPrefabComponent", "Id": cid()},
        "TransformComponent": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent", "Id": cid(), "Parent Entity": ""},
    },
}
out_dir = "%s/Levels/%s" % (PROJ, LEVEL); os.makedirs(out_dir, exist_ok=True)
out = "%s/%s.prefab" % (out_dir, LEVEL)
json.dump({"ContainerEntity": container, "Entities": entities}, open(out, "w"), indent=4)
print("wrote %s (%d entities)" % (out, len(entities)))
