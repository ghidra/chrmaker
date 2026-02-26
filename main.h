#pragma once
#include <stdbool.h>
#include "chr.h"

typedef enum {
    VIEW_GRAYSCALE,
    VIEW_NES_COLOR
} ViewMode;

typedef enum {
    WRAP_NONE,
    WRAP_H,
    WRAP_V,
    WRAP_BOTH
} WrapMode;

typedef enum { INPUT_SAVE_AS, INPUT_OPEN, INPUT_RESIZE } InputType;

typedef enum {
    SPRITE_8,    /* standard 8×8 tile display                          */
    SPRITE_16    /* 4 sequential tiles shown as 16×16: [0][2] / [1][3] */
} SpriteMode;

typedef struct {
    ChrPage      chr;
    PaletteState pal;

    /* Canvas dimensions (runtime — updated by state_update_dims in main.c) */
    int          chr_cols;          /* tiles per row (default 16)          */
    int          chr_rows;          /* tile rows     (default 16)          */
    int          zoom;              /* screen pixels per NES pixel (1-4)   */
    int          canvas_w;          /* chr_cols * TILE_W * zoom            */
    int          canvas_h;          /* chr_rows * TILE_H * zoom            */
    int          win_w;             /* canvas_w + PANEL_W                  */
    int          win_h;             /* max(canvas_h, PANEL_FULL_H) + STATUS_H */
    bool         want_resize;       /* triggers SDL_SetWindowSize in main  */

    /* Rendering */
    ViewMode     view_mode;
    bool         show_tile_grid;
    bool         show_pixel_grid;

    /* Active drawing colour (0-3, NES bitplane index) */
    int          color;

    /* Palette panel */
    int          active_sub_pal;    /* 0-7 */
    int          active_swatch;     /* 0-3: slot being edited in active sub-pal */

    /* Sprite display mode */
    SpriteMode   sprite_mode;

    /* Tile-wrap painting mode */
    bool         tile_mode;
    int          sel_tile_x;        /* tile col of selection (sprite16: always even) */
    int          sel_tile_y;        /* tile row of selection (sprite16: always even) */
    WrapMode     wrap_mode;

    /* Input */
    bool         mouse_down;
    int          mouse_x, mouse_y;   /* current screen cursor position */

    /* File operations */
    char         current_path[256]; /* active file path for save/load      */
    bool         want_save;         /* save to current_path                */
    bool         want_load;         /* load from current_path              */

    /* Text-input overlay (Save As / Open) */
    bool         input_mode;        /* text-input overlay is open          */
    InputType    input_type;
    char         input_buf[256];    /* text being typed                    */
    int          input_len;         /* strlen of input_buf                 */

    /* Help overlay */
    bool         show_help;

    /* Loop control */
    bool         running;
} EditorState;
