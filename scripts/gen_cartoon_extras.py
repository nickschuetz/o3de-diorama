#!/usr/bin/env python3
"""
Cartoon asteroid, comet, and starry-sky background to match gen_cartoon_planets,
for the solar-system diorama. Output cartoon_asteroid.png, cartoon_comet.png,
cartoon_sky.png to the gem's Assets/Diorama/Textures.
"""
import os, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.expanduser("~/PROJECTS/o3de-diorama/Assets/Diorama/Textures")
os.makedirs(OUT, exist_ok=True)
L = np.array([-0.45, -0.5, 0.74]); L /= np.linalg.norm(L)


def asteroid(size=300, seed=5):
    random.seed(seed); SS = 4; n = size * SS
    # lumpy closed blob (radius wobbles with angle)
    cx = cy = n / 2
    pts = []
    base = n * 0.36
    waves = [(random.uniform(0.06, 0.16), random.randint(2, 6), random.uniform(0, 6.28)) for _ in range(4)]
    for a in np.linspace(0, 2 * math.pi, 90, endpoint=False):
        r = base * (1 + sum(amp * math.sin(k * a + ph) for amp, k, ph in waves))
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    mask = Image.new("L", (n, n), 0); ImageDraw.Draw(mask).polygon(pts, fill=255)
    arr = np.zeros((n, n, 3)) + np.array([150, 142, 130], float)
    # craters
    ys, xs = np.mgrid[0:n, 0:n]
    for _ in range(10):
        ang = random.uniform(0, 6.28); rad = random.uniform(0, base * 0.7)
        ox, oy = cx + rad * math.cos(ang), cy + rad * math.sin(ang); cr = random.uniform(n * 0.02, n * 0.06)
        arr[(xs - ox) ** 2 + (ys - oy) ** 2 <= cr * cr] *= 0.8
    # gentle shading via a fake normal from distance-to-edge gradient
    x = (xs - cx) / base; y = (ys - cy) / base
    fac = np.clip(0.78 - 0.25 * (x * 0.6 + y * 0.6), 0.55, 1.05)
    arr = np.clip(arr * fac[..., None], 0, 255)
    out = Image.fromarray(np.dstack([arr, np.asarray(mask)]).astype("uint8"), "RGBA").resize((size, size), Image.LANCZOS)
    pad = size // 6; canvas = Image.new("RGBA", (size + 2 * pad, size + 2 * pad), (0, 0, 0, 0)); canvas.alpha_composite(out, (pad, pad))
    canvas.save(f"{OUT}/cartoon_asteroid.png"); print("cartoon_asteroid", canvas.size)


def comet(W=720, H=360):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    nx, ny = int(W * 0.78), H // 2; nr = int(H * 0.16)
    # tail: glowing cone fading to the left
    tail = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(tail)
    for i in range(nx):
        t = i / nx                       # 0 at left .. 1 at nucleus
        half = int((H * 0.02) + (H * 0.30) * t)
        a = int(150 * (t ** 1.6))
        d.line([(i, ny - half), (i, ny + half)], fill=(120, 200, 255, a))
    tail = tail.filter(ImageFilter.GaussianBlur(6))
    img.alpha_composite(tail)
    # coma glow + icy nucleus
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0)); ImageDraw.Draw(glow).ellipse([nx - nr * 2, ny - nr * 2, nx + nr * 2, ny + nr * 2], fill=(150, 220, 255, 150))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(14)))
    nuc = Image.new("RGBA", (W, H), (0, 0, 0, 0)); ImageDraw.Draw(nuc).ellipse([nx - nr, ny - nr, nx + nr, ny + nr], fill=(225, 245, 255, 255))
    img.alpha_composite(nuc)
    img.save(f"{OUT}/cartoon_comet.png"); print("cartoon_comet", img.size)


def sky(W=1600, H=900, seed=11):
    rng = np.random.RandomState(seed)
    # TWILIGHT gradient for a setting sun: deep indigo up high (where the stars and
    # planets read) warming down through dusky pink to a sunset glow near the horizon.
    # warm band lifted high in the texture so it shows ABOVE the scene's horizon
    stops = [(0.00, (30, 34, 78)), (0.20, (66, 60, 120)), (0.32, (150, 98, 128)),
             (0.42, (236, 146, 86)), (1.00, (250, 182, 104))]
    v = np.linspace(0, 1, H)
    col = np.zeros((H, 3))
    for i in range(len(stops) - 1):
        v0, c0 = stops[i]; v1, c1 = stops[i + 1]
        m = (v >= v0) & (v <= v1)
        t = ((v[m] - v0) / (v1 - v0))[:, None]
        col[m] = np.array(c0, float) * (1 - t) + np.array(c1, float) * t
    arr = np.repeat(col[:, None, :], W, axis=1)
    img = Image.fromarray(arr.astype("uint8"), "RGB").convert("RGBA")
    # soft nebula, only up in the cool region
    neb = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(neb)
    for _ in range(5):
        cx, cy = rng.randint(0, W), rng.randint(0, int(H * 0.38)); r = rng.randint(120, 300)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=random.choice([(80, 60, 150), (60, 80, 160)]) + (32,))
    img.alpha_composite(neb.filter(ImageFilter.GaussianBlur(80)))
    # stars: only in the upper (dark) sky, fading out before the warm band
    d = ImageDraw.Draw(img)
    for _ in range(520):
        y = rng.randint(0, int(H * 0.42)); x = rng.randint(0, W)
        fade = max(0.0, 1.0 - (y / (H * 0.42)))          # dimmer toward the warm band
        if rng.random() > fade * 0.9 + 0.1:
            continue
        s = int(rng.choice([1, 1, 1, 2, 2, 3])); b = int(rng.randint(150, 255) * (0.4 + 0.6 * fade))
        tint = random.choice([(b, b, b), (b, b, 255), (255, b, b)])
        d.ellipse([x, y, x + s, y + s], fill=tint + (255,))
    img.convert("RGB").save(f"{OUT}/cartoon_sky.png"); print("cartoon_sky", (W, H))


def spark(size=128):
    # soft round particle (white core fading to transparent); tinted per-particle
    # by the emitter's start/end color. Used for the comet's particle fountain.
    n = size; ys, xs = np.mgrid[0:n, 0:n]; c = (n - 1) / 2.0
    r = np.sqrt((xs - c) ** 2 + (ys - c) ** 2) / c
    a = np.clip(1.0 - r, 0, 1) ** 1.8
    arr = np.dstack([np.full((n, n), 255), np.full((n, n), 255), np.full((n, n), 255), a * 255])
    Image.fromarray(arr.astype("uint8"), "RGBA").save(f"{OUT}/cartoon_spark.png"); print("cartoon_spark", (n, n))


if __name__ == "__main__":
    import random as _r; _r.seed(1)
    asteroid(); comet(); sky(); spark()
