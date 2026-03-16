#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ── NES screen constants ────────────────────────────────────── */
#define COMPOSE_NT_W       32   /* nametable width in tiles  */
#define COMPOSE_NT_H       30   /* nametable height in tiles */
#define COMPOSE_MAX_SPR    64   /* max sprites per scene     */
#define COMPOSE_MAX_SCENES 16

/* ── Sprite entry ────────────────────────────────────────────── */
typedef struct {
    uint8_t  x, y;          /* pixel position (0-255, 0-239)   */
    uint16_t tile;           /* CHR tile index                  */
    uint8_t  palette;        /* 4-7 (sprite palettes)           */
    bool     hflip, vflip;
    bool     behind_bg;
    bool     s16;            /* false=8×8, true=16×16 (4 tiles) */
} ComposeSprite;

/* ── One scene (nametable + attributes + sprites) ────────────── */
typedef struct {
    uint16_t      nametable[COMPOSE_NT_H][COMPOSE_NT_W];  /* tile indices (0-1023) */
    uint8_t       attr[15][16];     /* palette 0-3 per 2x2 tile block    */
    ComposeSprite sprites[COMPOSE_MAX_SPR];
    int           sprite_count;
} ComposeScene;

/* ── Multi-scene container ───────────────────────────────────── */
typedef struct {
    ComposeScene  scenes[COMPOSE_MAX_SCENES];
    int           scene_count;    /* >= 1 */
    int           active_scene;
} ComposeData;

/* ── Functions ───────────────────────────────────────────────── */
void compose_init(ComposeData *d);
int  compose_save(const ComposeData *d, const char *path);
int  compose_load(ComposeData *d, const char *path);
