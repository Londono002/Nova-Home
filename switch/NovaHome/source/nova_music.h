#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>

typedef struct NovaMusic NovaMusic;

/* Opens SDL_mixer, scans the menu playlist in RomFS, and starts playback. */
NovaMusic *nova_music_create(void);
void nova_music_destroy(NovaMusic *music);

/* Call once per frame while NOVA is running. Tracks advance and repeat automatically. */
void nova_music_update(NovaMusic *music);

/* Stop audio when leaving NOVA; reactivate it if launching a game fails. */
void nova_music_set_active(NovaMusic *music, bool active);
void nova_music_toggle_pause(NovaMusic *music);
void nova_music_next(NovaMusic *music);
void nova_music_previous(NovaMusic *music);
void nova_music_set_volume(NovaMusic *music, int percent);
void nova_music_adjust_volume(NovaMusic *music, int delta_percent);

bool nova_music_is_ready(const NovaMusic *music);
bool nova_music_is_paused(const NovaMusic *music);
int nova_music_volume(const NovaMusic *music);
size_t nova_music_track_count(const NovaMusic *music);
const char *nova_music_track_title(const NovaMusic *music);

/* Draws the native music panel inside the supplied screen-space rectangle. */
void nova_music_draw_panel(const NovaMusic *music, SDL_Renderer *renderer, SDL_Rect bounds);

/* Handles taps inside the panel; returns true when the touch was consumed. */
bool nova_music_handle_touch(NovaMusic *music, int x, int y, SDL_Rect bounds);
