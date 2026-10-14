# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
#
# Generates the FOSSynd release banner as a small, indexed-palette (pixel-art)
# PNG, using only the Python standard library so it runs anywhere. It is the
# source of truth for assets/ui/banner.png: re-run it to regenerate the art.
#
#   python3 assets/ui/make_banner.py                 # current release defaults
#   python3 assets/ui/make_banner.py 0.5.0 RECRUITMENT
#
# Original art only: the shapes are the same isometric diamonds the game draws
# at runtime, and the wordmark uses a tiny 5x7 bitmap font defined below. No
# third-party or copyrighted material (AGENTS.md golden rule 8, docs/ASSETS.md).

import struct
import sys
import zlib

NATIVE_W, NATIVE_H = 160, 56
SCALE = 4  # nearest-neighbour upscale for a legible, chunky banner

# Palette (index -> (r, g, b)). Kept small on purpose: it is a palette image.
PALETTE = [
    (12, 13, 12),    # 0 background
    (26, 30, 26),    # 1 grid line
    (58, 66, 58),    # 2 tile light
    (46, 53, 46),    # 3 tile dark
    (40, 170, 60),   # 4 player green
    (90, 230, 130),  # 5 player bright
    (200, 55, 45),   # 6 enemy red
    (255, 210, 90),  # 7 enemy flash
    (225, 235, 225),  # 8 text light
    (150, 160, 150),  # 9 text dim
    (90, 169, 230),  # 10 accent blue
    (255, 255, 255),  # 11 white
    (90, 205, 230),  # 12 follower cyan
]

BG, GRID, TILE_L, TILE_D = 0, 1, 2, 3
GREEN, GREEN_HI, RED, RED_HI = 4, 5, 6, 7
TEXT, DIM, ACCENT, WHITE = 8, 9, 10, 11
FOLLOWER = 12

# 5x7 bitmap font, one entry per glyph as seven 5-character rows.
FONT = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["01111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00010", "00010", "00010", "00010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "11011", "10001"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11111", "00010", "00100", "00010", "00001", "10001", "01110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
    ".": ["00000", "00000", "00000", "00000", "00000", "00000", "00100"],
    " ": ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
}


class Canvas:
    def __init__(self, w, h, fill=BG):
        self.w, self.h = w, h
        self.px = bytearray([fill]) * (w * h)

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.set(x, y, c)

    def diamond(self, cx, cy, half_w, c):
        # Flat isometric diamond (2:1), the same shape the game uses.
        half_h = max(1, half_w // 2)
        for dy in range(-half_h, half_h + 1):
            span = 1.0 - abs(dy) / float(half_h)
            if span <= 0:
                continue
            w = int(half_w * span)
            for x in range(cx - w, cx + w + 1):
                self.set(x, cy + dy, c)

    def text(self, x, y, s, color, scale=1, spacing=1):
        cx = x
        for ch in s:
            glyph = FONT.get(ch, FONT[" "])
            for gy, row in enumerate(glyph):
                for gx, bit in enumerate(row):
                    if bit == "1":
                        for sy in range(scale):
                            for sx in range(scale):
                                self.set(cx + gx * scale + sx, y + gy * scale + sy, color)
            cx += (5 + spacing) * scale
        return cx

    def text_width(self, s, scale=1, spacing=1):
        return len(s) * (5 + spacing) * scale - spacing * scale


def write_png(path, canvas, palette, scale):
    """Writes an 8-bit indexed (color type 3) PNG, upscaled nearest-neighbour."""
    w, h = canvas.w * scale, canvas.h * scale
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0 (None)
        sy = y // scale
        for x in range(w):
            raw.append(canvas.px[sy * canvas.w + (x // scale)])

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)
    plte = b"".join(bytes(c) for c in palette)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"PLTE", plte)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    with open(path, "wb") as f:
        f.write(png)


def main():
    version = sys.argv[1] if len(sys.argv) > 1 else "0.4.0"
    subtitle = sys.argv[2] if len(sys.argv) > 2 else "SMART ENEMIES"

    c = Canvas(NATIVE_W, NATIVE_H, BG)

    # Isometric floor: a few tile diamonds plus grid lines, low contrast.
    for tx in range(0, NATIVE_W, 24):
        c.diamond(tx + 12, 8, 14, TILE_D)
    c.diamond(36, 8, 14, TILE_L)
    c.diamond(108, 8, 14, TILE_L)
    for gx in range(0, NATIVE_W, 24):
        for y in range(0, 20):
            c.set(gx, y, GRID)

    # Squad of four player diamonds and one enemy, echoing the in-game markers.
    for i, x in enumerate((46, 62, 78, 94)):
        c.diamond(x, 13 if i % 2 == 0 else 10, 6, GREEN if i % 2 == 0 else GREEN_HI)
    c.diamond(124, 12, 6, RED)
    c.diamond(124, 12, 2, RED_HI)

    # Two recruited followers trailing the squad (v0.5.0's Persuadertron), drawn
    # in the same cyan the game uses for followers.
    c.diamond(34, 16, 5, FOLLOWER)
    c.diamond(102, 17, 5, FOLLOWER)

    # Wordmark and version/subtitle, centred.
    title = "FOSSYND"
    c.text((NATIVE_W - c.text_width(title, 3)) // 2, 22, title, WHITE, scale=3)
    sub = ("V%s  %s" % (version, subtitle)).upper()
    c.text((NATIVE_W - c.text_width(sub, 1)) // 2, 46, sub, DIM, scale=1)

    # A thin accent underline under the wordmark.
    c.rect(24, 42, NATIVE_W - 24, 43, ACCENT)

    write_png("assets/ui/banner.png", c, PALETTE, SCALE)
    print("wrote assets/ui/banner.png (%dx%d, native %dx%d) v%s %s" % (
        NATIVE_W * SCALE, NATIVE_H * SCALE, NATIVE_W, NATIVE_H, version, subtitle))


if __name__ == "__main__":
    main()
