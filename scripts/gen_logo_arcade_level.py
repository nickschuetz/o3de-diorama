#!/usr/bin/env python3
"""
Author the DioramaLogoArcade level prefab offline: the baked arcade textures
composited as Diorama sprites, plus the gem's CRT component for real in-engine
scanlines + tint. Run gen_logo_arcade_textures.py first. Then inject a camera with
prep_demo_camera.py and capture with capture_level.sh.
"""
import hashlib, json, os
from PIL import Image

PROJ = os.path.expanduser("~/PROJECTS/DioramaSandbox")
GEMTX = os.path.expanduser("~/PROJECTS/o3de-diorama/Assets/Diorama/Textures")
LEVEL = "DioramaLogoArcade"
HW = 8.2                       # ortho half-width; frame 16.4 x 9.225
FRAME_W = 2 * HW


def o3de_guid(p):
    h = hashlib.sha1(p.lower().replace("\\", "/").encode()).digest()
    b = bytearray(h[:16]); b[6] = (b[6] & 0xF) | 0x50; b[8] = (b[8] & 0x3F) | 0x80
    s = b.hex().upper()
    return "{%s-%s-%s-%s-%s}" % (s[0:8], s[8:12], s[12:16], s[16:20], s[20:32])


def tex(name):
    src = "diorama/textures/%s.png" % name
    return {"assetId": {"guid": o3de_guid(src), "subId": 1000}, "assetHint": src + ".streamingimage"}


def aspect(name):
    im = Image.open("%s/%s.png" % (GEMTX, name)); return im.width / im.height


_cid = [990300000000]
def cid():
    _cid[0] += 1; return _cid[0]


CONTAINER = "Entity_[990300000000]"
entities = {}
order = []


def add_sprite(name, texname, x, y, w, h, tint, sort):
    eid = "Entity_[%d]" % cid()
    entities[eid] = {
        "Id": eid, "Name": name,
        "Components": {
            "T": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent", "Id": cid(),
                  "Parent Entity": CONTAINER, "Transform Data": {"Translate": [x, y, 1.0]}},
            "S": {"$type": "Diorama::EditorSpriteComponent", "Id": cid(),
                  "Config": {"Texture": tex(texname), "Size": [w, h], "Billboard": True,
                             "Tint": tint, "SortOffset": float(sort)}},
        },
    }
    order.append(eid)


# backdrop fills the frame
add_sprite("Backdrop", "arcade_backdrop", 0.0, 0.0, 17.6, 9.9, [1, 1, 1, 1], -100)

# title: ~83% of frame width, centered, lifted slightly
tw = FRAME_W * 0.84
th = tw / aspect("arcade_title")
add_sprite("Title", "arcade_title", 0.0, 0.55, tw, th, [1, 1, 1, 1], 10)

# subtitle, tucked close under the title
sw = FRAME_W * 0.60
sh = sw / aspect("arcade_subtitle")
TITLE_Y = 0.55
GAP = 0.18  # small gap between the title and the hearts / subtitle
add_sprite("Subtitle", "arcade_subtitle", 0.0, TITLE_Y - th / 2 - GAP - sh / 2, sw, sh, [1, 1, 1, 1], 8)

# three classic pixel hearts, tucked close above the title
hh = 1.05
hw_ = hh * aspect("arcade_heart")
gap = 0.55
row = hw_ * 3 + gap * 2
for i in range(3):
    cx = -row / 2 + hw_ / 2 + i * (hw_ + gap)
    add_sprite("Heart_%d" % (i + 1), "arcade_heart", cx, TITLE_Y + th / 2 + GAP + hh / 2, hw_, hh, [1, 1, 1, 1], 8)

# CRT full-screen overlay (real gem scanlines + tint), on its own entity
crt = "Entity_[990300009999]"
entities[crt] = {
    "Id": crt, "Name": "CRT",
    "Components": {
        "T": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent", "Id": cid(),
              "Parent Entity": CONTAINER, "Transform Data": {"Translate": [0.0, 0.0, 0.0]}},
        "CRT": {"$type": "Diorama::EditorDioramaCRTComponent", "Id": cid(),
                "Config": {"enabled": True, "lineSpacing": 5.0, "lineDarkness": 0.42, "tint": 0.07}},
    },
}
order.append(crt)

container = {
    "Id": CONTAINER, "Name": "Level",
    "Components": {
        "EditorEntitySortComponent": {"$type": "EditorEntitySortComponent", "Id": cid(), "Child Entity Order": order},
        "EditorPrefabComponent": {"$type": "EditorPrefabComponent", "Id": cid()},
        "TransformComponent": {"$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent",
                               "Id": cid(), "Parent Entity": ""},
    },
}

out_dir = "%s/Levels/%s" % (PROJ, LEVEL)
os.makedirs(out_dir, exist_ok=True)
out = "%s/%s.prefab" % (out_dir, LEVEL)
json.dump({"ContainerEntity": container, "Entities": entities}, open(out, "w"), indent=4)
print("wrote %s (%d entities)" % (out, len(entities)))
