#!/usr/bin/env python3
"""
Bake the arcade-logo pixel-art textures into the GEM folder
(~/PROJECTS/o3de-diorama/Assets/Diorama/Textures -> product "diorama/textures/").
These are sprite textures the gem composites in-engine; the scanlines/tint come
from the gem's CRT component at capture time, not baked here.

Outputs: arcade_title.png, arcade_subtitle.png, arcade_heart.png, arcade_backdrop.png
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

OUT = os.path.expanduser("~/PROJECTS/o3de-diorama/Assets/Diorama/Textures")
os.makedirs(OUT, exist_ok=True)
MONT = "/usr/share/fonts/julietaula-montserrat-fonts/Montserrat-%s.otf"


def glow(layer, color, radius):
    a = layer.split()[3]
    solid = Image.new("RGBA", layer.size, color + (255,))
    g = Image.composite(solid, Image.new("RGBA", layer.size, (0, 0, 0, 0)), a)
    out = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    for r in (radius, radius // 2):
        out = ImageChops.add(out, g.filter(ImageFilter.GaussianBlur(r)))
    return out


def pixel_text(text, fontpath, target_h, fill, scale, outline, ow=1):
    small_h = max(8, target_h // scale)
    font = ImageFont.truetype(fontpath, small_h)
    tmp = Image.new("RGBA", (10, 10)); d = ImageDraw.Draw(tmp)
    bb = d.textbbox((0, 0), text, font=font)
    w, h = bb[2] - bb[0] + 4, bb[3] - bb[1] + 4
    small = Image.new("L", (w, h), 0)
    ImageDraw.Draw(small).text((2 - bb[0], 2 - bb[1]), text, font=font, fill=255)
    small = small.point(lambda v: 255 if v > 110 else 0)
    big = small.resize((w * scale, h * scale), Image.NEAREST)
    grad = Image.new("RGBA", big.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad); c0, c1 = fill
    for yy in range(big.size[1]):
        t = yy / big.size[1]
        gd.line([(0, yy), (big.size[0], yy)], fill=(int(c0[0] + (c1[0] - c0[0]) * t),
                int(c0[1] + (c1[1] - c0[1]) * t), int(c0[2] + (c1[2] - c0[2]) * t), 255))
    col = Image.composite(grad, Image.new("RGBA", big.size, (0, 0, 0, 0)), big)
    # Outline by dilating the mask (MaxFilter does not wrap, unlike ImageChops.offset).
    oa = big.filter(ImageFilter.MaxFilter(2 * ow * scale + 1))
    outline_img = Image.composite(Image.new("RGBA", big.size, outline + (255,)),
                                  Image.new("RGBA", big.size, (0, 0, 0, 0)), oa)
    return Image.alpha_composite(outline_img, col).crop(Image.alpha_composite(outline_img, col).getbbox())


def pixel_heart(cell=9):
    # Classic pixel heart: pronounced lobes + clear cleft, a thin dark outline,
    # red fill -- fine pixels to match the letters (matches the reference logo).
    pat = [
        "..XXX....XXX..",
        ".XXXXX..XXXXX.",
        "XXXXXXXXXXXXXX",
        "XXXXXXXXXXXXXX",
        "XXXXXXXXXXXXXX",
        ".XXXXXXXXXXXX.",
        ".XXXXXXXXXXXX.",
        "..XXXXXXXXXX..",
        "...XXXXXXXX...",
        "....XXXXXX....",
        ".....XXXX.....",
        "......XX......",
    ]
    cols, rows = len(pat[0]), len(pat)
    inner = Image.new("RGBA", (cols, rows), (0, 0, 0, 0)); px = inner.load()
    for y, row in enumerate(pat):
        for x, c in enumerate(row):
            if c == "X":
                t = y / rows
                px[x, y] = (255, int(35 - 28 * t), int(45 - 35 * t), 255)  # red
    base = Image.new("RGBA", (cols + 2, rows + 2), (0, 0, 0, 0))
    base.alpha_composite(inner, (1, 1))
    big = base.resize(((cols + 2) * cell, (rows + 2) * cell), Image.NEAREST)
    # 1-cell dark-maroon outline (MaxFilter dilates without wrapping).
    oa = big.split()[3].filter(ImageFilter.MaxFilter(2 * cell + 1))
    out = Image.composite(Image.new("RGBA", big.size, (60, 8, 22, 255)),
                          Image.new("RGBA", big.size, (0, 0, 0, 0)), oa)
    return Image.alpha_composite(out, big)


def pad(img, m):
    out = Image.new("RGBA", (img.width + 2 * m, img.height + 2 * m), (0, 0, 0, 0))
    out.alpha_composite(img, (m, m)); return out


# --- title: pixel DIORAMA, yellow->orange, magenta glow halo, transparent bg ---
title = pixel_text("DIORAMA", MONT % "ExtraBold", 330, ((255, 244, 130), (255, 150, 30)),
                   scale=6, outline=(70, 12, 46), ow=1)
title = pad(title, 70)
title_glow = glow(title, (255, 60, 160), 24)
title_final = Image.alpha_composite(title_glow, title)
title_final.save(f"{OUT}/arcade_title.png"); print("arcade_title", title_final.size)

# --- subtitle: pixel, cyan, slight glow ---
sub = pixel_text("WORLD-SPACE 2D / 2.5D FOR O3DE", MONT % "ExtraBold", 64,
                 ((120, 240, 255), (60, 170, 235)), scale=5, outline=(10, 20, 50), ow=1)
sub = pad(sub, 20)
sub_final = Image.alpha_composite(glow(sub, (40, 150, 220), 8), sub)
sub_final.save(f"{OUT}/arcade_subtitle.png"); print("arcade_subtitle", sub_final.size)

# --- single pixel heart (placed x3 in-engine), with glow ---
heart = pad(pixel_heart(), 20)
# subtle glow matching the DIORAMA title glow color (magenta)
heart_final = Image.alpha_composite(glow(heart, (255, 60, 160), 11), heart)
heart_final.save(f"{OUT}/arcade_heart.png"); print("arcade_heart", heart_final.size)

# --- backdrop: plain dark (gem CRT adds scanlines+tint); subtle radial lift ---
import numpy as np
W, H = 1280, 720
base = (14, 10, 28)
arr = np.zeros((H, W, 3), float) + base
yy, xx = np.mgrid[0:H, 0:W]
d = np.sqrt(((xx - W / 2) / (W * 0.6)) ** 2 + ((yy - H * 0.42) / (H * 0.6)) ** 2)
lift = np.clip(0.5 - d, 0, 0.5)[..., None] * np.array([30, 14, 46])
arr = np.clip(arr + lift, 0, 255)
Image.fromarray(arr.astype("uint8"), "RGB").save(f"{OUT}/arcade_backdrop.png")
print("arcade_backdrop", (W, H))
