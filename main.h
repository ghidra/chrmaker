#pragma once
#include <stdbool.h>
#include "chr.h"
#include "compose.h"

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

typedef enum { COMPOSE_BG, COMPOSE_SPR } ComposeLayer;

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
    bool         tile_edit;         /* enlarged tile editor in panel        */
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

    /* Address overlay — show tile index + CHR byte offset in status bar */
    bool         show_addr;

    /* Animation / onion-skin mode */
    AnimState    anim_state;
    int          anim_first;        /* tile index of first frame (stride-aligned) */
    int          anim_last;         /* tile index of last frame  (stride-aligned) */
    int          anim_cur;          /* current frame index (0-based)              */
    int          anim_frame_count;  /* total number of frames                     */
    int          anim_preview_zoom; /* preview scale: 1 or 2 (click preview to toggle) */

    /* Clipboard (tile copy/paste in tile mode) */
    bool         has_clipboard;
    uint8_t      clipboard[4][TILE_H][TILE_W];  /* up to 4 sub-tiles (sprite-16) */
    bool         clipboard_s16;                 /* was copy done in sprite-16 mode? */

    /* Compose mode — NES screen layout editor */
    bool         compose_mode;
    ComposeData  compose;
    ComposeLayer compose_layer;
    int          brush_tile;          /* selected tile from CHR picker       */
    bool         brush_hflip, brush_vflip;  /* sprite-only flip state        */
    bool         brush_s16;           /* place 16×16 sprites (vs 8×8)        */
    int          compose_zoom;        /* 1-3, default 2                      */
    int          compose_canvas_w;    /* 256 * compose_zoom                  */
    int          compose_canvas_h;    /* 240 * compose_zoom                  */
    int          compose_hover_x;     /* tile col under cursor (-1 = none)   */
    int          compose_hover_y;     /* tile row under cursor (-1 = none)   */
    int          compose_spr_sel;     /* selected sprite index, -1 = none    */
    int          compose_spr_drag;    /* sprite being dragged, -1 = none     */
    int          drag_off_x, drag_off_y; /* offset from sprite origin        */
    bool         compose_show_attr_grid; /* attribute grid (16px blocks)     */
    bool         compose_show_help;     /* compose help overlay              */
    bool         want_save_scene;
    bool         want_load_scene;
    char         scene_path[256];

    /* Loop control */
    bool         running;
} EditorState;
