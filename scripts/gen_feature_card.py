#!/usr/bin/env python3
"""
Diorama feature-overview card, styled to match the arcade logo: black field, the
real in-engine logo as the header, neon-bordered tiles with glowing monospace
titles, and a CRT scanline overlay. Every label is real and matches the shipped
feature set (no version string). Output: Docs/images/diorama-features.png
"""
import os, textwrap
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.expanduser("~/PROJECTS/o3de-diorama")
MONO = "/usr/share/fonts/liberation-mono-fonts/LiberationMono-%s.ttf"
SANS = "/usr/share/fonts/liberation-sans-fonts/LiberationSans-%s.ttf"
def mono(sz): return ImageFont.truetype(MONO % "Bold", sz)
def sans(sz): return ImageFont.truetype(SANS % "Regular", sz)

W, H = 1600, 1320
img = Image.new("RGBA", (W, H), (6, 6, 9, 255))     # near-black arcade field
d = ImageDraw.Draw(img)

# --- header: drop the actual arcade logo in (exact style match) ---
logo = Image.open(f"{ROOT}/Docs/images/diorama-logo.png").convert("RGBA")
lw = 880; lh = int(lw * logo.height / logo.width)
logo = logo.resize((lw, lh), Image.LANCZOS)
img.alpha_composite(logo, ((W - lw) // 2, 24))


def neon_text(xy, text, font, color, glow=8):
    g = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).text(xy, text, font=font, fill=color + (255,))
    img.alpha_composite(g.filter(ImageFilter.GaussianBlur(glow)))
    d.text(xy, text, font=font, fill=color + (255,))


# arcade neon palette (echoes the logo's sunset + cyan)
TILES = [
    ("SPRITE", (255, 206, 70), "World-space sprite quads via Atom; atlas UV, flip, 90-degree transpose, point-filter pixel art."),
    ("RENDERING", (255, 150, 52), "Batched feature processor (texture + sort-layer); camera-distance depth sort; off-screen culling."),
    ("DEPTH+SHADOW", (90, 210, 255), "2.5D draw order by world depth, plus soft ground shadows under billboarded sprites."),
    ("2.5D+PARALLAX", (120, 235, 150), "Depth-sorted layers, parallax backgrounds, and a tilted 2.5D camera."),
    ("TILEMAP", (90, 225, 210), "Atlas tilemaps: autotiling (4-bit, 47-blob, custom rules), in-editor paint, Tiled .tmj import."),
    ("ANIMATION", (200, 150, 255), "Flipbook + frame events; skeletal cutout + cross-fade; .aseprite import; state machine."),
    ("LIGHTING", (255, 224, 110), "Gem-native 2D point + directional lights; normal-mapped sprites for shaped shading."),
    ("EFFECTS", (255, 96, 150), "2D particles; per-sprite flash, outline, emissive; hit-stop / slow-motion time-scale."),
    ("POST", (180, 130, 255), "Bloom + vignette via a 2D Look component, plus a retro CRT scanline pass."),
    ("CAMERA", (90, 200, 255), "Follow, deadzone, bounds, shake; versus framing + auto-zoom; ortho / pixel-perfect."),
    ("GAMEPLAY", (140, 230, 130), "2D collision (triggers, queries, pushbox), frame-data hitboxes, rebindable input + motions."),
    ("SCRIPTING", (255, 210, 90), "Typed per-feature request buses, usable from Lua, Python, and Script Canvas."),
    ("DETERMINISM", (110, 240, 190), "Fixed-step clock, seeded RNG, snapshot/restore + state hash, input ring; rollback-ready, CI-proven."),
    ("GRID INTEL", (255, 170, 90), "A* pathfinding, movement range, and shadowcast field of view over any tile grid."),
    ("AUDIO", (150, 200, 255), "One-shot and ambience conveniences over MiniAudio, on the same typed buses."),
    ("SAMPLES", (240, 120, 200), "Shmup, brawler, platformer, and fighting slices with how-tos, plus per-feature demos."),
]
cols, rows = 4, 4
mx, top_y, gx, gy = 40, 30 + lh + 28, 20, 18
tw = (W - 2 * mx - (cols - 1) * gx) // cols
th = (H - top_y - 58 - (rows - 1) * gy) // rows
tfont, dfont = mono(23), sans(20)
for i, (title, col, desc) in enumerate(TILES):
    cx = mx + (i % cols) * (tw + gx); cy = top_y + (i // cols) * (th + gy)
    d.rounded_rectangle([cx, cy, cx + tw, cy + th], radius=10, fill=(13, 13, 19, 255), outline=col + (255,), width=2)
    neon_text((cx + 18, cy + 16), title, tfont, col, glow=6)
    yy = cy + 60
    # Pixel-measured wrap: character counts lie (glyph widths vary), so accumulate
    # words against the actual rendered width inside the tile padding.
    text_w = tw - 36  # 18px padding each side
    lines, cur = [], ""
    for word in desc.split():
        trial = word if not cur else cur + " " + word
        if dfont.getlength(trial) <= text_w:
            cur = trial
        else:
            lines.append(cur)
            cur = word
    if cur:
        lines.append(cur)
    max_lines = (th - 60 - 14) // 27  # text area below the title, 14px bottom pad
    assert len(lines) <= max_lines, f"{title}: {len(lines)} lines > {max_lines} (tile overflow)"
    for line in lines:
        assert dfont.getlength(line) <= text_w, f"{title}: line wider than tile: {line!r}"
        d.text((cx + 18, yy), line, font=dfont, fill=(206, 212, 224, 255)); yy += 27

# footer (no version)
d.text((mx + 2, H - 44), "World-space 2D + 2.5D for the Open 3D Engine    \"New 2.5D Game\" template    Deterministic when you want it",
       font=sans(21), fill=(120, 150, 185, 255))

# --- CRT scanlines over the whole card, to match the logo ---
scan = Image.new("RGBA", (W, H), (0, 0, 0, 0)); sd = ImageDraw.Draw(scan)
for y in range(0, H, 3):
    sd.line([(0, y), (W, y)], fill=(0, 0, 0, 55))
img.alpha_composite(scan)

out = f"{ROOT}/Docs/images/diorama-features.png"
img.convert("RGB").save(out)
print("wrote", out, img.size)
