#include "export.h"
#include <stdio.h>

/* Writes raw NES CHR data to path.
   Output is ntiles * 16 bytes, no header.

   NES planar format per tile:
     bytes  0- 7: bitplane 0 — bit 0 of each pixel, MSB = leftmost pixel
     bytes  8-15: bitplane 1 — bit 1 of each pixel, MSB = leftmost pixel

   Pixel value v at column c contributes:
     bp0 row byte |= (v & 1)        << (7 - c)
     bp1 row byte |= ((v >> 1) & 1) << (7 - c)

   Returns 0 on success, -1 on I/O error.                         */
int export_chr(const ChrPage *chr, int ntiles, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    for (int tile = 0; tile < ntiles; tile++) {
        uint8_t buf[16] = {0};

        for (int row = 0; row < TILE_H; row++) {
            uint8_t bp0 = 0, bp1 = 0;
            for (int col = 0; col < TILE_W; col++) {
                uint8_t v = chr->px[tile][row][col] & 3;
                bp0 |= (uint8_t)( (v & 1)        << (7 - col) );
                bp1 |= (uint8_t)( ((v >> 1) & 1) << (7 - col) );
            }
            buf[row]     = bp0;
            buf[8 + row] = bp1;
        }

        if (fwrite(buf, 1, 16, f) != 16) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}
