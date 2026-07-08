#!/usr/bin/env python3
"""
Generate an original, IP-free DragonBones surface-deform rig: a rippling water panel.

A single "surface" bone carries an (segmentX+1)x(segmentY+1) control-point grid; a flat
water quad (a matching tessellated grid, textured with a blue gradient) is bound to it.
The "ripple" animation is a type-50 surface-deform channel that offsets the control-point
Y in a horizontal traveling wave, looping seamlessly. No bones-skinning, no nesting - the
clean single-surface case, which is exactly what the Diorama surface-deform path renders.

Outputs <out>/water_ske.json, water_tex.json, water_tex.png. Pass an output dir as argv[1]
(default: the gem's Examples/Skinned assets).
"""
import json
import math
import os
import struct
import sys
import zlib

SEG_X = 12
SEG_Y = 4
HALF = 200.0            # DragonBones surface canonical half-size (fixed)
DX = 2.0 * HALF / SEG_X
DY = 2.0 * HALF / SEG_Y
NX = SEG_X + 1
NY = SEG_Y + 1

FRAME_RATE = 30
SAMPLES = 16           # wave phase samples (must divide CYCLE_FRAMES for a seamless loop)
FRAMES_PER_SAMPLE = 4
CYCLE_FRAMES = SAMPLES * FRAMES_PER_SAMPLE
AMPLITUDE = 34.0       # control-point Y offset (on the +/-200 grid)
WAVES = 2.0            # horizontal wavelengths across the panel

TEX_W, TEX_H = 256, 128

assert CYCLE_FRAMES % SAMPLES == 0, "SAMPLES must divide CYCLE_FRAMES for a seamless loop"


def canonical(i, j):
    return (-HALF + i * DX, -HALF + j * DY)


def cp_index(i, j):
    return i + j * NX


def build_ske():
    # Control points (bind) trace the canonical grid, so the bind pose is an identity warp.
    verts = []
    for j in range(NY):
        for i in range(NX):
            x, y = canonical(i, j)
            verts += [x, y]

    # The mesh is the same grid; UVs span 0..1; two triangles per cell.
    mesh_verts = []
    uvs = []
    for j in range(NY):
        for i in range(NX):
            x, y = canonical(i, j)
            mesh_verts += [x, y]
            uvs += [i / SEG_X, j / SEG_Y]
    tris = []
    for j in range(SEG_Y):
        for i in range(SEG_X):
            a = cp_index(i, j)
            b = cp_index(i + 1, j)
            c = cp_index(i, j + 1)
            d = cp_index(i + 1, j + 1)
            tris += [a, b, d, a, d, c]

    # Ripple: a traveling wave on the control-point Y. Frame s at phase 2*pi*s/SAMPLES; a
    # closing frame equal to frame 0 (phase 2*pi) makes the loop seamless.
    frames = []
    for s in range(SAMPLES + 1):
        phase = 2.0 * math.pi * s / SAMPLES
        value = []
        for j in range(NY):
            for i in range(NX):
                dy = AMPLITUDE * math.sin(2.0 * math.pi * WAVES * i / SEG_X + phase)
                value += [0.0, dy]
        duration = FRAMES_PER_SAMPLE if s < SAMPLES else 0
        frame = {"duration": duration, "tweenEasing": 0, "value": [round(v, 3) for v in value]}
        frames.append(frame)

    armature = {
        "name": "water",
        "frameRate": FRAME_RATE,
        "bone": [
            {"name": "root"},
            {"name": "water", "type": "surface", "parent": "root",
             "segmentX": SEG_X, "segmentY": SEG_Y, "vertices": [round(v, 3) for v in verts]},
        ],
        "slot": [{"name": "water", "parent": "water", "displayIndex": 0}],
        "skin": [{
            "slot": [{
                "name": "water",
                "display": [{
                    "type": "mesh",
                    "name": "water/surf",
                    "vertices": [round(v, 3) for v in mesh_verts],
                    "uvs": [round(v, 5) for v in uvs],
                    "triangles": tris,
                }],
            }],
        }],
        "animation": [
            # Direct: the surface-deform channel plays the traveling wave itself.
            {
                "name": "ripple",
                "duration": CYCLE_FRAMES,
                "playTimes": 0,
                "timeline": [{"name": "water", "type": 50, "frame": frames}],
            },
            # Parameter-driven: PARAM_WAVE holds the same wave, and the "flow" clip scrubs its
            # progress 0..1 through a type-40 AnimationProgress channel. Playing "flow" runs the
            # ripple entirely through the DragonBones animation-parameter layer.
            {
                "name": "PARAM_WAVE",
                "duration": CYCLE_FRAMES,
                "playTimes": 0,
                "timeline": [{"name": "water", "type": 50, "frame": frames}],
            },
            {
                "name": "flow",
                "duration": CYCLE_FRAMES,
                "playTimes": 0,
                "timeline": [{"name": "PARAM_WAVE", "type": 40, "frame": [
                    {"duration": CYCLE_FRAMES, "tweenEasing": 0, "value": 0.0},
                    {"duration": 0, "value": 1.0},
                ]}],
            },
        ],
    }
    return {"frameRate": FRAME_RATE, "name": "water", "armature": [armature]}


def build_tex():
    return {
        "width": TEX_W,
        "height": TEX_H,
        "name": "water",
        "imagePath": "water_tex.png",
        "SubTexture": [{"name": "water/surf", "x": 0, "y": 0, "width": TEX_W, "height": TEX_H}],
    }


def write_png(path, w, h):
    # A simple blue water gradient with faint horizontal banding (no external deps).
    rows = []
    for y in range(h):
        t = y / (h - 1)
        band = 12 * math.sin(y * 0.4)
        r = int(30 + 30 * t)
        g = int(110 + 80 * t + band)
        b = int(170 + 70 * t + band)
        row = bytearray()
        row.append(0)  # PNG filter type 0
        for x in range(w):
            row += bytes((max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b))))
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)  # 8-bit RGB
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    default_out = os.path.join(os.path.dirname(__file__), "..", "Assets", "Diorama", "Examples", "Skinned")
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else default_out)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "water_ske.json"), "w") as f:
        json.dump(build_ske(), f)
        f.write("\n")
    with open(os.path.join(out, "water_tex.json"), "w") as f:
        json.dump(build_tex(), f)
        f.write("\n")
    write_png(os.path.join(out, "water_tex.png"), TEX_W, TEX_H)
    print("wrote water_ske.json / water_tex.json / water_tex.png to {}".format(out))


if __name__ == "__main__":
    main()
