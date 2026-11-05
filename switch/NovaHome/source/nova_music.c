#include "nova_music.h"
#include "nova_ui.h"

#include <SDL_mixer.h>
#include <SDL2_gfxPrimitives.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <switch.h>

#define NOVA_MUSIC_DIRECTORY "romfs:/songs"
#define NOVA_MUSIC_VOLUME_DEFAULT 35

typedef struct {
    char *filename;
    char *title;
} NovaMusicTrack;

struct NovaMusic {
    NovaMusicTrack *tracks;
    size_t track_count;
    size_t current_track;
    Mix_Music *current_music;
    int volume;
    bool active;
    bool paused;
    bool audio_subsystem;
    bool mixer_initialized;
    bool audio_open;
    bool ready;
};

static bool has_mp3_extension(const char *name) {
    const size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".mp3") == 0;
}

static char *track_title_from_filename(const char *filename) {
    const size_t n = strlen(filename);
    const size_t title_size = n > 4 ? n - 4 : n;
    char *title = malloc(title_size + 1);
    if (!title) return NULL;
    memcpy(title, filename, title_size);
    title[title_size] = '\0';
    return title;
}

static int compare_tracks(const void *a, const void *b) {
    const NovaMusicTrack *left = a;
    const NovaMusicTrack *right = b;
    return strcasecmp(left->title, right->title);
}

static void scan_playlist(NovaMusic *music) {
    DIR *directory = opendir(NOVA_MUSIC_DIRECTORY);
    if (!directory) return;

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!has_mp3_extension(entry->d_name)) continue;

        NovaMusicTrack *tracks = realloc(music->tracks,
            (music->track_count + 1) * sizeof(*music->tracks));
        if (!tracks) break;
        music->tracks = tracks;

        const size_t path_size = strlen(NOVA_MUSIC_DIRECTORY) + strlen(entry->d_name) + 2;
        char *path = malloc(path_size);
        char *title = track_title_from_filename(entry->d_name);
        if (!path || !title) {
            free(path);
            free(title);
            break;
        }
        snprintf(path, path_size, "%s/%s", NOVA_MUSIC_DIRECTORY, entry->d_name);
        music->tracks[music->track_count++] = (NovaMusicTrack){path, title};
    }
    closedir(directory);

    if (music->track_count > 1)
        qsort(music->tracks, music->track_count, sizeof(*music->tracks), compare_tracks);
}

static void free_current_track(NovaMusic *music) {
    if (!music->current_music) return;
    Mix_HaltMusic();
    Mix_FreeMusic(music->current_music);
    music->current_music = NULL;
}

static bool start_track(NovaMusic *music, size_t first_candidate) {
    if (!music || !music->ready || music->track_count == 0) return false;

    free_current_track(music);
    music->paused = false;

    for (size_t tries = 0; tries < music->track_count; tries++) {
        const size_t index = (first_candidate + tries) % music->track_count;
        Mix_Music *track = Mix_LoadMUS(music->tracks[index].filename);
        if (!track) continue;
        if (Mix_PlayMusic(track, 0) < 0) {
            Mix_FreeMusic(track);
            continue;
        }
        music->current_track = index;
        music->current_music = track;
        Mix_VolumeMusic(music->volume * MIX_MAX_VOLUME / 100);
        return true;
    }

    return false;
}

NovaMusic *nova_music_create(void) {
    NovaMusic *music = calloc(1, sizeof(*music));
    if (!music) return NULL;

    music->volume = NOVA_MUSIC_VOLUME_DEFAULT;
    music->active = true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return music;
    music->audio_subsystem = true;

    const int codecs = Mix_Init(MIX_INIT_MP3);
    music->mixer_initialized = true;
    if ((codecs & MIX_INIT_MP3) == 0) return music;
    if (Mix_OpenAudio(48000, AUDIO_S16, 2, 4096) < 0) return music;
    music->audio_open = true;
    music->ready = true;

    scan_playlist(music);
    if (music->track_count > 0) {
        const size_t start=(size_t)(randomGet64()%music->track_count);
        start_track(music, start);
    }
    return music;
}

void nova_music_destroy(NovaMusic *music) {
    if (!music) return;
    free_current_track(music);
    for (size_t i = 0; i < music->track_count; i++) {
        free(music->tracks[i].filename);
        free(music->tracks[i].title);
    }
    free(music->tracks);
    if (music->audio_open) Mix_CloseAudio();
    if (music->mixer_initialized) Mix_Quit();
    if (music->audio_subsystem) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(music);
}

void nova_music_update(NovaMusic *music) {
    if (!music || !music->ready || !music->active || music->paused || music->track_count == 0)
        return;
    if (!Mix_PlayingMusic())
        start_track(music, (music->current_track + 1) % music->track_count);
}

void nova_music_set_active(NovaMusic *music, bool active) {
    if (!music || music->active == active) return;
    music->active = active;
    if (!active) {
        music->paused = false;
        free_current_track(music);
    } else if (music->ready && music->track_count > 0) {
        start_track(music, music->current_track);
    }
}

void nova_music_toggle_pause(NovaMusic *music) {
    if (!music || !music->ready || !music->active || !music->current_music) return;
    if (music->paused) {
        Mix_ResumeMusic();
        music->paused = false;
    } else {
        Mix_PauseMusic();
        music->paused = true;
    }
}

void nova_music_next(NovaMusic *music) {
    if (!music || !music->ready || !music->active || music->track_count == 0) return;
    start_track(music, (music->current_track + 1) % music->track_count);
}

void nova_music_previous(NovaMusic *music) {
    if (!music || !music->ready || !music->active || music->track_count == 0) return;
    start_track(music, (music->current_track + music->track_count - 1) % music->track_count);
}

void nova_music_set_volume(NovaMusic *music, int percent) {
    if (!music) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    music->volume = percent;
    if (music->audio_open) Mix_VolumeMusic(percent * MIX_MAX_VOLUME / 100);
}

void nova_music_adjust_volume(NovaMusic *music, int delta_percent) {
    if (music) nova_music_set_volume(music, music->volume + delta_percent);
}

bool nova_music_is_ready(const NovaMusic *music) { return music && music->ready; }
bool nova_music_is_paused(const NovaMusic *music) { return music && music->paused; }
int nova_music_volume(const NovaMusic *music) { return music ? music->volume : 0; }
size_t nova_music_track_count(const NovaMusic *music) { return music ? music->track_count : 0; }

const char *nova_music_track_title(const NovaMusic *music) {
    if (!music || music->track_count == 0) return "Sin canciones";
    return music->tracks[music->current_track].title;
}

void nova_music_draw_panel(const NovaMusic *music, SDL_Renderer *renderer, SDL_Rect bounds) {
    if (!renderer) return;
    const int x = bounds.x, y = bounds.y, w = bounds.w, h = bounds.h;
    roundedBoxRGBA(renderer, x, y, x+w, y+h, 18, 14, 16, 25, 250);
    roundedRectangleRGBA(renderer, x, y, x+w, y+h, 18, 89, 76, 139, 255);
    roundedBoxRGBA(renderer, x+1, y+1, x+w-1, y+48, 17, 91, 73, 152, 52);
    nova_ui_draw_text(renderer, x+24, y+17, "NOVA  /  AUDIO DEL MENÚ", (SDL_Color){207,197,255,255}, NOVA_FONT_MEDIUM, 13);
    const size_t total = nova_music_track_count(music);
    if (total) {
        char count[32];
        snprintf(count, sizeof(count), "%02u  /  %02u", (unsigned)(music->current_track+1), (unsigned)total);
        nova_ui_draw_text(renderer, x+w-105, y+17, count, (SDL_Color){181,173,205,255}, NOVA_FONT_MEDIUM, 13);
    }

    const int disc_x=x+68, disc_y=y+107;
    filledCircleRGBA(renderer,disc_x,disc_y,43,33,29,52,255);
    circleRGBA(renderer,disc_x,disc_y,40,138,119,208,255);
    circleRGBA(renderer,disc_x,disc_y,32,76,65,113,255);
    circleRGBA(renderer,disc_x,disc_y,24,56,48,87,255);
    filledCircleRGBA(renderer,disc_x,disc_y,11,160,139,239,255);
    filledCircleRGBA(renderer,disc_x,disc_y,3,35,30,53,255);
    for(int i=0;i<7;i++){
        int bar_h=5+(i*11+(int)(SDL_GetTicks()/90)%13)%17;
        roundedBoxRGBA(renderer,x+137+i*8,y+111-bar_h/2,x+141+i*8,y+111+bar_h/2,2,145,125,227,210);
    }

    const char *title = nova_music_track_title(music);
    nova_ui_draw_text_fit(renderer, x+137, y+126, title, (SDL_Color){255,255,255,255}, NOVA_FONT_SEMIBOLD, 16, w-162);

    const char *status = "AUDIO NO DISPONIBLE";
    if (music && music->ready) {
        if (music->track_count == 0) status = "No hay MP3 en Songs/";
        else if (music->paused) status = "PAUSADA";
        else if (music->current_music && Mix_PlayingMusic()) status = "REPRODUCIENDO";
        else status = "LISTA";
    }
    roundedBoxRGBA(renderer,x+137,y+151,x+137+(int)strlen(status)*6+18,y+174,9,128,101,245,48);
    nova_ui_draw_text(renderer, x+146, y+156, status, (SDL_Color){196,188,220,255}, NOVA_FONT_MEDIUM, 13);

    const int cy = y+187;
    roundedBoxRGBA(renderer, x+w/2-96, cy, x+w/2-48, cy+38, 12, 255,255,255,18);
    roundedBoxRGBA(renderer, x+w/2-38, cy-4, x+w/2+38, cy+42, 16, 128,101,245,255);
    roundedBoxRGBA(renderer, x+w/2+48, cy, x+w/2+96, cy+38, 12, 255,255,255,18);
    nova_ui_draw_text(renderer, x+w/2-82, cy+13, "|<", (SDL_Color){241,238,255,255}, NOVA_FONT_BOLD, 13);
    nova_ui_draw_text(renderer, x+w/2-13, cy+12, music && music->paused ? ">" : "Ⅱ", (SDL_Color){255,255,255,255}, NOVA_FONT_BOLD, 13);
    nova_ui_draw_text(renderer, x+w/2+64, cy+13, ">|", (SDL_Color){241,238,255,255}, NOVA_FONT_BOLD, 13);

    nova_ui_draw_text(renderer, x+25, y+239, "VOLUMEN", (SDL_Color){168,165,187,255}, NOVA_FONT_MEDIUM, 13);
    if (music) {
        char percent[12];
        snprintf(percent, sizeof(percent), "%d%%", music->volume);
        nova_ui_draw_text(renderer, x+w-62, y+239, percent, (SDL_Color){214,203,255,255}, NOVA_FONT_SEMIBOLD, 13);
    }
    const int slider_x = x+25, slider_w = w-50, slider_y = y+261;
    roundedBoxRGBA(renderer, slider_x, slider_y, slider_x+slider_w, slider_y+6, 3, 54,56,68,255);
    const int volume = music ? music->volume : 0;
    roundedBoxRGBA(renderer, slider_x, slider_y, slider_x+slider_w*volume/100, slider_y+6, 3, 173,151,255,255);
    filledCircleRGBA(renderer, slider_x+slider_w*volume/100, slider_y+3, 7, 238,231,255,255);
}

bool nova_music_handle_touch(NovaMusic *music, int x, int y, SDL_Rect bounds) {
    if (!music || x < bounds.x || x > bounds.x+bounds.w || y < bounds.y || y > bounds.y+bounds.h)
        return false;

    const int local_x = x-bounds.x, local_y = y-bounds.y;
    if (local_y >= 183 && local_y <= 229) {
        if (local_x < bounds.w/2-38) nova_music_previous(music);
        else if (local_x > bounds.w/2+38) nova_music_next(music);
        else nova_music_toggle_pause(music);
    } else if (local_y >= 250 && local_y <= 270) {
        int percent=(local_x-25)*100/(bounds.w-50);
        nova_music_set_volume(music,percent);
    }
    return true;
}
