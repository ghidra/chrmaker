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


def load_existing(path):
    """Load existing tiles from a CHR file, returning a list of 16-byte tiles."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except FileNotFoundError:
        return []
    tiles = []
    for i in range(0, len(data), TILE_BYTES):
        chunk = data[i:i + TILE_BYTES]
        if len(chunk) == TILE_BYTES:
            tiles.append(chunk)
    return tiles


def find_first_free(tiles):
    """Return the index of the first blank tile (all zeroes)."""
    blank = bytes(TILE_BYTES)
    for i, t in enumerate(tiles):
        if t == blank:
            return i
    return len(tiles)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <output.chr>", file=sys.stderr)
        sys.exit(1)

    output_path = sys.argv[1]
    sprites = json.load(sys.stdin)

    new_tiles = []
    for i, sprite in enumerate(sprites):
        size   = sprite.get("size", 8)
        pixels = sprite["pixels"]
        if size == 8:
            new_tiles.append(encode_tile(pixels))
        elif size == 16:
            for quad in split_s16(pixels):
                new_tiles.append(encode_tile(quad))
        else:
            print(f"sprite {i}: unknown size {size}", file=sys.stderr)
            sys.exit(1)

    # Load existing file and inject after last used tile
    existing = load_existing(output_path)
    blank = bytes(TILE_BYTES)

    # Pad existing to MAX_TILES
    while len(existing) < MAX_TILES:
        existing.append(blank)

    insert_at = find_first_free(existing)
    n_new = len(new_tiles)

    if insert_at + n_new > MAX_TILES:
        avail = MAX_TILES - insert_at
        print(f"warning: only {avail} free slots, truncating {n_new} new tiles",
              file=sys.stderr)
        new_tiles = new_tiles[:avail]
        n_new = len(new_tiles)

    for i, tile in enumerate(new_tiles):
        existing[insert_at + i] = tile

    with open(output_path, "wb") as f:
        for tile in existing:
            f.write(tile)

    print(
        f"{len(sprites)} sprite(s) → {n_new} tile(s) @ slot {insert_at} → {output_path}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
