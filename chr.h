#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ── NES tile constants ───────────────────────────────────────── */
#define TILE_W   8          /* pixels per tile, horizontal */
#define TILE_H   8          /* pixels per tile, vertical   */

/* ── Window / panel geometry (fixed) ─────────────────────────── */
#define PANEL_W   128       /* palette panel minimum width (runtime    */
                            /* value lives in EditorState.panel_w)     */
#define STATUS_H   20       /* status bar height    — always 20 px    */

/* canvas_w, canvas_h, win_w, win_h are runtime-computed from
   chr_cols, chr_rows, and zoom, and live in EditorState.        */

/* ── CHR capacity ─────────────────────────────────────────────── */
#define CHR_MAX_TILES    1024  /* max tiles a ChrPage can hold         */
#define CHR_DEFAULT_COLS   16  /* default tiles per row                */
#define CHR_DEFAULT_ROWS   32  /* default tile rows (512 tiles = 8KB NES CHR) */

/* ── Palette capacity ─────────────────────────────────────────────
   CHR editor exposes PAL_COUNT slots. Slots 0-3 are BG / 4-7 SPR
   (the NES hardware constraint used by compose mode).  Slots 8+ are
   an "extended library" usable only in the CHR editor — compose mode
   can't directly reference them, but the user can copy any library
   palette's four colours into an active compose slot (0-7).        */
#define PAL_COUNT   32
#define PAL_VISIBLE  8         /* rows shown at once in the panel */

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
    SubPalette sub[PAL_COUNT];       /* [0-3] BG, [4-7] SPR, [8+] extended library */
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

/* Save/load editor palette state to/from a binary .pal sidecar file.
   Format v2: magic "NPL2" (4B) + count (1B) + reserved (1B) +
              count × SubPalette (4B each) + tile_pal[CHR_MAX_TILES] (1024B).
   Legacy v1 "NPAL" files (8 sub-palettes) are still accepted on load;
   remaining slots are zero-filled.                                  */
int  palette_save(const PaletteState *p, const char *path);
int  palette_load(PaletteState *p, const char *path);
