#include "render.h"
#include "panel.h"
#include "font.h"
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
static SDL_Texture *canvas_tex = NULL;

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

void render_init(SDL_Renderer *ren, const EditorState *s) {
    create_canvas_tex(ren, s);
}

void render_resize(SDL_Renderer *ren, const EditorState *s) {
    if (canvas_tex) { SDL_DestroyTexture(canvas_tex); canvas_tex = NULL; }
    create_canvas_tex(ren, s);
}

void render_destroy(void) {
    if (canvas_tex) { SDL_DestroyTexture(canvas_tex); canvas_tex = NULL; }
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

    int tx = 76;
    fill(ren, tx - 1, SY - 1, SW + 2, SH + 2, 55, 55, 55);
    if (s->tile_mode)
        fill(ren, tx, SY, SW, SH, 210, 155, 20);
    else
        fill(ren, tx, SY, SW, SH, 38,  38,  38);

    int hx = 96;
    bool h_on = (s->wrap_mode == WRAP_H || s->wrap_mode == WRAP_BOTH);
    fill(ren, hx - 1, SY - 1, SW + 2, SH + 2, 55, 55, 55);
    fill(ren, hx, SY, SW, SH, 28, 28, 28);
    if (h_on) hline(ren, hx + 1, SY + SH / 2, SW - 2, 60, 210, 60);

    int vx = 116;
    bool v_on = (s->wrap_mode == WRAP_V || s->wrap_mode == WRAP_BOTH);
    fill(ren, vx - 1, SY - 1, SW + 2, SH + 2, 55, 55, 55);
    fill(ren, vx, SY, SW, SH, 28, 28, 28);
    if (v_on) vline(ren, vx + SW / 2, SY + 1, SH - 2, 60, 210, 60);

    /* Zoom indicator */
    {
        char zbuf[4];
        snprintf(zbuf, sizeof(zbuf), "%dX", s->zoom);
        static const SDL_Color ZCOL = {140, 160, 200, 255};
        font_draw_str(ren, zbuf, 140, STATUS_Y + (STATUS_H - font_line_h()) / 2 + 1, ZCOL);
    }

    /* Sprite-16 mode indicator */
    if (s->sprite_mode == SPRITE_16) {
        static const SDL_Color S16COL = {80, 210, 140, 255};
        font_draw_str(ren, "S16", 170, STATUS_Y + (STATUS_H - font_line_h()) / 2 + 1, S16COL);
    }
}

/* ── Palette panel ────────────────────────────────────────────── */

static void render_panel(SDL_Renderer *ren, const EditorState *s) {
    const int BX = s->canvas_w;   /* panel left edge in screen coords */

    fill(ren, BX, 0, PANEL_W, s->win_h - STATUS_H, 18, 18, 30);

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
    }

    {
        int sep = PANEL_PAL_Y0 + 4 * PANEL_PAL_ROW - 3;
        hline(ren, BX + 2, sep, PANEL_W - 4, 55, 55, 80);
    }

    hline(ren, BX + 2, PANEL_ACT_Y0 - 4, PANEL_W - 4, 55, 55, 80);

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

    hline(ren, BX + 2, PANEL_NES_Y0 - 4, PANEL_W - 4, 55, 55, 80);

    uint8_t cur = s->pal.sub[s->active_sub_pal].idx[s->active_swatch] & 0x3F;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int  idx = row * 8 + col;
            int  sx  = BX + PANEL_NES_X0 + col * PANEL_NES_STEP;
            int  sy  =       PANEL_NES_Y0 + row * PANEL_NES_STEP;
            SDL_Color c = NES_MASTER_PALETTE[idx];

            if ((uint8_t)idx == cur)
                fill(ren, sx-1, sy-1, PANEL_NES_SZ+2, PANEL_NES_SZ+2,
                     255, 255, 255);

            fill(ren, sx, sy, PANEL_NES_SZ, PANEL_NES_SZ, c.r, c.g, c.b);
        }
    }

    hline(ren, BX + 2, PANEL_VIEW_Y0 - 4, PANEL_W - 4, 55, 55, 80);

    /* View-mode indicator dot */
    bool nes = (s->view_mode == VIEW_NES_COLOR);
    fill(ren, BX + 4, PANEL_VIEW_Y0, 12, 12,
         nes ? 40  : 28,
         nes ? 180 : 40,
         nes ? 40  : 28);

    /* Help (?) button — click zone: py in [PANEL_VIEW_Y0, +16), px in [22, 34) */
    {
        bool h = s->show_help;
        fill(ren, BX + 21, PANEL_VIEW_Y0 - 1, 14, 18,
             h ? 60 : 32, h ? 80 : 32, h ? 190 : 55);
        SDL_Color qcol = {200, 210, 240, 255};
        font_draw_char(ren, '?', BX + 22, PANEL_VIEW_Y0 + 1, qcol);
    }
}

/* ── Help overlay ─────────────────────────────────────────────── */
static void render_help_overlay(SDL_Renderer *ren, const EditorState *s) {
    if (!s->show_help) return;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 18, 228);
    SDL_Rect bg = {0, 0, s->canvas_w, s->win_h - STATUS_H};
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
    font_draw_str(ren, " RMB    ASSIGN PAL TO TILE",      x, y, WHT); y += lh + hg;

    font_draw_str(ren, "TILE MODE",                       x, y, CYN); y += lh;
    font_draw_str(ren, " T      SELECT TILE",             x, y, WHT); y += lh;
    font_draw_str(ren, " W      CYCLE WRAP MODE",         x, y, WHT); y += lh;
    font_draw_str(ren, " [/]    CYCLE TILE PALETTE",      x, y, WHT); y += lh;
    font_draw_str(ren, " ESC    EXIT TILE MODE",          x, y, WHT); y += lh + hg;

    font_draw_str(ren, "VIEW & GRID",                     x, y, CYN); y += lh;
    font_draw_str(ren, " V      GRAYSCALE / NES COLOUR",  x, y, WHT); y += lh;
    font_draw_str(ren, " G      TILE GRID",               x, y, WHT); y += lh;
    font_draw_str(ren, " P      PIXEL GRID",              x, y, WHT); y += lh;
    font_draw_str(ren, " M      SPRITE 16 MODE",          x, y, WHT); y += lh + hg;

    font_draw_str(ren, "FILES & CANVAS",                  x, y, CYN); y += lh;
    font_draw_str(ren, " CTRL+S      SAVE",               x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+SHFT+S SAVE AS",            x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+O      OPEN",               x, y, WHT); y += lh;
    font_draw_str(ren, " CTRL+R      RESIZE CANVAS",      x, y, WHT); y += lh;
    font_draw_str(ren, " DROP FILE   OPEN",               x, y, WHT); y += lh;
    font_draw_str(ren, " =/-         ZOOM IN/OUT",        x, y, WHT); y += lh + hg;

    font_draw_str(ren, " F1 OR ?   TOGGLE HELP",          x, y, DIM); y += lh;
    font_draw_str(ren, " ESC       QUIT",                 x, y, DIM);
    (void)y;
}

/* ── Text-input overlay ───────────────────────────────────────── */
static void render_input_overlay(SDL_Renderer *ren, const EditorState *s) {
    if (!s->input_mode) return;

    const int BOX_W = 480, BOX_H = 76;
    const int BOX_X = (s->canvas_w        - BOX_W) / 2;
    const int BOX_Y = (s->win_h - STATUS_H - BOX_H) / 2;

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
    if      (s->input_type == INPUT_SAVE_AS) title = "SAVE AS:";
    else if (s->input_type == INPUT_OPEN)    title = "OPEN FILE:";
    else                                     title = "RESIZE (COLSxROWS):";
    font_draw_str(ren, title, tx, ty, YLW);
    ty += lh;

    /* Show last 36 chars of the buffer so cursor is always visible. */
    char display[40];
    int  start = (s->input_len > 36) ? (s->input_len - 36) : 0;
    int  dlen  = s->input_len - start;
    memcpy(display, s->input_buf + start, (size_t)dlen);
    bool cursor_on = (SDL_GetTicks() % 1000) < 500;
    display[dlen]     = cursor_on ? '_' : ' ';
    display[dlen + 1] = '\0';
    font_draw_str(ren, display, tx, ty, WHT);
    ty += lh;

    font_draw_str(ren, "ENTER=OK  ESC=CANCEL", tx, ty + 2, DIM);
}

/* ── Main render entry ────────────────────────────────────────── */
void render_frame(SDL_Renderer *ren, const EditorState *s) {
    set_color(ren, 10, 10, 10);
    SDL_RenderClear(ren);

    render_canvas(s);
    SDL_Rect canvas_dst = {0, 0, s->canvas_w, s->canvas_h};
    SDL_RenderCopy(ren, canvas_tex, NULL, &canvas_dst);

    if (s->show_pixel_grid) render_pixel_grid(ren, s);
    if (s->show_tile_grid)  render_tile_grid(ren, s);

    render_tile_highlight(ren, s);
    render_panel(ren, s);
    render_status(ren, s);

    vline(ren, s->canvas_w, 0,                   s->win_h - STATUS_H, 55, 55, 80);
    hline(ren, 0,           s->win_h - STATUS_H, s->win_w,            55, 55, 80);

    render_help_overlay(ren, s);
    render_input_overlay(ren, s);

    SDL_RenderPresent(ren);
}
