"""Turn a frame dumped by the board (M4 step 6) into a picture.

Usage:  python3 host/frame_to_png.py <monitor log> [output.png]
Reads the last FRAME_BEGIN ... FRAME_END block in the log and writes a PNG with
three panels side by side: the camera picture, the red mask (white = red pixel)
and the picture with the mask outlined. The centroid sent by the board is a green cross.
No extra packages needed (uses zlib + struct to write the PNG).
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUT = os.path.join(ROOT, "figures", "M4", "m4-step6-frame.png")
SCALE = 3                               # each camera pixel → 3×3 screen pixels
GREEN = (40, 220, 90)


def read_frame(path):
    """Return width, height, valid, cx, cy, pixels (rows of RGB), mask (rows of 0/1)."""
    lines = open(path, errors="replace").read().splitlines()
    begin = max(i for i, l in enumerate(lines) if l.startswith("FRAME_BEGIN"))
    _, w, h, valid, cx, cy = lines[begin].split()
    w, h = int(w), int(h)
    pixels, mask = [], []
    # Other tasks keep printing while the frame is dumped, so their lines can land between
    # the frame rows. Only accept lines shaped like a frame row: 4·w hex, a space, w/4 hex.
    hexdigits = set("0123456789ABCDEFabcdef")
    for row in lines[begin + 1:]:
        row = row.strip()
        if row.startswith("FRAME_END") or len(pixels) == h:
            break
        parts = row.split(" ")
        if (len(parts) != 2 or len(parts[0]) != 4 * w or len(parts[1]) != w // 4
                or not set(parts[0] + parts[1]) <= hexdigits):
            continue                                        # not a frame row: skip it
        hex_pixels, hex_mask = parts
        raw = bytes.fromhex(hex_pixels)
        rgb_row = []
        for x in range(w):
            p = (raw[2 * x] << 8) | raw[2 * x + 1]          # high byte first, same as the board
            r5, g6, b5 = (p >> 11) & 0x1F, (p >> 5) & 0x3F, p & 0x1F
            rgb_row.append((r5 * 255 // 31, g6 * 255 // 63, b5 * 255 // 31))
        pixels.append(rgb_row)
        bits = bin(int(hex_mask, 16))[2:].zfill(w)          # 160 mask bits
        mask.append([int(b) for b in bits])
    if len(pixels) != h:
        sys.exit(f"frame incomplete: found {len(pixels)} of {h} rows")
    return w, h, valid == "1", float(cx), float(cy), pixels, mask


def write_png(path, width, height, rows):
    """rows: list of bytes objects, each width*3 long (RGB)."""
    raw = b"".join(b"\x00" + r for r in rows)                # filter type 0 per row
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python3 host/frame_to_png.py <monitor log> [output.png]")
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    w, h, valid, cx, cy, pixels, mask = read_frame(sys.argv[1])

    # Three panels: picture | mask | picture with the mask's edge drawn in.
    def panel_pixel(panel, x, y):
        if panel == 0:
            return pixels[y][x]
        if panel == 1:
            return (255, 255, 255) if mask[y][x] else (40, 40, 40)
        edge = mask[y][x] and any(
            not (0 <= x + dx < w and 0 <= y + dy < h and mask[y + dy][x + dx])
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        return (255, 255, 0) if edge else pixels[y][x]

    gap = 4
    W = (w * 3 + gap * 2) * SCALE
    H = h * SCALE
    image = [[(20, 20, 20)] * W for _ in range(H)]
    for panel in range(3):
        x0 = panel * (w + gap)
        for y in range(h):
            for x in range(w):
                c = panel_pixel(panel, x, y)
                for sy in range(SCALE):
                    row = image[y * SCALE + sy]
                    for sx in range(SCALE):
                        row[(x0 + x) * SCALE + sx] = c
        if valid:                                           # green cross at the board's centroid
            px, py = int(round((x0 + cx) * SCALE)), int(round(cy * SCALE))
            for d in range(-4 * SCALE, 4 * SCALE + 1):
                for t in (-1, 0, 1):
                    if 0 <= px + d < W and 0 <= py + t < H:
                        image[py + t][px + d] = GREEN
                    if 0 <= px + t < W and 0 <= py + d < H:
                        image[py + d][px + t] = GREEN

    write_png(out, W, H, [bytes(v for c in row for v in c) for row in image])
    print(f"wrote {out}  ({w}x{h}, valid={valid}, centroid=({cx}, {cy}), red pixels={sum(map(sum, mask))})")


if __name__ == "__main__":
    main()
