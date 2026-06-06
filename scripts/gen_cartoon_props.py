#!/usr/bin/env python3
"""
Organic / fractal cartoon foreground props for the diorama: recursive-branch
trees with clustered canopies, midpoint-displaced (jagged) rocks, and faceted
crystal clusters. Output cartoon_tree.png, cartoon_rock.png, cartoon_crystal.png
to the gem's Assets/Diorama/Textures.
"""
import os, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.expanduser("~/PROJECTS/o3de-diorama/Assets/Diorama/Textures")
os.makedirs(OUT, exist_ok=True)


def midpoint_poly(cx, cy, base_r, rough, n, seed):
    """A closed organic silhouette via per-vertex radius + midpoint displacement."""
    rng = random.Random(seed)
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        r = base_r * (1 + rng.uniform(-rough, rough))
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    # one round of midpoint displacement for finer detail
    out = []
    for i in range(len(pts)):
        x1, y1 = pts[i]; x2, y2 = pts[(i + 1) % len(pts)]
        out.append((x1, y1))
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        d = rng.uniform(-rough, rough) * base_r * 0.5
        out.append((mx + d, my + d))
    return out


def tree(size=320, seed=1):
    W = H = size * 2
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    rng = random.Random(seed)
    canopy = []   # collect leaf-cluster centres
    def branch(x, y, ang, length, width, depth):
        if depth == 0 or length < 8:
            canopy.append((x, y, max(14, length * 1.4)))
            return
        x2 = x + length * math.cos(ang); y2 = y - length * math.sin(ang)
        d.line([(x, y), (x2, y2)], fill=(74, 52, 34, 255), width=max(1, int(width)))
        for dv in (rng.uniform(0.35, 0.6), -rng.uniform(0.35, 0.6), rng.uniform(-0.15, 0.15)):
            branch(x2, y2, ang + dv, length * rng.uniform(0.66, 0.78), width * 0.7, depth - 1)
    branch(W // 2, int(H * 0.92), math.pi / 2, size * 0.32, size * 0.06, 6)
    # organic canopy: overlapping irregular blobs (two greens) over the cluster tips
    leaf = Image.new("RGBA", (W, H), (0, 0, 0, 0)); ld = ImageDraw.Draw(leaf)
    for (x, y, r) in canopy:
        for _ in range(6):
            ox, oy = rng.uniform(-r, r), rng.uniform(-r, r); rr = rng.uniform(r * 0.5, r)
            col = rng.choice([(60, 130, 70), (78, 152, 84), (50, 110, 62)])
            ld.ellipse([x + ox - rr, y + oy - rr, x + ox + rr, y + oy + rr], fill=col + (255,))
    img.alpha_composite(leaf.filter(ImageFilter.GaussianBlur(1)))
    img = img.crop(img.getbbox())
    img.save(f"{OUT}/cartoon_tree.png"); print("cartoon_tree", img.size)


def rock(size=300, seed=2):
    W = H = size; rng = random.Random(seed)
    pts = midpoint_poly(W / 2, H * 0.60, size * 0.34, 0.20, 14, seed)
    mask = Image.new("L", (W, H), 0); ImageDraw.Draw(mask).polygon(pts, fill=255)
    # round off the polygon's flat edges/corners so the silhouette reads organic
    # (a flat edge catches light and looks like a box)
    mask = mask.filter(ImageFilter.GaussianBlur(W * 0.04)).point(lambda v: 255 if v > 128 else 0)
    arr = np.zeros((H, W, 3)) + np.array([96, 98, 104], float)
    ys, xs = np.mgrid[0:H, 0:W]
    # top-lit shading
    fac = np.clip(1.05 - 0.6 * (ys / H), 0.55, 1.1)
    arr = arr * fac[..., None]
    # a couple of facets (lighter planes)
    for _ in range(3):
        fx, fy = rng.randint(W // 4, 3 * W // 4), rng.randint(H // 3, 3 * H // 4); fr = rng.randint(20, 50)
        m = (xs - fx) ** 2 + (ys - fy) ** 2 <= fr * fr
        arr[m] = np.clip(arr[m] * 1.12, 0, 255)
    out = Image.fromarray(np.dstack([np.clip(arr, 0, 255), np.asarray(mask)]).astype("uint8"), "RGBA")
    # lean left (billboards ignore entity roll, so the tilt is baked into the art)
    out = out.rotate(17, expand=True, resample=Image.BICUBIC)
    out = out.crop(out.getbbox())
    out.save(f"{OUT}/cartoon_rock.png"); print("cartoon_rock", out.size)


def crystal(size=300, seed=3, palette=None, out="cartoon_crystal"):
    # palette = candidate shard colours; default purple. A teal variant is baked
    # as its own texture (a multiply-tint can't turn purple into teal -> muddy).
    if palette is None:
        palette = [(150, 110, 235), (120, 110, 220), (180, 130, 240)]
    W = H = size * 2; rng = random.Random(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    base = (W // 2, int(H * 0.92))
    shards = []
    for i in range(rng.randint(4, 6)):
        ox = rng.randint(-size // 2, size // 2)
        ht = rng.randint(int(size * 0.7), int(size * 1.4))
        wd = rng.randint(int(size * 0.10), int(size * 0.22))
        tilt = rng.uniform(-0.2, 0.2)
        shards.append((base[0] + ox, base[1], wd, ht, tilt))
    shards.sort(key=lambda s: s[3])   # short ones behind
    for (x, y, wd, ht, tilt) in shards:
        tipx = x + int(ht * tilt)
        col = rng.choice(palette)
        s = Image.new("RGBA", (W, H), (0, 0, 0, 0)); sd = ImageDraw.Draw(s)
        sd.polygon([(tipx, y - ht), (x - wd, y - ht * 0.3), (x - wd // 2, y), (x + wd // 2, y), (x + wd, y - ht * 0.3)], fill=col + (235,))
        # bright facet
        sd.polygon([(tipx, y - ht), (x - wd, y - ht * 0.3), (x - wd // 4, y)], fill=tuple(min(255, c + 50) for c in col) + (235,))
        img.alpha_composite(s)
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    glow.alpha_composite(img); glow = glow.filter(ImageFilter.GaussianBlur(10))
    final = Image.new("RGBA", img.size, (0, 0, 0, 0)); final.alpha_composite(glow); final.alpha_composite(img)
    final = final.crop(final.getbbox())
    final.save(f"{OUT}/{out}.png"); print(out, final.size)


def campfire(size=320, seed=9):
    # just the fire PIT: an oval ash base ringed by stones, with crossed logs. The
    # flame itself is particles (a fire emitter the level attaches), so there is no
    # painted flame here -- only the pit + transparent space above for the fire.
    SS = 4; W = H = size * SS; rng = random.Random(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    cx, cy = W / 2, H * 0.74; rx, ry = W * 0.36, H * 0.155     # pit centre + oval radii
    # ash / charred base (oval) so the pit reads as a circle seen at an angle
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(54, 46, 42, 255))
    d.ellipse([cx - rx * 0.66, cy - ry * 0.62, cx + rx * 0.66, cy + ry * 0.7], fill=(34, 28, 26, 255))

    def stone(sx, sy, scale):
        r = rng.uniform(W * 0.05, W * 0.072) * scale
        base = rng.choice([(122, 122, 130), (144, 142, 144), (104, 108, 116)])
        d.ellipse([sx - r, sy - r * 0.78, sx + r, sy + r * 0.78], fill=base + (255,))
        d.ellipse([sx - r * 0.55, sy - r * 0.78, sx + r * 0.2, sy + r * 0.05], fill=tuple(min(255, c + 24) for c in base) + (255,))

    ring = [(2 * math.pi * i / 13) for i in range(13)]
    back = [a for a in ring if math.sin(a) < 0]      # top of the oval (behind the fire)
    front = [a for a in ring if math.sin(a) >= 0]     # bottom of the oval (in front)
    for a in back:
        stone(cx + rx * math.cos(a), cy + ry * math.sin(a), 0.82)
    # crossed logs across the pit
    for ang in (0.5, -0.4, 0.12):
        dx, dy = math.cos(ang) * W * 0.2, math.sin(ang) * H * 0.05
        d.line([(cx - dx, cy - dy - H * 0.02), (cx + dx, cy + dy - H * 0.02)], fill=(96, 64, 38, 255), width=int(W * 0.05))
        d.ellipse([cx + dx - W * 0.032, cy + dy - H * 0.02 - W * 0.032, cx + dx + W * 0.032, cy + dy - H * 0.02 + W * 0.032], fill=(128, 88, 54, 255))
    for a in front:
        stone(cx + rx * math.cos(a), cy + ry * math.sin(a), 1.0)
    img = img.resize((size, size), Image.LANCZOS); img = img.crop(img.getbbox())
    img.save(f"{OUT}/cartoon_campfire.png"); print("cartoon_campfire", img.size)


if __name__ == "__main__":
    tree(); rock(); crystal()
    # teal/cyan variant for the second crystal colour
    crystal(seed=3, palette=[(90, 200, 215), (110, 220, 230), (70, 175, 205)], out="cartoon_crystal_teal")
    campfire()
