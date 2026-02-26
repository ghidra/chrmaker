#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "chr.h"
#include "main.h"
#include "panel.h"
#include "render.h"
#include "input.h"
#include "export.h"

/* ── Dimension helpers ────────────────────────────────────────── */

/* Call whenever chr_cols, chr_rows, or zoom change. */
static void state_update_dims(EditorState *s) {
    s->canvas_w = s->chr_cols * TILE_W * s->zoom;
    s->canvas_h = s->chr_rows * TILE_H * s->zoom;
    s->win_w    = s->canvas_w + PANEL_W;
    s->win_h    = (s->canvas_h < PANEL_FULL_H ? PANEL_FULL_H : s->canvas_h)
                  + STATUS_H;
}

/* ── State init ───────────────────────────────────────────────── */

static void state_init(EditorState *s, const char *path, int cols, int rows) {
    memset(s, 0, sizeof(EditorState));
    chr_init(&s->chr);
    palette_init(&s->pal);

    s->chr_cols        = cols;
    s->chr_rows        = rows;
    s->zoom            = 3;
    state_update_dims(s);
    s->want_resize     = false;

    s->view_mode       = VIEW_GRAYSCALE;
    s->show_tile_grid  = true;
    s->show_pixel_grid = false;
    s->color           = 3;
    s->active_sub_pal  = 0;
    s->active_swatch   = 1;
    s->tile_mode       = false;
    s->sel_tile_x      = 0;
    s->sel_tile_y      = 0;
    s->sprite_mode     = SPRITE_8;
    s->wrap_mode       = WRAP_NONE;
    s->mouse_down      = false;
    s->show_help       = false;
    s->input_mode      = false;
    s->input_len       = 0;
    s->want_save       = false;
    s->want_load       = false;
    s->running         = true;

    snprintf(s->current_path, sizeof(s->current_path), "%s", path);
}

/* Update the window title with a short status message. */
static void set_title(SDL_Window *win, const char *msg) {
    char t[320];
    snprintf(t, sizeof(t), "chrmaker — %s", msg);
    SDL_SetWindowTitle(win, t);
}

int main(int argc, char *argv[]) {
    const char *arg_path = (argc > 1) ? argv[1] : "output.chr";

    /* Optional second arg: "16x32" — canvas dimensions in tiles. */
    int arg_cols = CHR_DEFAULT_COLS, arg_rows = CHR_DEFAULT_ROWS;
    if (argc > 2) {
        int c = 0, r = 0;
        if (sscanf(argv[2], "%dx%d", &c, &r) == 2 && c >= 1 && r >= 1) {
            if (c <= 128) arg_cols = c;
            if (r <=  64) arg_rows = r;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    /* Temporary state to get initial window dimensions. */
    EditorState state;
    state_init(&state, arg_path, arg_cols, arg_rows);

    SDL_Window *win = SDL_CreateWindow(
        "chrmaker",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        state.win_w, state.win_h,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    render_init(ren, &state);

    /* Auto-load argv[1] if it already exists on disk. */
    {
        FILE *probe = fopen(arg_path, "rb");
        if (probe) {
            fclose(probe);
            int tiles = chr_load(&state.chr, arg_path);
            if (tiles > 0) {
                /* Auto-detect rows from tile count, keeping cols fixed. */
                int rows = (tiles + state.chr_cols - 1) / state.chr_cols;
                if (rows != state.chr_rows) {
                    state.chr_rows = rows;
                    state_update_dims(&state);
                    SDL_SetWindowSize(win, state.win_w, state.win_h);
                    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED,
                                              SDL_WINDOWPOS_CENTERED);
                    render_resize(ren, &state);
                }
                set_title(win, arg_path);
            }
        }
    }

    SDL_Event e;
    while (state.running) {
        while (SDL_PollEvent(&e))
            input_handle(&e, &state);

        /* ── File operations ── */
        if (state.want_save) {
            state.want_save = false;
            char msg[300];
            int  ntiles = state.chr_cols * state.chr_rows;
            if (export_chr(&state.chr, ntiles, state.current_path) == 0)
                snprintf(msg, sizeof(msg), "saved: %s (%d tiles)",
                         state.current_path, ntiles);
            else
                snprintf(msg, sizeof(msg), "ERROR saving %s", state.current_path);
            set_title(win, msg);
        }
        if (state.want_load) {
            state.want_load = false;
            char msg[300];
            int tiles = chr_load(&state.chr, state.current_path);
            if (tiles > 0) {
                int rows = (tiles + state.chr_cols - 1) / state.chr_cols;
                if (rows != state.chr_rows) {
                    state.chr_rows = rows;
                    state.want_resize = true;   /* handled below */
                }
                snprintf(msg, sizeof(msg), "opened: %s (%d tiles)",
                         state.current_path, tiles);
            } else {
                snprintf(msg, sizeof(msg), "ERROR opening %s", state.current_path);
            }
            set_title(win, msg);
        }

        /* ── Resize — MUST come after want_load, before render_frame ── */
        if (state.want_resize) {
            state.want_resize = false;
            state_update_dims(&state);
            SDL_SetWindowSize(win, state.win_w, state.win_h);
            SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED);
            render_resize(ren, &state);
        }

        render_frame(ren, &state);
        SDL_Delay(16);
    }

    render_destroy();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
