#include "compose.h"
#include <string.h>
#include <stdio.h>

void compose_init(ComposeData *d) {
    memset(d, 0, sizeof(ComposeData));
    d->scene_count  = 1;
    d->active_scene = 0;
}

/* ── Scene file format (.scn) ────────────────────────────────────
   Header:
     "NSCN"       4 bytes (magic)
     version      1 byte  (= 1)
     scene_count  1 byte  (1-16)
   Per scene:
     nametable    960 bytes (32x30 tile indices)
     attributes   240 bytes (15x16 palette indices)
     sprite_count 1 byte
     sprites      sprite_count * 6 bytes each:
       x, y, tile_lo, tile_hi, palette, flags
       flags: bit 0 = hflip, bit 1 = vflip, bit 2 = behind_bg
   ─────────────────────────────────────────────────────────────── */

int compose_save(const ComposeData *d, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    /* Header — version 2: uint16_t nametable entries */
    fwrite("NSCN", 1, 4, f);
    uint8_t ver = 2;
    fwrite(&ver, 1, 1, f);
    uint8_t sc = (uint8_t)d->scene_count;
    fwrite(&sc, 1, 1, f);

    for (int i = 0; i < d->scene_count; i++) {
        const ComposeScene *s = &d->scenes[i];

        /* Nametable: 1920 bytes (32×30 × 2 bytes, little-endian) */
        for (int r = 0; r < COMPOSE_NT_H; r++) {
            for (int c = 0; c < COMPOSE_NT_W; c++) {
                uint8_t lo = (uint8_t)(s->nametable[r][c] & 0xFF);
                uint8_t hi = (uint8_t)((s->nametable[r][c] >> 8) & 0xFF);
                fwrite(&lo, 1, 1, f);
                fwrite(&hi, 1, 1, f);
            }
        }

        /* Attributes: 240 bytes */
        fwrite(s->attr, 1, 15 * 16, f);

        /* Sprites */
        uint8_t spr_cnt = (uint8_t)s->sprite_count;
        fwrite(&spr_cnt, 1, 1, f);

        for (int j = 0; j < s->sprite_count; j++) {
            const ComposeSprite *sp = &s->sprites[j];
            uint8_t buf[6];
            buf[0] = sp->x;
            buf[1] = sp->y;
            buf[2] = (uint8_t)(sp->tile & 0xFF);
            buf[3] = (uint8_t)((sp->tile >> 8) & 0xFF);
            buf[4] = sp->palette;
            buf[5] = (sp->hflip ? 1 : 0)
                    | (sp->vflip ? 2 : 0)
                    | (sp->behind_bg ? 4 : 0)
                    | (sp->s16 ? 8 : 0);
            fwrite(buf, 1, 6, f);
        }
    }

    fclose(f);
    return 0;
}

int compose_load(ComposeData *d, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "NSCN", 4) != 0) {
        fclose(f); return -1;
    }

    uint8_t ver, sc;
    if (fread(&ver, 1, 1, f) != 1 || (ver != 1 && ver != 2)) { fclose(f); return -1; }
    if (fread(&sc, 1, 1, f) != 1 || sc < 1 || sc > COMPOSE_MAX_SCENES) {
        fclose(f); return -1;
    }

    compose_init(d);
    d->scene_count = sc;

    for (int i = 0; i < sc; i++) {
        ComposeScene *s = &d->scenes[i];

        if (ver == 1) {
            /* v1: 1 byte per nametable entry */
            uint8_t nt8[COMPOSE_NT_H * COMPOSE_NT_W];
            if (fread(nt8, 1, sizeof(nt8), f) != sizeof(nt8)) { fclose(f); return -1; }
            for (int r = 0; r < COMPOSE_NT_H; r++)
                for (int c = 0; c < COMPOSE_NT_W; c++)
                    s->nametable[r][c] = nt8[r * COMPOSE_NT_W + c];
        } else {
            /* v2: 2 bytes per nametable entry (little-endian) */
            for (int r = 0; r < COMPOSE_NT_H; r++) {
                for (int c = 0; c < COMPOSE_NT_W; c++) {
                    uint8_t pair[2];
                    if (fread(pair, 1, 2, f) != 2) { fclose(f); return -1; }
                    s->nametable[r][c] = pair[0] | ((uint16_t)pair[1] << 8);
                }
            }
        }

        if (fread(s->attr, 1, 15 * 16, f) != 15 * 16) { fclose(f); return -1; }

        uint8_t spr_cnt;
        if (fread(&spr_cnt, 1, 1, f) != 1) { fclose(f); return -1; }
        if (spr_cnt > COMPOSE_MAX_SPR) spr_cnt = COMPOSE_MAX_SPR;
        s->sprite_count = spr_cnt;

        for (int j = 0; j < spr_cnt; j++) {
            uint8_t buf[6];
            if (fread(buf, 1, 6, f) != 6) { fclose(f); return -1; }
            ComposeSprite *sp = &s->sprites[j];
            sp->x         = buf[0];
            sp->y         = buf[1];
            sp->tile      = buf[2] | ((uint16_t)buf[3] << 8);
            sp->palette   = buf[4];
            sp->hflip     = (buf[5] & 1) != 0;
            sp->vflip     = (buf[5] & 2) != 0;
            sp->behind_bg = (buf[5] & 4) != 0;
            sp->s16       = (buf[5] & 8) != 0;
        }
    }

    fclose(f);
    return 0;
}
