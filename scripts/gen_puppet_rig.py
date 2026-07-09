#!/usr/bin/env python3
"""
Generate an original, IP-clean DragonBones cutout rig: a little humanoid puppet.

A readable articulated figure - head (with a face), torso, two arms with elbows, and two
legs with knees - built as rigid quads each weighted fully to one bone, plus an "idle" clip
that sways the body, swings the arms, and bobs, so it clearly reads as a character
animating. Authored entirely here (no third-party art), in the same DragonBones 5.5
"*_ske.json" + "*_tex.json" + atlas PNG form the Diorama skinned-sprite importer reads, so
it doubles as the runnable skeletal / bone-animation example (the water demo covers surface
deform; this covers bone-driven motion on a recognizable figure).

Outputs (under Assets/Diorama/Examples/Skinned/):
  puppet_ske.json  - armature: humanoid bone tree + limb quads + "idle" animation
  puppet_tex.json  - atlas: one page, four regions (face / shirt / skin / pants)
  puppet_tex.png   - the character texture

Run: python3 scripts/gen_puppet_rig.py [out_dir]
"""
import json
import math
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_OUT = os.path.join(HERE, "..", "Assets", "Diorama", "Examples", "Skinned")

# DragonBones units ~ pixels (the component scales them to world units). y is DOWN, so "up
# the body" is negative y; the component flips vertical so the puppet stands upright.

# Bones: (name, parent, local x, local y). A humanoid tree rooted at the pelvis.
BONES = [
    ("root", None, 0.0, 0.0),      # pelvis / whole-figure root (bobs)
    ("torso", "root", 0.0, 0.0),   # upper body, pivots at the pelvis (sways)
    ("head", "torso", 0.0, -130.0),
    ("armL_up", "torso", -38.0, -100.0),  # left shoulder
    ("armL_lo", "armL_up", 0.0, 52.0),    # left elbow
    ("armR_up", "torso", 38.0, -100.0),
    ("armR_lo", "armR_up", 0.0, 52.0),
    ("legL", "root", -20.0, 0.0),   # left hip
    ("legL_lo", "legL", 0.0, 66.0), # left knee
    ("legR", "root", 20.0, 0.0),
    ("legR_lo", "legR", 0.0, 66.0),
]
BONE_INDEX = {name: i for i, (name, *_1) in enumerate(BONES)}

# Texture page: four 128x128 regions in a 256x256 page.
TEX = 256
HALF = TEX // 2
FACE = (0.0, 0.0, 0.5, 0.5)    # top-left
SHIRT = (0.5, 0.0, 1.0, 0.5)   # top-right
SKIN = (0.0, 0.5, 0.5, 1.0)    # bottom-left
PANTS = (0.5, 0.5, 1.0, 1.0)   # bottom-right

# Limbs: (bone, x center, y top, y bottom, half width, uv region). Listed back-to-front so
# the mesh triangles draw in that order (legs and far limbs first, head last).
LIMBS = [
    ("legL_lo", -20.0, 66.0, 138.0, 10.0, PANTS),
    ("legL", -20.0, 0.0, 70.0, 11.0, PANTS),
    ("legR_lo", 20.0, 66.0, 138.0, 10.0, PANTS),
    ("legR", 20.0, 0.0, 70.0, 11.0, PANTS),
    ("torso", 0.0, -116.0, 6.0, 30.0, SHIRT),
    ("armL_up", -38.0, -100.0, -46.0, 9.0, SKIN),
    ("armL_lo", -38.0, -50.0, 8.0, 8.0, SKIN),
    ("armR_up", 38.0, -100.0, -46.0, 9.0, SKIN),
    ("armR_lo", 38.0, -50.0, 8.0, 8.0, SKIN),
    ("head", 0.0, -192.0, -122.0, 26.0, FACE),
]

# Animation: whole frames, SAMPLES must divide CYCLE_FRAMES so keyframe durations fill the
# clip (a shortfall holds the last pose then snaps, reading as a hitch at the loop).
FPS = 24
SAMPLES = 24
FRAMES_PER_SAMPLE = 4
CYCLE_FRAMES = SAMPLES * FRAMES_PER_SAMPLE  # 96 -> 4s loop, seamless


def bind_worlds():
    """Absolute bind-pose position of each bone (the tree only translates, so parent-sum)."""
    world = {}
    for name, parent, lx, ly in BONES:
        px, py = (0.0, 0.0) if parent is None else world[parent]
        world[name] = (px + lx, py + ly)
    return world


def build_texture():
    """The character page: a face (skin, hair, eyes, smile), a shirt, skin, and pants."""
    img = Image.new("RGBA", (TEX, TEX), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    skin = (240, 200, 165, 255)
    # Face region (top-left).
    d.rectangle([0, 0, HALF - 1, HALF - 1], fill=skin)
    d.rectangle([0, 0, HALF - 1, 34], fill=(90, 62, 44, 255))          # hair band on top
    d.ellipse([34, 50, 54, 74], fill=(40, 40, 46, 255))               # left eye
    d.ellipse([74, 50, 94, 74], fill=(40, 40, 46, 255))               # right eye
    d.arc([40, 66, 88, 104], start=15, end=165, fill=(150, 70, 60, 255), width=5)  # smile
    d.ellipse([22, 78, 38, 92], fill=(240, 160, 150, 90))             # cheeks
    d.ellipse([90, 78, 106, 92], fill=(240, 160, 150, 90))
    # Shirt region (top-right).
    d.rectangle([HALF, 0, TEX - 1, HALF - 1], fill=(52, 96, 190, 255))
    d.rectangle([HALF, 0, TEX - 1, 18], fill=(40, 74, 150, 255))       # collar
    # Skin region (bottom-left) - arms.
    d.rectangle([0, HALF, HALF - 1, TEX - 1], fill=skin)
    # Pants region (bottom-right) - legs.
    d.rectangle([HALF, HALF, TEX - 1, TEX - 1], fill=(64, 72, 92, 255))
    return img


def build_bones():
    out = []
    for name, parent, lx, ly in BONES:
        bone = {"name": name}
        if parent is not None:
            bone["parent"] = parent
            bone["transform"] = {"x": lx, "y": ly}
        out.append(bone)
    return out


def build_mesh():
    """All limb quads in one mesh, each of its four vertices weighted fully to the limb's
    bone. Vertices are in root (slot) space at their bind-pose positions; skinning then moves
    each quad rigidly with its bone."""
    vertices, uvs, weights, triangles = [], [], [], []
    for bone, xc, y_top, y_bottom, hw, (u0, v0, u1, v1) in LIMBS:
        gi = BONE_INDEX[bone]
        base = len(vertices) // 2
        # BL, BR, TL, TR
        vertices += [xc - hw, y_bottom, xc + hw, y_bottom, xc - hw, y_top, xc + hw, y_top]
        uvs += [u0, v1, u1, v1, u0, v0, u1, v0]
        weights += [1, gi, 1.0, 1, gi, 1.0, 1, gi, 1.0, 1, gi, 1.0]
        triangles += [base, base + 1, base + 3, base, base + 3, base + 2]

    # bonePose: each bone's bind world (identity rotation + its bind-world translation).
    world = bind_worlds()
    bone_pose = []
    for name, _p, _lx, _ly in BONES:
        wx, wy = world[name]
        bone_pose += [BONE_INDEX[name], 1.0, 0.0, 0.0, 1.0, wx, wy]

    return {
        "type": "mesh",
        "name": "puppet/body",
        "width": TEX,
        "height": TEX,
        "vertices": vertices,
        "uvs": uvs,
        "triangles": triangles,
        "slotPose": [1, 0, 0, 1, 0, 0],
        "bonePose": bone_pose,
        "weights": weights,
    }


def _sine_rotate(amp_deg, phase, cycles=1):
    """SAMPLES rotate keyframes tracing amp*sin, plus a closing frame equal to the first."""
    frames = []
    for k in range(SAMPLES):
        r = amp_deg * math.sin(cycles * 2.0 * math.pi * k / SAMPLES + phase)
        frames.append({"duration": FRAMES_PER_SAMPLE, "tweenEasing": 0, "rotate": round(r, 3)})
    frames.append({"duration": 0, "rotate": round(amp_deg * math.sin(phase), 3)})
    assert sum(f["duration"] for f in frames) == CYCLE_FRAMES
    return frames


def build_animation():
    """A gentle idle: torso sways, head counter-sways, arms swing in opposition, lower arms
    and legs follow slightly, and the whole figure bobs (root translate, twice per cycle)."""
    bone_tracks = [
        {"name": "torso", "rotateFrame": _sine_rotate(6.0, 0.0)},
        {"name": "head", "rotateFrame": _sine_rotate(-3.0, 0.0)},
        {"name": "armL_up", "rotateFrame": _sine_rotate(15.0, 0.0)},
        {"name": "armR_up", "rotateFrame": _sine_rotate(-15.0, 0.0)},
        {"name": "armL_lo", "rotateFrame": _sine_rotate(8.0, -0.6)},
        {"name": "armR_lo", "rotateFrame": _sine_rotate(-8.0, -0.6)},
        {"name": "legL", "rotateFrame": _sine_rotate(-4.0, 0.0)},
        {"name": "legR", "rotateFrame": _sine_rotate(4.0, 0.0)},
    ]
    # Root bob: translate up/down twice per cycle.
    bob = []
    for k in range(SAMPLES):
        ty = -4.0 * (0.5 - 0.5 * math.cos(2.0 * 2.0 * math.pi * k / SAMPLES))
        bob.append({"duration": FRAMES_PER_SAMPLE, "tweenEasing": 0, "x": 0.0, "y": round(ty, 3)})
    bob.append({"duration": 0, "x": 0.0, "y": 0.0})
    bone_tracks.append({"name": "root", "translateFrame": bob})
    return {"name": "idle", "duration": CYCLE_FRAMES, "playTimes": 0, "bone": bone_tracks}


def build_slots():
    return [{"name": "body", "parent": "root"}]


def build_ske():
    return {
        "frameRate": FPS,
        "name": "puppet",
        "version": "5.5",
        "armature": [
            {
                "type": "Armature",
                "frameRate": FPS,
                "name": "puppet",
                "bone": build_bones(),
                "slot": build_slots(),
                "skin": [{"name": "", "slot": [{"name": "body", "display": [build_mesh()]}]}],
                "animation": [build_animation()],
            }
        ],
    }


def build_atlas():
    return {
        "width": TEX,
        "height": TEX,
        "name": "puppet",
        "imagePath": "puppet_tex.png",
        "SubTexture": [{"name": "puppet/body", "x": 0, "y": 0, "width": TEX, "height": TEX}],
    }


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT
    os.makedirs(out_dir, exist_ok=True)
    build_texture().save(os.path.join(out_dir, "puppet_tex.png"))
    with open(os.path.join(out_dir, "puppet_tex.json"), "w") as f:
        json.dump(build_atlas(), f, indent=2)
        f.write("\n")
    with open(os.path.join(out_dir, "puppet_ske.json"), "w") as f:
        json.dump(build_ske(), f, indent=2)
        f.write("\n")
    print("wrote puppet_ske.json, puppet_tex.json, puppet_tex.png to {}".format(os.path.normpath(out_dir)))


if __name__ == "__main__":
    main()
