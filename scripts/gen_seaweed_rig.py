#!/usr/bin/env python3
"""
Generate an original, IP-clean DragonBones mesh-deform rig: a swaying seaweed frond.

The rig is a vertical strip mesh weighted to a chain of bones, plus a "sway" clip that
runs a traveling wave up the chain so the frond undulates. It is authored entirely here
(no third-party art), in the same DragonBones 5.5 "*_ske.json" + "*_tex.json" + atlas PNG
form the Diorama skinned-sprite importer reads, so it doubles as the mesh-deform example.

Outputs (under Assets/Diorama/Examples/Skinned/):
  seaweed_ske.json  - armature: bone chain + weighted strip mesh + "sway" animation
  seaweed_tex.json  - atlas: one sub-texture covering the frond
  seaweed_tex.png   - the frond texture (a green gradient with a midrib and soft edges)

Run: python3 scripts/gen_seaweed_rig.py
"""
import json
import math
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "..", "Assets", "Diorama", "Examples", "Skinned")

# Rig shape (DragonBones units ~ pixels; the component scales them down to world units).
SEGMENTS = 9  # -> SEGMENTS+1 bone joints and vertex rows
SEG_LEN = 60.0  # length of one segment (grows upward = negative y in DragonBones)
BASE_HALF_WIDTH = 34.0  # half-width of the frond at the base
TIP_TAPER = 0.35  # tip half-width as a fraction of the base

# Texture page.
TEX_W = 96
TEX_H = 512

# Animation. SAMPLES must divide CYCLE_FRAMES exactly so the keyframe durations (whole
# frames) sum to the clip length; otherwise the sway finishes early and holds, which reads
# as a hitch at the loop point.
FPS = 24
SAMPLES = 16  # keyframes per bone across the cycle (more = smoother sine)
FRAMES_PER_SAMPLE = 4
CYCLE_FRAMES = SAMPLES * FRAMES_PER_SAMPLE  # = 64 -> ~2.7s loop, seamless
MAX_AMP_DEG = 9.0  # peak per-bone rotation; accumulates up the chain
PHASE_LAG = 0.6  # radians of wave lag per segment (makes it travel up)


def half_width(row):
    """Frond half-width at a given vertex row (0 = base, SEGMENTS = tip)."""
    t = row / SEGMENTS
    return BASE_HALF_WIDTH * (1.0 - (1.0 - TIP_TAPER) * t)


def build_texture():
    """A green frond: vertical gradient (darker base -> brighter tip), a darker midrib,
    and soft (alpha-faded) side edges so it does not read as a hard rectangle."""
    xs = np.linspace(0.0, 1.0, TEX_W)  # 0..1 across width
    ys = np.linspace(0.0, 1.0, TEX_H)  # 0 = top (tip), 1 = bottom (base)

    base = np.array([34, 92, 40], dtype=np.float32)  # deep green (base)
    tip = np.array([120, 196, 86], dtype=np.float32)  # bright yellow-green (tip)
    grad = base[None, :] * ys[:, None] + tip[None, :] * (1.0 - ys[:, None])  # (H, 3)
    rgb = np.repeat(grad[:, None, :], TEX_W, axis=1)  # (H, W, 3)

    # Midrib: darken toward the center column.
    midrib = np.exp(-((xs - 0.5) ** 2) / (2 * 0.06 ** 2))  # (W,)
    rgb *= (1.0 - 0.35 * midrib)[None, :, None]

    # Soft side edges via alpha falloff near u = 0 and u = 1.
    edge = np.clip(np.minimum(xs, 1.0 - xs) / 0.14, 0.0, 1.0)  # (W,)
    edge = edge * edge * (3.0 - 2.0 * edge)  # smoothstep
    alpha = np.repeat(edge[None, :], TEX_H, axis=0) * 255.0  # (H, W)

    out = np.zeros((TEX_H, TEX_W, 4), dtype=np.uint8)
    out[..., :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    out[..., 3] = alpha.astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def build_bones():
    """A chain: root at the base, each next bone one segment up (negative y)."""
    bones = [{"name": "root"}]
    for i in range(1, SEGMENTS + 1):
        parent = "root" if i == 1 else "seg{}".format(i - 1)
        bones.append({"name": "seg{}".format(i), "parent": parent, "transform": {"y": -SEG_LEN}})
    return bones


def bone_world_ty(index):
    """Bind-pose world ty of a bone (the chain only translates, so it is -index*SEG_LEN)."""
    return -SEG_LEN * index


def build_mesh():
    """A strip of SEGMENTS quads. Row i sits at y=-i*SEG_LEN, weighted fully to bone i."""
    vertices = []
    uvs = []
    weights = []
    for i in range(SEGMENTS + 1):
        hw = half_width(i)
        y = -SEG_LEN * i
        v = 1.0 - i / SEGMENTS  # base -> v=1 (texture bottom), tip -> v=0 (texture top)
        # left then right vertex
        vertices += [-hw, y, hw, y]
        uvs += [0.0, v, 1.0, v]
        # each vertex: 1 influence, bone i (global index i), weight 1
        weights += [1, i, 1.0, 1, i, 1.0]

    triangles = []
    for s in range(SEGMENTS):
        bl = 2 * s
        br = 2 * s + 1
        tl = 2 * s + 2
        tr = 2 * s + 3
        triangles += [bl, br, tr, bl, tr, tl]

    # bonePose: every bone is used; its bind world is identity rotation + (0, -i*SEG_LEN).
    bone_pose = []
    for i in range(SEGMENTS + 1):
        bone_pose += [i, 1.0, 0.0, 0.0, 1.0, 0.0, bone_world_ty(i)]

    return {
        "type": "mesh",
        "name": "seaweed/frond",
        "width": TEX_W,
        "height": TEX_H,
        "vertices": vertices,
        "uvs": uvs,
        "triangles": triangles,
        "slotPose": [1, 0, 0, 1, 0, 0],
        "bonePose": bone_pose,
        "weights": weights,
    }


def build_animation():
    """A traveling-wave sway: each bone rotates sinusoidally, amplitude growing up the
    chain and phase lagging per segment, sampled into linear keyframes that loop."""
    bones = []
    for i in range(1, SEGMENTS + 1):
        amp = MAX_AMP_DEG * (i / SEGMENTS)
        frames = []
        for k in range(SAMPLES):
            rotate = amp * math.sin(2.0 * math.pi * k / SAMPLES - i * PHASE_LAG)
            frames.append({"duration": FRAMES_PER_SAMPLE, "tweenEasing": 0, "rotate": round(rotate, 3)})
        # closing keyframe (duration 0) equals the first sample so the loop is seamless
        first = MAX_AMP_DEG * (i / SEGMENTS) * math.sin(-i * PHASE_LAG)
        frames.append({"duration": 0, "rotate": round(first, 3)})
        # The keyframe durations (whole frames) must sum to the clip length so playback loops
        # seamlessly (a shortfall would hold the last pose, then snap back).
        assert sum(f["duration"] for f in frames) == CYCLE_FRAMES, "sway keyframes do not fill the clip"
        bones.append({"name": "seg{}".format(i), "rotateFrame": frames})
    return {"name": "sway", "duration": CYCLE_FRAMES, "playTimes": 0, "bone": bones}


def build_slots():
    return [{"name": "frond", "parent": "root"}]


def build_ske():
    return {
        "frameRate": FPS,
        "name": "seaweed",
        "version": "5.5",
        "armature": [
            {
                "type": "Armature",
                "frameRate": FPS,
                "name": "seaweed",
                "bone": build_bones(),
                "slot": build_slots(),
                "skin": [{"name": "", "slot": [{"name": "frond", "display": [build_mesh()]}]}],
                "animation": [build_animation()],
            }
        ],
    }


def build_atlas():
    return {
        "width": TEX_W,
        "height": TEX_H,
        "name": "seaweed",
        "imagePath": "seaweed_tex.png",
        "SubTexture": [{"name": "seaweed/frond", "x": 0, "y": 0, "width": TEX_W, "height": TEX_H}],
    }


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    build_texture().save(os.path.join(OUT_DIR, "seaweed_tex.png"))
    with open(os.path.join(OUT_DIR, "seaweed_tex.json"), "w") as f:
        json.dump(build_atlas(), f, indent=2)
    with open(os.path.join(OUT_DIR, "seaweed_ske.json"), "w") as f:
        json.dump(build_ske(), f, indent=2)
    print("wrote seaweed_ske.json, seaweed_tex.json, seaweed_tex.png to {}".format(os.path.normpath(OUT_DIR)))


if __name__ == "__main__":
    main()
