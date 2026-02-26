#pragma once
#include <SDL2/SDL.h>
#include "main.h"

/* Call once after SDL_CreateRenderer. */
void render_init(SDL_Renderer *ren, const EditorState *s);

/* Recreate the canvas texture after a dimension change. */
void render_resize(SDL_Renderer *ren, const EditorState *s);

/* Call before SDL_DestroyRenderer. */
void render_destroy(void);

/* Draw one complete frame. */
void render_frame(SDL_Renderer *ren, const EditorState *s);
