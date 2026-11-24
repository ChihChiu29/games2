#!/usr/bin/env python3
"""Convert generated magenta-keyed Sword & Shield viewmodel JPGs into crisp
GZDoom/UZDoom transparent PNG sprites with embedded `grAb` offset chunks.
"""
import collections
import os
import struct
import zlib
from PIL import Image, ImageEnhance

SRC_DIR = "/home/chih/.gemini/antigravity-cli/brain/3f9b7779-6bf4-4641-80c9-b64910621dd8"
OUT_DIR = "/home/chih/Workspace/git/games2/mydoom/sekiro_pb/sprites"

FILES = {
    "idle": "sword_shield_idle_1795559842195.jpg",
    "slash": "sword_shield_slash_1795559859769.jpg",
    "block": "sword_shield_block_1795559912493.jpg",
}


def chroma_key_magenta(img: Image.Image) -> Image.Image:
    """Flood-fill magenta (#FF00FF) from borders with JPEG tolerance + edge despill."""
    rgba = img.convert("RGBA")
    w, h = rgba.size
    px = rgba.load()

    def is_bg(r, g, b):
        # Pure magenta has high R, low G, high B
        return (r > 165 and b > 165 and g < 115 and (r - g) > 70 and (b - g) > 70)

    visited = bytearray(w * h)
    q = collections.deque()

    for x in range(w):
        for y in (0, h - 1):
            r, g, b, _ = px[x, y]
            if is_bg(r, g, b):
                visited[y * w + x] = 1
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not visited[y * w + x]:
                r, g, b, _ = px[x, y]
                if is_bg(r, g, b):
                    visited[y * w + x] = 1
                    q.append((x, y))

    while q:
        x, y = q.popleft()
        px[x, y] = (0, 0, 0, 0)
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < w and 0 <= ny < h and not visited[ny * w + nx]:
                r, g, b, _ = px[nx, ny]
                if is_bg(r, g, b):
                    visited[ny * w + nx] = 1
                    q.append((nx, ny))

    # Also clear any isolated magenta pockets (e.g. between sword & shield arc)
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a > 0 and r > 195 and b > 195 and g < 85 and (r - g) > 120 and (b - g) > 120:
                px[x, y] = (0, 0, 0, 0)

    # Despill 1-2px magenta fringe on remaining opaque pixels bordering transparency
    for _ in range(2):
        to_clear = []
        to_despill = []
        for y in range(1, h - 1):
            for x in range(1, w - 1):
                r, g, b, a = px[x, y]
                if a == 0:
                    continue
                has_trans = (
                    px[x - 1, y][3] == 0 or px[x + 1, y][3] == 0 or
                    px[x, y - 1][3] == 0 or px[x, y + 1][3] == 0
                )
                if has_trans and r > g + 35 and b > g + 35:
                    if r > 140 and b > 140 and g < 110:
                        to_clear.append((x, y))
                    else:
                        # Neutralize magenta cast to gray/brown
                        avg = (g + min(r, b)) // 2
                        to_despill.append((x, y, avg, g, avg, a))
        for x, y in to_clear:
            px[x, y] = (0, 0, 0, 0)
        for x, y, r, g, b, a in to_despill:
            px[x, y] = (r, g, b, a)

    return rgba


def fit_to_doom_canvas(rgba: Image.Image, target_w=320, target_h=200, bottom_pad=0) -> Image.Image:
    """Scale preserving aspect ratio and anchor to bottom of 320x200 Doom weapon canvas."""
    w, h = rgba.size
    scale = min(target_w / w, (target_h - bottom_pad) / h)
    nw, nh = max(1, int(round(w * scale))), max(1, int(round(h * scale)))
    resized = rgba.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (target_w, target_h), (0, 0, 0, 0))
    ox = (target_w - nw) // 2
    oy = target_h - nh - bottom_pad
    canvas.paste(resized, (ox, oy), resized)
    return canvas


def save_doom_png(img: Image.Image, out_path: str, grab_x: int = 0, grab_y: int = 32):
    """Save PNG with GZDoom `grAb` chunk inserted right after IHDR."""
    tmp = out_path + ".tmp.png"
    img.save(tmp, format="PNG", optimize=True)
    raw = open(tmp, "rb").read()
    os.remove(tmp)

    # PNG signature (8B) + IHDR chunk (4 len + 4 type + 13 data + 4 crc = 25B)
    ihdr_end = 8 + 25
    assert raw[:8] == b"\x89PNG\r\n\x1a\n" and raw[12:16] == b"IHDR"
    grab_data = struct.pack(">ii", grab_x, grab_y)
    grab_chunk = struct.pack(">I", 8) + b"grAb" + grab_data + struct.pack(">I", zlib.crc32(b"grAb" + grab_data) & 0xFFFFFFFF)
    with open(out_path, "wb") as f:
        f.write(raw[:ihdr_end] + grab_chunk + raw[ihdr_end:])


def make_parry_flash(block_img: Image.Image) -> Image.Image:
    """Create bright golden-white parry flash variant of the raised shield."""
    bright = ImageEnhance.Brightness(block_img).enhance(1.35)
    px = bright.load()
    w, h = bright.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a > 0:
                px[x, y] = (min(255, int(r * 1.15 + 35)), min(255, int(g * 1.08 + 24)), min(255, int(b * 0.9 + 5)), a)
    return bright


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    idle = fit_to_doom_canvas(chroma_key_magenta(Image.open(os.path.join(SRC_DIR, FILES["idle"]))))
    slash1 = fit_to_doom_canvas(chroma_key_magenta(Image.open(os.path.join(SRC_DIR, FILES["slash"]))))
    block = fit_to_doom_canvas(chroma_key_magenta(Image.open(os.path.join(SRC_DIR, FILES["block"]))))

    # Slash2: shifted follow-through variant (slightly lower/left for 2-hit feel)
    slash2 = Image.new("RGBA", slash1.size, (0, 0, 0, 0))
    slash2.paste(slash1, (-10, 8), slash1)

    parry_flash = make_parry_flash(block)

    save_doom_png(idle, os.path.join(OUT_DIR, "SWSHA0.png"), 0, 32)
    save_doom_png(slash1, os.path.join(OUT_DIR, "SWSHB0.png"), 0, 32)
    save_doom_png(slash2, os.path.join(OUT_DIR, "SWSHC0.png"), 0, 32)
    save_doom_png(block, os.path.join(OUT_DIR, "SWSHD0.png"), 0, 32)
    save_doom_png(parry_flash, os.path.join(OUT_DIR, "SWSHE0.png"), 0, 32)
    print("Generated 5 Doom PNG sprites in", OUT_DIR)
    for fn in sorted(os.listdir(OUT_DIR)):
        p = os.path.join(OUT_DIR, fn)
        print(f"  {fn}: {os.path.getsize(p):,} bytes")


if __name__ == "__main__":
    main()
