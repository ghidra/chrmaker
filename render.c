#include "render.h"
#include "panel.h"
#include "font.h"
#include "compose.h"
#include <stdio.h>
#include <string.h>

/* ── NES master palette (NTSC 2C02) ──────────────────────────────
   64 hardware-defined colours. Indices $0E/$0F/$1E/$1F/$2E/$2F/
   $3E/$3F are black on real hardware; kept as black here.         */
static const SDL_Color NES_MASTER_PALETTE[64] = {
    /* $00 */ {  84,  84,  84, 255 }, /* $01 */ {   0,  30, 116, 255 },
    /* $02 */ {   8,  16, 144, 255 }, /* $03 */ {  48,   0, 136, 255 },
    /* $04 */ {  68,   0, 100, 255 }, /* $05 */ {  92,   0,  48, 255 },
    /* $06 */ {  84,   4,   0, 255 }, /* $07 */ {  60,  24,   0, 255 },
    /* $08 */ {  32,  42,   0, 255 }, /* $09 */ {   8,  58,   0, 255 },
    /* $0A */ {   0,  64,   0, 255 }, /* $0B */ {   0,  60,   0, 255 },
    /* $0C */ {   0,  50,  60, 255 }, /* $0D */ {   0,   0,   0, 255 },
    /* $0E */ {   0,   0,   0, 255 }, /* $0F */ {   0,   0,   0, 255 },

    /* $10 */ { 152, 150, 152, 255 }, /* $11 */ {   8,  76, 196, 255 },
    /* $12 */ {  48,  50, 236, 255 }, /* $13 */ {  92,  30, 228, 255 },
    /* $14 */ { 136,  20, 176, 255 }, /* $15 */ { 160,  20, 100, 255 },
    /* $16 */ { 152,  34,  32, 255 }, /* $17 */ { 120,  60,   0, 255 },
    /* $18 */ {  84,  90,   0, 255 }, /* $19 */ {  40, 114,   0, 255 },
    /* $1A */ {   8, 124,   0, 255 }, /* $1B */ {   0, 118,  40, 255 },
    /* $1C */ {   0, 102, 120, 255 }, /* $1D */ {   0,   0,   0, 255 },
    /* $1E */ {   0,   0,   0, 255 }, /* $1F */ {   0,   0,   0, 255 },

    /* $20 */ { 236, 238, 236, 255 }, /* $21 */ {  76, 154, 236, 255 },
    /* $22 */ { 120, 124, 236, 255 }, /* $23 */ { 176,  98, 236, 255 },
    /* $24 */ { 228,  84, 236, 255 }, /* $25 */ { 236,  88, 180, 255 },
    /* $26 */ { 236, 106, 100, 255 }, /* $27 */ { 212, 136,  32, 255 },
    /* $28 */ { 160, 170,   0, 255 }, /* $29 */ { 116, 196,   0, 255 },
    /* $2A */ {  76, 208,  32, 255 }, /* $2B */ {  56, 204, 108, 255 },
    /* $2C */ {  56, 180, 204, 255 }, /* $2D */ {  60,  60,  60, 255 },
    /* $2E */ {   0,   0,   0, 255 }, /* $2F */ {   0,   0,   0, 255 },

    /* $30 */ { 236, 238, 236, 255 }, /* $31 */ { 168, 204, 236, 255 },
    /* $32 */ { 188, 188, 236, 255 }, /* $33 */ { 212, 178, 236, 255 },
    /* $34 */ { 236, 174, 236, 255 }, /* $35 */ { 236, 174, 212, 255 },
    /* $36 */ { 236, 180, 176, 255 }, /* $37 */ { 228, 196, 144, 255 },
    /* $38 */ { 204, 210, 120, 255 }, /* $39 */ { 180, 222, 120, 255 },
    /* $3A */ { 168, 226, 144, 255 }, /* $3B */ { 152, 226, 180, 255 },
    /* $3C */ { 160, 214, 228, 255 }, /* $3D */ { 160, 162, 160, 255 },
    /* $3E */ {   0,   0,   0, 255 }, /* $3F */ {   0,   0,   0, 255 },
};

/* Grayscale ramp used in VIEW_GRAYSCALE mode. */
static const SDL_Color GRAY_RAMP[4] = {
    {   0,   0,   0, 255 }, /* 0 — black      */
    {  85,  85,  85, 255 }, /* 1 — dark gray  */
    { 170, 170, 170, 255 }, /* 2 — light gray */
    { 255, 255, 255, 255 }, /* 3 — white      */
};

/* ── Canvas texture ───────────────────────────────────────────── */
/* NES-resolution streaming texture; SDL scales it to canvas_w×CANVAS_H. */
static SDL_Texture *canvas_tex  = NULL;
static SDL_Texture *compose_tex = NULL;   /* 256x240 for compose mode */

static void create_canvas_tex(SDL_Renderer *ren, const EditorState *s) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    int tex_w = s->chr_cols * TILE_W;
    int tex_h = s->chr_rows * TILE_H;
    canvas_tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        tex_w, tex_h
    );
    if (!canvas_tex)
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
}

static void create_compose_tex(SDL_Renderer *ren) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    compose_tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 256, 240);
    if (!compose_tex)
        fprintf(stderr, "compose_tex: %s\n", SDL_GetError());
}

void render_init(SDL_Renderer *ren, const EditorState *s) {
    create_canvas_tex(ren, s);
    create_compose_tex(ren);
}

void render_resize(SDL_Renderer *ren, const EditorState *s) {
    if (canvas_tex) { SDL_DestroyTexture(canvas_tex); canvas_tex = NULL; }
    create_canvas_tex(ren, s);
    if (compose_tex) { SDL_DestroyTexture(compose_tex); compose_tex = NULL; }
    create_compose_tex(ren);
}

void render_destroy(void) {
    if (canvas_tex)  { SDL_DestroyTexture(canvas_tex);  canvas_tex  = NULL; }
    if (compose_tex) { SDL_DestroyTexture(compose_tex); compose_tex = NULL; }
}

/* ── Colour lookup ────────────────────────────────────────────── */
static SDL_Color get_display_color(const EditorState *s, int tile, int val) {
    if (s->view_mode == VIEW_GRAYSCALE)
        return GRAY_RAMP[val & 3];

    uint8_t sub    = s->pal.tile_pal[tile];
    uint8_t master = s->pal.sub[sub & 7].idx[val & 3] & 0x3F;
    return NES_MASTER_PALETTE[master];
}

/* ── Canvas rendering ─────────────────────────────────────────── */
static void render_canvas(const EditorState *s) {
    if (!canvas_tex) return;

    void *pixels; int pitch;
    if (SDL_LockTexture(canvas_tex, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture: %s\n", SDL_GetError());
        return;
    }

    uint32_t *dst    = (uint32_t *)pixels;
    int       stride = pitch / 4;
    int       ntiles = s->chr_cols * s->chr_rows;

    /* Sprite-16 layout: 4 sequential tiles → [0][2]
                                               [1][3]
       p=0 top-left, p=1 bottom-left, p=2 top-right, p=3 bottom-right
       sub_x = p>>1 (col within sprite), sub_y = p&1 (row within sprite) */
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        int sprite_cols = s->chr_cols / 2;
        for (int tile = 0; tile < ntiles; tile++) {
            int p  = tile % 4;
            int S  = tile / 4;
            int sx = S % sprite_cols;
            int sy = S / sprite_cols;
            int tx = (sx * 2 + (p >> 1)) * TILE_W;
            int ty = (sy * 2 + (p  & 1)) * TILE_H;

            for (int row = 0; row < TILE_H; row++) {
                for (int col = 0; col < TILE_W; col++) {
                    uint8_t   val = s->chr.px[tile][row][col] & 3;
                    SDL_Color c   = get_display_color(s, tile, val);
                    dst[(ty + row) * stride + (tx + col)] =
                        (0xFFu << 24) | ((uint32_t)c.r << 16) |
                        ((uint32_t)c.g <<  8) | c.b;
                }
            }
        }
    } else {
        for (int tile = 0; tile < ntiles; tile++) {
            int tx = (tile % s->chr_cols) * TILE_W;
            int ty = (tile / s->chr_cols) * TILE_H;

            for (int row = 0; row < TILE_H; row++) {
                for (int col = 0; col < TILE_W; col++) {
                    uint8_t   val = s->chr.px[tile][row][col] & 3;
                    SDL_Color c   = get_display_color(s, tile, val);
                    dst[(ty + row) * stride + (tx + col)] =
                        (0xFFu << 24) | ((uint32_t)c.r << 16) |
                        ((uint32_t)c.g <<  8) | c.b;
                }
            }
        }
    }

    SDL_UnlockTexture(canvas_tex);
}

/* ── Helpers ──────────────────────────────────────────────────── */
static void set_color(SDL_Renderer *ren, uint8_t r, uint8_t g, uint8_t b) {
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
}

static void fill(SDL_Renderer *ren, int x, int y, int w, int h,
                 uint8_t r, uint8_t g, uint8_t b) {
    set_color(ren, r, g, b);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(ren, &rect);
}

static void hline(SDL_Renderer *ren, int x, int y, int len,
                  uint8_t r, uint8_t g, uint8_t b) {
    set_color(ren, r, g, b);
    SDL_RenderDrawLine(ren, x, y, x + len - 1, y);
}

static void vline(SDL_Renderer *ren, int x, int y, int len,
                  uint8_t r, uint8_t g, uint8_t b) {
    set_color(ren, r, g, b);
    SDL_RenderDrawLine(ren, x, y, x, y + len - 1);
}

/* ── Grid overlays ────────────────────────────────────────────── */

static void render_tile_grid(SDL_Renderer *ren, const EditorState *s) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 60, 100, 200, 130);

    for (int i = 1; i < s->chr_cols; i++) {
        int x = i * TILE_W * s->zoom;
        SDL_RenderDrawLine(ren, x, 0, x, s->canvas_h - 1);
    }
    for (int i = 1; i < s->chr_rows; i++) {
        int y = i * TILE_H * s->zoom;
        SDL_RenderDrawLine(ren, 0, y, s->canvas_w - 1, y);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void render_pixel_grid(SDL_Renderer *ren, const EditorState *s) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 80, 80, 110, 55);

    int nes_cols = s->chr_cols * TILE_W;
    int nes_rows = s->chr_rows * TILE_H;

    for (int i = 1; i < nes_cols; i++) {
        int x = i * s->zoom;
        SDL_RenderDrawLine(ren, x, 0, x, s->canvas_h - 1);
    }
    for (int i = 1; i < nes_rows; i++) {
        int y = i * s->zoom;
        SDL_RenderDrawLine(ren, 0, y, s->canvas_w - 1, y);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* ── Animation helpers ────────────────────────────────────────── */

/* Compute screen position and base tile for animation frame f. */
static void anim_frame_screen(const EditorState *s, int f,
                               int *out_sx, int *out_sy, int *out_base) {
    int stride = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
    int base   = s->anim_first + f * stride;
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        int sprite_cols = s->chr_cols / 2;
        int S  = base / 4;
        int sx = S % sprite_cols;
        int sy = S / sprite_cols;
        *out_sx = sx * 2 * TILE_W * s->zoom;
        *out_sy = sy * 2 * TILE_H * s->zoom;
    } else {
        *out_sx = (base % s->chr_cols) * TILE_W * s->zoom;
        *out_sy = (base / s->chr_cols) * TILE_H * s->zoom;
    }
    *out_base = base;
}

/* Draw one ghost frame at screen position (sx, sy) with given alpha. */
static void draw_ghost_tile(SDL_Renderer *ren, const EditorState *s,
                             int base_tile, int sx, int sy, Uint8 alpha) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        for (int row = 0; row < TILE_H * 2; row++) {
            for (int col = 0; col < TILE_W * 2; col++) {
                int p = (col / TILE_W) * 2 + (row / TILE_H);
                int t = base_tile + p;
                if (t < 0 || t >= CHR_MAX_TILES) continue;
                uint8_t val = s->chr.px[t][row % TILE_H][col % TILE_W] & 3;
                SDL_Color c = get_display_color(s, t, val);
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, alpha);
                SDL_Rect r = { sx + col * s->zoom, sy + row * s->zoom,
                               s->zoom, s->zoom };
                SDL_RenderFillRect(ren, &r);
            }
        }
    } else {
        if (base_tile < 0 || base_tile >= CHR_MAX_TILES) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            return;
        }
        for (int row = 0; row < TILE_H; row++) {
            for (int col = 0; col < TILE_W; col++) {
                uint8_t val = s->chr.px[base_tile][row][col] & 3;
                SDL_Color c = get_display_color(s, base_tile, val);
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, alpha);
                SDL_Rect r = { sx + col * s->zoom, sy + row * s->zoom,
                               s->zoom, s->zoom };
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* ── Tile highlight ───────────────────────────────────────────── */
static void render_tile_highlight(SDL_Renderer *ren, const EditorState *s) {
    if (!s->tile_mode) return;

    int mult = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 2 : 1;
    int tx = s->sel_tile_x * TILE_W * s->zoom;
    int ty = s->sel_tile_y * TILE_H * s->zoom;
    int tw = TILE_W * mult * s->zoom;
    int th = TILE_H * mult * s->zoom;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_Rect shadow = {tx - 1, ty - 1, tw + 2, th + 2};
    SDL_RenderDrawRect(ren, &shadow);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 255, 210, 40, 255);
    SDL_Rect border = {tx, ty, tw, th};
    SDL_RenderDrawRect(ren, &border);
}

/* ── Animation frame highlight (cyan border) ─────────────────── */
static void render_anim_frame_highlight(SDL_Renderer *ren, const EditorState *s) {
    if (s->anim_state != ANIM_ACTIVE) return;

    int sx, sy, base;
    anim_frame_screen(s, s->anim_cur, &sx, &sy, &base);

    int mult = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 2 : 1;
    int tw   = TILE_W * mult * s->zoom;
    int th   = TILE_H * mult * s->zoom;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_Rect shadow = { sx - 1, sy - 1, tw + 2, th + 2 };
    SDL_RenderDrawRect(ren, &shadow);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 0, 220, 255, 255);
    SDL_Rect border = { sx, sy, tw, th };
    SDL_RenderDrawRect(ren, &border);
}

/* ── Animation ghost overlays ─────────────────────────────────── */
static void render_anim_ghosts(SDL_Renderer *ren, const EditorState *s) {
    if (s->anim_state != ANIM_ACTIVE) return;

    int sx, sy, cur_base;
    anim_frame_screen(s, s->anim_cur, &sx, &sy, &cur_base);

    if (s->anim_cur > 0) {
        int prev_base, dummy_sx, dummy_sy;
        anim_frame_screen(s, s->anim_cur - 1, &dummy_sx, &dummy_sy, &prev_base);
        draw_ghost_tile(ren, s, prev_base, sx, sy, 80);
    }
    if (s->anim_cur < s->anim_frame_count - 1) {
        int next_base, dummy_sx, dummy_sy;
        anim_frame_screen(s, s->anim_cur + 1, &dummy_sx, &dummy_sy, &next_base);
        draw_ghost_tile(ren, s, next_base, sx, sy, 50);
    }
}

/* ── Screen-to-tile (for address display) ─────────────────────── */
static int screen_to_tile_idx(const EditorState *s, int mx, int my) {
    int nx = mx / s->zoom;
    int ny = my / s->zoom;
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        int sprite_cols = s->chr_cols / 2;
        int sprite_x    = nx / (TILE_W * 2);
        int sprite_y    = ny / (TILE_H * 2);
        int sub_x       = (nx / TILE_W) % 2;
        int sub_y       = (ny / TILE_H) % 2;
        int p           = sub_x * 2 + sub_y;
        return (sprite_y * sprite_cols + sprite_x) * 4 + p;
    }
    return (ny / TILE_H) * s->chr_cols + (nx / TILE_W);
}

/* ── Status bar ───────────────────────────────────────────────── */
static void render_status(SDL_Renderer *ren, const EditorState *s) {
    const int STATUS_Y = s->win_h - STATUS_H;
    fill(ren, 0, STATUS_Y, s->win_w, STATUS_H, 12, 12, 12);

    static const SDL_Color SWATCH[4] = {
        {   0,   0,   0, 255 },
        {  85,  85,  85, 255 },
        { 170, 170, 170, 255 },
        { 255, 255, 255, 255 },
    };

    const int SW = 12, SH = 12;
    const int SY = STATUS_Y + (STATUS_H - SH) / 2;

    for (int i = 0; i < 4; i++) {
        int sx = 4 + i * (SW + 3);
        if (i == s->color)
            fill(ren, sx - 2, SY - 2, SW + 4, SH + 4, 220, 220, 220);
        else
            fill(ren, sx - 1, SY - 1, SW + 2, SH + 2, 60, 60, 60);
        SDL_Color c = SWATCH[i];
        fill(ren, sx, SY, SW, SH, c.r, c.g, c.b);
    }

    /* Tile-mode + wrap indicator — one box, amber when active,
       green crosshair lines show wrap axes (─ H, │ V, ┼ both). */
    int tx = 76;
    fill(ren, tx - 1, SY - 1, SW + 2, SH + 2, 55, 55, 55);
    fill(ren, tx, SY, SW, SH, s->tile_mode ? 210 : 38,
                              s->tile_mode ? 155 : 38,
                              s->tile_mode ?  20 : 38);
    if (s->wrap_mode == WRAP_H || s->wrap_mode == WRAP_BOTH)
        hline(ren, tx + 1, SY + SH / 2, SW - 2, 0, 0, 0);
    if (s->wrap_mode == WRAP_V || s->wrap_mode == WRAP_BOTH)
        vline(ren, tx + SW / 2, SY + 1, SH - 2, 0, 0, 0);

    /* Tile address display — shows tile index + CHR byte offset under cursor */
    if (s->show_addr) {
        int mx = s->mouse_x, my = s->mouse_y;
        int ty_addr = STATUS_Y + (STATUS_H - font_line_h()) / 2 + 1;
        if (mx >= 0 && mx < s->canvas_w && my >= 0 && my < s->canvas_h) {
            int tile = screen_to_tile_idx(s, mx, my);
            int addr = tile * 16;
            char abuf[24];
            snprintf(abuf, sizeof(abuf), "#$%02X @$%04X", tile, addr);
            static const SDL_Color ACOL = {180, 220, 160, 255};
            font_draw_str(ren, abuf, 96, ty_addr, ACOL);
        } else {
            static const SDL_Color DCOL = {80, 80, 100, 255};
            font_draw_str(ren, "N:ADDR", 96, ty_addr, DCOL);
        }
    }

    /* Zoom and sprite-mode indicators — right-aligned so they always fit. */
    {
        int ty_ind = STATUS_Y + (STATUS_H - font_line_h()) / 2 + 1;
        int cw     = font_char_w();

        char zbuf[4];
        snprintf(zbuf, sizeof(zbuf), "%dX", s->zoom);
        int zx = s->win_w - (int)strlen(zbuf) * cw - 4;
        static const SDL_Color ZCOL = {140, 160, 200, 255};
        font_draw_str(ren, zbuf, zx, ty_ind, ZCOL);

        int ind_x = zx;
        if (s->sprite_mode == SPRITE_16) {
            ind_x -= 3 * cw + 4;
            static const SDL_Color S16COL = {80, 210, 140, 255};
            font_draw_str(ren, "S16", ind_x, ty_ind, S16COL);
        }
        if (s->tile_edit) {
            ind_x -= 4 * cw + 4;
            static const SDL_Color EDITCOL = {255, 210, 40, 255};
            font_draw_str(ren, "EDIT", ind_x, ty_ind, EDITCOL);
        }
        if (s->anim_state != ANIM_OFF) {
            ind_x -= 4 * cw + 4;
            static const SDL_Color ANIMCOL = {0, 200, 255, 255};
            font_draw_str(ren, "ANIM", ind_x, ty_ind, ANIMCOL);
        }
    }
}

/* ── Animation panel section ──────────────────────────────────── */
static void render_anim_section(SDL_Renderer *ren, const EditorState *s,
                                 int panel_view_y0) {
    const int BX    = s->canvas_w;
    int anim_y0     = panel_view_y0 + 44 + 8;

    hline(ren, BX + 2, panel_view_y0 + 44 - 4, s->panel_w - 4, 55, 55, 80);

    static const SDL_Color CYN = {  0, 200, 255, 255 };
    static const SDL_Color DIM = {100, 100, 130, 255 };
    static const SDL_Color WHT = {200, 200, 200, 255 };

    font_draw_str(ren, "ANIM", BX + PANEL_PAL_X0, anim_y0, CYN);

    const char *hint;
    if      (s->anim_state == ANIM_PICKING_FIRST) hint = "PICK 1ST";
    else if (s->anim_state == ANIM_PICKING_LAST)  hint = "PICK LST";
    else                                           hint = "A:EXIT";
    font_draw_str(ren, hint, BX + PANEL_PAL_X0 + 5 * font_char_w(), anim_y0, DIM);

    /* Dynamic preview size: 4 screen px per NES px × anim_preview_zoom.
       sprite-8 → 32 or 64; sprite-16 → 64 or 128. Capped to panel width. */
    bool s16        = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
    int  nes_sz     = s16 ? 16 : 8;
    int  pz         = (s->anim_preview_zoom >= 2) ? 2 : 1;
    int  preview_sz = nes_sz * 4 * pz;
    int  max_prev   = s->panel_w - 2 * PANEL_PAL_X0;
    if (preview_sz > max_prev) preview_sz = max_prev;
    int  pix_sz     = preview_sz / nes_sz;

    int preview_y = anim_y0 + 12 + 4;
    int preview_x = BX + (s->panel_w - preview_sz) / 2;

    /* Dark background; dim border hints at clickability */
    fill(ren, preview_x - 1, preview_y - 1, preview_sz + 2, preview_sz + 2, 40, 40, 40);
    SDL_SetRenderDrawColor(ren, 60, 60, 90, 255);
    SDL_Rect pborder = { preview_x - 1, preview_y - 1, preview_sz + 2, preview_sz + 2 };
    SDL_RenderDrawRect(ren, &pborder);

    /* Zoom indicator in top-right corner of preview */
    {
        static const SDL_Color ZDIM = {80, 80, 110, 255};
        char zlab[4]; snprintf(zlab, sizeof(zlab), "X%d", pz);
        font_draw_str(ren, zlab, preview_x + preview_sz - 2 * font_char_w() - 1,
                      preview_y + 1, ZDIM);
    }

    if (s->anim_state != ANIM_OFF) {
        int stride = s16 ? 4 : 1;
        int base   = s->anim_first + s->anim_cur * stride;

        if (s16) {
            for (int row = 0; row < TILE_H * 2; row++) {
                for (int col = 0; col < TILE_W * 2; col++) {
                    int p = (col / TILE_W) * 2 + (row / TILE_H);
                    int t = base + p;
                    if (t < 0 || t >= CHR_MAX_TILES) continue;
                    uint8_t val = s->chr.px[t][row % TILE_H][col % TILE_W] & 3;
                    SDL_Color c = get_display_color(s, t, val);
                    fill(ren, preview_x + col * pix_sz, preview_y + row * pix_sz,
                         pix_sz, pix_sz, c.r, c.g, c.b);
                }
            }
        } else {
            if (base >= 0 && base < CHR_MAX_TILES) {
                for (int row = 0; row < TILE_H; row++) {
                    for (int col = 0; col < TILE_W; col++) {
                        uint8_t val = s->chr.px[base][row][col] & 3;
                        SDL_Color c = get_display_color(s, base, val);
                        fill(ren, preview_x + col * pix_sz, preview_y + row * pix_sz,
                             pix_sz, pix_sz, c.r, c.g, c.b);
                    }
                }
            }
        }
    }

    /* Frame counter */
    int counter_y = preview_y + preview_sz + 6;
    if (s->anim_state == ANIM_ACTIVE) {
        char cbuf[24];
        snprintf(cbuf, sizeof(cbuf), "%d/%d", s->anim_cur + 1, s->anim_frame_count);
        int cw = (int)strlen(cbuf) * font_char_w();
        font_draw_str(ren, cbuf, BX + (s->panel_w - cw) / 2, counter_y, WHT);
    }

    /* Scrubber bar */
    int scrub_y0 = counter_y + 14 + 6;
    int scrub_x0 = BX + PANEL_PAL_X0;
    int scrub_w  = s->panel_w - 2 * PANEL_PAL_X0;
    fill(ren, scrub_x0, scrub_y0, scrub_w, PANEL_ANIM_SCRUB_H, 30, 30, 50);

    if (s->anim_state == ANIM_ACTIVE && s->anim_frame_count > 1) {
        int fill_w = s->anim_cur * scrub_w / (s->anim_frame_count - 1);
        if (fill_w > 0)
            fill(ren, scrub_x0, scrub_y0, fill_w, PANEL_ANIM_SCRUB_H, 0, 180, 220);
        int knob_x = scrub_x0 + fill_w - 2;
        if (knob_x < scrub_x0) knob_x = scrub_x0;
        fill(ren, knob_x, scrub_y0 - 1, 4, PANEL_ANIM_SCRUB_H + 2, 0, 220, 255);
    }
}

/* Index of the top-left tile of the currently selected tile/sprite (render-side). */
static int sel_tile_idx_r(const EditorState *s) {
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2)
        return ((s->sel_tile_y / 2) * (s->chr_cols / 2) + (s->sel_tile_x / 2)) * 4;
    return s->sel_tile_y * s->chr_cols + s->sel_tile_x;
}

/* ── Tile edit panel (enlarged tile view for drawing) ─────────── */

static void render_tile_edit_panel(SDL_Renderer *ren, const EditorState *s) {
    const int BX = s->canvas_w;

    fill(ren, BX, 0, s->panel_w, s->win_h - STATUS_H, 18, 18, 30);

    static const SDL_Color YLW = {220, 195, 50, 255};
    static const SDL_Color DIM = {100, 100, 130, 255};
    font_draw_str(ren, "TILE EDIT", BX + PANEL_EDIT_MARGIN, PANEL_EDIT_MARGIN, YLW);

    bool s16 = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2);
    int edit_dim = s16 ? 16 : 8;
    int pixel_sz = (s->panel_w - 2 * PANEL_EDIT_MARGIN) / edit_dim;
    int edit_sz  = pixel_sz * edit_dim;
    int edit_x0  = BX + (s->panel_w - edit_sz) / 2;
    int edit_y0  = PANEL_EDIT_MARGIN + 20;

    int base = sel_tile_idx_r(s);

    /* Draw enlarged tile pixels */
    for (int py = 0; py < edit_dim; py++) {
        for (int px = 0; px < edit_dim; px++) {
            int tile, lr, lc;
            if (s16) {
                int sub_x = px / TILE_W;
                int sub_y = py / TILE_H;
                int p = sub_x * 2 + sub_y;
                tile = base + p;
                lr = py % TILE_H;
                lc = px % TILE_W;
            } else {
                tile = base;
                lr = py;
                lc = px;
            }
            uint8_t val = s->chr.px[tile][lr][lc] & 3;
            SDL_Color c = get_display_color(s, tile, val);
            fill(ren, edit_x0 + px * pixel_sz, edit_y0 + py * pixel_sz,
                 pixel_sz, pixel_sz, c.r, c.g, c.b);
        }
    }

    /* Pixel grid */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 80, 80, 110, 100);
    for (int i = 1; i < edit_dim; i++) {
        int x = edit_x0 + i * pixel_sz;
        SDL_RenderDrawLine(ren, x, edit_y0, x, edit_y0 + edit_sz - 1);
        int y = edit_y0 + i * pixel_sz;
        SDL_RenderDrawLine(ren, edit_x0, y, edit_x0 + edit_sz - 1, y);
    }
    /* Tile boundary lines for sprite-16 */
    if (s16) {
        SDL_SetRenderDrawColor(ren, 60, 100, 200, 180);
        int mid_x = edit_x0 + TILE_W * pixel_sz;
        SDL_RenderDrawLine(ren, mid_x, edit_y0, mid_x, edit_y0 + edit_sz - 1);
        int mid_y = edit_y0 + TILE_H * pixel_sz;
        SDL_RenderDrawLine(ren, edit_x0, mid_y, edit_x0 + edit_sz - 1, mid_y);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    /* Amber border */
    SDL_SetRenderDrawColor(ren, 255, 210, 40, 255);
    SDL_Rect border = { edit_x0 - 1, edit_y0 - 1, edit_sz + 2, edit_sz + 2 };
    SDL_RenderDrawRect(ren, &border);

    /* Colour swatches */
    int sw_y  = edit_y0 + edit_sz + 8;
    int sw_sz = 14;
    for (int i = 0; i < 4; i++) {
        int sx = edit_x0 + i * (sw_sz + 3);
        if (i == s->color)
            fill(ren, sx - 1, sw_y - 1, sw_sz + 2, sw_sz + 2, 220, 220, 220);
        SDL_Color c = get_display_color(s, base, i);
        fill(ren, sx, sw_y, sw_sz, sw_sz, c.r, c.g, c.b);
    }

    /* Wrap mode label */
    const char *wlabel = "WRAP:NONE";
    if (s->wrap_mode == WRAP_H)    wlabel = "WRAP:H";
    if (s->wrap_mode == WRAP_V)    wlabel = "WRAP:V";
    if (s->wrap_mode == WRAP_BOTH) wlabel = "WRAP:HV";
    font_draw_str(ren, wlabel, edit_x0, sw_y + sw_sz + 8, DIM);

    /* Hints */
    font_draw_str(ren, "ESC:BACK  W:WRAP", BX + PANEL_EDIT_MARGIN,
                  s->win_h - STATUS_H - font_line_h() - 4, DIM);
}

/* ── Palette panel ────────────────────────────────────────────── */

static void render_panel(SDL_Renderer *ren, const EditorState *s) {
    const int BX = s->canvas_w;   /* panel left edge in screen coords */

    /* Runtime NES picker geometry (cell size scales with zoom). */
    int nes_cell      = s->zoom * PANEL_NES_CELL_BASE;
    int nes_step      = nes_cell + PANEL_NES_GAP;
    int nes_x0        = (s->panel_w - PANEL_NES_COLS * nes_step + PANEL_NES_GAP) / 2;
    int panel_view_y0 = PANEL_NES_Y0 + PANEL_NES_ROWS * nes_step + 10;

    fill(ren, BX, 0, s->panel_w, s->win_h - STATUS_H, 18, 18, 30);

    for (int i = 0; i < 8; i++) {
        int ry = PANEL_PAL_Y0 + i * PANEL_PAL_ROW;

        if (i == s->active_sub_pal)
            fill(ren, BX + PANEL_PAL_X0 - 1, ry - 1,
                 4*(PANEL_PAL_SW + PANEL_PAL_XGAP) + 1, PANEL_PAL_SH + 2,
                 50, 50, 80);

        if (i < 4)
            fill(ren, BX + 1, ry, 2, PANEL_PAL_SH,  50,  80, 160);
        else
            fill(ren, BX + 1, ry, 2, PANEL_PAL_SH, 160,  60,  60);

        for (int j = 0; j < 4; j++) {
            int sx = BX + PANEL_PAL_X0 + j * (PANEL_PAL_SW + PANEL_PAL_XGAP);
            SDL_Color c = NES_MASTER_PALETTE[s->pal.sub[i].idx[j] & 0x3F];
            fill(ren, sx, ry, PANEL_PAL_SW, PANEL_PAL_SH, c.r, c.g, c.b);
        }

        /* Active-palette marker: amber * to the right of the swatches */
        if (i == s->active_sub_pal) {
            static const SDL_Color MARKC = {220, 200, 40, 255};
            font_draw_char(ren, '*', BX + 104, ry - 2, MARKC);
        }
    }

    {
        int sep = PANEL_PAL_Y0 + 4 * PANEL_PAL_ROW - 3;
        hline(ren, BX + 2, sep, s->panel_w - 4, 55, 55, 80);
    }

    hline(ren, BX + 2, PANEL_ACT_Y0 - 4, s->panel_w - 4, 55, 55, 80);

    for (int j = 0; j < 4; j++) {
        int sx = BX + PANEL_PAL_X0 + j * (PANEL_ACT_SW + PANEL_ACT_XGAP);

        if (j == s->active_swatch)
            fill(ren, sx-2, PANEL_ACT_Y0-2, PANEL_ACT_SW+4, PANEL_ACT_SH+4,
                 220, 220, 220);
        else
            fill(ren, sx-1, PANEL_ACT_Y0-1, PANEL_ACT_SW+2, PANEL_ACT_SH+2,
                 60, 60, 70);

        SDL_Color c = NES_MASTER_PALETTE[
            s->pal.sub[s->active_sub_pal].idx[j] & 0x3F];
        fill(ren, sx, PANEL_ACT_Y0, PANEL_ACT_SW, PANEL_ACT_SH,
             c.r, c.g, c.b);
    }

    hline(ren, BX + 2, PANEL_NES_Y0 - 4, s->panel_w - 4, 55, 55, 80);

    uint8_t cur = s->pal.sub[s->active_sub_pal].idx[s->active_swatch] & 0x3F;

    for (int row = 0; row < PANEL_NES_ROWS; row++) {
        for (int col = 0; col < PANEL_NES_COLS; col++) {
            int  idx = row * PANEL_NES_COLS + col;
            int  sx  = BX + nes_x0 + col * nes_step;
            int  sy  =      PANEL_NES_Y0 + row * nes_step;
            SDL_Color c = NES_MASTER_PALETTE[idx];

            if ((uint8_t)idx == cur)
                fill(ren, sx-1, sy-1, nes_cell+2, nes_cell+2, 255, 255, 255);

            fill(ren, sx, sy, nes_cell, nes_cell, c.r, c.g, c.b);
        }
    }

    hline(ren, BX + 2, panel_view_y0 - 4, s->panel_w - 4, 55, 55, 80);

    /* View-mode indicator dot */
    bool nes = (s->view_mode == VIEW_NES_COLOR);
    fill(ren, BX + 4, panel_view_y0, 12, 12,
         nes ? 40  : 28,
         nes ? 180 : 40,
         nes ? 40  : 28);

    /* Help (?) button — click zone: py in [panel_view_y0, +16), px in [22, 34) */
    {
        bool h = s->show_help;
        fill(ren, BX + 21, panel_view_y0 - 1, 14, 18,
             h ? 60 : 32, h ? 80 : 32, h ? 190 : 55);
        SDL_Color qcol = {200, 210, 240, 255};
        font_draw_char(ren, '?', BX + 22, panel_view_y0 + 1, qcol);
    }

    render_anim_section(ren, s, panel_view_y0);
}

/* ── Help overlay ─────────────────────────────────────────────── */
static void render_help_overlay(SDL_Renderer *ren, const EditorState *s) {
    if (!s->show_help) return;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 18, 228);
    SDL_Rect bg = {0, 0, s->win_w, s->win_h - STATUS_H};
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 60, 80, 180, 255);
    SDL_RenderDrawRect(ren, &bg);

    static const SDL_Color WHT = {210, 210, 210, 255};
    static const SDL_Color DIM = {120, 120, 150, 255};
    static const SDL_Color YLW = {220, 195,  50, 255};
    static const SDL_Color CYN = {100, 190, 210, 255};

    int x  = 20;
    int y  = 14;
    int lh = font_line_h();
    int hg = lh / 2;

    font_draw_str(ren, "CHRMAKER - NES CHR EDITOR", x, y, YLW); y += lh + hg;

    font_draw_str(ren, "DRAWING",                         x, y, CYN); y += lh;
    font_draw_str(ren, " 0-3    SELECT COLOUR",           x, y, WHT); y += lh;
    font_draw_str(ren, " LMB    PAINT PIXELS",            x, y, WHT); y += lh;
    font_draw_str(ren, " RMB    ASSIGN/PAINT PALETTE",    x, y, WHT); y += lh + hg;

    font_draw_str(ren, "TILE MODE",                       x, y, CYN); y += lh;
    font_draw_str(ren, " T      SELECT TILE",             x, y, WHT); y += lh;
    font_draw_str(ren, " W      CYCLE WRAP MODE",         x, y, WHT); y += lh;
    font_draw_str(ren, " E      TILE EDIT (PANEL)",       x, y, WHT); y += lh;
    font_draw_str(ren, " [/]    CYCLE TILE PALETTE",      x, y, WHT); y += lh;
    font_draw_str(ren, " ESC    EXIT TILE MODE",          x, y, WHT); y += lh + hg;

    font_draw_str(ren, "VIEW & GRID",                     x, y, CYN); y += lh;
    font_draw_str(ren, " V      GRAYSCALE / NES COLOUR",  x, y, WHT); y += lh;
    font_draw_str(ren, " G      TILE GRID",               x, y, WHT); y += lh;
    font_draw_str(ren, " P      PIXEL GRID",              x, y, WHT); y += lh;
    font_draw_str(ren, " M      SPRITE 16 MODE",          x, y, WHT); y += lh;
    font_draw_str(ren, " N      SHOW TILE ADDRESS",        x, y, WHT); y += lh + hg;

    font_draw_str(ren, "FILES & CANVAS",                  x, y, CYN); y += lh;
    font_draw_str(ren, " CTRL+S      SAVE",               x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+SHFT+S SAVE AS",            x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+O      OPEN",               x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+R      RESIZE CANVAS",      x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+SHFT+P SAVE PALETTE",       x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+P      LOAD PALETTE",       x, y, WHT); y += lh;
    font_draw_str(ren, " DROP FILE   OPEN",               x, y, WHT); y += lh;
    font_draw_str(ren, " =/-         ZOOM IN/OUT",        x, y, WHT); y += lh + hg;

    font_draw_str(ren, "COMPOSE MODE",                     x, y, CYN); y += lh;
    font_draw_str(ren, " TAB         COMPOSE MODE",       x, y, WHT); y += lh + hg;

    font_draw_str(ren, " F1 OR ?   TOGGLE HELP",          x, y, DIM); y += lh;
    font_draw_str(ren, " ESC       QUIT",                 x, y, DIM);
    (void)y;
}

/* ── Text-input overlay ───────────────────────────────────────── */
static void render_input_overlay(SDL_Renderer *ren, const EditorState *s) {
    if (!s->input_mode) return;

    const int BOX_H  = 76;
    const int BOX_W  = (s->win_w - 20 < 480) ? s->win_w - 20 : 480;
    const int BOX_X  = (s->win_w - BOX_W) / 2;
    const int BOX_Y  = (s->win_h - STATUS_H - BOX_H) / 2;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 8, 8, 28, 235);
    SDL_Rect bg = {BOX_X, BOX_Y, BOX_W, BOX_H};
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 80, 120, 220, 255);
    SDL_RenderDrawRect(ren, &bg);

    static const SDL_Color WHT = {220, 220, 220, 255};
    static const SDL_Color YLW = {220, 195,  50, 255};
    static const SDL_Color DIM = {120, 120, 150, 255};

    int tx = BOX_X + 12;
    int ty = BOX_Y + 10;
    int lh = font_line_h();

    const char *title;
    if      (s->input_type == INPUT_SAVE_AS)  title = "SAVE AS:";
    else if (s->input_type == INPUT_OPEN)     title = "OPEN FILE:";
    else if (s->input_type == INPUT_OPEN_PAL) title = "OPEN PALETTE:";
    else                                      title = "RESIZE (COLSxROWS):";
    font_draw_str(ren, title, tx, ty, YLW);
    ty += lh;

    /* Show as many trailing chars as fit in the box; cursor always visible. */
    int  max_chars = (BOX_W - 24) / font_char_w();
    if (max_chars < 1) max_chars = 1;
    char display[40];
    int  start = (s->input_len > max_chars - 1) ? (s->input_len - max_chars + 1) : 0;
    int  dlen  = s->input_len - start;
    memcpy(display, s->input_buf + start, (size_t)dlen);
    bool cursor_on = (SDL_GetTicks() % 1000) < 500;
    display[dlen]     = cursor_on ? '_' : ' ';
    display[dlen + 1] = '\0';
    font_draw_str(ren, display, tx, ty, WHT);
    ty += lh;

    font_draw_str(ren, "ENTER=OK  ESC=CANCEL", tx, ty + 2, DIM);
}

/* ── Compose canvas rendering ─────────────────────────────────── */

static SDL_Color compose_get_bg_color(const EditorState *s, int pal_idx, int val) {
    uint8_t master = s->pal.sub[pal_idx & 3].idx[val & 3] & 0x3F;
    return NES_MASTER_PALETTE[master];
}

static SDL_Color compose_get_spr_color(const EditorState *s, int pal_idx, int val) {
    uint8_t master = s->pal.sub[pal_idx & 7].idx[val & 3] & 0x3F;
    return NES_MASTER_PALETTE[master];
}

static void render_compose_canvas(const EditorState *s) {
    if (!compose_tex) return;

    void *pixels; int pitch;
    if (SDL_LockTexture(compose_tex, NULL, &pixels, &pitch) != 0) return;

    uint32_t *dst    = (uint32_t *)pixels;
    int       stride = pitch / 4;

    const ComposeScene *sc = &s->compose.scenes[s->compose.active_scene];

    /* Universal background color: palette 0, color 0 */
    SDL_Color bg = compose_get_bg_color(s, 0, 0);
    uint32_t bg_px = (0xFFu << 24) | ((uint32_t)bg.r << 16) |
                     ((uint32_t)bg.g << 8) | bg.b;
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 256; x++)
            dst[y * stride + x] = bg_px;

    /* Draw nametable (BG tiles) */
    for (int ty = 0; ty < COMPOSE_NT_H; ty++) {
        for (int tx = 0; tx < COMPOSE_NT_W; tx++) {
            uint8_t tile_idx = sc->nametable[ty][tx];
            int pal_idx = sc->attr[ty / 2][tx / 2] & 3;

            for (int row = 0; row < TILE_H; row++) {
                for (int col = 0; col < TILE_W; col++) {
                    int px_x = tx * TILE_W + col;
                    int px_y = ty * TILE_H + row;
                    if (px_x >= 256 || px_y >= 240) continue;
                    uint8_t val = s->chr.px[tile_idx][row][col] & 3;
                    if (tile_idx == 0 && val == 0) continue; /* transparent BG */
                    SDL_Color c = compose_get_bg_color(s, pal_idx, val);
                    dst[px_y * stride + px_x] =
                        (0xFFu << 24) | ((uint32_t)c.r << 16) |
                        ((uint32_t)c.g << 8) | c.b;
                }
            }
        }
    }

    /* Draw sprites (front to back — later sprites draw on top) */
    for (int i = 0; i < sc->sprite_count; i++) {
        const ComposeSprite *sp = &sc->sprites[i];
        bool s16 = (s->sprite_mode == SPRITE_16);
        int spr_w = s16 ? 16 : 8;
        int spr_h = s16 ? 16 : 8;

        for (int row = 0; row < spr_h; row++) {
            for (int col = 0; col < spr_w; col++) {
                int src_col = sp->hflip ? (spr_w - 1 - col) : col;
                int src_row = sp->vflip ? (spr_h - 1 - row) : row;

                int tile;
                int lr, lc;
                if (s16) {
                    /* Column-major layout: [0][2] / [1][3] */
                    int sub_x = src_col / TILE_W;
                    int sub_y = src_row / TILE_H;
                    int p = sub_x * 2 + sub_y;
                    tile = sp->tile + p;
                    lr = src_row % TILE_H;
                    lc = src_col % TILE_W;
                } else {
                    tile = sp->tile;
                    lr = src_row;
                    lc = src_col;
                }

                if (tile >= CHR_MAX_TILES) continue;
                uint8_t val = s->chr.px[tile][lr][lc] & 3;
                if (val == 0) continue; /* transparent */

                int px_x = sp->x + col;
                int px_y = sp->y + row;
                if (px_x >= 256 || px_y >= 240) continue;

                SDL_Color c = compose_get_spr_color(s, sp->palette, val);
                dst[px_y * stride + px_x] =
                    (0xFFu << 24) | ((uint32_t)c.r << 16) |
                    ((uint32_t)c.g << 8) | c.b;
            }
        }
    }

    SDL_UnlockTexture(compose_tex);
}

/* ── Compose: attribute grid overlay ─────────────────────────── */
static void render_compose_attr_grid(SDL_Renderer *ren, const EditorState *s) {
    if (!s->compose_show_attr_grid) return;

    int z = s->compose_zoom;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 100, 60, 60, 80);

    /* Vertical lines every 16px (2 tiles) */
    for (int i = 1; i < 16; i++) {
        int x = i * 16 * z;
        SDL_RenderDrawLine(ren, x, 0, x, s->compose_canvas_h - 1);
    }
    /* Horizontal lines every 16px */
    for (int i = 1; i < 15; i++) {
        int y = i * 16 * z;
        SDL_RenderDrawLine(ren, 0, y, s->compose_canvas_w - 1, y);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* ── Compose: hover ghost + highlights ───────────────────────── */
static void render_compose_hover(SDL_Renderer *ren, const EditorState *s) {
    int hx = s->compose_hover_x;
    int hy = s->compose_hover_y;
    if (hx < 0 || hy < 0) return;

    int z = s->compose_zoom;

    if (s->compose_layer == COMPOSE_BG) {
        /* Translucent ghost of brush tile */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (int row = 0; row < TILE_H; row++) {
            for (int col = 0; col < TILE_W; col++) {
                uint8_t val = s->chr.px[s->brush_tile & 0xFF][row][col] & 3;
                SDL_Color c = compose_get_bg_color(s, s->active_sub_pal, val);
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 120);
                SDL_Rect r = { hx * TILE_W * z + col * z,
                               hy * TILE_H * z + row * z, z, z };
                SDL_RenderFillRect(ren, &r);
            }
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        /* Border */
        SDL_SetRenderDrawColor(ren, 200, 200, 60, 255);
        SDL_Rect border = { hx * TILE_W * z, hy * TILE_H * z, TILE_W * z, TILE_H * z };
        SDL_RenderDrawRect(ren, &border);
    }
}

/* ── Compose: selected sprite highlight ──────────────────────── */
static void render_compose_spr_highlight(SDL_Renderer *ren, const EditorState *s) {
    if (s->compose_spr_sel < 0) return;
    const ComposeScene *sc = &s->compose.scenes[s->compose.active_scene];
    if (s->compose_spr_sel >= sc->sprite_count) return;

    const ComposeSprite *sp = &sc->sprites[s->compose_spr_sel];
    int z = s->compose_zoom;
    int spr_w = (s->sprite_mode == SPRITE_16) ? 16 : 8;
    int spr_h = (s->sprite_mode == SPRITE_16) ? 16 : 8;

    SDL_SetRenderDrawColor(ren, 0, 220, 255, 255);
    SDL_Rect border = { sp->x * z, sp->y * z, spr_w * z, spr_h * z };
    SDL_RenderDrawRect(ren, &border);
}

/* ── Compose panel ───────────────────────────────────────────── */
static void render_compose_panel(SDL_Renderer *ren, const EditorState *s) {
    const int BX = s->compose_canvas_w;
    int pw = s->panel_w;

    /* Background */
    fill(ren, BX, 0, pw, s->win_h - STATUS_H, 18, 18, 30);

    static const SDL_Color WHT = {200, 200, 200, 255};
    static const SDL_Color DIM = {100, 100, 130, 255};
    static const SDL_Color YLW = {220, 195,  50, 255};

    int pscale    = COMPOSE_PICKER_SCALE;
    int picker_x0 = BX + PANEL_PAL_X0;
    int picker_y0 = 8;
    int picker_w  = s->chr_cols * TILE_W * pscale;
    int picker_h  = s->chr_rows * TILE_H * pscale;
    int ntiles    = s->chr_cols * s->chr_rows;

    /* Right column x: after picker + gap */
    int ctrl_x = picker_x0 + picker_w + 8;

    /* ── Left column: CHR picker (COMPOSE_PICKER_SCALE×) ── */
    fill(ren, picker_x0 - 1, picker_y0 - 1, picker_w + 2, picker_h + 2, 8, 8, 8);

    for (int t = 0; t < ntiles && t < CHR_MAX_TILES; t++) {
        int tx_off = (t % s->chr_cols) * TILE_W * pscale;
        int ty_off = (t / s->chr_cols) * TILE_H * pscale;

        for (int row = 0; row < TILE_H; row++) {
            for (int col = 0; col < TILE_W; col++) {
                uint8_t val = s->chr.px[t][row][col] & 3;
                SDL_Color c = get_display_color(s, t, val);
                fill(ren, picker_x0 + tx_off + col * pscale,
                          picker_y0 + ty_off + row * pscale,
                          pscale, pscale, c.r, c.g, c.b);
            }
        }
    }

    /* Highlight selected brush tile */
    {
        int bt = s->brush_tile;
        if (bt >= 0 && bt < ntiles) {
            int bx = (bt % s->chr_cols) * TILE_W * pscale;
            int by = (bt / s->chr_cols) * TILE_H * pscale;
            SDL_SetRenderDrawColor(ren, 255, 210, 40, 255);
            SDL_Rect hl = { picker_x0 + bx, picker_y0 + by,
                            TILE_W * pscale, TILE_H * pscale };
            SDL_RenderDrawRect(ren, &hl);
        }
    }

    /* ── Right column: controls ── */
    int y = picker_y0;

    /* Brush preview (4x) */
    font_draw_str(ren, "BRUSH", ctrl_x, y, DIM);
    y += font_line_h();
    {
        int prev_sz = 32; /* 8 * 4x */
        fill(ren, ctrl_x - 1, y - 1, prev_sz + 2, prev_sz + 2, 8, 8, 8);

        int bt = s->brush_tile & 0xFF;
        for (int row = 0; row < TILE_H; row++) {
            for (int col = 0; col < TILE_W; col++) {
                int src_r = s->brush_vflip ? (TILE_H - 1 - row) : row;
                int src_c = s->brush_hflip ? (TILE_W - 1 - col) : col;
                uint8_t val = s->chr.px[bt][src_r][src_c] & 3;
                SDL_Color c = get_display_color(s, bt, val);
                fill(ren, ctrl_x + col * 4, y + row * 4, 4, 4, c.r, c.g, c.b);
            }
        }

        /* Flip indicators next to preview */
        int info_x = ctrl_x + prev_sz + 8;
        if (s->compose_layer == COMPOSE_SPR) {
            SDL_Color fc = s->brush_hflip ? YLW : DIM;
            font_draw_str(ren, "H", info_x, y, fc);
            fc = s->brush_vflip ? YLW : DIM;
            font_draw_str(ren, "V", info_x, y + font_line_h(), fc);
        }
    }
    y += 32 + 8;

    /* Palette rows */
    font_draw_str(ren, "PAL", ctrl_x, y, DIM);
    y += font_line_h();

    for (int i = 0; i < 8; i++) {
        int ry = y + i * 14;

        if (i == s->active_sub_pal)
            fill(ren, ctrl_x - 1, ry - 1,
                 4 * (10 + 2) + 1, 10 + 2, 50, 50, 80);

        /* BG/SPR indicator */
        if (i < 4)
            fill(ren, ctrl_x - 3, ry, 2, 10, 50, 80, 160);
        else
            fill(ren, ctrl_x - 3, ry, 2, 10, 160, 60, 60);

        for (int j = 0; j < 4; j++) {
            int sx = ctrl_x + j * 12;
            SDL_Color c = NES_MASTER_PALETTE[s->pal.sub[i].idx[j] & 0x3F];
            fill(ren, sx, ry, 10, 10, c.r, c.g, c.b);
        }
    }
    y += 8 * 14 + 8;

    /* Layer toggle */
    {
        bool is_bg = (s->compose_layer == COMPOSE_BG);
        SDL_Color bgc = is_bg ? YLW : DIM;
        SDL_Color spc = is_bg ? DIM : YLW;
        font_draw_str(ren, "BG", ctrl_x, y, bgc);
        font_draw_str(ren, "/", ctrl_x + 2 * font_char_w(), y, DIM);
        font_draw_str(ren, "SPR", ctrl_x + 3 * font_char_w(), y, spc);
    }
    y += font_line_h() + 4;

    /* Scene navigator */
    {
        char sbuf[24];
        snprintf(sbuf, sizeof(sbuf), "SCN %d/%d",
                 s->compose.active_scene + 1, s->compose.scene_count);
        font_draw_str(ren, sbuf, ctrl_x, y, WHT);
        font_draw_str(ren, "PGUP/DN", ctrl_x, y + font_line_h(), DIM);
    }
    y += font_line_h() * 2 + 4;

    /* Sprite counter */
    {
        const ComposeScene *sc = &s->compose.scenes[s->compose.active_scene];
        char sbuf[24];
        snprintf(sbuf, sizeof(sbuf), "SPR %d/%d", sc->sprite_count, COMPOSE_MAX_SPR);
        SDL_Color sc_col = (sc->sprite_count >= COMPOSE_MAX_SPR)
                               ? (SDL_Color){220, 60, 60, 255} : WHT;
        font_draw_str(ren, sbuf, ctrl_x, y, sc_col);
    }
    y += font_line_h() + 8;

    /* Hints */
    font_draw_str(ren, "TAB:EXIT", ctrl_x, y, DIM);
    y += font_line_h();
    font_draw_str(ren, "?:HELP", ctrl_x, y, DIM);
    y += font_line_h();
    font_draw_str(ren, "CTRL+S:SAVE", ctrl_x, y, DIM);

    /* Panel separator line */
    vline(ren, BX, 0, s->win_h - STATUS_H, 55, 55, 80);
}

/* ── Compose status bar ──────────────────────────────────────── */
static void render_compose_status(SDL_Renderer *ren, const EditorState *s) {
    const int STATUS_Y = s->win_h - STATUS_H;
    fill(ren, 0, STATUS_Y, s->win_w, STATUS_H, 12, 12, 12);
    hline(ren, 0, STATUS_Y, s->win_w, 55, 55, 80);

    int ty = STATUS_Y + (STATUS_H - font_line_h()) / 2 + 1;
    int cw = font_char_w();

    /* COMPOSE indicator */
    static const SDL_Color CMPC = {180, 120, 220, 255};
    font_draw_str(ren, "COMPOSE", 4, ty, CMPC);

    /* Layer indicator */
    int lx = 4 + 8 * cw;
    if (s->compose_layer == COMPOSE_BG) {
        static const SDL_Color BGC = {80, 140, 220, 255};
        font_draw_str(ren, "BG", lx, ty, BGC);
    } else {
        static const SDL_Color SPC = {220, 100, 80, 255};
        font_draw_str(ren, "SPR", lx, ty, SPC);
    }

    /* Hover tile coords */
    if (s->compose_hover_x >= 0 && s->compose_hover_y >= 0) {
        char cbuf[24];
        snprintf(cbuf, sizeof(cbuf), "%d,%d", s->compose_hover_x, s->compose_hover_y);
        static const SDL_Color POS = {140, 160, 200, 255};
        font_draw_str(ren, cbuf, lx + 5 * cw, ty, POS);
    }

    /* Zoom — right-aligned */
    char zbuf[4];
    snprintf(zbuf, sizeof(zbuf), "%dX", s->compose_zoom);
    int zx = s->win_w - (int)strlen(zbuf) * cw - 4;
    static const SDL_Color ZCOL = {140, 160, 200, 255};
    font_draw_str(ren, zbuf, zx, ty, ZCOL);
}

/* ── Compose help overlay ─────────────────────────────────────── */
static void render_compose_help(SDL_Renderer *ren, const EditorState *s) {
    if (!s->compose_show_help) return;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 18, 228);
    SDL_Rect bg = {0, 0, s->win_w, s->win_h - STATUS_H};
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 120, 60, 180, 255);
    SDL_RenderDrawRect(ren, &bg);

    static const SDL_Color WHT = {210, 210, 210, 255};
    static const SDL_Color DIM = {120, 120, 150, 255};
    static const SDL_Color YLW = {220, 195,  50, 255};
    static const SDL_Color CYN = {100, 190, 210, 255};

    int x  = 20;
    int y  = 14;
    int lh = font_line_h();
    int hg = lh / 2;

    font_draw_str(ren, "COMPOSE MODE", x, y, YLW); y += lh + hg;

    font_draw_str(ren, "LAYERS",                          x, y, CYN); y += lh;
    font_draw_str(ren, " B       BG LAYER",               x, y, WHT); y += lh;
    font_draw_str(ren, " L       SPRITE LAYER",           x, y, WHT); y += lh + hg;

    font_draw_str(ren, "BG LAYER",                        x, y, CYN); y += lh;
    font_draw_str(ren, " LMB     PLACE TILE",             x, y, WHT); y += lh;
    font_draw_str(ren, " RMB     EYEDROPPER",             x, y, WHT); y += lh;
    font_draw_str(ren, " SHFT+LMB  ERASE TILE",          x, y, WHT); y += lh;
    font_draw_str(ren, " [/]     CYCLE PALETTE (0-3)",    x, y, WHT); y += lh + hg;

    font_draw_str(ren, "SPRITE LAYER",                    x, y, CYN); y += lh;
    font_draw_str(ren, " LMB     PLACE / SELECT SPRITE",  x, y, WHT); y += lh;
    font_draw_str(ren, " DRAG    MOVE SPRITE",            x, y, WHT); y += lh;
    font_draw_str(ren, " RMB     DELETE SPRITE",          x, y, WHT); y += lh;
    font_draw_str(ren, " ARROWS  NUDGE SPRITE 1PX",      x, y, WHT); y += lh;
    font_draw_str(ren, " DEL     DELETE SELECTED",        x, y, WHT); y += lh;
    font_draw_str(ren, " H/F     FLIP H/V (BRUSH)",      x, y, WHT); y += lh;
    font_draw_str(ren, " [/]     CYCLE PALETTE (4-7)",    x, y, WHT); y += lh + hg;

    font_draw_str(ren, "VIEW & SCENES",                   x, y, CYN); y += lh;
    font_draw_str(ren, " G       TOGGLE ATTR GRID",       x, y, WHT); y += lh;
    font_draw_str(ren, " =/-     ZOOM IN/OUT",            x, y, WHT); y += lh;
    font_draw_str(ren, " PGUP/DN SWITCH SCENE",           x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+N  ADD NEW SCENE",          x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+S  SAVE SCENE FILE",        x, y, WHT); y += lh + hg;

    font_draw_str(ren, " TAB/ESC EXIT COMPOSE",           x, y, DIM); y += lh;
    font_draw_str(ren, " F1 OR ? TOGGLE HELP",            x, y, DIM);
    (void)y;
}

/* ── Main render entry ────────────────────────────────────────── */
void render_frame(SDL_Renderer *ren, const EditorState *s) {
    set_color(ren, 10, 10, 10);
    SDL_RenderClear(ren);

    if (s->compose_mode) {
        render_compose_canvas(s);
        SDL_Rect dst = {0, 0, s->compose_canvas_w, s->compose_canvas_h};
        SDL_RenderCopy(ren, compose_tex, NULL, &dst);

        render_compose_attr_grid(ren, s);
        render_compose_hover(ren, s);
        render_compose_spr_highlight(ren, s);
        render_compose_panel(ren, s);
        render_compose_status(ren, s);
        render_compose_help(ren, s);
        SDL_RenderPresent(ren);
        return;
    }

    render_canvas(s);
    SDL_Rect canvas_dst = {0, 0, s->canvas_w, s->canvas_h};
    SDL_RenderCopy(ren, canvas_tex, NULL, &canvas_dst);

    if (s->show_pixel_grid) render_pixel_grid(ren, s);
    if (s->show_tile_grid)  render_tile_grid(ren, s);

    render_tile_highlight(ren, s);
    render_anim_frame_highlight(ren, s);
    render_anim_ghosts(ren, s);
    if (s->tile_edit)
        render_tile_edit_panel(ren, s);
    else
        render_panel(ren, s);
    render_status(ren, s);

    vline(ren, s->canvas_w, 0,                   s->win_h - STATUS_H, 55, 55, 80);
    hline(ren, 0,           s->win_h - STATUS_H, s->win_w,            55, 55, 80);

    render_help_overlay(ren, s);
    render_input_overlay(ren, s);

    SDL_RenderPresent(ren);
}
