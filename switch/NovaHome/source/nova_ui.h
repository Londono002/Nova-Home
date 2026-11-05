#pragma once

#include <SDL.h>
#include <stdbool.h>

typedef enum {
    NOVA_FONT_REGULAR = 0,
    NOVA_FONT_MEDIUM,
    NOVA_FONT_SEMIBOLD,
    NOVA_FONT_BOLD,
    NOVA_FONT_COUNT
} NovaFontWeight;

bool nova_ui_init(SDL_Renderer *renderer);
void nova_ui_draw_text(SDL_Renderer *renderer, int x, int y, const char *text,
                       SDL_Color color, NovaFontWeight weight, int size);
void nova_ui_draw_text_centered(SDL_Renderer *renderer, int center_x, int y, const char *text,
                                SDL_Color color, NovaFontWeight weight, int size);
void nova_ui_draw_text_fit(SDL_Renderer *renderer, int x, int y, const char *text,
                           SDL_Color color, NovaFontWeight weight, int size, int max_width);
void nova_ui_draw_text_centered_fit(SDL_Renderer *renderer, int center_x, int y, const char *text,
                                    SDL_Color color, NovaFontWeight weight, int size, int max_width);
void nova_ui_destroy(void);
