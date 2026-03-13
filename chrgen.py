#!/usr/bin/env python3
"""
chrgen.py — NES CHR binary generator (stdin → binary)

Usage:
    python3 chrgen.py <output.chr>

Reads a JSON array of sprite definitions from stdin.
Writes exactly 4096 bytes of raw NES CHR binary to <output.chr>.
Only error/status messages go to stderr.

Sprite definition format:
    [
      {
        "size": 8,
        "pixels": [
          "00111100",    <- 8 rows × 8 chars, values 0-3
          ...
        ]
      },
      {
        "size": 16,
        "pixels": [
          "0011111111110000",   <- 16 rows × 16 chars
          ...
        ]
      }
    ]

Pixel values:  0 = bg/transparent  1 = colour 1  2 = colour 2  3 = colour 3

S16 tile ordering (matches chrmaker SPRITE_16 mode):
    A 16×16 sprite is split into 4 tiles stored as: TL, BL, TR, BR
    so chrmaker renders them as  [TL][TR] / [BL][BR].
    chrgen handles this automatically — just supply the full 16×16 grid.

Output is padded to 256 tiles (4096 bytes) with blank tiles.
"""

import sys, json

MAX_TILES  = 256
TILE_BYTES = 16


def encode_tile(rows):
    if len(rows) != 8:
        raise ValueError(f"tile must have 8 rows, got {len(rows)}")
    bp0, bp1 = bytearray(8), bytearray(8)
    for r, row in enumerate(rows):
        row = row.replace(" ", "")
        if len(row) != 8:
            raise ValueError(f"row {r} must be 8 pixels, got {row!r}")
        for c, ch in enumerate(row):
            v = int(ch)
            if v & 1: bp0[r] |= 1 << (7 - c)
            if v & 2: bp1[r] |= 1 << (7 - c)
    return bytes(bp0 + bp1)


def split_s16(pixels):
    """Split 16×16 grid → [tl, bl, tr, br] for S16 column-major layout."""
    if len(pixels) != 16:
        raise ValueError(f"16x16 sprite must have 16 rows, got {len(pixels)}")
    px = [row.replace(" ", "") for row in pixels]
    for i, row in enumerate(px):
        if len(row) != 16:
            raise ValueError(f"16x16 row {i} must be 16 pixels, got {row!r}")
    return [
        [px[r][0:8]  for r in range(0, 8)],   # TL  tile 0
        [px[r][0:8]  for r in range(8, 16)],   # BL  tile 1
        [px[r][8:16] for r in range(0, 8)],    # TR  tile 2
        [px[r][8:16] for r in range(8, 16)],   # BR  tile 3
    ]


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <output.chr>", file=sys.stderr)
        sys.exit(1)

    output_path = sys.argv[1]
    sprites = json.load(sys.stdin)

    tiles = []
    for i, sprite in enumerate(sprites):
        size   = sprite.get("size", 8)
        pixels = sprite["pixels"]
        if size == 8:
            tiles.append(encode_tile(pixels))
        elif size == 16:
            for quad in split_s16(pixels):
                tiles.append(encode_tile(quad))
        else:
            print(f"sprite {i}: unknown size {size}", file=sys.stderr)
            sys.exit(1)

    n_tiles = len(tiles)
    if n_tiles > MAX_TILES:
        print(f"warning: {n_tiles} tiles exceeds {MAX_TILES}, truncating", file=sys.stderr)
        tiles = tiles[:MAX_TILES]

    blank = bytes(TILE_BYTES)
    while len(tiles) < MAX_TILES:
        tiles.append(blank)

    with open(output_path, "wb") as f:
        for tile in tiles:
            f.write(tile)

    print(
        f"{len(sprites)} sprite(s) → {n_tiles} tile(s) → {output_path}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
