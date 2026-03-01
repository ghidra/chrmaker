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

typedef enum { INPUT_SAVE_AS, INPUT_OPEN, INPUT_RESIZE, INPUT_OPEN_PAL } InputType;

typedef enum {
    SPRITE_8,    /* standard 8×8 tile display                          */
    SPRITE_16    /* 4 sequential tiles shown as 16×16: [0][2] / [1][3] */
} SpriteMode;

typedef enum {
    ANIM_OFF,
    ANIM_PICKING_FIRST,
    ANIM_PICKING_LAST,
    ANIM_ACTIVE
} AnimState;

typedef struct {
    ChrPage      chr;
    PaletteState pal;

    /* Canvas dimensions (runtime — updated by state_update_dims in main.c) */
    int          chr_cols;          /* tiles per row (default 16)          */
    int          chr_rows;          /* tile rows     (default 16)          */
    int          zoom;              /* screen pixels per NES pixel (1-4)   */
    int          canvas_w;          /* chr_cols * TILE_W * zoom            */
    int          canvas_h;          /* chr_rows * TILE_H * zoom            */
    int          panel_w;           /* panel width; expands with zoom      */
    int          win_w;             /* canvas_w + panel_w                  */
    int          win_h;             /* max(canvas_h, panel_full_h) + STATUS_H */
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
    bool         right_mouse_down;
    int          mouse_x, mouse_y;   /* current screen cursor position */

    /* File operations */
    char         current_path[256]; /* active CHR file path for save/load  */
    bool         want_save;         /* save CHR to current_path            */
    bool         want_load;         /* load CHR from current_path          */
    char         pal_path[256];     /* palette file path for manual load   */
    bool         want_save_pal;     /* save palette (derived from current_path) */
    bool         want_load_pal;     /* load palette from pal_path          */

    /* Text-input overlay (Save As / Open) */
    bool         input_mode;        /* text-input overlay is open          */
    InputType    input_type;
    char         input_buf[256];    /* text being typed                    */
    int          input_len;         /* strlen of input_buf                 */

    /* Help overlay */
    bool         show_help;

    /* Animation / onion-skin mode */
    AnimState    anim_state;
    int          anim_first;        /* tile index of first frame (stride-aligned) */
    int          anim_last;         /* tile index of last frame  (stride-aligned) */
    int          anim_cur;          /* current frame index (0-based)              */
    int          anim_frame_count;  /* total number of frames                     */
    int          anim_preview_zoom; /* preview scale: 1 or 2 (click preview to toggle) */

    /* Loop control */
    bool         running;
} EditorState;
