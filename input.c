#include "input.h"
#include "panel.h"
#include <string.h>
#include <stdio.h>

/* ── Helpers ──────────────────────────────────────────────────── */

static int wmod(int v, int n) { return ((v % n) + n) % n; }

/* Map screen coords to the tile index under the cursor.
   In sprite-16 mode the tile layout is remapped, so screen position
   does not equal (row*cols + col) — we compute the correct sub-tile. */
static int screen_to_tile(const EditorState *s, int mx, int my) {
    int nx = mx / s->zoom;
    int ny = my / s->zoom;
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

/* ── Paint ────────────────────────────────────────────────────── */

static void paint_at(EditorState *s, int mx, int my) {
    if (mx < 0 || mx >= s->canvas_w || my < 0 || my >= s->canvas_h) return;

    int px_x = mx / s->zoom;
    int px_y = my / s->zoom;
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
    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
        /* Snap to sprite (2-tile) boundary so sel_tile_x/y are always even. */
        s->sel_tile_x = ((mx / s->zoom) / (TILE_W * 2)) * 2;
        s->sel_tile_y = ((my / s->zoom) / (TILE_H * 2)) * 2;
    } else {
        s->sel_tile_x = (mx / s->zoom) / TILE_W;
        s->sel_tile_y = (my / s->zoom) / TILE_H;
    }
    s->tile_mode = true;
}

/* ── Panel click ──────────────────────────────────────────────── */

static void panel_click(EditorState *s, int px, int py) {
    if (py >= PANEL_PAL_Y0 && py < PANEL_PAL_Y0 + 8 * PANEL_PAL_ROW) {
        int row = (py - PANEL_PAL_Y0) / PANEL_PAL_ROW;
        if (row < 0 || row > 7) return;
        s->active_sub_pal = row;
        if (s->tile_mode) {
            int base = sel_tile_idx(s);
            int cnt  = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
            for (int p = 0; p < cnt; p++)
                s->pal.tile_pal[base + p] = (uint8_t)row;
        }
        return;
    }

    if (py >= PANEL_ACT_Y0 && py < PANEL_ACT_Y0 + PANEL_ACT_SH) {
        int col = (px - PANEL_PAL_X0) / (PANEL_ACT_SW + PANEL_ACT_XGAP);
        if (col >= 0 && col < 4) s->active_swatch = col;
        return;
    }

    if (py >= PANEL_NES_Y0 && py < PANEL_NES_Y0 + 8 * PANEL_NES_STEP &&
        px >= PANEL_NES_X0 && px < PANEL_NES_X0 + 8 * PANEL_NES_STEP) {
        int col = (px - PANEL_NES_X0) / PANEL_NES_STEP;
        int row = (py - PANEL_NES_Y0) / PANEL_NES_STEP;
        if (col >= 0 && col < 8 && row >= 0 && row < 8)
            s->pal.sub[s->active_sub_pal].idx[s->active_swatch] =
                (uint8_t)(row * 8 + col);
        return;
    }

    if (py >= PANEL_VIEW_Y0 && py < PANEL_VIEW_Y0 + 16 &&
        px >= 22 && px < 34)
        s->show_help = !s->show_help;
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
                    if (s->show_help)    s->show_help = false;
                    else if (s->tile_mode) s->tile_mode = false;
                    else                 s->running   = false;
                    break;

                case SDLK_0: s->color = 0; break;
                case SDLK_1: s->color = 1; break;
                case SDLK_2: s->color = 2; break;
                case SDLK_3: s->color = 3; break;

                case SDLK_g: s->show_tile_grid  = !s->show_tile_grid;  break;
                case SDLK_p: s->show_pixel_grid = !s->show_pixel_grid; break;
                case SDLK_v:
                    s->view_mode = (s->view_mode == VIEW_GRAYSCALE)
                                 ? VIEW_NES_COLOR : VIEW_GRAYSCALE;
                    break;

                case SDLK_SLASH:
                    if (e->key.keysym.mod & KMOD_SHIFT)
                        s->show_help = !s->show_help;
                    break;
                case SDLK_F1:
                    s->show_help = !s->show_help;
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
                case SDLK_e:
                    if (e->key.keysym.mod & KMOD_CTRL)
                        s->want_save = true;
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
                    } else if (can) {
                        s->sprite_mode = SPRITE_16;
                        s->tile_mode   = false;
                    }
                    break;
                }

                case SDLK_t: select_tile_under_cursor(s); break;
                case SDLK_w:
                    s->wrap_mode = (WrapMode)((s->wrap_mode + 1) % 4);
                    break;

                case SDLK_LEFTBRACKET:
                    if (s->tile_mode) {
                        int base = sel_tile_idx(s);
                        int cnt  = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
                        for (int p = 0; p < cnt; p++)
                            s->pal.tile_pal[base + p] = (s->pal.tile_pal[base + p] + 7) % 8;
                    }
                    break;
                case SDLK_RIGHTBRACKET:
                    if (s->tile_mode) {
                        int base = sel_tile_idx(s);
                        int cnt  = (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) ? 4 : 1;
                        for (int p = 0; p < cnt; p++)
                            s->pal.tile_pal[base + p] = (s->pal.tile_pal[base + p] + 1) % 8;
                    }
                    break;

                default: break;
            }
            break;

        case SDL_MOUSEBUTTONDOWN: {
            int mx = e->button.x, my = e->button.y;
            s->mouse_x = mx; s->mouse_y = my;
            if (e->button.button == SDL_BUTTON_LEFT) {
                s->mouse_down = true;
                if (mx < s->canvas_w) paint_at(s, mx, my);
                else                  panel_click(s, mx - s->canvas_w, my);
            }
            if (e->button.button == SDL_BUTTON_RIGHT) {
                if (mx >= 0 && mx < s->canvas_w && my >= 0 && my < s->canvas_h) {
                    int t = screen_to_tile(s, mx, my);
                    if (s->sprite_mode == SPRITE_16 && s->chr_cols >= 2) {
                        int base = (t / 4) * 4;
                        for (int p = 0; p < 4; p++)
                            s->pal.tile_pal[base + p] = (uint8_t)s->active_sub_pal;
                    } else {
                        s->pal.tile_pal[t] = (uint8_t)s->active_sub_pal;
                    }
                }
            }
            break;
        }

        case SDL_MOUSEBUTTONUP:
            if (e->button.button == SDL_BUTTON_LEFT) s->mouse_down = false;
            break;

        case SDL_MOUSEMOTION: {
            int mx = e->motion.x, my = e->motion.y;
            s->mouse_x = mx; s->mouse_y = my;
            if (s->mouse_down) {
                if (mx < s->canvas_w) paint_at(s, mx, my);
                else                  panel_click(s, mx - s->canvas_w, my);
            }
            break;
        }

        default: break;
    }
}
