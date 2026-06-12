#!/usr/bin/env python3
"""Generate the Diorama shmup dogfood art with PIL.

A small vertical-scrolling shoot-em-up built to dogfood the gem (bullet emitter,
collision, camera, particles, parallax, sprites, input). These are simple procedural
placeholders rendered at 4x and downscaled for anti-aliasing; swap in nicer art by
replacing the PNGs (the asset paths/GUIDs stay the same, so no rewiring).

  shmup_player.png  - the player fighter (points up)
  shmup_enemy.png   - a hostile interceptor (points down)
  shmup_shot.png    - the player's energy bolt (bright cyan)
  shmup_bullet.png  - an enemy danmaku orb (soft magenta)
  shmup_star.png    - a soft star for the parallax starfield
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SS = 4  # supersample factor


def canvas(n):
    return Image.new("RGBA", (n * SS, n * SS), (0, 0, 0, 0))


def save(img, name, n):
    img.resize((n, n), Image.LANCZOS).save(os.path.join(HERE, name))
    print("wrote", name)


def _glow(img, radius):
    """Add a soft additive glow behind the opaque pixels."""
    alpha = img.split()[3]
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    glow.putalpha(alpha.filter(ImageFilter.GaussianBlur(radius)))
    return Image.alpha_composite(glow, img)


def player(n=256):
    img = canvas(n)
    d = ImageDraw.Draw(img)
    w = n * SS
    cx = w / 2
    # Hull: a sleek upward-pointing dart.
    hull = (0.30, 0.55, 0.95, 255)  # blue
    accent = (0.65, 0.85, 1.0, 255)
    body = [(cx, 0.08 * w), (0.66 * w, 0.62 * w), (cx, 0.78 * w), (0.34 * w, 0.62 * w)]
    d.polygon([(x, y) for x, y in body], fill=tuple(int(c * 255) if i < 3 else c for i, c in enumerate(hull)))
    # Wings.
    d.polygon([(0.34 * w, 0.50 * w), (0.10 * w, 0.74 * w), (0.34 * w, 0.68 * w)],
              fill=(60, 110, 210, 255))
    d.polygon([(0.66 * w, 0.50 * w), (0.90 * w, 0.74 * w), (0.66 * w, 0.68 * w)],
              fill=(60, 110, 210, 255))
    # Cockpit.
    d.ellipse([cx - 0.07 * w, 0.30 * w, cx + 0.07 * w, 0.48 * w],
              fill=tuple(int(c * 255) if i < 3 else c for i, c in enumerate(accent)))
    # Thruster flare.
    d.polygon([(0.42 * w, 0.78 * w), (cx, 0.92 * w), (0.58 * w, 0.78 * w)], fill=(255, 180, 60, 230))
    return _glow(img, 6 * SS), n


def enemy(n=256):
    img = canvas(n)
    d = ImageDraw.Draw(img)
    w = n * SS
    cx = w / 2
    # A downward-pointing interceptor in hostile red.
    d.polygon([(cx, 0.92 * w), (0.68 * w, 0.36 * w), (cx, 0.20 * w), (0.32 * w, 0.36 * w)],
              fill=(200, 50, 60, 255))
    d.polygon([(0.32 * w, 0.46 * w), (0.08 * w, 0.24 * w), (0.32 * w, 0.30 * w)], fill=(150, 30, 45, 255))
    d.polygon([(0.68 * w, 0.46 * w), (0.92 * w, 0.24 * w), (0.68 * w, 0.30 * w)], fill=(150, 30, 45, 255))
    # Menacing eye.
    d.ellipse([cx - 0.08 * w, 0.46 * w, cx + 0.08 * w, 0.62 * w], fill=(255, 220, 120, 255))
    d.ellipse([cx - 0.03 * w, 0.50 * w, cx + 0.03 * w, 0.58 * w], fill=(120, 0, 0, 255))
    return _glow(img, 6 * SS), n


def _soft_disc(n, rgb, core=1.0):
    img = canvas(n)
    d = ImageDraw.Draw(img)
    w = n * SS
    cx = cy = w / 2
    r = 0.30 * w
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(rgb[0], rgb[1], rgb[2], int(255 * core)))
    return _glow(img, 10 * SS)


def shot(n=128):
    # A bright cyan energy bolt: a vertical capsule with a glow.
    img = canvas(n)
    d = ImageDraw.Draw(img)
    w = n * SS
    cx = w / 2
    d.rounded_rectangle([cx - 0.10 * w, 0.20 * w, cx + 0.10 * w, 0.80 * w],
                        radius=0.10 * w, fill=(150, 255, 255, 255))
    d.rounded_rectangle([cx - 0.04 * w, 0.16 * w, cx + 0.04 * w, 0.84 * w],
                        radius=0.04 * w, fill=(255, 255, 255, 255))
    return _glow(img, 8 * SS), n


def bullet(n=128):
    return _soft_disc(n, (255, 90, 220)), n


def star(n=64):
    return _soft_disc(n, (235, 240, 255), core=0.9), n


def main():
    for fn in (player, enemy, shot, bullet, star):
        img, n = fn()
        save(img, "shmup_{}.png".format(fn.__name__), n)


if __name__ == "__main__":
    main()
