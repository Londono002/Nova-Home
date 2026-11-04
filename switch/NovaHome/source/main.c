#include <switch.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define GAME_COUNT 7

typedef struct {
    const char *title;
    const char *short_title;
    const char *time;
    const char *mark;
    SDL_Color top;
    SDL_Color bottom;
    SDL_Color accent;
} Game;

static const Game games[GAME_COUNT] = {
    {"The Legend of Zelda: Tears of the Kingdom", "ZELDA", "65 h o mas", "HYRULE", {104,135,147,255}, {30,50,53,255}, {213,190,119,255}},
    {"Metroid Dread", "METROID DREAD", "18 h o mas", "DREAD", {107,49,57,255}, {24,22,35,255}, {240,104,55,255}},
    {"Super Mario Odyssey", "SUPER MARIO", "30 h o mas", "ODYSSEY", {74,158,218,255}, {196,81,69,255}, {255,206,83,255}},
    {"Hollow Knight", "HOLLOW KNIGHT", "42 h o mas", "HOLLOW", {49,84,92,255}, {20,31,40,255}, {161,220,222,255}},
    {"Hades", "HADES", "26 h o mas", "HADES", {114,54,47,255}, {36,26,38,255}, {249,164,71,255}},
    {"Animal Crossing: New Horizons", "ANIMAL CROSSING", "120 h o mas", "NEW HORIZONS", {127,185,185,255}, {72,126,99,255}, {241,218,139,255}},
    {"Celeste", "CELESTE", "12 h o mas", "CELESTE", {85,93,145,255}, {40,39,66,255}, {232,151,175,255}}
};

static void color(SDL_Renderer *r, SDL_Color c) { SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); }
static void fill(SDL_Renderer *r,int x,int y,int w,int h,SDL_Color c) { color(r,c); SDL_Rect q={x,y,w,h}; SDL_RenderFillRect(r,&q); }
static void round_box(SDL_Renderer *r,int x,int y,int w,int h,int rad,SDL_Color c) { roundedBoxRGBA(r,x,y,x+w,y+h,rad,c.r,c.g,c.b,c.a); }
static void txt(SDL_Renderer *r,int x,int y,const char *s,SDL_Color c) { stringRGBA(r,x,y,s,c.r,c.g,c.b,c.a); }

static void gradient(SDL_Renderer *r, int selected) {
    const Game *g=&games[selected];
    for(int y=0;y<SCREEN_H;y+=4) {
        float t=(float)y/(float)SCREEN_H;
        SDL_Color c={(Uint8)(g->top.r+(g->bottom.r-g->top.r)*t),(Uint8)(g->top.g+(g->bottom.g-g->top.g)*t),(Uint8)(g->top.b+(g->bottom.b-g->top.b)*t),255};
        fill(r,0,y,SCREEN_W,4,c);
    }
    /* Distant landscape bands give each selected game a matching ambient scene. */
    round_box(r,SCREEN_W-410,150,275,250,120,(SDL_Color){g->accent.r,g->accent.g,g->accent.b,30});
    fill(r,0,425,SCREEN_W,295,(SDL_Color){8,10,16,108});
    for(int y=425;y<SCREEN_H;y+=3) {
        Uint8 a=(Uint8)(50+145*(y-425)/(SCREEN_H-425));
        fill(r,0,y,SCREEN_W,3,(SDL_Color){8,10,16,a});
    }
}

static void draw_case(SDL_Renderer *r,int x,int y,int w,int h,const Game *g,int selected,int tick) {
    const int rad=12;
    round_box(r,x-4,y-4,w+8,h+8,rad+3,selected?(SDL_Color){194,177,255,255}:(SDL_Color){255,255,255,30});
    round_box(r,x,y,w,h,rad,g->bottom);
    SDL_Rect clip={x+1,y+1,w-2,h-2}; SDL_RenderSetClipRect(r,&clip);
    for(int py=0;py<h;py+=3) {
        float t=(float)py/(float)h;
        SDL_Color c={(Uint8)(g->top.r+(g->bottom.r-g->top.r)*t),(Uint8)(g->top.g+(g->bottom.g-g->top.g)*t),(Uint8)(g->top.b+(g->bottom.b-g->top.b)*t),255};
        fill(r,x+1,y+py,w-2,3,c);
    }
    /* Abstract cover illustration, drawn natively so no external assets are needed. */
    round_box(r,x+w/2-w/5,y+h/4,w*2/5,w*2/5,w/5,(SDL_Color){g->accent.r,g->accent.g,g->accent.b,65});
    filledCircleRGBA(r,x+w/2,y+h/2,w/5,g->accent.r,g->accent.g,g->accent.b,170);
    filledCircleRGBA(r,x+w/2,y+h/2,w/8,g->top.r,g->top.g,g->top.b,245);
    lineRGBA(r,x+w/6,y+h/2,x+w*5/6,y+h/2,g->accent.r,g->accent.g,g->accent.b,95);
    fill(r,x,y+h*3/4,w,h/4,(SDL_Color){8,10,15,190});
    txt(r,x+12,y+h-37,g->mark,(SDL_Color){255,255,255,245});
    if(selected) {
        char index[24]; snprintf(index,sizeof(index),"%02d / %02d",tick+1,GAME_COUNT);
        txt(r,x+12,y+14,"NOVA  /  LIBRARY",(SDL_Color){255,255,255,210});
        txt(r,x+w-65,y+14,index,(SDL_Color){255,255,255,190});
    }
    SDL_RenderSetClipRect(r,NULL);
}

static void draw(SDL_Renderer *r,int selected,float animation,int tick,int show_options,int show_start) {
    gradient(r,selected);
    /* Subtle dark veil behind the system header. */
    round_box(r,24,20,288,44,10,(SDL_Color){12,14,21,185});
    filledCircleRGBA(r,49,42,14,145,126,241,255);
    txt(r,44,38,"P",(SDL_Color){255,255,255,255});
    txt(r,73,38,"PLAYER ONE",(SDL_Color){255,255,255,245});
    txt(r,73,51,"PROFILE",(SDL_Color){165,168,183,240});
    txt(r,216,43,"12:34",(SDL_Color){255,255,255,255});
    round_box(r,267,36,27,13,4,(SDL_Color){205,207,216,255});
    fill(r,271,40,17,5,(SDL_Color){147,131,238,255});
    round_box(r,1030,20,226,44,10,(SDL_Color){12,14,21,185});
    const char *icons[]={"JOY","SET","MUS","ALB"};
    for(int i=0;i<4;i++) {
        int bx=1040+i*53;
        round_box(r,bx,27,45,30,7,(SDL_Color){255,255,255,12});
        txt(r,bx+13,38,icons[i],(SDL_Color){227,222,255,255});
    }

    round_box(r,28,78,116,28,7,(SDL_Color){12,14,21,175});
    txt(r,40,89,"SORT  |  A-Z",(SDL_Color){245,245,249,255});

    /* Carousel boxes stay packed together; the selected case grows with an ease animation. */
    int center=SCREEN_W/2;
    for(int i=0;i<GAME_COUNT;i++) {
        int offset=i-selected;
        if(offset>GAME_COUNT/2) offset-=GAME_COUNT;
        if(offset<-(GAME_COUNT/2)) offset+=GAME_COUNT;
        float weight=(offset==0)?animation:1.0f;
        int w=(int)(148+(202-148)*weight),h=(int)(232+(304-232)*weight);
        int gap=159;
        int x=center+offset*gap-w/2;
        int y=174+(304-h)/2;
        if(x+w<0||x>SCREEN_W)continue;
        draw_case(r,x,y,w,h,&games[i],i==selected,tick);
    }

    const Game *g=&games[selected];
    txt(r,74,510,g->title,(SDL_Color){255,255,255,255});
    txt(r,76,535,"PLAY TIME",(SDL_Color){180,181,193,255});
    txt(r,76,552,g->time,(SDL_Color){255,255,255,255});
    round_box(r,890,510,152,45,9,(SDL_Color){237,234,255,255});
    txt(r,945,527,"A  START",(SDL_Color){32,29,47,255});
    round_box(r,1050,510,152,45,9,(SDL_Color){255,255,255,22});
    txt(r,1105,527,"+  OPTIONS",(SDL_Color){255,255,255,255});
    txt(r,450,632,"L / R   NAVIGATE       A   START       +   OPTIONS",(SDL_Color){204,204,216,220});
    if(show_start && tick < show_start) {
        round_box(r,460,580,360,35,8,(SDL_Color){13,15,23,225});
        txt(r,478,593,"DEMO: GAME LAUNCH IS NOT CONNECTED YET",(SDL_Color){255,255,255,255});
    }
    if(show_options) {
        round_box(r,380,205,520,300,16,(SDL_Color){14,16,24,248});
        round_box(r,380,205,520,300,16,(SDL_Color){161,142,255,120});
        txt(r,415,235,"NOVA  /  GAME OPTIONS",(SDL_Color){194,179,255,255});
        txt(r,415,275,"A   Start software",(SDL_Color){255,255,255,255});
        txt(r,415,310,"X   Software information",(SDL_Color){225,225,235,255});
        txt(r,415,345,"Y   Sort library",(SDL_Color){225,225,235,255});
        txt(r,415,395,"Game management will be connected in a later build.",(SDL_Color){165,168,183,255});
        txt(r,415,445,"B   Close",(SDL_Color){194,179,255,255});
    }
}

int main(int argc,char *argv[]) {
    (void)argc; (void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)<0) return 1;
    SDL_Window *window=SDL_CreateWindow("NOVA HOME",0,0,SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);
    if(!window) { SDL_Quit(); return 1; }
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_RenderSetLogicalSize(renderer,SCREEN_W,SCREEN_H);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    int selected=0,tick=0,done=0,show_options=0,show_start=0; float scale=0.0f;
    while(appletMainLoop()&&!done) {
        SDL_Event event; while(SDL_PollEvent(&event)) if(event.type==SDL_QUIT) done=1;
        padUpdate(&pad); u64 pressed=padGetButtonsDown(&pad);
        if(pressed&HidNpadButton_Plus) show_options=!show_options;
        if(pressed&HidNpadButton_B) show_options=0;
        if(pressed&HidNpadButton_Left) { selected=(selected+GAME_COUNT-1)%GAME_COUNT;scale=0.0f; }
        if(pressed&HidNpadButton_Right) { selected=(selected+1)%GAME_COUNT;scale=0.0f; }
        if(pressed&HidNpadButton_A) { show_options=0; show_start=tick+180; }
        if(scale<1.0f) { scale+=0.09f; if(scale>1.0f)scale=1.0f; }
        float eased=1.0f-(1.0f-scale)*(1.0f-scale);
        draw(renderer,selected,eased,tick++,show_options,show_start);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
