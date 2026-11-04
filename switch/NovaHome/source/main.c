#include <switch.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <SDL_image.h>
#include "nova_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define GAME_CAP 384
#define SMALL_W 146
#define SMALL_H 224
#define BIG_W 206
#define BIG_H 310
#define CARD_GAP 12

typedef struct {
    u64 id;
    char title[512];
    u64 play_ns;
    u64 last_play;
    SDL_Texture *icon;
    bool icon_loaded;
    SDL_Color top, bottom, accent;
} Game;

typedef enum { SORT_AZ, SORT_PLAYTIME, SORT_RECENT } SortMode;
typedef enum { FOCUS_LIBRARY, FOCUS_TOP, FOCUS_SORT, FOCUS_ACTIONS } FocusArea;
static Game games[GAME_CAP];
static int game_count=0, selected=0, sort_mode=SORT_AZ;
static int focus_area=FOCUS_LIBRARY, focus_top=0, focus_action=0;
static int options_open=0, option_cursor=0, panel_open=-1, show_start_until=0;
static int frame_tick=0;
static int stick_repeat=0;
static char profile_name[48]="Perfil";
static char clock_text[8]="--:--";
static char battery_text[8]="--%";
static int battery_level=-1;
static SDL_Texture *profile_icon=NULL;
static NsApplicationControlData *control_data=NULL;
static SDL_Renderer *app_renderer=NULL;
static bool has_time_service=false,has_battery_service=false,playtime_available=false;
static NovaMusic *menu_music=NULL;

static void set_color(SDL_Renderer *r,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);}
static void fill(SDL_Renderer *r,int x,int y,int w,int h,SDL_Color c){set_color(r,c);SDL_Rect q={x,y,w,h};SDL_RenderFillRect(r,&q);}
static void round_box(SDL_Renderer *r,int x,int y,int w,int h,int rad,SDL_Color c){roundedBoxRGBA(r,x,y,x+w,y+h,rad,c.r,c.g,c.b,c.a);}
static void txt(SDL_Renderer *r,int x,int y,const char *s,SDL_Color c){stringRGBA(r,x,y,s,c.r,c.g,c.b,c.a);}
static char *ascii_text(const char *src,char *dst,size_t cap){size_t j=0;for(size_t i=0;src[i]&&j+1<cap;i++){unsigned char c=(unsigned char)src[i];if(c<128)dst[j++]=(char)c;else if((c&0xC0)!=0x80)dst[j++]='?';}dst[j]=0;return dst;}

static void update_system_status(void){
    u64 timestamp=0; TimeCalendarTime cal={0}; TimeCalendarAdditionalInfo info={0};
    if(has_time_service&&R_SUCCEEDED(timeGetCurrentTime(TimeType_LocalSystemClock,&timestamp))&&R_SUCCEEDED(timeToCalendarTimeWithMyRule(timestamp,&cal,&info)))snprintf(clock_text,sizeof(clock_text),"%02u:%02u",cal.hour,cal.minute);
    u32 battery=0;if(has_battery_service&&R_SUCCEEDED(psmGetBatteryChargePercentage(&battery))){battery_level=(int)battery;snprintf(battery_text,sizeof(battery_text),"%u%%",battery);}
}

static void load_profile(void){
    if(R_FAILED(accountInitialize(AccountServiceType_Application)))return;
    AccountUid uid={0};AccountProfile profile={0};AccountProfileBase base={0};
    if(R_SUCCEEDED(accountGetLastOpenedUser(&uid))&&R_SUCCEEDED(accountGetProfile(&profile,uid))){
        if(R_SUCCEEDED(accountProfileGet(&profile,NULL,&base))&&base.nickname[0]){char nickname[sizeof(base.nickname)+1];memcpy(nickname,base.nickname,sizeof(base.nickname));nickname[sizeof(base.nickname)]=0;ascii_text(nickname,profile_name,sizeof(profile_name));}
        u32 size=0;if(R_SUCCEEDED(accountProfileGetImageSize(&profile,&size))&&size>0&&size<2*1024*1024){
            void *buf=malloc(size);u32 actual=0;
            if(buf&&R_SUCCEEDED(accountProfileLoadImage(&profile,buf,size,&actual))){SDL_RWops *rw=SDL_RWFromConstMem(buf,(int)actual);SDL_Surface *surface=rw?IMG_Load_RW(rw,1):NULL;if(surface){profile_icon=SDL_CreateTextureFromSurface(app_renderer,surface);SDL_FreeSurface(surface);}}
            free(buf);
        }
        accountProfileClose(&profile);
    }
    accountExit();
}

static SDL_Texture *load_jpeg_texture(SDL_Renderer *r,const void *bytes,int size){
    if(!bytes||size<=0)return NULL;
    SDL_RWops *rw=SDL_RWFromConstMem(bytes,size);if(!rw)return NULL;
    SDL_Surface *s=IMG_Load_RW(rw,1);if(!s)return NULL;
    SDL_Texture *t=SDL_CreateTextureFromSurface(r,s);SDL_FreeSurface(s);return t;
}

static void seed_palette(Game *g){
    u32 h=(u32)(g->id^(g->id>>32));g->top=(SDL_Color){(Uint8)(45+(h&127)),(Uint8)(55+((h>>7)&119)),(Uint8)(75+((h>>15)&115)),255};
    g->bottom=(SDL_Color){(Uint8)(15+((h>>4)&58)),(Uint8)(18+((h>>12)&47)),(Uint8)(27+((h>>20)&56)),255};
    g->accent=(SDL_Color){(Uint8)(128+(h&127)),(Uint8)(100+((h>>8)&135)),(Uint8)(120+((h>>16)&135)),255};
}

static void query_play_stats(void){
    if(!game_count)return;
    u64 *ids=calloc((size_t)game_count,sizeof(*ids));PdmApplicationPlayStatistics *stats=calloc((size_t)game_count,sizeof(*stats));
    if(!ids||!stats){free(ids);free(stats);return;}for(int i=0;i<game_count;i++)ids[i]=games[i].id;
    s32 got=0;if(R_SUCCEEDED(appletQueryApplicationPlayStatistics(stats,ids,game_count,&got))){playtime_available=true;if(got>game_count)got=game_count;for(s32 j=0;j<got;j++)for(int i=0;i<game_count;i++)if(games[i].id==stats[j].application_id){games[i].play_ns=stats[j].playtime;break;}}
    if(R_SUCCEEDED(pdmqryInitialize())){PdmLastPlayTime *lasts=calloc((size_t)game_count,sizeof(*lasts));if(lasts){got=0;if(R_SUCCEEDED(pdmqryQueryLastPlayTime(false,lasts,ids,game_count,&got))){if(got>game_count)got=game_count;for(s32 j=0;j<got;j++)for(int i=0;i<game_count;i++)if(games[i].id==lasts[j].application_id){games[i].last_play=lasts[j].flag?pdmPlayTimestampToPosix(lasts[j].timestamp_user):0;break;}}free(lasts);}pdmqryExit();}
    free(ids);free(stats);
}

static void load_library(void){
    if(R_FAILED(nsInitialize()))return;
    NsApplicationRecord *records=calloc(GAME_CAP,sizeof(*records));
    control_data=malloc(sizeof(*control_data));
    if(!records||!control_data){free(records);free(control_data);control_data=NULL;nsExit();return;}
    s32 total=0;
    if(R_SUCCEEDED(nsListApplicationRecord(records,GAME_CAP,0,&total))&&total>GAME_CAP)total=GAME_CAP;
    for(s32 i=0;i<total&&game_count<GAME_CAP;i++){
        u64 actual=0;memset(control_data,0,sizeof(*control_data));
        if(R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage,records[i].application_id,control_data,sizeof(*control_data),&actual))||actual<sizeof(control_data->nacp))continue;
        NacpLanguageEntry *entry=NULL;if(R_FAILED(nacpGetLanguageEntry(&control_data->nacp,&entry))||!entry||!entry->name[0])continue;
        Game *g=&games[game_count];memset(g,0,sizeof(*g));g->id=records[i].application_id;char raw_title[sizeof(entry->name)+1];memcpy(raw_title,entry->name,sizeof(entry->name));raw_title[sizeof(entry->name)]=0;ascii_text(raw_title,g->title,sizeof(g->title));seed_palette(g);
        game_count++;
    }
    free(records);free(control_data);control_data=NULL;nsExit();query_play_stats();
}

static void update_visible_icons(SDL_Renderer *r){
    if(!game_count||R_FAILED(nsInitialize()))return;
    if(!control_data)control_data=malloc(sizeof(*control_data));
    if(!control_data){nsExit();return;}
    for(int i=0;i<game_count;i++){int off=i-selected;if(abs(off)>3&&games[i].icon){SDL_DestroyTexture(games[i].icon);games[i].icon=NULL;games[i].icon_loaded=false;}}
    int first=selected-3;if(first<0)first=0;int last=selected+3;if(last>=game_count)last=game_count-1;
    for(int i=first;i<=last;i++){Game *g=&games[i];if(g->icon_loaded)continue;g->icon_loaded=true;
        memset(control_data,0,sizeof(*control_data));u64 actual=0;if(R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage,g->id,control_data,sizeof(*control_data),&actual))&&actual>=sizeof(control_data->nacp)){
            size_t size=(size_t)(actual-sizeof(control_data->nacp));if(size>sizeof(control_data->icon))size=sizeof(control_data->icon);g->icon=load_jpeg_texture(r,control_data->icon,(int)size);
        }
    }
    free(control_data);control_data=NULL;nsExit();
}

static void free_game_textures(void){for(int i=0;i<game_count;i++)if(games[i].icon){SDL_DestroyTexture(games[i].icon);games[i].icon=NULL;}}
static int compare_games(const void *aa,const void *bb){const Game *a=aa,*b=bb;if(sort_mode==SORT_PLAYTIME){if(a->play_ns>b->play_ns)return -1;if(a->play_ns<b->play_ns)return 1;}else if(sort_mode==SORT_RECENT){if(a->last_play>b->last_play)return -1;if(a->last_play<b->last_play)return 1;}int c=strcasecmp(a->title,b->title);return c;}
static void sort_library(SortMode mode){sort_mode=mode;if(game_count>1)qsort(games,(size_t)game_count,sizeof(Game),compare_games);selected=0;}

static void draw_gradient(SDL_Renderer *r,const Game *g){
    SDL_Color a=g?g->top:(SDL_Color){35,34,54,255},b=g?g->bottom:(SDL_Color){12,14,21,255};
    for(int y=0;y<SCREEN_H;y+=4){float t=(float)y/SCREEN_H;fill(r,0,y,SCREEN_W,4,(SDL_Color){(Uint8)(a.r+(b.r-a.r)*t),(Uint8)(a.g+(b.g-a.g)*t),(Uint8)(a.b+(b.b-a.b)*t),255});}
    if(g&&g->icon){int iw=0,ih=0;SDL_QueryTexture(g->icon,NULL,NULL,&iw,&ih);if(iw>0&&ih>0){int w=490,h=(int)((double)w*ih/iw);SDL_Rect image={704,90,w,h};SDL_SetTextureColorMod(g->icon,175,175,190);SDL_SetTextureAlphaMod(g->icon,112);SDL_RenderCopy(r,g->icon,NULL,&image);SDL_SetTextureColorMod(g->icon,255,255,255);SDL_SetTextureAlphaMod(g->icon,255);}fill(r,0,0,SCREEN_W,SCREEN_H,(SDL_Color){8,10,16,92});}
    if(g)filledCircleRGBA(r,1000,315,150,g->accent.r,g->accent.g,g->accent.b,42);
    for(int y=430;y<SCREEN_H;y+=4){Uint8 alpha=(Uint8)(40+175*(y-430)/(SCREEN_H-430));fill(r,0,y,SCREEN_W,4,(SDL_Color){8,10,16,alpha});}
}

static int card_center(int offset,int selected_width){
    if(offset==0)return SCREEN_W/2;
    int sign=offset<0?-1:1,n=abs(offset);
    int distance=selected_width/2+CARD_GAP+SMALL_W/2+(n-1)*(SMALL_W+CARD_GAP);return SCREEN_W/2+sign*distance;
}

static void draw_card(SDL_Renderer *r,const Game *g,int cx,int y,int w,int h,int chosen,int idx){
    int x=cx-w/2;round_box(r,x-3,y-3,w+6,h+6,15,chosen?(SDL_Color){192,174,255,255}:(SDL_Color){255,255,255,38});
    round_box(r,x,y,w,h,12,g->bottom);SDL_Rect clip={x+1,y+1,w-2,h-2};SDL_RenderSetClipRect(r,&clip);
    for(int py=0;py<h;py+=4){float t=(float)py/h;fill(r,x+1,y+py,w-2,4,(SDL_Color){(Uint8)(g->top.r+(g->bottom.r-g->top.r)*t),(Uint8)(g->top.g+(g->bottom.g-g->top.g)*t),(Uint8)(g->top.b+(g->bottom.b-g->top.b)*t),255});}
    if(g->icon){SDL_Rect dst={x+7,y+9,w-14,h*2/3};SDL_RenderCopy(r,g->icon,NULL,&dst);}
    else {filledCircleRGBA(r,cx,y+h/3,w/4,g->accent.r,g->accent.g,g->accent.b,185);}
    fill(r,x,y+h*3/4,w,h/4,(SDL_Color){9,11,17,220});
    char number[16];snprintf(number,sizeof(number),"%02d",idx+1);txt(r,x+9,y+10,number,(SDL_Color){255,255,255,215});
    char title[36];ascii_text(g->title,title,sizeof(title));txt(r,x+10,y+h-35,title,(SDL_Color){255,255,255,250});
    SDL_RenderSetClipRect(r,NULL);
}

static void draw_header(SDL_Renderer *r){
    round_box(r,24,18,310,46,10,(focus_area==FOCUS_TOP&&focus_top==0)?(SDL_Color){82,67,143,230}:(SDL_Color){12,14,21,195});
    if(profile_icon){SDL_Rect p={32,24,34,34};SDL_RenderCopy(r,profile_icon,NULL,&p);}else filledCircleRGBA(r,49,41,14,145,126,241,255);
    if(!profile_icon)txt(r,45,37,"P",(SDL_Color){255,255,255,255});
    char name[32];ascii_text(profile_name,name,sizeof(name));txt(r,74,35,name,(SDL_Color){255,255,255,250});txt(r,74,48,"PERFIL",(SDL_Color){165,168,183,255});
    txt(r,211,39,clock_text,(SDL_Color){255,255,255,255});
    round_box(r,267,35,25,13,4,(SDL_Color){205,207,216,255});fill(r,270,38,battery_level<0?2:20*battery_level/100,7,(SDL_Color){147,131,238,255});txt(r,297,38,battery_text,(SDL_Color){236,236,243,255});
    round_box(r,1004,18,252,46,10,(SDL_Color){12,14,21,195});
    for(int i=0;i<4;i++){
        int x=1011+i*60;round_box(r,x,25,51,32,7,(focus_area==FOCUS_TOP&&focus_top==i+1)?(SDL_Color){117,94,204,210}:(SDL_Color){255,255,255,15});
        if(i==0){roundedBoxRGBA(r,x+13,34,x+38,49,6,220,222,238,255);lineRGBA(r,x+18,38,x+18,45,54,54,69,255);lineRGBA(r,x+15,41,x+21,41,54,54,69,255);filledCircleRGBA(r,x+32,39,2,126,100,225,255);filledCircleRGBA(r,x+35,44,2,126,100,225,255);}
        else if(i==1){circleRGBA(r,x+25,41,9,220,222,238,255);circleRGBA(r,x+25,41,3,167,147,255,255);lineRGBA(r,x+25,29,x+25,32,220,222,238,255);lineRGBA(r,x+25,50,x+25,53,220,222,238,255);lineRGBA(r,x+13,41,x+16,41,220,222,238,255);lineRGBA(r,x+34,41,x+37,41,220,222,238,255);}
        else if(i==2){lineRGBA(r,x+20,30,x+20,45,220,222,238,255);lineRGBA(r,x+20,30,x+34,27,220,222,238,255);lineRGBA(r,x+34,27,x+34,42,220,222,238,255);filledCircleRGBA(r,x+17,46,4,220,222,238,255);filledCircleRGBA(r,x+31,43,4,220,222,238,255);}
        else {roundedRectangleRGBA(r,x+13,30,x+38,51,4,220,222,238,255);filledCircleRGBA(r,x+20,36,2,167,147,255,255);lineRGBA(r,x+15,47,x+22,40,220,222,238,255);lineRGBA(r,x+22,40,x+28,46,220,222,238,255);lineRGBA(r,x+28,46,x+33,41,220,222,238,255);}
    }
}

static void draw_screen(SDL_Renderer *r,float anim,int tick){
    const Game *g=game_count?&games[selected]:(Game*)0;draw_gradient(r,g);draw_header(r);
    const char *sort_names[]={"A-Z","MAS JUGADO","RECIENTE"};round_box(r,26,78,166,28,7,(focus_area==FOCUS_SORT)?(SDL_Color){117,94,204,230}:(SDL_Color){12,14,21,180});txt(r,37,89,"ORDEN",(SDL_Color){188,178,255,255});txt(r,78,89,sort_names[sort_mode],(SDL_Color){255,255,255,255});
    int selected_w=(int)(SMALL_W+(BIG_W-SMALL_W)*anim),selected_h=(int)(SMALL_H+(BIG_H-SMALL_H)*anim);
    if(game_count){
        for(int i=0;i<game_count;i++){
            int off=i-selected;if(abs(off)>3)continue;
            int w=off==0?selected_w:SMALL_W,h=off==0?selected_h:SMALL_H;int cx=card_center(off,selected_w);int y=158+(BIG_H-h)/2;
            draw_card(r,&games[i],cx,y,w,h,off==0,i);
        }
        char title[92];ascii_text(g->title,title,sizeof(title));txt(r,56,500,title,(SDL_Color){255,255,255,255});
        char play[64];if(playtime_available){u64 hours=g->play_ns/3600000000000ULL;snprintf(play,sizeof(play),"TIEMPO DE JUEGO  %llu h",(unsigned long long)hours);}else snprintf(play,sizeof(play),"TIEMPO DE JUEGO  --");txt(r,58,529,play,(SDL_Color){220,219,232,255});
    }else{
        round_box(r,330,230,620,180,18,(SDL_Color){13,15,23,210});txt(r,418,280,"NO SE ENCONTRARON JUEGOS",(SDL_Color){255,255,255,255});txt(r,395,315,"La consola no devolvio aplicaciones instaladas.",(SDL_Color){188,190,201,255});txt(r,488,350,"Pulsa - para salir",(SDL_Color){188,178,255,255});
    }
    int action_y=520;round_box(r,830,action_y,168,45,9,(focus_area==FOCUS_ACTIONS&&focus_action==0)?(SDL_Color){183,163,255,255}:(SDL_Color){238,235,255,255});txt(r,882,537,"A  INICIAR",(SDL_Color){32,29,47,255});round_box(r,1009,action_y,172,45,9,(focus_area==FOCUS_ACTIONS&&focus_action==1)?(SDL_Color){117,94,204,255}:(SDL_Color){255,255,255,22});txt(r,1051,537,"+  OPCIONES",(SDL_Color){255,255,255,255});
    txt(r,406,632,"ARRIBA / ABAJO  MENU       IZQ. / DER.  NAVEGAR       A  ELEGIR       -  SALIR",(SDL_Color){204,204,216,220});
    if(show_start_until>tick){round_box(r,420,580,440,34,8,(SDL_Color){13,15,23,230});txt(r,437,592,"No se pudo iniciar el juego seleccionado.",(SDL_Color){255,255,255,255});}
    if(options_open){round_box(r,370,195,540,330,16,(SDL_Color){14,16,24,248});txt(r,405,225,"NOVA  /  OPCIONES",(SDL_Color){194,179,255,255});const char *opts[]={"Iniciar el juego seleccionado","Ordenar: A-Z","Ordenar: mas jugados","Ordenar: recientes"};for(int i=0;i<4;i++){char row[96];snprintf(row,sizeof(row),"%c %s",i==option_cursor?'>':' ',opts[i]);txt(r,405,265+i*34,row,i==option_cursor?(SDL_Color){255,255,255,255}:(SDL_Color){225,225,235,255});}txt(r,405,410,"A   Elegir       B   Cerrar",(SDL_Color){194,179,255,255});}
    if(panel_open>=0){const char *names[]={"CONTROLES","AJUSTES","MUSICA","ALBUM / HOMEBREW","PERFIL"};SDL_Rect panel={400,215,480,270};if(panel_open==2)nova_music_draw_panel(menu_music,r,panel);else{round_box(r,panel.x,panel.y,panel.w,panel.h,16,(SDL_Color){14,16,24,248});txt(r,440,250,names[panel_open],(SDL_Color){194,179,255,255});txt(r,440,292,panel_open==4?profile_name:"Este panel se conectara en una proxima etapa.",(SDL_Color){255,255,255,255});txt(r,440,390,"A / B  Cerrar",(SDL_Color){194,179,255,255});}}
}

static void activate_header(void){if(focus_top==0)panel_open=4;else panel_open=focus_top-1;}
static void launch_selected(void){if(!game_count)return;nova_music_set_active(menu_music,false);if(R_FAILED(appletRequestLaunchApplication(games[selected].id,NULL))){show_start_until=frame_tick+180;nova_music_set_active(menu_music,true);}}
static void activate_sort_option(void){options_open=0;sort_library(option_cursor==1?SORT_AZ:option_cursor==2?SORT_PLAYTIME:SORT_RECENT);}

int main(int argc,char *argv[]){
    (void)argc;(void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0)return 1;
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
    SDL_Window *window=SDL_CreateWindow("NOVA HOME",0,0,SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);if(!window){IMG_Quit();SDL_Quit();return 1;}
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer){SDL_DestroyWindow(window);IMG_Quit();SDL_Quit();return 1;}
    SDL_RenderSetLogicalSize(renderer,SCREEN_W,SCREEN_H);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);app_renderer=renderer;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);hidInitializeTouchScreen();
    romfsInit();menu_music=nova_music_create();
    has_time_service=R_SUCCEEDED(timeInitialize());has_battery_service=R_SUCCEEDED(psmInitialize());load_profile();load_library();sort_library(SORT_AZ);update_visible_icons(renderer);update_system_status();
    int tick=0,done=0,touching=0;float anim=0.0f;
    while(appletMainLoop()&&!done){
        frame_tick=tick;int selection_changed=0;padUpdate(&pad);u64 down=padGetButtonsDown(&pad);if(down&HidNpadButton_Plus){if(panel_open>=0)panel_open=-1;else{options_open=!options_open;option_cursor=0;}}if(down&HidNpadButton_Minus)done=1;
        int nav_left=(down&HidNpadButton_Left)!=0,nav_right=(down&HidNpadButton_Right)!=0;
        int nav_up=(down&HidNpadButton_Up)!=0,nav_down=(down&HidNpadButton_Down)!=0;
        HidAnalogStickState left_stick=padGetStickPos(&pad,0);
        if(stick_repeat>0)stick_repeat--;
        if(!(options_open||panel_open>=0)&&stick_repeat==0){
            if(left_stick.x < -16000){nav_left=1;stick_repeat=11;}
            else if(left_stick.x > 16000){nav_right=1;stick_repeat=11;}
            else if(left_stick.y > 16000){nav_up=1;stick_repeat=11;}
            else if(left_stick.y < -16000){nav_down=1;stick_repeat=11;}
        }
        if(panel_open==2){if(nav_left)nova_music_previous(menu_music);if(nav_right)nova_music_next(menu_music);}
        else if(!(options_open||panel_open>=0)&&(nav_left||nav_right)){
            int dir=nav_left?-1:1;
            if(focus_area==FOCUS_LIBRARY&&game_count){int next=selected+dir;if(next>=0&&next<game_count){selected=next;anim=0.0f;selection_changed=1;}}
            else if(focus_area==FOCUS_TOP)focus_top=(focus_top+dir+5)%5;
            else if(focus_area==FOCUS_SORT){sort_library((SortMode)((sort_mode+dir+3)%3));selection_changed=1;}
            else if(focus_area==FOCUS_ACTIONS){int next=focus_action+dir;if(next>=0&&next<=1)focus_action=next;}
        }
        if(panel_open==2){if(nav_up)nova_music_adjust_volume(menu_music,5);if(nav_down)nova_music_adjust_volume(menu_music,-5);}
        else if(nav_up){
            if(options_open)option_cursor=(option_cursor+3)%4;
            else if(panel_open>=0)panel_open=-1;
            else if(focus_area==FOCUS_ACTIONS)focus_area=FOCUS_LIBRARY;
            else if(focus_area==FOCUS_LIBRARY)focus_area=FOCUS_SORT;
            else if(focus_area==FOCUS_SORT)focus_area=FOCUS_TOP;
        }
        if(nav_down&&panel_open!=2){
            if(options_open)option_cursor=(option_cursor+1)%4;
            else if(panel_open>=0)panel_open=-1;
            else if(focus_area==FOCUS_TOP)focus_area=FOCUS_SORT;
            else if(focus_area==FOCUS_SORT)focus_area=FOCUS_LIBRARY;
            else if(focus_area==FOCUS_LIBRARY)focus_area=FOCUS_ACTIONS;
        }
        if(down&HidNpadButton_B){options_open=0;panel_open=-1;}
        if(down&HidNpadButton_X){sort_library(SORT_AZ);selection_changed=1;}
        if(down&HidNpadButton_Y){sort_library(SORT_PLAYTIME);selection_changed=1;}
        if(down&HidNpadButton_R){sort_library(SORT_RECENT);selection_changed=1;}
        if(down&HidNpadButton_A){if(panel_open==2)nova_music_toggle_pause(menu_music);else if(panel_open>=0)panel_open=-1;else if(options_open){if(option_cursor==0){options_open=0;launch_selected();}else{activate_sort_option();selection_changed=1;}}else if(focus_area==FOCUS_TOP)activate_header();else if(focus_area==FOCUS_SORT){options_open=1;option_cursor=sort_mode+1;}else if(focus_area==FOCUS_ACTIONS&&focus_action==1){options_open=1;option_cursor=0;}else launch_selected();}
        HidTouchScreenState touch={0};if(hidGetTouchScreenStates(&touch,1)){
            if(touch.count>0&&!touching){touching=1;int x=touch.touches[0].x,y=touch.touches[0].y;
                if(options_open){if(x>370&&x<910&&y>=250&&y<390){if(y<284){options_open=0;launch_selected();}else{int row=(y-284)/34+1;if(row>3)row=3;options_open=0;sort_library(row==1?SORT_AZ:row==2?SORT_PLAYTIME:SORT_RECENT);selection_changed=1;}}else options_open=0;}
                else if(panel_open==2){SDL_Rect music_bounds={400,215,480,270};nova_music_handle_touch(menu_music,x,y,music_bounds);}
                else if(panel_open>=0){panel_open=-1;}
                else if(y<68&&x>990){focus_area=FOCUS_TOP;focus_top=1+(x-1008)/60;if(focus_top<1)focus_top=1;if(focus_top>4)focus_top=4;activate_header();}
                else if(y<68&&x<340){focus_area=FOCUS_TOP;focus_top=0;activate_header();}
                else if(y<115&&x<200){focus_area=FOCUS_SORT;sort_library((SortMode)((sort_mode+1)%3));selection_changed=1;}
                else if(y>500&&x>820){focus_area=FOCUS_ACTIONS;focus_action=x>1000?1:0;if(focus_action){options_open=1;option_cursor=0;}else launch_selected();}
                else if(y>140&&y<490){int chosen=-1;for(int i=0;i<game_count;i++){int off=i-selected;if(abs(off)>3)continue;int w=off==0?(int)(SMALL_W+(BIG_W-SMALL_W)*anim):SMALL_W;int cx=card_center(off,w);if(x>=cx-w/2&&x<=cx+w/2){chosen=i;break;}}if(chosen>=0){if(chosen==selected)launch_selected();else{selected=chosen;anim=0.0f;selection_changed=1;}focus_area=FOCUS_LIBRARY;}}
            }else if(touch.count==0)touching=0;
        }
        if(selection_changed)update_visible_icons(renderer);
        if(game_count&&anim<1.0f){anim+=0.08f;if(anim>1.0f)anim=1.0f;}
        if(tick%60==0)update_system_status();
        nova_music_update(menu_music);
        draw_screen(renderer,1.0f-(1.0f-anim)*(1.0f-anim),tick++);SDL_RenderPresent(renderer);SDL_Delay(16);
    }
    nova_music_destroy(menu_music);free_game_textures();if(profile_icon)SDL_DestroyTexture(profile_icon);if(has_time_service)timeExit();if(has_battery_service)psmExit();romfsExit();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);IMG_Quit();SDL_Quit();return 0;
}
