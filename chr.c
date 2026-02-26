#include "chr.h"
#include <string.h>
#include <stdio.h>

void chr_init(ChrPage *c) {
    memset(c, 0, sizeof(ChrPage));
}

/* Each tile gets a 2×2 block pattern showing all four values:
      0 1
      2 3
   (top-left 4×4 pixels = 0, top-right = 1, bottom-left = 2, bottom-right = 3)
   Makes tile boundaries and all four grayscale values immediately visible. */
void chr_fill_debug(ChrPage *c) {
    const int n = CHR_DEFAULT_COLS * CHR_DEFAULT_ROWS;
    for (int t = 0; t < n; t++)
        for (int r = 0; r < TILE_H; r++)
            for (int col = 0; col < TILE_W; col++)
                c->px[t][r][col] = ((r >= 4) ? 2 : 0) | (col >= 4 ? 1 : 0);
}

/* Load raw NES planar CHR data from path.
   Returns number of tiles loaded (>= 1), or -1 on error.
   Accepts any file that is a multiple of 16 bytes (one tile each).
   At most CHR_MAX_TILES tiles are loaded; larger files are truncated.
   Bit extraction: px = ((bp0 >> (7-col)) & 1) | (((bp1 >> (7-col)) & 1) << 1) */
int chr_load(ChrPage *c, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz < 16) { fclose(f); return -1; }   /* need at least one tile */

    int num_tiles = (int)(sz / 16);
    if (num_tiles > CHR_MAX_TILES) num_tiles = CHR_MAX_TILES;

    /* Zero everything first so unused tiles are blank. */
    memset(c->px, 0, sizeof(c->px));

    for (int tile = 0; tile < num_tiles; tile++) {
        uint8_t buf[16];
        if (fread(buf, 1, 16, f) != 16) { fclose(f); return tile; }

        for (int row = 0; row < TILE_H; row++) {
            uint8_t bp0 = buf[row];
            uint8_t bp1 = buf[8 + row];
            for (int col = 0; col < TILE_W; col++) {
                int bit = 7 - col;
                c->px[tile][row][col] =
                    (uint8_t)(((bp0 >> bit) & 1) | (((bp1 >> bit) & 1) << 1));
            }
        }
    }

    fclose(f);
    return num_tiles;
}

void palette_init(PaletteState *p) {
    memset(p, 0, sizeof(PaletteState));

    /* Default sub-palette 0: a basic grayscale ramp using NES master indices.
       0x0F = black, 0x00 = dark gray, 0x10 = light gray, 0x30 = white */
    p->sub[0].idx[0] = 0x0F;
    p->sub[0].idx[1] = 0x00;
    p->sub[0].idx[2] = 0x10;
    p->sub[0].idx[3] = 0x30;

    /* All tiles assigned to sub-palette 0 by default */
    memset(p->tile_pal, 0, sizeof(p->tile_pal));
}
