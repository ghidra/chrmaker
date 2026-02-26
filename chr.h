#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ── NES tile constants ───────────────────────────────────────── */
#define TILE_W   8          /* pixels per tile, horizontal */
#define TILE_H   8          /* pixels per tile, vertical   */

/* ── Window / panel geometry (fixed) ─────────────────────────── */
#define PANEL_W   128       /* palette panel width  — always 128 px   */
#define STATUS_H   20       /* status bar height    — always 20 px    */

/* canvas_w, canvas_h, win_w, win_h are runtime-computed from
   chr_cols, chr_rows, and zoom, and live in EditorState.        */

/* ── CHR capacity ─────────────────────────────────────────────── */
#define CHR_MAX_TILES    1024  /* max tiles a ChrPage can hold         */
#define CHR_DEFAULT_COLS   16  /* default tiles per row                */
#define CHR_DEFAULT_ROWS   16  /* default tile rows                    */

/* ── Data types ───────────────────────────────────────────────── */

/* px[tile][row][col]: 2-bit value 0-3 (NES bitplane index, never RGB). */
typedef struct {
    uint8_t px[CHR_MAX_TILES][TILE_H][TILE_W];
} ChrPage;

/* One NES sub-palette: 4 indices into the 64-colour master palette.
   idx[0] is the background / transparent colour.                  */
typedef struct { uint8_t idx[4]; } SubPalette;

/* All palette editor state — separate from CHR pixel data. */
typedef struct {
    SubPalette sub[8];               /* [0-3] background, [4-7] sprite  */
    uint8_t    tile_pal[CHR_MAX_TILES]; /* sub-palette index per tile   */
} PaletteState;

/* ── Functions ────────────────────────────────────────────────── */

void chr_init(ChrPage *c);
void palette_init(PaletteState *p);

/* Fill with a visible 4-quadrant test pattern (dev/debug use). */
void chr_fill_debug(ChrPage *c);

/* Load raw NES CHR binary from path.
   Returns the number of tiles loaded (>= 1), or -1 on error.
   Loads at most CHR_MAX_TILES tiles; larger files are truncated. */
int  chr_load(ChrPage *c, const char *path);
