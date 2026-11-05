#include "nova_ui.h"

#include <SDL2_gfxPrimitives.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_CACHE_CAPACITY 256
#define TEXT_CACHE_LENGTH 256

typedef struct {
    char text[TEXT_CACHE_LENGTH];
    SDL_Color color;
    NovaFontWeight weight;
    int size;
    unsigned long age;
    SDL_Texture *texture;
    int width;
    int height;
} TextCacheEntry;

static SDL_Renderer *ui_renderer;
static TTF_Font *fonts[NOVA_FONT_COUNT][2];
static TextCacheEntry text_cache[TEXT_CACHE_CAPACITY];
static unsigned long cache_age;
static bool ttf_ready;

bool nova_ui_init(SDL_Renderer *renderer) {
    static const char *paths[NOVA_FONT_COUNT] = {
        "romfs:/fonts/Manrope-Regular.ttf",
        "romfs:/fonts/Manrope-Medium.ttf",
        "romfs:/fonts/Manrope-SemiBold.ttf",
        "romfs:/fonts/Manrope-Bold.ttf"
    };
    ui_renderer = renderer;
    if (TTF_Init() < 0) return false;
    ttf_ready = true;
    for (int i = 0; i < NOVA_FONT_COUNT; i++) {
        fonts[i][0] = TTF_OpenFont(paths[i], 11);
        fonts[i][1] = TTF_OpenFont(paths[i], 18);
        if (!fonts[i][0] || !fonts[i][1]) {
            fprintf(stderr, "NOVA font missing: %s (%s)\n", paths[i], TTF_GetError());
            nova_ui_destroy();
            return false;
        }
    }
    return true;
}

static bool same_color(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static TextCacheEntry *get_entry(const char *text, SDL_Color color,
                                 NovaFontWeight weight, int size) {
    TextCacheEntry *oldest = &text_cache[0];
    for (int i = 0; i < TEXT_CACHE_CAPACITY; i++) {
        TextCacheEntry *entry = &text_cache[i];
        if (entry->texture && entry->weight == weight && entry->size == size &&
            same_color(entry->color, color) && strcmp(entry->text, text) == 0) {
            entry->age = ++cache_age;
            return entry;
        }
        if (!entry->texture) oldest = entry;
        else if (oldest->texture && entry->age < oldest->age) oldest = entry;
    }

    if (oldest->texture) SDL_DestroyTexture(oldest->texture);
    SDL_Surface *surface = TTF_RenderUTF8_Blended(fonts[weight][size > 14 ? 1 : 0], text, color);
    if (!surface) return NULL;
    oldest->texture = SDL_CreateTextureFromSurface(ui_renderer, surface);
    oldest->width = surface->w;
    oldest->height = surface->h;
    SDL_FreeSurface(surface);
    if (!oldest->texture) return NULL;
    SDL_SetTextureBlendMode(oldest->texture, SDL_BLENDMODE_BLEND);
    snprintf(oldest->text, sizeof(oldest->text), "%s", text);
    oldest->color = color;
    oldest->weight = weight;
    oldest->size = size;
    oldest->age = ++cache_age;
    return oldest;
}

void nova_ui_draw_text(SDL_Renderer *renderer, int x, int y, const char *text,
                       SDL_Color color, NovaFontWeight weight, int size) {
    if (!renderer || !text || !*text) return;
    if (!ttf_ready || weight < 0 || weight >= NOVA_FONT_COUNT) {
        stringRGBA(renderer, x, y, text, color.r, color.g, color.b, color.a);
        return;
    }
    TextCacheEntry *entry = get_entry(text, color, weight, size);
    if (!entry) {
        stringRGBA(renderer, x, y, text, color.r, color.g, color.b, color.a);
        return;
    }
    SDL_Rect dst = {x, y, entry->width, entry->height};
    SDL_RenderCopy(renderer, entry->texture, NULL, &dst);
}

void nova_ui_draw_text_centered(SDL_Renderer *renderer, int center_x, int y, const char *text,
                                SDL_Color color, NovaFontWeight weight, int size) {
    if (!renderer || !text || !*text) return;
    TextCacheEntry *entry = get_entry(text, color, weight, size);
    if (!entry) { nova_ui_draw_text(renderer, center_x, y, text, color, weight, size); return; }
    SDL_Rect dst = {center_x - entry->width / 2, y, entry->width, entry->height};
    SDL_RenderCopy(renderer, entry->texture, NULL, &dst);
}

void nova_ui_draw_text_fit(SDL_Renderer *renderer, int x, int y, const char *text,
                           SDL_Color color, NovaFontWeight weight, int size, int max_width) {
    if (!text || max_width <= 0 || !ttf_ready || weight < 0 || weight >= NOVA_FONT_COUNT) {
        nova_ui_draw_text(renderer, x, y, text, color, weight, size);
        return;
    }
    int width=0,height=0;
    TTF_Font *font=fonts[weight][size>14?1:0];
    if(TTF_SizeUTF8(font,text,&width,&height)==0&&width<=max_width){
        nova_ui_draw_text(renderer,x,y,text,color,weight,size);
        return;
    }
    const size_t source_len=strlen(text);
    size_t length=source_len<sizeof(((TextCacheEntry*)0)->text)-4?source_len:sizeof(((TextCacheEntry*)0)->text)-4;
    char clipped[TEXT_CACHE_LENGTH];
    memcpy(clipped,text,length);clipped[length]='\0';
    while(length>0){
        size_t cut=length-1;
        while(cut>0&&((unsigned char)clipped[cut]&0xC0)==0x80)cut--;
        length=cut;clipped[length]='\0';
        strcat(clipped,"...");
        if(TTF_SizeUTF8(font,clipped,&width,&height)==0&&width<=max_width)break;
        length=cut;clipped[length]='\0';
    }
    nova_ui_draw_text(renderer,x,y,length?clipped:"...",color,weight,size);
}

void nova_ui_draw_text_centered_fit(SDL_Renderer *renderer, int center_x, int y, const char *text,
                                    SDL_Color color, NovaFontWeight weight, int size, int max_width) {
    if (!renderer || !text || !*text || max_width <= 0 || !ttf_ready || weight < 0 || weight >= NOVA_FONT_COUNT) {
        nova_ui_draw_text_fit(renderer, center_x - max_width / 2, y, text, color, weight, size, max_width);
        return;
    }
    TTF_Font *font=fonts[weight][size>14?1:0];int width=0,height=0;const char *draw_text=text;char clipped[TEXT_CACHE_LENGTH];
    if(TTF_SizeUTF8(font,text,&width,&height)<0){nova_ui_draw_text(renderer,center_x,y,text,color,weight,size);return;}
    if(width>max_width){size_t length=strlen(text);if(length>=sizeof(clipped)-4)length=sizeof(clipped)-4;memcpy(clipped,text,length);clipped[length]=0;
        while(length>0){size_t cut=length-1;while(cut>0&&((unsigned char)clipped[cut]&0xC0)==0x80)cut--;length=cut;clipped[length]=0;strcat(clipped,"...");if(TTF_SizeUTF8(font,clipped,&width,&height)==0&&width<=max_width)break;length=cut;clipped[length]=0;}
        if(length==0)snprintf(clipped,sizeof(clipped),"...");
        draw_text=clipped;
    }
    TextCacheEntry *entry=get_entry(draw_text,color,weight,size);if(!entry)return;
    SDL_Rect dst={center_x-entry->width/2,y,entry->width,entry->height};SDL_RenderCopy(renderer,entry->texture,NULL,&dst);
}

void nova_ui_destroy(void) {
    for (int i = 0; i < TEXT_CACHE_CAPACITY; i++) {
        if (text_cache[i].texture) SDL_DestroyTexture(text_cache[i].texture);
        memset(&text_cache[i], 0, sizeof(text_cache[i]));
    }
    for (int i = 0; i < NOVA_FONT_COUNT; i++) {
        for (int s = 0; s < 2; s++) {
            if (fonts[i][s]) TTF_CloseFont(fonts[i][s]);
            fonts[i][s] = NULL;
        }
    }
    if (ttf_ready) TTF_Quit();
    ttf_ready = false;
    ui_renderer = NULL;
}
