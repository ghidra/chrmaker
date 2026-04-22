#include "input.h"
#include "panel.h"
#include "compose.h"
#include "font.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* ── Undo / redo ring buffer ──────────────────────────────────── */

#define UNDO_MAX 64

typedef struct {
    ChrPage      chr;
    PaletteState pal;
    ComposeScene scene;
    int          active_scene;
} UndoEntry;

static UndoEntry undo_buf[UNDO_MAX];
static int undo_head  = 0;   /* next write slot                          */
static int undo_count = 0;   /* valid entries behind head                */
static int undo_redo  = 0;   /* redo entries ahead of current position   */

static void undo_push(const EditorState *s) {
    UndoEntry *e = &undo_buf[undo_head];
    e->chr          = s->chr;
    e->pal          = s->pal;
    e->active_scene = s->compose.active_scene;
    e->scene        = s->compose.scenes[s->compose.active_scene];
    undo_head = (undo_head + 1) % UNDO_MAX;
    if (undo_count < UNDO_MAX) undo_count++;
    undo_redo = 0;   /* new action invalidates redo history */
}

static void undo_pop(EditorState *s) {
    if (undo_count == 0) return;
    /* Save current state for redo before restoring */
    int redo_slot = undo_head;
    UndoEntry *re = &undo_buf[redo_slot];
    re->chr          = s->chr;
    re->pal          = s->pal;
    re->active_scene = s->compose.active_scene;
    re->scene        = s->compose.scenes[s->compose.active_scene];

    undo_head = (undo_head - 1 + UNDO_MAX) % UNDO_MAX;
    undo_count--;
    undo_redo++;

    UndoEntry *e = &undo_buf[undo_head];
    s->chr = e->chr;
    s->pal = e->pal;
    s->compose.scenes[e->active_scene] = e->scene;
}

static void undo_redo_pop(EditorState *s) {
    if (undo_redo == 0) return;
    int redo_slot = (undo_head + 1) % UNDO_MAX;
    UndoEntry *e = &undo_buf[redo_slot];

    /* Push current state so undo still works */
    UndoEntry *cur = &undo_buf[undo_head];
    cur->chr          = s->chr;
    cur->pal          = s->pal;
    cur->active_scene = s->compose.active_scene;
    cur->scene        = s->compose.scenes[s->compose.active_scene];

    s->chr = e->chr;
    s->pal = e->pal;
    s->compose.scenes[e->active_scene] = e->scene;

    undo_head = redo_slot;
    undo_count++;
    undo_redo--;
}

/* ── Helpers ──────────────────────────────────────────────────── */

static int wmod(int v, int n) { return ((v % n) + n) % n; }

/* ── Palette clipboard (assembly-style hex bytes via system clipboard) ── */

static void pal_copy_to_clipboard(const EditorState *s) {
    const SubPalette *sp = &s->pal.sub[s->active_sub_pal];
    char buf[64];
    snprintf(buf, sizeof(buf), ".db $%02X,$%02X,$%02X,$%02X",
             sp->idx[0] & 0x3F, sp->idx[1] & 0x3F,
             sp->idx[2] & 0x3F, sp->idx[3] & 0x3F);
    SDL_SetClipboardText(buf);
}

/* Parse the first 4 NES hex bytes out of an assembly-style string.
   Accepts `$XX`, `0xXX`, or bare `XX` (when preceded by a separator).
   Stops at `;` (comment). Returns true iff exactly 4 bytes were read. */
static bool pal_parse_bytes(const char *txt, uint8_t out[4]) {
    int count = 0;
    const char *p = txt;
    char prev = ' ';
    while (*p && *p != ';' && count < 4) {
        int v = 0, n = 0;
        bool begin = false;

        if (*p == '$') {
            p++; begin = true;
        } else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
                   isxdigit((unsigned char)p[2])) {
            p += 2; begin = true;
        } else if (isxdigit((unsigned char)*p) &&
                   (prev == ' ' || prev == '\t' || prev == ',' ||
                    prev == '\n' || prev == '\r' || prev == '(' ||
                    prev == '{'  || prev == '[')) {
            begin = true;
        }

        if (!begin) { prev = *p; p++; continue; }

        while (n < 2 && isxdigit((unsigned char)*p)) {
            int d = (*p >= '0' && *p <= '9') ? *p - '0'
                  : (*p >= 'a' && *p <= 'f') ? *p - 'a' + 10
                  : *p - 'A' + 10;
            v = v * 16 + d;
            prev = *p;
            p++; n++;
        }
        if (n > 0) out[count++] = (uint8_t)(v & 0x3F);
    }
    return count == 4;
}

/* Returns true and pushes undo if the active palette was updated. */
static bool pal_paste_from_clipboard(EditorState *s) {
    char *text = SDL_GetClipboardText();
    if (!text) return false;
    uint8_t bytes[4];
    bool ok = pal_parse_bytes(text, bytes);
    SDL_free(text);
    if (!ok) return false;
    undo_push(s);
    SubPalette *sp = &s->pal.sub[s->active_sub_pal];
    for (int i = 0; i < 4; i++) sp->idx[i] = bytes[i];
    return true;
}

/* ── File-based clipboard (cross-instance copy/paste) ────────── */

#define CLIPBOARD_PATH "/tmp/chrmaker_clipboard.bin"

static void clipboard_save(const EditorState *s) {
    FILE *f = fopen(CLIPBOARD_PATH, "wb");
    if (!f) return;
    uint8_t flag = s->clipboard_s16 ? 1 : 0;
    fwrite(&flag, 1, 1, f);
    int cnt = s->clipboard_s16 ? 4 : 1;
    for (int p = 0; p < cnt; p++)
        fwrite(s->clipboard[p], 1, TILE_H * TILE_W, f);
    fclose(f);
}

static void clipboard_load(EditorState *s) {
    FILE *f = fopen(CLIPBOARD_PATH, "rb");
    if (!f) return;
    uint8_t flag;
    if (fread(&flag, 1, 1, f) != 1) { fclose(f); return; }
    s->clipboard_s16 = (flag != 0);
    int cnt = s->clipboard_s16 ? 4 : 1;
    for (int p = 0; p < cnt; p++) {
        if (fread(s->clipboard[p], 1, TILE_H * TILE_W, f) != TILE_H * TILE_W) {
            fclose(f); return;
        }
    }
    s->has_clipboard = true;
    fclose(f);
}

static void anim_finish_pick(EditorState *s) {
    int stride = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
    if (s->anim_last < s->anim_first) {
        int tmp = s->anim_first; s->anim_first = s->anim_last; s->anim_last = tmp;
    }
    s->anim_frame_count = (s->anim_last - s->anim_first) / stride + 1;
    s->anim_cur         = 0;
    s->anim_state       = ANIM_ACTIVE;
}

/* ── Focus zoom / pan helpers ─────────────────────────────────── */

#define SB_THICKNESS 8
#define SB_MIN_THUMB 12
#define FOCUS_ZOOM_MAX 32

static inline int fz_scale(const EditorState *s) {
    return s->zoom * s->focus_zoom;
}

static void clamp_pan(EditorState *s) {
    if (s->focus_zoom <= 1) { s->pan_x = 0; s->pan_y = 0; return; }
    int scale = fz_scale(s);
    int content_w = s->chr_cols * TILE_W;
    int content_h = s->chr_rows * TILE_H;
    int vis_w = s->canvas_w / scale;
    int vis_h = s->canvas_h / scale;
    int max_x = content_w - vis_w; if (max_x < 0) max_x = 0;
    int max_y = content_h - vis_h; if (max_y < 0) max_y = 0;
    if (s->pan_x < 0) s->pan_x = 0;
    if (s->pan_y < 0) s->pan_y = 0;
    if (s->pan_x > max_x) s->pan_x = max_x;
    if (s->pan_y > max_y) s->pan_y = max_y;
}

/* Convert canvas screen coords to NES pixel coords (accounts for pan+focus). */
static inline int sx_to_nx(const EditorState *s, int mx) {
    return s->pan_x + mx / fz_scale(s);
}
static inline int sy_to_ny(const EditorState *s, int my) {
    return s->pan_y + my / fz_scale(s);
}

/* Horizontal scrollbar geometry (in screen px, canvas-relative).
   Returns false if scrollbar not visible. */
static bool sb_h_geom(const EditorState *s, int *track_x, int *track_w,
                      int *thumb_x, int *thumb_w) {
    if (s->focus_zoom <= 1) return false;
    int scale = fz_scale(s);
    int content_px = s->chr_cols * TILE_W * scale;
    int visible_px = s->canvas_w;
    if (content_px <= visible_px) return false;
    *track_x = 0;
    *track_w = s->canvas_w - SB_THICKNESS;
    if (*track_w < SB_MIN_THUMB) *track_w = SB_MIN_THUMB;
    int tw = (int)((long long)(*track_w) * visible_px / content_px);
    if (tw < SB_MIN_THUMB) tw = SB_MIN_THUMB;
    if (tw > *track_w)     tw = *track_w;
    int max_pan_nes = s->chr_cols * TILE_W - s->canvas_w / scale;
    int off = (max_pan_nes > 0)
        ? (int)((long long)s->pan_x * (*track_w - tw) / max_pan_nes) : 0;
    *thumb_x = *track_x + off;
    *thumb_w = tw;
    return true;
}

static bool sb_v_geom(const EditorState *s, int *track_y, int *track_h,
                      int *thumb_y, int *thumb_h) {
    if (s->focus_zoom <= 1) return false;
    int scale = fz_scale(s);
    int content_px = s->chr_rows * TILE_H * scale;
    int visible_px = s->canvas_h;
    if (content_px <= visible_px) return false;
    *track_y = 0;
    *track_h = s->canvas_h - SB_THICKNESS;
    if (*track_h < SB_MIN_THUMB) *track_h = SB_MIN_THUMB;
    int th = (int)((long long)(*track_h) * visible_px / content_px);
    if (th < SB_MIN_THUMB) th = SB_MIN_THUMB;
    if (th > *track_h)     th = *track_h;
    int max_pan_nes = s->chr_rows * TILE_H - s->canvas_h / scale;
    int off = (max_pan_nes > 0)
        ? (int)((long long)s->pan_y * (*track_h - th) / max_pan_nes) : 0;
    *thumb_y = *track_y + off;
    *thumb_h = th;
    return true;
}

/* Zoom focus around a pixel-under-cursor. Keeps (mx,my) NES pixel stable. */
static void focus_zoom_at(EditorState *s, int mx, int my, int new_focus) {
    if (new_focus < 1) new_focus = 1;
    if (new_focus > FOCUS_ZOOM_MAX) new_focus = FOCUS_ZOOM_MAX;
    if (new_focus == s->focus_zoom) return;
    /* NES pixel currently under cursor */
    int nx = sx_to_nx(s, mx);
    int ny = sy_to_ny(s, my);
    s->focus_zoom = new_focus;
    /* After zoom change: pan so the same NES pixel lands under cursor again */
    int scale = fz_scale(s);
    s->pan_x = nx - mx / scale;
    s->pan_y = ny - my / scale;
    clamp_pan(s);
}

/* Map screen coords to the tile index under the cursor.
   In sprite-16 mode the tile layout is remapped, so screen position
   does not equal (row*cols + col) — we compute the correct sub-tile. */
static int screen_to_tile(const EditorState *s, int mx, int my) {
    int nx = sx_to_nx(s, mx);
    int ny = sy_to_ny(s, my);
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        int sprite_cols = s->chr_cols / 2;
        int sprite_x    = nx / (TILE_W * 2);
        int sprite_y    = ny / (TILE_H * 2);
        int sub_x       = (nx / TILE_W) % 2;   /* 0=left col, 1=right col */
        int sub_y       = (ny / TILE_H) % 2;   /* 0=top row,  1=bot row   */
        int p           = sub_x * 2 + sub_y;   /* matches render layout   */
        return (sprite_y * sprite_cols + sprite_x) * 4 + p;
    }
    return (ny / TILE_H) * s->chr_cols + (nx / TILE_W);
}

/* Index of the top-left tile of the currently selected tile/sprite. */
static int sel_tile_idx(const EditorState *s) {
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2)
        return ((s->sel_tile_y / 2) * (s->chr_cols / 2) + (s->sel_tile_x / 2)) * 4;
    return s->sel_tile_y * s->chr_cols + s->sel_tile_x;
}

/* ── Palette assignment ───────────────────────────────────────── */

/* Assign the active sub-palette to the tile (or sprite) under (mx, my). */
static void assign_pal_at(EditorState *s, int mx, int my) {
    if (mx < 0 || mx >= s->canvas_w || my < 0 || my >= s->canvas_h) return;
    int t = screen_to_tile(s, mx, my);
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        int base = (t / 4) * 4;
        for (int p = 0; p < 4; p++)
            s->pal.tile_pal[base + p] = (uint8_t)s->active_sub_pal;
    } else {
        s->pal.tile_pal[t] = (uint8_t)s->active_sub_pal;
    }
}

/* ── Text-input overlay ───────────────────────────────────────── */

static void input_begin(EditorState *s, InputType type) {
    s->input_mode = true;
    s->input_type = type;
    s->input_len  = 0;
    s->input_buf[0] = '\0';

    if (type == INPUT_SAVE_AS) {
        snprintf(s->input_buf, sizeof(s->input_buf), "%s", s->current_path);
        s->input_len = (int)strlen(s->input_buf);
    } else if (type == INPUT_RESIZE) {
        snprintf(s->input_buf, sizeof(s->input_buf), "%dx%d",
                 s->chr_cols, s->chr_rows);
        s->input_len = (int)strlen(s->input_buf);
    } else if (type == INPUT_OPEN_PAL) {
        snprintf(s->input_buf, sizeof(s->input_buf), "%s", s->pal_path);
        s->input_len = (int)strlen(s->input_buf);
    }

    SDL_StartTextInput();
}

static void input_confirm(EditorState *s) {
    if (s->input_len == 0) { s->input_mode = false; SDL_StopTextInput(); return; }

    if (s->input_type == INPUT_RESIZE) {
        int cols = 0, rows = 0;
        if (sscanf(s->input_buf, "%dx%d", &cols, &rows) == 2
            && cols >= 1 && cols <= 128
            && rows >= 1 && rows <=  64) {
            s->chr_cols  = cols;
            s->chr_rows  = rows;
            s->want_resize = true;
        }
        /* silently discard invalid input */
    } else if (s->input_type == INPUT_OPEN_PAL) {
        snprintf(s->pal_path, sizeof(s->pal_path), "%s", s->input_buf);
        s->want_load_pal = true;
    } else {
        snprintf(s->current_path, sizeof(s->current_path), "%s", s->input_buf);
        if (s->input_type == INPUT_SAVE_AS) s->want_save = true;
        else                                s->want_load = true;
    }

    s->input_mode = false;
    SDL_StopTextInput();
}

static void input_cancel(EditorState *s) {
    s->input_mode = false;
    SDL_StopTextInput();
}

/* ── Tile edit paint (enlarged tile in panel) ─────────────────── */

static void tile_edit_paint(EditorState *s, int px, int py) {
    bool s16 = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
    int edit_dim = s16 ? 16 : 8;
    int pixel_sz = (s->panel_w - 2 * PANEL_EDIT_MARGIN) / edit_dim;
    int edit_sz  = pixel_sz * edit_dim;
    int edit_x0  = (s->panel_w - edit_sz) / 2;
    int edit_y0  = PANEL_EDIT_MARGIN + 20;

    int ox = px - edit_x0;
    int oy = py - edit_y0;

    bool wx = (s->wrap_mode == WRAP_H || s->wrap_mode == WRAP_BOTH);
    bool wy = (s->wrap_mode == WRAP_V || s->wrap_mode == WRAP_BOTH);

    if (wx) ox = wmod(ox, edit_sz); else if (ox < 0 || ox >= edit_sz) return;
    if (wy) oy = wmod(oy, edit_sz); else if (oy < 0 || oy >= edit_sz) return;

    int lx = ox / pixel_sz;
    int ly = oy / pixel_sz;

    if (s16) {
        int sub_x = lx / TILE_W;
        int sub_y = ly / TILE_H;
        int p     = sub_x * 2 + sub_y;
        int tile  = sel_tile_idx(s) + p;
        s->chr.px[tile][ly % TILE_H][lx % TILE_W] = (uint8_t)s->color;
    } else {
        int tile = sel_tile_idx(s);
        s->chr.px[tile][ly][lx] = (uint8_t)s->color;
    }
}

/* ── Paint ────────────────────────────────────────────────────── */

static void paint_at(EditorState *s, int mx, int my) {
    if (mx < 0 || mx >= s->canvas_w || my < 0 || my >= s->canvas_h) return;

    int px_x = sx_to_nx(s, mx);
    int px_y = sy_to_ny(s, my);
    int tile, local_x, local_y;

    if (s->tile_mode) {
        int ox = px_x - s->sel_tile_x * TILE_W;
        int oy = px_y - s->sel_tile_y * TILE_H;
        bool wx = (s->wrap_mode == WRAP_H || s->wrap_mode == WRAP_BOTH);
        bool wy = (s->wrap_mode == WRAP_V || s->wrap_mode == WRAP_BOTH);

        if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
            /* Loop over the full 16×16 sprite area, then map to sub-tile. */
            const int SPR_W = TILE_W * 2, SPR_H = TILE_H * 2;
            int lox, loy;
            if (wx)                            lox = wmod(ox, SPR_W);
            else if (ox >= 0 && ox < SPR_W)   lox = ox;
            else                               return;
            if (wy)                            loy = wmod(oy, SPR_H);
            else if (oy >= 0 && oy < SPR_H)   loy = oy;
            else                               return;
            int sub_x = lox / TILE_W;         /* 0=left col, 1=right col */
            int sub_y = loy / TILE_H;         /* 0=top row,  1=bot row   */
            int p     = sub_x * 2 + sub_y;
            tile    = sel_tile_idx(s) + p;
            local_x = lox % TILE_W;
            local_y = loy % TILE_H;
        } else {
            if (wx)                          local_x = wmod(ox, TILE_W);
            else if (ox >= 0 && ox < TILE_W) local_x = ox;
            else                             return;
            if (wy)                          local_y = wmod(oy, TILE_H);
            else if (oy >= 0 && oy < TILE_H) local_y = oy;
            else                             return;
            tile = sel_tile_idx(s);
        }
    } else {
        tile    = screen_to_tile(s, mx, my);
        local_x = px_x % TILE_W;
        local_y = px_y % TILE_H;
    }

    s->chr.px[tile][local_y][local_x] = (uint8_t)s->color;
}

/* ── Tile selection ───────────────────────────────────────────── */

static void select_tile_under_cursor(EditorState *s) {
    int mx = s->mouse_x, my = s->mouse_y;
    if (mx < 0 || mx >= s->canvas_w || my < 0 || my >= s->canvas_h) return;
    int nx = sx_to_nx(s, mx);
    int ny = sy_to_ny(s, my);
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        /* Snap to sprite (2-tile) boundary so sel_tile_x/y are always even. */
        s->sel_tile_x = (nx / (TILE_W * 2)) * 2;
        s->sel_tile_y = (ny / (TILE_H * 2)) * 2;
    } else {
        s->sel_tile_x = nx / TILE_W;
        s->sel_tile_y = ny / TILE_H;
    }
    s->tile_mode = true;
}

/* ── Panel click ──────────────────────────────────────────────── */

static void panel_click(EditorState *s, int px, int py) {
    /* Runtime NES picker geometry — must match render_panel. */
    int nes_cell      = s->zoom * PANEL_NES_CELL_BASE;
    int nes_step      = nes_cell + PANEL_NES_GAP;
    int nes_x0        = (s->panel_w - PANEL_NES_COLS * nes_step + PANEL_NES_GAP) / 2;
    int panel_view_y0 = PANEL_NES_Y0 + PANEL_NES_ROWS * nes_step + 10;

    if (py >= PANEL_PAL_Y0 && py < PANEL_PAL_Y0 + PAL_VISIBLE * PANEL_PAL_ROW) {
        int row = (py - PANEL_PAL_Y0) / PANEL_PAL_ROW;
        if (row < 0 || row >= PAL_VISIBLE) return;
        int pal_idx = s->palette_scroll + row;
        if (pal_idx < 0 || pal_idx >= PAL_COUNT) return;
        s->active_sub_pal = pal_idx;
        if (s->tile_mode) {
            int base = sel_tile_idx(s);
            int cnt  = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
            for (int p = 0; p < cnt; p++)
                s->pal.tile_pal[base + p] = (uint8_t)pal_idx;
        }
        return;
    }

    if (py >= PANEL_ACT_Y0 && py < PANEL_ACT_Y0 + PANEL_ACT_SH) {
        int col = (px - PANEL_PAL_X0) / (PANEL_ACT_SW + PANEL_ACT_XGAP);
        if (col >= 0 && col < 4) s->active_swatch = col;
        return;
    }

    if (py >= PANEL_NES_Y0 && py < PANEL_NES_Y0 + PANEL_NES_ROWS * nes_step &&
        px >= nes_x0 && px < nes_x0 + PANEL_NES_COLS * nes_step) {
        int col = (px - nes_x0) / nes_step;
        int row = (py - PANEL_NES_Y0) / nes_step;
        if (col >= 0 && col < PANEL_NES_COLS && row >= 0 && row < PANEL_NES_ROWS)
            s->pal.sub[s->active_sub_pal].idx[s->active_swatch] =
                (uint8_t)(row * PANEL_NES_COLS + col);
        return;
    }

    if (py >= panel_view_y0 && py < panel_view_y0 + 16 &&
        px >= 22 && px < 34) {
        s->show_help = !s->show_help;
        s->help_scroll = 0;
    }

    /* Animation section hit-tests (preview + scrubber) */
    {
        bool s16        = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
        int  nes_sz     = s16 ? 16 : 8;
        int  pz         = (s->anim_preview_zoom >= 2) ? 2 : 1;
        int  preview_sz = nes_sz * 4 * pz;
        int  max_prev   = s->panel_w - 2 * PANEL_PAL_X0;
        if (preview_sz > max_prev) preview_sz = max_prev;

        int anim_y0   = panel_view_y0 + 44 + 8;
        int preview_y = anim_y0 + 12 + 4;
        int preview_x = (s->panel_w - preview_sz) / 2;  /* panel-relative */

        /* Click preview → toggle zoom (×1 ↔ ×2) */
        if (s->anim_state != ANIM_OFF &&
            py >= preview_y && py < preview_y + preview_sz &&
            px >= preview_x && px < preview_x + preview_sz) {
            s->anim_preview_zoom = (s->anim_preview_zoom == 1) ? 2 : 1;
            s->want_resize = true;
            return;
        }

        /* Scrubber hit-test */
        int scrub_y0 = preview_y + preview_sz + 6 + 14 + 6;
        int scrub_x0 = PANEL_PAL_X0;
        int scrub_w  = s->panel_w - 2 * PANEL_PAL_X0;
        if (s->anim_state == ANIM_ACTIVE &&
            py >= scrub_y0 && py < scrub_y0 + PANEL_ANIM_SCRUB_H &&
            px >= scrub_x0 && px < scrub_x0 + scrub_w) {
            int frame = (px - scrub_x0) * s->anim_frame_count / scrub_w;
            if (frame < 0) frame = 0;
            if (frame >= s->anim_frame_count) frame = s->anim_frame_count - 1;
            s->anim_cur = frame;
        }
    }
}

/* ── Compose mode: get active scene ───────────────────────────── */
static ComposeScene *active_scene(EditorState *s) {
    return &s->compose.scenes[s->compose.active_scene];
}

/* ── Compose mode: focus zoom / pan helpers ───────────────────── */
static inline int cmp_fz_scale(const EditorState *s) {
    return s->compose_zoom * s->focus_zoom;
}

static void cmp_clamp_pan(EditorState *s) {
    if (s->focus_zoom <= 1) { s->pan_x = 0; s->pan_y = 0; return; }
    int scale = cmp_fz_scale(s);
    int vis_w = s->compose_canvas_w / scale;
    int vis_h = s->compose_canvas_h / scale;
    int max_x = 256 - vis_w; if (max_x < 0) max_x = 0;
    int max_y = 240 - vis_h; if (max_y < 0) max_y = 0;
    if (s->pan_x < 0) s->pan_x = 0;
    if (s->pan_y < 0) s->pan_y = 0;
    if (s->pan_x > max_x) s->pan_x = max_x;
    if (s->pan_y > max_y) s->pan_y = max_y;
}

static inline int cmp_sx_to_nx(const EditorState *s, int mx) {
    return s->pan_x + mx / cmp_fz_scale(s);
}
static inline int cmp_sy_to_ny(const EditorState *s, int my) {
    return s->pan_y + my / cmp_fz_scale(s);
}

static bool cmp_sb_h_geom(const EditorState *s, int *track_x, int *track_w,
                          int *thumb_x, int *thumb_w) {
    if (s->focus_zoom <= 1) return false;
    int scale = cmp_fz_scale(s);
    int content_px = 256 * scale;
    int visible_px = s->compose_canvas_w;
    if (content_px <= visible_px) return false;
    *track_x = 0;
    *track_w = s->compose_canvas_w - SB_THICKNESS;
    if (*track_w < SB_MIN_THUMB) *track_w = SB_MIN_THUMB;
    int tw = (int)((long long)(*track_w) * visible_px / content_px);
    if (tw < SB_MIN_THUMB) tw = SB_MIN_THUMB;
    if (tw > *track_w)     tw = *track_w;
    int max_pan_nes = 256 - s->compose_canvas_w / scale;
    int off = (max_pan_nes > 0)
        ? (int)((long long)s->pan_x * (*track_w - tw) / max_pan_nes) : 0;
    *thumb_x = *track_x + off;
    *thumb_w = tw;
    return true;
}

static bool cmp_sb_v_geom(const EditorState *s, int *track_y, int *track_h,
                          int *thumb_y, int *thumb_h) {
    if (s->focus_zoom <= 1) return false;
    int scale = cmp_fz_scale(s);
    int content_px = 240 * scale;
    int visible_px = s->compose_canvas_h;
    if (content_px <= visible_px) return false;
    *track_y = 0;
    *track_h = s->compose_canvas_h - SB_THICKNESS;
    if (*track_h < SB_MIN_THUMB) *track_h = SB_MIN_THUMB;
    int th = (int)((long long)(*track_h) * visible_px / content_px);
    if (th < SB_MIN_THUMB) th = SB_MIN_THUMB;
    if (th > *track_h)     th = *track_h;
    int max_pan_nes = 240 - s->compose_canvas_h / scale;
    int off = (max_pan_nes > 0)
        ? (int)((long long)s->pan_y * (*track_h - th) / max_pan_nes) : 0;
    *thumb_y = *track_y + off;
    *thumb_h = th;
    return true;
}

static void cmp_focus_zoom_at(EditorState *s, int mx, int my, int new_focus) {
    if (new_focus < 1) new_focus = 1;
    if (new_focus > FOCUS_ZOOM_MAX) new_focus = FOCUS_ZOOM_MAX;
    if (new_focus == s->focus_zoom) return;
    int nx = cmp_sx_to_nx(s, mx);
    int ny = cmp_sy_to_ny(s, my);
    s->focus_zoom = new_focus;
    int scale = cmp_fz_scale(s);
    s->pan_x = nx - mx / scale;
    s->pan_y = ny - my / scale;
    cmp_clamp_pan(s);
}

/* ── Compose mode: panel click ───────────────────────────────── */
static void compose_panel_click(EditorState *s, int px, int py) {
    /* Two-column layout: picker on left, controls on right.
       All coords are panel-relative (px=0 at panel left edge). */
    int pscale    = COMPOSE_PICKER_SCALE;
    int picker_y0 = 8;
    int picker_x0 = PANEL_PAL_X0;
    int picker_w  = s->chr_cols * TILE_W * pscale;
    int picker_h  = s->chr_rows * TILE_H * pscale;

    /* Right column starts after picker + gap */
    int ctrl_x = picker_x0 + picker_w + 8;

    /* ── Left column: CHR picker ── */
    if (py >= picker_y0 && py < picker_y0 + picker_h &&
        px >= picker_x0 && px < picker_x0 + picker_w) {
        int tx = (px - picker_x0) / (TILE_W * pscale);
        int ty = (py - picker_y0) / (TILE_H * pscale);
        s->brush_tile = ty * s->chr_cols + tx;
        return;
    }

    /* ── Right column: controls (must match render layout) ── */
    /* Only respond if click is in the right column area */
    if (px < ctrl_x) return;

    /* Control y positions mirror render_compose_panel right column:
       y starts at picker_y0 (=8), then:
       brush label (font_line_h=18), preview (32), gap (8),
       pal label (18), 8 rows × 14, gap (8),
       layer (font_line_h + 4), ... */
    int y = picker_y0;
    y += 18;      /* "BRUSH" label */
    y += 32 + 8;  /* preview + gap */

    /* Palette rows (scrollable) */
    int pal_label_y = y;
    y += 18;  /* "PAL" label */
    int pal_y0 = y;
    int pal_row_h = 14;
    if (py >= pal_y0 && py < pal_y0 + PAL_VISIBLE * pal_row_h) {
        int row = (py - pal_y0) / pal_row_h;
        if (row < 0 || row >= PAL_VISIBLE) return;
        int pal_idx = s->palette_scroll + row;
        if (pal_idx < 0 || pal_idx >= PAL_COUNT) return;
        if (pal_idx < 8) {
            /* Regular slot — select, respecting layer constraint. */
            if (s->compose_layer == COMPOSE_BG && pal_idx < 4)
                s->active_sub_pal = pal_idx;
            else if (s->compose_layer == COMPOSE_SPR && pal_idx >= 4)
                s->active_sub_pal = pal_idx;
            /* wrong-layer click: ignore silently */
        } else {
            /* Library palette — copy its colors into the active compose slot. */
            int dst = s->active_sub_pal & 7;
            undo_push(s);
            s->pal.sub[dst] = s->pal.sub[pal_idx];
        }
        return;
    }
    (void)pal_label_y;
    y += PAL_VISIBLE * pal_row_h + 8;

    /* Layer toggle */
    if (py >= y && py < y + 18) {
        s->compose_layer = (s->compose_layer == COMPOSE_BG) ? COMPOSE_SPR : COMPOSE_BG;
        return;
    }
}

/* ── Compose mode: canvas click ──────────────────────────────── */
static void compose_canvas_click(EditorState *s, int mx, int my, bool left, bool shift) {
    int nx = cmp_sx_to_nx(s, mx);
    int ny = cmp_sy_to_ny(s, my);
    int tile_x = nx / TILE_W;
    int tile_y = ny / TILE_H;
    if (tile_x < 0 || tile_x >= COMPOSE_NT_W || tile_y < 0 || tile_y >= COMPOSE_NT_H) return;

    ComposeScene *sc = active_scene(s);

    if (s->compose_layer == COMPOSE_BG) {
        if (left) {
            if (shift) {
                /* Erase: set tile to 0 */
                sc->nametable[tile_y][tile_x] = 0;
            } else {
                /* Place tile */
                sc->nametable[tile_y][tile_x] = (uint16_t)s->brush_tile;
                /* Auto-set attribute for this 2x2 block */
                int ax = tile_x / 2;
                int ay = tile_y / 2;
                if (ay < 15 && ax < 16)
                    sc->attr[ay][ax] = (uint8_t)(s->active_sub_pal & 3);
            }
        } else {
            /* Right-click: eyedropper — pick tile + palette */
            s->brush_tile = sc->nametable[tile_y][tile_x];
            int ax = tile_x / 2;
            int ay = tile_y / 2;
            if (ay < 15 && ax < 16)
                s->active_sub_pal = sc->attr[ay][ax] & 3;
        }
    } else {
        /* Sprite layer */
        int px_x = nx;
        int px_y = ny;

        if (left) {
            /* Check if clicking on existing sprite */
            for (int i = sc->sprite_count - 1; i >= 0; i--) {
                ComposeSprite *sp = &sc->sprites[i];
                int spr_w = sp->s16 ? 16 : 8;
                int spr_h = sp->s16 ? 16 : 8;
                if (px_x >= sp->x && px_x < sp->x + spr_w &&
                    px_y >= sp->y && px_y < sp->y + spr_h) {
                    s->compose_spr_sel  = i;
                    s->compose_spr_drag = i;
                    s->drag_off_x = px_x - sp->x;
                    s->drag_off_y = px_y - sp->y;
                    return;
                }
            }
            /* Place new sprite */
            if (sc->sprite_count < COMPOSE_MAX_SPR) {
                int idx = sc->sprite_count++;
                ComposeSprite *sp = &sc->sprites[idx];
                sp->x       = (uint8_t)(px_x < 255 ? px_x : 255);
                sp->y       = (uint8_t)(px_y < 239 ? px_y : 239);
                sp->tile    = (uint16_t)s->brush_tile;
                sp->palette = (uint8_t)(s->active_sub_pal & 7);
                sp->hflip   = s->brush_hflip;
                sp->vflip   = s->brush_vflip;
                sp->behind_bg = false;
                sp->s16     = s->brush_s16;
                s->compose_spr_sel = idx;
            }
        } else {
            /* Right-click: delete sprite under cursor */
            for (int i = sc->sprite_count - 1; i >= 0; i--) {
                ComposeSprite *sp = &sc->sprites[i];
                int spr_w = sp->s16 ? 16 : 8;
                int spr_h = sp->s16 ? 16 : 8;
                if (px_x >= sp->x && px_x < sp->x + spr_w &&
                    px_y >= sp->y && px_y < sp->y + spr_h) {
                    /* Remove by shifting */
                    for (int j = i; j < sc->sprite_count - 1; j++)
                        sc->sprites[j] = sc->sprites[j + 1];
                    sc->sprite_count--;
                    if (s->compose_spr_sel == i) s->compose_spr_sel = -1;
                    else if (s->compose_spr_sel > i) s->compose_spr_sel--;
                    break;
                }
            }
        }
    }
}

/* ── Compose mode: full input handler ────────────────────────── */
static void compose_input(const SDL_Event *e, EditorState *s) {
    int cw = s->compose_canvas_w;
    int ch = s->compose_canvas_h;

    switch (e->type) {
        case SDL_QUIT: s->running = false; break;

        case SDL_KEYDOWN:
            switch (e->key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (s->compose_show_help)
                        s->compose_show_help = false;
                    else if (s->compose_spr_sel >= 0)
                        s->compose_spr_sel = -1;
                    else {
                        s->compose_mode = false;
                        s->focus_zoom   = 1;
                        s->pan_x = s->pan_y = 0;
                        s->want_resize  = true;
                    }
                    break;
                case SDLK_TAB:
                    s->compose_mode = false;
                    s->focus_zoom   = 1;
                    s->pan_x = s->pan_y = 0;
                    s->want_resize  = true;
                    break;
                case SDLK_F1:
                    s->compose_show_help = !s->compose_show_help;
                    s->help_scroll = 0;
                    break;
                case SDLK_SLASH:
                    if (e->key.keysym.mod & KMOD_SHIFT) {
                        s->compose_show_help = !s->compose_show_help;
                        s->help_scroll = 0;
                    }
                    break;
                case SDLK_b: s->compose_layer = COMPOSE_BG;  break;
                case SDLK_l: s->compose_layer = COMPOSE_SPR; break;
                case SDLK_h:
                    if (s->compose_layer == COMPOSE_SPR)
                        s->brush_hflip = !s->brush_hflip;
                    break;
                case SDLK_f:
                    if (s->compose_layer == COMPOSE_SPR)
                        s->brush_vflip = !s->brush_vflip;
                    break;
                case SDLK_m:
                    s->brush_s16 = !s->brush_s16;
                    break;
                case SDLK_g:
                    s->compose_show_attr_grid = !s->compose_show_attr_grid;
                    break;
                case SDLK_LEFTBRACKET: {
                    if (s->compose_layer == COMPOSE_BG)
                        s->active_sub_pal = (s->active_sub_pal + 3) % 4;
                    else
                        s->active_sub_pal = 4 + (s->active_sub_pal + 3) % 4;
                    break;
                }
                case SDLK_RIGHTBRACKET: {
                    if (s->compose_layer == COMPOSE_BG)
                        s->active_sub_pal = (s->active_sub_pal + 1) % 4;
                    else
                        s->active_sub_pal = 4 + (s->active_sub_pal + 1) % 4;
                    break;
                }
                case SDLK_EQUALS:
                    if (s->compose_zoom < 4) { s->compose_zoom++; s->want_resize = true; }
                    break;
                case SDLK_MINUS:
                    if (s->compose_zoom > 1) { s->compose_zoom--; s->want_resize = true; }
                    break;
                case SDLK_SPACE:
                    if (!e->key.repeat) s->space_held = true;
                    break;
                case SDLK_0:
                    /* Reset focus zoom/pan. (F is vflip in sprite layer, so '0' here.) */
                    s->focus_zoom = 1;
                    s->pan_x = 0;
                    s->pan_y = 0;
                    break;
                case SDLK_PAGEUP:
                    if (s->compose.active_scene > 0) s->compose.active_scene--;
                    break;
                case SDLK_PAGEDOWN:
                    if (s->compose.active_scene < s->compose.scene_count - 1)
                        s->compose.active_scene++;
                    break;
                case SDLK_n:
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        if (s->compose.scene_count < COMPOSE_MAX_SCENES) {
                            int idx = s->compose.scene_count++;
                            memset(&s->compose.scenes[idx], 0, sizeof(ComposeScene));
                            s->compose.active_scene = idx;
                        }
                    }
                    break;
                case SDLK_z:
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        if (e->key.keysym.mod & KMOD_SHIFT)
                            undo_redo_pop(s);
                        else
                            undo_pop(s);
                    }
                    break;
                case SDLK_y:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        undo_redo_pop(s);
                    break;
                case SDLK_c:
                    if ((e->key.keysym.mod & KMOD_CTRL) &&
                        (e->key.keysym.mod & KMOD_SHIFT))
                        pal_copy_to_clipboard(s);
                    break;
                case SDLK_v:
                    if ((e->key.keysym.mod & KMOD_CTRL) &&
                        (e->key.keysym.mod & KMOD_SHIFT))
                        pal_paste_from_clipboard(s);
                    break;
                case SDLK_s:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        s->want_save_scene = true;
                    break;
                case SDLK_DELETE:
                    if (s->compose_spr_sel >= 0) {
                        undo_push(s);
                        ComposeScene *sc = active_scene(s);
                        int i = s->compose_spr_sel;
                        for (int j = i; j < sc->sprite_count - 1; j++)
                            sc->sprites[j] = sc->sprites[j + 1];
                        sc->sprite_count--;
                        s->compose_spr_sel = -1;
                    }
                    break;
                case SDLK_UP:
                    if (s->compose_spr_sel >= 0) {
                        ComposeSprite *sp = &active_scene(s)->sprites[s->compose_spr_sel];
                        if (sp->y > 0) sp->y--;
                    }
                    break;
                case SDLK_DOWN:
                    if (s->compose_spr_sel >= 0) {
                        ComposeSprite *sp = &active_scene(s)->sprites[s->compose_spr_sel];
                        if (sp->y < 239) sp->y++;
                    }
                    break;
                case SDLK_LEFT:
                    if (s->compose_spr_sel >= 0) {
                        ComposeSprite *sp = &active_scene(s)->sprites[s->compose_spr_sel];
                        if (sp->x > 0) sp->x--;
                    }
                    break;
                case SDLK_RIGHT:
                    if (s->compose_spr_sel >= 0) {
                        ComposeSprite *sp = &active_scene(s)->sprites[s->compose_spr_sel];
                        if (sp->x < 255) sp->x++;
                    }
                    break;
                default: break;
            }
            break;

        case SDL_KEYUP:
            if (e->key.keysym.sym == SDLK_SPACE) s->space_held = false;
            break;

        case SDL_MOUSEBUTTONDOWN: {
            int mx = e->button.x, my = e->button.y;
            s->mouse_x = mx; s->mouse_y = my;
            bool left  = (e->button.button == SDL_BUTTON_LEFT);
            bool right = (e->button.button == SDL_BUTTON_RIGHT);
            bool mid   = (e->button.button == SDL_BUTTON_MIDDLE);
            bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

            /* Middle button → pan canvas */
            if (mid && mx < cw && my < ch) {
                s->panning = true;
                s->pan_anchor_mx = mx; s->pan_anchor_my = my;
                s->pan_anchor_px = s->pan_x; s->pan_anchor_py = s->pan_y;
                break;
            }

            if (left) {
                /* Space + LMB → pan */
                if (s->space_held && mx < cw && my < ch) {
                    s->panning = true;
                    s->pan_anchor_mx = mx; s->pan_anchor_my = my;
                    s->pan_anchor_px = s->pan_x; s->pan_anchor_py = s->pan_y;
                    break;
                }
                /* Scrollbar hit-test */
                if (s->focus_zoom > 1 && mx < cw && my < ch) {
                    int tx, tw, thx, thw;
                    int ty, th, thy, thh;
                    bool hh = cmp_sb_h_geom(s, &tx, &tw, &thx, &thw);
                    bool vv = cmp_sb_v_geom(s, &ty, &th, &thy, &thh);
                    if (hh && my >= ch - SB_THICKNESS && mx < cw - SB_THICKNESS) {
                        if (!(mx >= thx && mx < thx + thw)) {
                            int max_pan_nes = 256 - cw / cmp_fz_scale(s);
                            if (tw > thw && max_pan_nes > 0)
                                s->pan_x = (int)((long long)(mx - tx - thw/2) * max_pan_nes / (tw - thw));
                            cmp_clamp_pan(s);
                        }
                        s->sb_drag = 1;
                        s->sb_drag_anchor = mx;
                        s->sb_drag_pan_start = s->pan_x;
                        break;
                    }
                    if (vv && mx >= cw - SB_THICKNESS && my < ch - SB_THICKNESS) {
                        if (!(my >= thy && my < thy + thh)) {
                            int max_pan_nes = 240 - ch / cmp_fz_scale(s);
                            if (th > thh && max_pan_nes > 0)
                                s->pan_y = (int)((long long)(my - ty - thh/2) * max_pan_nes / (th - thh));
                            cmp_clamp_pan(s);
                        }
                        s->sb_drag = 2;
                        s->sb_drag_anchor = my;
                        s->sb_drag_pan_start = s->pan_y;
                        break;
                    }
                }
            }

            if (left) s->mouse_down = true;
            if (right) s->right_mouse_down = true;

            if (mx < cw && my < ch) {
                if (left || right) {
                    undo_push(s);
                    compose_canvas_click(s, mx, my, left, shift);
                }
            } else if (mx >= cw) {
                if (left)
                    compose_panel_click(s, mx - cw, my);
            }
            break;
        }

        case SDL_MOUSEBUTTONUP:
            if (e->button.button == SDL_BUTTON_MIDDLE) s->panning = false;
            if (e->button.button == SDL_BUTTON_LEFT) {
                s->mouse_down = false;
                s->compose_spr_drag = -1;
                s->panning = false;
                s->sb_drag = 0;
            }
            if (e->button.button == SDL_BUTTON_RIGHT)
                s->right_mouse_down = false;
            break;

        case SDL_MOUSEMOTION: {
            int mx = e->motion.x, my = e->motion.y;
            s->mouse_x = mx; s->mouse_y = my;

            if (s->panning) {
                int scale = cmp_fz_scale(s);
                s->pan_x = s->pan_anchor_px - (mx - s->pan_anchor_mx) / scale;
                s->pan_y = s->pan_anchor_py - (my - s->pan_anchor_my) / scale;
                cmp_clamp_pan(s);
                break;
            }
            if (s->sb_drag == 1) {
                int tx, tw, thx, thw;
                if (cmp_sb_h_geom(s, &tx, &tw, &thx, &thw)) {
                    int max_pan_nes = 256 - cw / cmp_fz_scale(s);
                    if (tw > thw && max_pan_nes > 0) {
                        int d = mx - s->sb_drag_anchor;
                        s->pan_x = s->sb_drag_pan_start
                            + (int)((long long)d * max_pan_nes / (tw - thw));
                        cmp_clamp_pan(s);
                    }
                }
                break;
            }
            if (s->sb_drag == 2) {
                int ty, th, thy, thh;
                if (cmp_sb_v_geom(s, &ty, &th, &thy, &thh)) {
                    int max_pan_nes = 240 - ch / cmp_fz_scale(s);
                    if (th > thh && max_pan_nes > 0) {
                        int d = my - s->sb_drag_anchor;
                        s->pan_y = s->sb_drag_pan_start
                            + (int)((long long)d * max_pan_nes / (th - thh));
                        cmp_clamp_pan(s);
                    }
                }
                break;
            }

            /* Update hover position (pan-aware) */
            if (mx < cw && my < ch) {
                s->compose_hover_x = cmp_sx_to_nx(s, mx) / TILE_W;
                s->compose_hover_y = cmp_sy_to_ny(s, my) / TILE_H;
            } else {
                s->compose_hover_x = -1;
                s->compose_hover_y = -1;
            }

            /* Sprite dragging */
            if (s->compose_spr_drag >= 0 && s->mouse_down) {
                int px_x = cmp_sx_to_nx(s, mx) - s->drag_off_x;
                int px_y = cmp_sy_to_ny(s, my) - s->drag_off_y;
                if (px_x < 0)   px_x = 0;
                if (px_x > 255) px_x = 255;
                if (px_y < 0)   px_y = 0;
                if (px_y > 239) px_y = 239;
                ComposeSprite *sp = &active_scene(s)->sprites[s->compose_spr_drag];
                sp->x = (uint8_t)px_x;
                sp->y = (uint8_t)px_y;
            }
            /* BG tile painting while dragging */
            else if (s->mouse_down && s->compose_layer == COMPOSE_BG &&
                     mx < cw && my < ch) {
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                compose_canvas_click(s, mx, my, true, shift);
            }
            break;
        }

        case SDL_MOUSEWHEEL:
            if (s->compose_show_help) {
                s->help_scroll -= e->wheel.y * font_line_h() * 2;
                if (s->help_scroll < 0) s->help_scroll = 0;
            } else if (s->mouse_x < cw) {
                /* Wheel over canvas → focus zoom at cursor */
                cmp_focus_zoom_at(s, s->mouse_x, s->mouse_y,
                                  s->focus_zoom + e->wheel.y);
            } else if (s->mouse_x >= cw) {
                /* Wheel over compose panel → scroll the palette list. */
                s->palette_scroll -= e->wheel.y;
                int maxs = PAL_COUNT - PAL_VISIBLE;
                if (s->palette_scroll < 0)    s->palette_scroll = 0;
                if (s->palette_scroll > maxs) s->palette_scroll = maxs;
            }
            break;

        default: break;
    }
}

/* ── Event dispatch ───────────────────────────────────────────── */

void input_handle(const SDL_Event *e, EditorState *s) {

    if (s->input_mode) {
        switch (e->type) {
            case SDL_TEXTINPUT: {
                const char *t = e->text.text;
                int add = (int)strlen(t);
                if (s->input_len + add < (int)sizeof(s->input_buf) - 1) {
                    strcat(s->input_buf, t);
                    s->input_len += add;
                }
                break;
            }
            case SDL_KEYDOWN:
                switch (e->key.keysym.sym) {
                    case SDLK_RETURN:  case SDLK_KP_ENTER: input_confirm(s); break;
                    case SDLK_ESCAPE:                       input_cancel(s);  break;
                    case SDLK_BACKSPACE:
                        if (s->input_len > 0)
                            s->input_buf[--s->input_len] = '\0';
                        break;
                    default: break;
                }
                break;
            case SDL_QUIT: s->running = false; break;
            default: break;
        }
        return;
    }

    /* Compose mode gets its own full input handler */
    if (s->compose_mode) {
        compose_input(e, s);
        return;
    }

    switch (e->type) {

        case SDL_QUIT: s->running = false; break;

        case SDL_DROPFILE: {
            char *dropped = e->drop.file;
            snprintf(s->current_path, sizeof(s->current_path), "%s", dropped);
            s->want_load = true;
            SDL_free(dropped);
            break;
        }

        case SDL_KEYDOWN:
            switch (e->key.keysym.sym) {

                case SDLK_ESCAPE:
                    if (s->show_help)      s->show_help = false;
                    else if (s->tile_edit) s->tile_edit = false;
                    else if (s->tile_mode) s->tile_mode = false;
                    break;

                case SDLK_z:
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        if (e->key.keysym.mod & KMOD_SHIFT)
                            undo_redo_pop(s);
                        else
                            undo_pop(s);
                    }
                    break;
                case SDLK_y:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        undo_redo_pop(s);
                    break;

                case SDLK_0: s->color = 0; break;
                case SDLK_1: s->color = 1; break;
                case SDLK_2: s->color = 2; break;
                case SDLK_3: s->color = 3; break;

                case SDLK_g: s->show_tile_grid  = !s->show_tile_grid;  break;
                case SDLK_p:
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        if (e->key.keysym.mod & KMOD_SHIFT)
                            s->want_save_pal = true;
                        else
                            input_begin(s, INPUT_OPEN_PAL);
                    } else {
                        s->show_pixel_grid = !s->show_pixel_grid;
                    }
                    break;
                case SDLK_v:
                    if ((e->key.keysym.mod & KMOD_CTRL) &&
                        (e->key.keysym.mod & KMOD_SHIFT)) {
                        pal_paste_from_clipboard(s);
                    } else if ((e->key.keysym.mod & KMOD_CTRL) && s->tile_mode) {
                        clipboard_load(s);  /* refresh from file (cross-instance) */
                        if (s->has_clipboard) {
                            undo_push(s);
                            int base = sel_tile_idx(s);
                            bool s16 = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
                            int cnt  = (s16 && s->clipboard_s16) ? 4 : 1;
                            for (int p = 0; p < cnt; p++)
                                memcpy(s->chr.px[base + p], s->clipboard[p], TILE_H * TILE_W);
                        }
                    } else if (!(e->key.keysym.mod & KMOD_CTRL)) {
                        s->view_mode = (s->view_mode == VIEW_GRAYSCALE)
                                     ? VIEW_NES_COLOR : VIEW_GRAYSCALE;
                    }
                    break;

                case SDLK_SLASH:
                    if (e->key.keysym.mod & KMOD_SHIFT) {
                        s->show_help = !s->show_help;
                        s->help_scroll = 0;
                    }
                    break;
                case SDLK_F1:
                    s->show_help = !s->show_help;
                    s->help_scroll = 0;
                    break;

                case SDLK_s:
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        if (e->key.keysym.mod & KMOD_SHIFT)
                            input_begin(s, INPUT_SAVE_AS);
                        else
                            s->want_save = true;
                    }
                    break;
                case SDLK_o:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        input_begin(s, INPUT_OPEN);
                    break;
                case SDLK_r:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        input_begin(s, INPUT_RESIZE);
                    break;
                case SDLK_c:
                    if ((e->key.keysym.mod & KMOD_CTRL) &&
                        (e->key.keysym.mod & KMOD_SHIFT)) {
                        pal_copy_to_clipboard(s);
                    } else if ((e->key.keysym.mod & KMOD_CTRL) && s->tile_mode) {
                        int base = sel_tile_idx(s);
                        bool s16 = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
                        int cnt  = s16 ? 4 : 1;
                        for (int p = 0; p < cnt; p++)
                            memcpy(s->clipboard[p], s->chr.px[base + p], TILE_H * TILE_W);
                        s->clipboard_s16  = s16;
                        s->has_clipboard  = true;
                        clipboard_save(s);
                    } else if (!(e->key.keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_GUI))) {
                        s->show_preview = !s->show_preview;
                        s->want_resize  = true;
                    }
                    break;
                case SDLK_x:
                    if ((e->key.keysym.mod & KMOD_CTRL) && s->tile_mode) {
                        undo_push(s);
                        int base = sel_tile_idx(s);
                        bool s16 = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
                        int cnt  = s16 ? 4 : 1;
                        for (int p = 0; p < cnt; p++) {
                            memcpy(s->clipboard[p], s->chr.px[base + p], TILE_H * TILE_W);
                            memset(s->chr.px[base + p], 0, TILE_H * TILE_W);
                        }
                        s->clipboard_s16  = s16;
                        s->has_clipboard  = true;
                        clipboard_save(s);
                    }
                    break;


                case SDLK_e:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        s->want_save = true;
                    else if (s->tile_mode && !s->tile_edit)
                        s->tile_edit = true;
                    break;

                case SDLK_EQUALS:
                    if (s->zoom < 4) { s->zoom++; s->want_resize = true; }
                    break;
                case SDLK_MINUS:
                    if (s->zoom > 1) { s->zoom--; s->want_resize = true; }
                    break;

                case SDLK_m: {
                    bool can = (s->chr_cols >= 2 && s->chr_cols % 2 == 0 &&
                                s->chr_rows >= 2 && s->chr_rows % 2 == 0);
                    if (s->sprite_mode == SPRITE_16) {
                        s->sprite_mode = SPRITE_8;
                        s->tile_mode   = false;
                        s->tile_edit   = false;
                    } else if (can) {
                        s->sprite_mode = SPRITE_16;
                        s->tile_mode   = false;
                        s->tile_edit   = false;
                    }
                    break;
                }

                case SDLK_n: s->show_addr = !s->show_addr; break;

                case SDLK_a:
                    if (s->anim_state == ANIM_OFF)
                        s->anim_state = ANIM_PICKING_FIRST;
                    else {
                        s->anim_state   = ANIM_OFF;
                        s->anim_playing = false;
                    }
                    break;

                case SDLK_TAB:
                    s->compose_mode = true;
                    s->tile_mode    = false;
                    s->tile_edit    = false;
                    s->focus_zoom   = 1;
                    s->pan_x = s->pan_y = 0;
                    s->want_resize  = true;
                    break;

                case SDLK_LEFT:
                    if (s->anim_state == ANIM_ACTIVE) {
                        s->anim_playing = false;
                        s->anim_cur = (s->anim_cur - 1 + s->anim_frame_count)
                                      % s->anim_frame_count;
                    }
                    break;
                case SDLK_RIGHT:
                    if (s->anim_state == ANIM_ACTIVE) {
                        s->anim_playing = false;
                        s->anim_cur = (s->anim_cur + 1) % s->anim_frame_count;
                    }
                    break;
                case SDLK_SPACE:
                    if (!e->key.repeat) s->space_held = true;
                    if (s->anim_state == ANIM_ACTIVE && !e->key.repeat) {
                        s->anim_playing = !s->anim_playing;
                        s->anim_last_tick = SDL_GetTicks();
                    }
                    break;
                case SDLK_f:
                    s->focus_zoom = 1;
                    s->pan_x = 0;
                    s->pan_y = 0;
                    break;
                case SDLK_COMMA:
                    if (s->anim_state == ANIM_ACTIVE && s->anim_speed > 1)
                        s->anim_speed--;
                    break;
                case SDLK_PERIOD:
                    if (s->anim_state == ANIM_ACTIVE && s->anim_speed < 30)
                        s->anim_speed++;
                    break;

                case SDLK_t: select_tile_under_cursor(s); break;
                case SDLK_w:
                    s->wrap_mode = (WrapMode)((s->wrap_mode + 1) % 4);
                    break;

                case SDLK_LEFTBRACKET:
                    if (s->tile_mode) {
                        undo_push(s);
                        int base = sel_tile_idx(s);
                        int cnt  = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
                        for (int p = 0; p < cnt; p++)
                            s->pal.tile_pal[base + p] =
                                (uint8_t)((s->pal.tile_pal[base + p] + PAL_COUNT - 1) % PAL_COUNT);
                    }
                    break;
                case SDLK_RIGHTBRACKET:
                    if (s->tile_mode) {
                        undo_push(s);
                        int base = sel_tile_idx(s);
                        int cnt  = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
                        for (int p = 0; p < cnt; p++)
                            s->pal.tile_pal[base + p] =
                                (uint8_t)((s->pal.tile_pal[base + p] + 1) % PAL_COUNT);
                    }
                    break;

                default: break;
            }
            break;

        case SDL_KEYUP:
            if (e->key.keysym.sym == SDLK_SPACE) s->space_held = false;
            break;

        case SDL_MOUSEBUTTONDOWN: {
            int mx = e->button.x, my = e->button.y;
            s->mouse_x = mx; s->mouse_y = my;

            bool in_preview = s->show_preview && mx >= s->preview_x0;

            /* Preview dock: any button starts a pan drag. */
            if (in_preview) {
                if (e->button.button == SDL_BUTTON_LEFT ||
                    e->button.button == SDL_BUTTON_MIDDLE ||
                    e->button.button == SDL_BUTTON_RIGHT) {
                    s->preview_panning = true;
                    s->preview_pan_anchor_mx = mx;
                    s->preview_pan_anchor_my = my;
                    s->preview_pan_anchor_px = s->preview_pan_x;
                    s->preview_pan_anchor_py = s->preview_pan_y;
                }
                break;
            }

            /* Middle button → start panning (canvas only) */
            if (e->button.button == SDL_BUTTON_MIDDLE && mx < s->canvas_w) {
                s->panning = true;
                s->pan_anchor_mx = mx; s->pan_anchor_my = my;
                s->pan_anchor_px = s->pan_x; s->pan_anchor_py = s->pan_y;
                break;
            }

            if (e->button.button == SDL_BUTTON_LEFT) {
                /* Space + LMB in canvas → pan instead of paint */
                if (s->space_held && mx < s->canvas_w) {
                    s->panning = true;
                    s->pan_anchor_mx = mx; s->pan_anchor_my = my;
                    s->pan_anchor_px = s->pan_x; s->pan_anchor_py = s->pan_y;
                    break;
                }
                /* Scrollbar hit-test (LMB on visible scrollbar overlays) */
                if (s->focus_zoom > 1 && mx < s->canvas_w && my < s->canvas_h) {
                    int tx, tw, thx, thw;
                    int ty, th, thy, thh;
                    bool h = sb_h_geom(s, &tx, &tw, &thx, &thw);
                    bool v = sb_v_geom(s, &ty, &th, &thy, &thh);
                    /* H scrollbar occupies bottom strip */
                    if (h && my >= s->canvas_h - SB_THICKNESS && mx < s->canvas_w - SB_THICKNESS) {
                        if (mx >= thx && mx < thx + thw) {
                            s->sb_drag = 1;
                            s->sb_drag_anchor = mx;
                            s->sb_drag_pan_start = s->pan_x;
                        } else {
                            /* Click outside thumb → jump */
                            int max_pan_nes = s->chr_cols * TILE_W - s->canvas_w / fz_scale(s);
                            if (tw > thw && max_pan_nes > 0)
                                s->pan_x = (int)((long long)(mx - tx - thw/2) * max_pan_nes / (tw - thw));
                            clamp_pan(s);
                            s->sb_drag = 1;
                            s->sb_drag_anchor = mx;
                            s->sb_drag_pan_start = s->pan_x;
                        }
                        break;
                    }
                    if (v && mx >= s->canvas_w - SB_THICKNESS && my < s->canvas_h - SB_THICKNESS) {
                        if (my >= thy && my < thy + thh) {
                            s->sb_drag = 2;
                            s->sb_drag_anchor = my;
                            s->sb_drag_pan_start = s->pan_y;
                        } else {
                            int max_pan_nes = s->chr_rows * TILE_H - s->canvas_h / fz_scale(s);
                            if (th > thh && max_pan_nes > 0)
                                s->pan_y = (int)((long long)(my - ty - thh/2) * max_pan_nes / (th - thh));
                            clamp_pan(s);
                            s->sb_drag = 2;
                            s->sb_drag_anchor = my;
                            s->sb_drag_pan_start = s->pan_y;
                        }
                        break;
                    }
                }

                s->mouse_down = true;
                s->drag_in_edit_panel = (mx >= s->canvas_w && s->tile_edit);
                /* Push undo before any canvas modification */
                if (mx < s->canvas_w &&
                    s->anim_state != ANIM_PICKING_FIRST &&
                    s->anim_state != ANIM_PICKING_LAST)
                    undo_push(s);
                else if (mx >= s->canvas_w && s->tile_edit)
                    undo_push(s);
                if (mx < s->canvas_w) {
                    if (s->anim_state == ANIM_PICKING_FIRST ||
                        s->anim_state == ANIM_PICKING_LAST) {
                        int ntiles = s->chr_cols * s->chr_rows;
                        int snapped;
                        int nx = sx_to_nx(s, mx);
                        int ny = sy_to_ny(s, my);
                        if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
                            /* In sprite-16 mode tiles are column-major remapped;
                               use sprite-grid coords to get the correct base tile. */
                            int sprite_cols = s->chr_cols / 2;
                            int sprite_x    = nx / (TILE_W * 2);
                            int sprite_y    = ny / (TILE_H * 2);
                            snapped = (sprite_y * sprite_cols + sprite_x) * 4;
                        } else {
                            snapped = (ny / TILE_H) * s->chr_cols
                                    + (nx / TILE_W);
                        }
                        if (snapped < 0) snapped = 0;
                        if (snapped >= ntiles) snapped = ntiles - 1;
                        if (s->anim_state == ANIM_PICKING_FIRST) {
                            s->anim_first = snapped;
                            s->anim_state = ANIM_PICKING_LAST;
                        } else {
                            s->anim_last = snapped;
                            anim_finish_pick(s);
                        }
                    } else {
                        paint_at(s, mx, my);
                    }
                } else {
                    if (s->tile_edit)
                        tile_edit_paint(s, mx - s->canvas_w, my);
                    else
                        panel_click(s, mx - s->canvas_w, my);
                }
            }
            if (e->button.button == SDL_BUTTON_RIGHT) {
                s->right_mouse_down = true;
                if (mx < s->canvas_w) undo_push(s);
                assign_pal_at(s, mx, my);
            }
            break;
        }

        case SDL_MOUSEBUTTONUP:
            if (e->button.button == SDL_BUTTON_MIDDLE) s->panning = false;
            if (e->button.button == SDL_BUTTON_LEFT) {
                s->mouse_down = false;
                s->drag_in_edit_panel = false;
                s->panning = false;
                s->sb_drag = 0;
            }
            if (e->button.button == SDL_BUTTON_RIGHT) s->right_mouse_down = false;
            s->preview_panning = false;
            break;

        case SDL_MOUSEMOTION: {
            int mx = e->motion.x, my = e->motion.y;
            s->mouse_x = mx; s->mouse_y = my;

            if (s->preview_panning) {
                int z = s->preview_zoom;
                s->preview_pan_x = s->preview_pan_anchor_px
                                 - (mx - s->preview_pan_anchor_mx) / z;
                s->preview_pan_y = s->preview_pan_anchor_py
                                 - (my - s->preview_pan_anchor_my) / z;
                int vw = s->preview_w / z; if (vw > 256) vw = 256;
                int vh = s->preview_h / z; if (vh > 240) vh = 240;
                int mpx = 256 - vw; if (mpx < 0) mpx = 0;
                int mpy = 240 - vh; if (mpy < 0) mpy = 0;
                if (s->preview_pan_x < 0)   s->preview_pan_x = 0;
                if (s->preview_pan_y < 0)   s->preview_pan_y = 0;
                if (s->preview_pan_x > mpx) s->preview_pan_x = mpx;
                if (s->preview_pan_y > mpy) s->preview_pan_y = mpy;
                break;
            }

            if (s->panning) {
                int scale = fz_scale(s);
                s->pan_x = s->pan_anchor_px - (mx - s->pan_anchor_mx) / scale;
                s->pan_y = s->pan_anchor_py - (my - s->pan_anchor_my) / scale;
                clamp_pan(s);
                break;
            }
            if (s->sb_drag == 1) {
                int tx, tw, thx, thw;
                if (sb_h_geom(s, &tx, &tw, &thx, &thw)) {
                    int max_pan_nes = s->chr_cols * TILE_W - s->canvas_w / fz_scale(s);
                    if (tw > thw && max_pan_nes > 0) {
                        int d = mx - s->sb_drag_anchor;
                        s->pan_x = s->sb_drag_pan_start
                            + (int)((long long)d * max_pan_nes / (tw - thw));
                        clamp_pan(s);
                    }
                }
                break;
            }
            if (s->sb_drag == 2) {
                int ty, th, thy, thh;
                if (sb_v_geom(s, &ty, &th, &thy, &thh)) {
                    int max_pan_nes = s->chr_rows * TILE_H - s->canvas_h / fz_scale(s);
                    if (th > thh && max_pan_nes > 0) {
                        int d = my - s->sb_drag_anchor;
                        s->pan_y = s->sb_drag_pan_start
                            + (int)((long long)d * max_pan_nes / (th - thh));
                        clamp_pan(s);
                    }
                }
                break;
            }

            if (s->mouse_down) {
                if (s->drag_in_edit_panel) {
                    tile_edit_paint(s, mx - s->canvas_w, my);
                } else if (mx < s->canvas_w) {
                    if (s->anim_state != ANIM_PICKING_FIRST &&
                        s->anim_state != ANIM_PICKING_LAST)
                        paint_at(s, mx, my);
                } else {
                    if (s->tile_edit)
                        tile_edit_paint(s, mx - s->canvas_w, my);
                    else
                        panel_click(s, mx - s->canvas_w, my);
                }
            }
            if (s->right_mouse_down)
                assign_pal_at(s, mx, my);
            break;
        }

        case SDL_MOUSEWHEEL:
            if (s->show_help) {
                s->help_scroll -= e->wheel.y * font_line_h() * 2;
                if (s->help_scroll < 0) s->help_scroll = 0;
            } else if (s->show_preview && s->mouse_x >= s->preview_x0) {
                /* Wheel over preview → zoom around cursor. */
                int old_z = s->preview_zoom;
                int new_z = old_z + e->wheel.y;
                if (new_z < 1) new_z = 1;
                if (new_z > 4) new_z = 4;
                if (new_z != old_z) {
                    int cx = s->mouse_x - s->preview_x0;
                    int cy = s->mouse_y;
                    int wx = s->preview_pan_x + cx / old_z;
                    int wy = s->preview_pan_y + cy / old_z;
                    s->preview_zoom  = new_z;
                    s->preview_pan_x = wx - cx / new_z;
                    s->preview_pan_y = wy - cy / new_z;
                    int vw = s->preview_w / new_z; if (vw > 256) vw = 256;
                    int vh = s->preview_h / new_z; if (vh > 240) vh = 240;
                    int mpx = 256 - vw; if (mpx < 0) mpx = 0;
                    int mpy = 240 - vh; if (mpy < 0) mpy = 0;
                    if (s->preview_pan_x < 0)   s->preview_pan_x = 0;
                    if (s->preview_pan_y < 0)   s->preview_pan_y = 0;
                    if (s->preview_pan_x > mpx) s->preview_pan_x = mpx;
                    if (s->preview_pan_y > mpy) s->preview_pan_y = mpy;
                }
            } else if (s->mouse_x < s->canvas_w) {
                /* Wheel over canvas → focus zoom centred on cursor. */
                focus_zoom_at(s, s->mouse_x, s->mouse_y,
                              s->focus_zoom + e->wheel.y);
            } else {
                /* Scroll the palette list when wheeling over the panel. */
                int py = s->mouse_y;
                if (py >= PANEL_PAL_Y0 &&
                    py < PANEL_PAL_Y0 + PAL_VISIBLE * PANEL_PAL_ROW) {
                    s->palette_scroll -= e->wheel.y;
                    int maxs = PAL_COUNT - PAL_VISIBLE;
                    if (s->palette_scroll < 0)    s->palette_scroll = 0;
                    if (s->palette_scroll > maxs) s->palette_scroll = maxs;
                }
            }
            break;

        default: break;
    }
}
