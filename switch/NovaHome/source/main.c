#include <switch.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <SDL_image.h>
#include "nova_music.h"
#include "nova_ui.h"
#include "nova_sgdb.h"
#include "nova_sgdb_task.h"
#include "nova_art_store.h"
#include <switch/applets/album_la.h>
#include <switch/applets/hid_la.h>
#include <switch/runtime/env.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#define SCREEN_W 1280
#define SCREEN_H 720
#define GAME_CAP 384
#define SMALL_W 146
#define SMALL_H 224
#define BIG_W 242
#define BIG_H 350
#define CARD_GAP 12

typedef struct {
    u64 id;
    char title[512];
    char author[256];
    char version[32];
    u64 play_ns;
    u64 last_play;
    bool playtime_loaded;
    bool playtime_valid;
    SDL_Texture *icon;
    SDL_Texture *art[NOVA_SGDB_KIND_COUNT];
    bool icon_loaded;
    unsigned art_loaded_mask;
    SDL_Color top, bottom, accent;
} Game;

typedef enum { SORT_AZ, SORT_PLAYTIME, SORT_RECENT } SortMode;
typedef enum { FOCUS_LIBRARY, FOCUS_TOP, FOCUS_SORT, FOCUS_ACTIONS } FocusArea;
static Game games[GAME_CAP];
static int game_count=0, selected=0, sort_mode=SORT_RECENT;
static int library_open=0,library_cursor=0;
static int library_scroll_row=0,library_prefetch_row=-1,library_prefetch_offset=0,library_prefetch_cursor=-1;
static int main_cover_prefetch_index=0;
static bool heavy_load_suppressed=false;
static int focus_area=FOCUS_LIBRARY, focus_top=0, focus_action=0;
static int options_open=0, personalization_open=0, option_cursor=0, panel_open=-1, show_start_until=0;
static int info_open=0,confirm_delete_open=0,done_requested=0;
static char transient_message[192]="";
static int frame_tick=0;
static int stick_repeat=0;
static int held_direction_frames=0;
static int sort_open=0,sort_cursor=0;
static int settings_cursor=0,settings_page=0;
static bool settings_wifi_enabled=false,settings_wifi_known=false;
static char settings_firmware[64]="No disponible",settings_atmosphere[64]="No disponible",settings_language[48]="No disponible";
static char settings_clock[64]="No disponible",settings_storage[96]="No disponible";
static float carousel_position=0.0f;
static int card_centers[GAME_CAP],card_widths[GAME_CAP];
static int icon_prefetch_center=-1,icon_prefetch_step=0;
static char profile_name[48]="Perfil";
static AccountUid active_profile_uid={0};
static char clock_text[8]="--:--";
static char battery_text[8]="--%";
static int battery_level=-1;
static SDL_Texture *profile_icon=NULL;
static AccountUid profile_uids[ACC_USER_LIST_SIZE];
static char profile_names[ACC_USER_LIST_SIZE][48];
static int profile_count=0,profile_cursor=0;
static NsApplicationControlData *control_data=NULL;
static SDL_Renderer *app_renderer=NULL;
static bool has_time_service=false,has_battery_service=false,pdm_service_ready=false,pdm_service_attempted=false;
static NovaMusic *menu_music=NULL;
static int steamgrid_open=0,steamgrid_step=0,steamgrid_cursor=0,steamgrid_menu_cursor=0,steamgrid_count=0,steamgrid_game_count=0,steamgrid_game_id=0;
static unsigned long long steamgrid_target_id=0;
static NovaSgdbKind steamgrid_kind=NOVA_SGDB_COVER;
static NovaSgdbEntry steamgrid_games[12],steamgrid_results[12];
static SDL_Texture *steamgrid_thumbs[12];
static NovaSgdbTask steamgrid_task;
static char steamgrid_key[160]="";
static char steamgrid_query[512]="";
static u64 selected_art_preload_id=0;
static int selected_art_preload_step=0;
static void launch_selected(void);
static void launch_game(const Game *game);
static void set_message(const char *message);
static void steamgrid_start_search(const char *query);
static int carousel_count(void);
static int main_game_count(void);
static int active_game_index(void);
static void open_library(void);
static int find_game_by_id(unsigned long long id);
static void update_library_scroll(void);

static void set_color(SDL_Renderer *r,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);}
static void fill(SDL_Renderer *r,int x,int y,int w,int h,SDL_Color c){set_color(r,c);SDL_Rect q={x,y,w,h};SDL_RenderFillRect(r,&q);}
static void round_box(SDL_Renderer *r,int x,int y,int w,int h,int rad,SDL_Color c){roundedBoxRGBA(r,x,y,x+w,y+h,rad,c.r,c.g,c.b,c.a);}
static void txt_weight(SDL_Renderer *r,int x,int y,const char *s,SDL_Color c,NovaFontWeight weight){nova_ui_draw_text(r,x,y,s,c,weight,13);}
static void txt(SDL_Renderer *r,int x,int y,const char *s,SDL_Color c){txt_weight(r,x,y,s,c,NOVA_FONT_REGULAR);}
static char *ascii_text(const char *src,char *dst,size_t cap){size_t j=0;for(size_t i=0;src[i]&&j+1<cap;i++){unsigned char c=(unsigned char)src[i];if(c<128)dst[j++]=(char)c;else if((c&0xC0)!=0x80)dst[j++]='?';}dst[j]=0;return dst;}

static void update_system_status(void){
    u64 timestamp=0; TimeCalendarTime cal={0}; TimeCalendarAdditionalInfo info={0};
    if(has_time_service&&R_SUCCEEDED(timeGetCurrentTime(TimeType_LocalSystemClock,&timestamp))&&R_SUCCEEDED(timeToCalendarTimeWithMyRule(timestamp,&cal,&info)))snprintf(clock_text,sizeof(clock_text),"%02u:%02u",cal.hour,cal.minute);
    u32 battery=0;if(has_battery_service&&R_SUCCEEDED(psmGetBatteryChargePercentage(&battery))){battery_level=(int)battery;snprintf(battery_text,sizeof(battery_text),"%u%%",battery);}
}

static void load_profile_user(AccountUid uid){
    if(profile_icon){SDL_DestroyTexture(profile_icon);profile_icon=NULL;}
    snprintf(profile_name,sizeof(profile_name),"Perfil");
    for(int i=0;i<game_count;i++){games[i].playtime_loaded=false;games[i].playtime_valid=false;games[i].play_ns=0;}
    if(R_FAILED(accountInitialize(AccountServiceType_Application)))return;
    AccountProfile profile={0};AccountProfileBase base={0};
    if(R_SUCCEEDED(accountGetProfile(&profile,uid))){
        active_profile_uid=uid;
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
static void load_profile(void){if(R_FAILED(accountInitialize(AccountServiceType_Application)))return;AccountUid uid={0};Result rc=accountGetLastOpenedUser(&uid);accountExit();if(R_SUCCEEDED(rc))load_profile_user(uid);}
static void open_profile_menu(void){profile_count=0;profile_cursor=0;if(R_FAILED(accountInitialize(AccountServiceType_Application))){set_message("No se pudieron consultar los perfiles.");return;}AccountUid listed[ACC_USER_LIST_SIZE];s32 total=0;if(R_SUCCEEDED(accountListAllUsers(listed,ACC_USER_LIST_SIZE,&total))){if(total>ACC_USER_LIST_SIZE)total=ACC_USER_LIST_SIZE;for(s32 i=0;i<total;i++){AccountProfile p={0};AccountProfileBase base={0};if(R_SUCCEEDED(accountGetProfile(&p,listed[i]))){if(R_SUCCEEDED(accountProfileGet(&p,NULL,&base))){profile_uids[profile_count]=listed[i];char nickname[sizeof(base.nickname)+1];memcpy(nickname,base.nickname,sizeof(base.nickname));nickname[sizeof(base.nickname)]=0;ascii_text(nickname,profile_names[profile_count],sizeof(profile_names[profile_count]));if(!profile_names[profile_count][0])snprintf(profile_names[profile_count],sizeof(profile_names[profile_count]),"Perfil %d",i+1);profile_count++;}accountProfileClose(&p);}}}accountExit();for(int i=0;i<profile_count;i++)if(profile_uids[i].uid[0]==active_profile_uid.uid[0]&&profile_uids[i].uid[1]==active_profile_uid.uid[1]){profile_cursor=i;break;}panel_open=4;}
static void select_profile_index(void){if(profile_cursor>=0&&profile_cursor<profile_count)load_profile_user(profile_uids[profile_cursor]);panel_open=-1;}

static void load_settings_summary(void){
    settings_wifi_known=false;
    if(R_SUCCEEDED(nifmInitialize(NifmServiceType_User))){settings_wifi_known=R_SUCCEEDED(nifmIsWirelessCommunicationEnabled(&settings_wifi_enabled));nifmExit();}
    SetSysFirmwareVersion fw={0};
    if(R_SUCCEEDED(setsysInitialize())){if(R_SUCCEEDED(setsysGetFirmwareVersion(&fw))&&fw.display_version[0]){char display[sizeof(fw.display_version)+1];memcpy(display,fw.display_version,sizeof(fw.display_version));display[sizeof(fw.display_version)]=0;char *ams=strstr(display,"|AMS ");if(ams){*ams=0;snprintf(settings_atmosphere,sizeof(settings_atmosphere),"%s",ams+5);}snprintf(settings_firmware,sizeof(settings_firmware),"%s",display);}setsysExit();}
    if(R_SUCCEEDED(setInitialize())){u64 code=0;SetLanguage lang;if(R_SUCCEEDED(setGetSystemLanguage(&code))&&R_SUCCEEDED(setMakeLanguage(code,&lang))){static const char *labels[]={"Japonés","Inglés (EE. UU.)","Francés","Alemán","Italiano","Español","Chino simplificado","Coreano","Neerlandés","Portugués","Ruso","Chino tradicional","Inglés (Reino Unido)","Francés (Canadá)","Español (Latinoamérica)","Chino simplificado","Chino tradicional","Portugués (Brasil)"};if((unsigned)lang<sizeof(labels)/sizeof(labels[0]))snprintf(settings_language,sizeof(settings_language),"%s",labels[lang]);}setExit();}
    u64 timestamp=0;TimeCalendarTime cal={0};TimeCalendarAdditionalInfo extra={0};
    if(R_SUCCEEDED(timeGetCurrentTime(TimeType_LocalSystemClock,&timestamp))&&R_SUCCEEDED(timeToCalendarTimeWithMyRule(timestamp,&cal,&extra)))snprintf(settings_clock,sizeof(settings_clock),"%04u-%02u-%02u  ·  %02u:%02u",cal.year,cal.month,cal.day,cal.hour,cal.minute);
    struct statvfs sv;if(statvfs("sdmc:/",&sv)==0){u64 total=(u64)sv.f_blocks*(u64)sv.f_frsize,freeb=(u64)sv.f_bavail*(u64)sv.f_frsize;snprintf(settings_storage,sizeof(settings_storage),"microSD: %.1f / %.1f GB libres",(double)freeb/1073741824.0,(double)total/1073741824.0);}
    FILE *vf=fopen("sdmc:/atmosphere/version.txt","r");if(vf){char line[64];if(fgets(line,sizeof(line),vf)){line[strcspn(line,"\r\n")]=0;if(line[0])snprintf(settings_atmosphere,sizeof(settings_atmosphere),"%s",line);}fclose(vf);}
}
static void open_settings_menu(void){settings_cursor=0;settings_page=0;load_settings_summary();panel_open=5;}
static void toggle_airplane_mode(void){
    bool enabled=false;if(R_FAILED(nifmInitialize(NifmServiceType_User))){set_message("No se pudo consultar el estado de red.");return;}
    Result read_rc=nifmIsWirelessCommunicationEnabled(&enabled);nifmExit();
    if(R_FAILED(read_rc)){set_message("No se pudo consultar el estado de red.");return;}
    Result rc=nifmInitialize(NifmServiceType_System);
    if(R_SUCCEEDED(rc)){rc=nifmSetWirelessCommunicationEnabled(!enabled);nifmExit();}
    if(R_FAILED(rc)){set_message("Atmosphere no concedió permiso para cambiar el modo avión desde NOVA.");return;}
    load_settings_summary();set_message(enabled?"Modo avión activado.":"Modo avión desactivado.");
}
static void open_network_configuration(void){
    if(R_FAILED(nifmInitialize(NifmServiceType_User))){set_message("No se pudo iniciar la configuración de internet.");return;}
    NifmRequest req={0};Result rc=nifmCreateRequest(&req,true);
    if(R_SUCCEEDED(rc)){rc=nifmRequestSubmitAndWait(&req);if(R_SUCCEEDED(rc))rc=nifmLaHandleNetworkRequestResult(&req);nifmRequestClose(&req);}
    nifmExit();if(R_FAILED(rc))set_message("No se pudo abrir la configuración de internet.");else load_settings_summary();
}
static void activate_settings_option(void){
    if(settings_page==0){switch(settings_cursor){case 0:toggle_airplane_mode();break;case 1:open_network_configuration();break;case 2:settings_page=1;load_settings_summary();break;case 3:settings_page=2;load_settings_summary();break;default:panel_open=-1;break;}}
    else settings_page=0;
}

static void draw_texture_fit(SDL_Renderer *r,SDL_Texture *texture,SDL_Rect bounds,bool crop){
    if(!texture)return;
    int iw=0,ih=0;
    if(SDL_QueryTexture(texture,NULL,NULL,&iw,&ih)<0||iw<=0||ih<=0)return;
    float source=(float)iw/(float)ih,target=(float)bounds.w/(float)bounds.h;
    if(crop){SDL_Rect src={0,0,iw,ih};if(source>target){src.w=(int)(ih*target);src.x=(iw-src.w)/2;}else if(source<target){src.h=(int)(iw/target);src.y=(ih-src.h)/2;}SDL_RenderCopy(r,texture,&src,&bounds);}
    else {int w=bounds.w,h=bounds.h;if(source>target)h=(int)(w/source);else w=(int)(h*source);SDL_Rect dst={bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2,w,h};SDL_RenderCopy(r,texture,NULL,&dst);}
}
static void draw_rounded_cover(SDL_Renderer *r,SDL_Texture *texture,SDL_Rect dst){
    if(!texture)return;
    int tw=0,th=0;if(SDL_QueryTexture(texture,NULL,NULL,&tw,&th)<0||tw<=0||th<=0)return;
    float image_ratio=(float)tw/(float)th,target_ratio=(float)dst.w/(float)dst.h;
    float u0=0.0f,v0=0.0f,u1=1.0f,v1=1.0f;
    if(image_ratio>target_ratio){float visible=target_ratio/image_ratio;u0=(1.0f-visible)*0.5f;u1=1.0f-u0;}
    else if(image_ratio<target_ratio){float visible=image_ratio/target_ratio;v0=(1.0f-visible)*0.5f;v1=1.0f-v0;}
    const int segments=6,per_corner=segments+1,total=per_corner*4;
    SDL_Vertex vertices[total+1];int indices[total*3];SDL_Color white={255,255,255,255};float radius=12.0f;
    if(radius>dst.w/3.0f)radius=dst.w/3.0f;
    if(radius>dst.h/3.0f)radius=dst.h/3.0f;
    float cx=dst.x+dst.w*0.5f,cy=dst.y+dst.h*0.5f;
    vertices[0].position=(SDL_FPoint){cx,cy};vertices[0].color=white;vertices[0].tex_coord=(SDL_FPoint){(u0+u1)*0.5f,(v0+v1)*0.5f};
    const float centers[4][2]={{dst.x+radius,dst.y+radius},{dst.x+dst.w-radius,dst.y+radius},{dst.x+dst.w-radius,dst.y+dst.h-radius},{dst.x+radius,dst.y+dst.h-radius}};
    const float starts[4]={180.0f,270.0f,0.0f,90.0f};
    for(int corner=0;corner<4;corner++)for(int step=0;step<=segments;step++){
        int index=1+corner*per_corner+step;float radians=(starts[corner]+90.0f*step/segments)*0.01745329252f;
        float x=centers[corner][0]+cosf(radians)*radius,y=centers[corner][1]+sinf(radians)*radius;
        vertices[index].position=(SDL_FPoint){x,y};vertices[index].color=white;
        vertices[index].tex_coord=(SDL_FPoint){u0+(x-dst.x)/dst.w*(u1-u0),v0+(y-dst.y)/dst.h*(v1-v0)};
    }
    for(int i=0;i<total;i++){indices[i*3]=0;indices[i*3+1]=1+(i+1)%total;indices[i*3+2]=1+i;}
    SDL_RenderGeometry(r,texture,vertices,total+1,indices,total*3);
}
static void draw_outlined_centered_fit_text(SDL_Renderer *r,int center_x,int y,int width,const char *text,int size){
    nova_ui_draw_text_centered_fit(r,center_x+1,y+2,text,(SDL_Color){5,5,10,245},NOVA_FONT_BOLD,size,width);nova_ui_draw_text_centered_fit(r,center_x,y,text,(SDL_Color){255,255,255,255},NOVA_FONT_SEMIBOLD,size,width);
}
static void purple_outline(SDL_Renderer *r,int x,int y,int w,int h,int radius,SDL_Color color){roundedRectangleRGBA(r,x,y,x+w,y+h,radius,color.r,color.g,color.b,color.a);}

static void art_path(const Game *g,int kind,char *path,size_t path_size){nova_art_store_path((unsigned long long)g->id,(NovaSgdbKind)kind,path,path_size);}
static void load_custom_art_kind(Game *g,int kind){
    if(!g||kind<0||kind>=NOVA_SGDB_KIND_COUNT||kind==NOVA_SGDB_ICON)return;
    unsigned bit=1u<<kind;if(g->art_loaded_mask&bit)return;char path[192];art_path(g,kind,path,sizeof(path));SDL_RWops *rw=SDL_RWFromFile(path,"rb");SDL_Surface *surface=rw?IMG_Load_RW(rw,1):NULL;
    if(!surface){g->art_loaded_mask|=bit;return;}
    int max_w=1920,max_h=1080;if(kind==NOVA_SGDB_COVER){max_w=1200;max_h=1800;}else if(kind==NOVA_SGDB_LOGO){max_w=2048;max_h=1024;}
    float scale=1.0f;if(surface->w>max_w)scale=(float)max_w/surface->w;if(surface->h*scale>max_h)scale=(float)max_h/surface->h;
    SDL_Surface *upload=surface;
    if(scale<1.0f){int width=(int)(surface->w*scale),height=(int)(surface->h*scale);if(width<1)width=1;if(height<1)height=1;SDL_Surface *scaled=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);if(!scaled||SDL_BlitScaled(surface,NULL,scaled,NULL)<0){if(scaled)SDL_FreeSurface(scaled);SDL_FreeSurface(surface);return;}upload=scaled;}
    g->art[kind]=SDL_CreateTextureFromSurface(app_renderer,upload);
    if(upload!=surface)SDL_FreeSurface(upload);
    SDL_FreeSurface(surface);
    if(g->art[kind])g->art_loaded_mask|=bit;
}
static void load_custom_art(Game *g,bool include_background,bool include_logo){
    load_custom_art_kind(g,NOVA_SGDB_COVER);
    if(include_background)load_custom_art_kind(g,NOVA_SGDB_BACKGROUND);
    if(include_logo)load_custom_art_kind(g,NOVA_SGDB_LOGO);
}
static void preload_selected_art_one(Game *g){
    if(!g)return;
    if(selected_art_preload_id!=g->id){selected_art_preload_id=g->id;selected_art_preload_step=0;}
    static const int kinds[]={NOVA_SGDB_COVER,NOVA_SGDB_BACKGROUND,NOVA_SGDB_LOGO};if(selected_art_preload_step<3)load_custom_art_kind(g,kinds[selected_art_preload_step++]);
}
static bool save_sgdb_key(void){if(mkdir("sdmc:/switch/NovaHome",0777)!=0&&errno!=EEXIST)return false;FILE *f=fopen("sdmc:/switch/NovaHome/steamgriddb.key","wb");if(!f)return false;size_t n=strlen(steamgrid_key);bool written=fwrite(steamgrid_key,1,n,f)==n;int closed=fclose(f);return written&&closed==0;}
static bool ensure_sgdb_key(void){
    if(steamgrid_key[0])return true;
    FILE *f=fopen("sdmc:/switch/NovaHome/steamgriddb.key","rb");if(f){size_t n=fread(steamgrid_key,1,sizeof(steamgrid_key)-1,f);fclose(f);while(n&&isspace((unsigned char)steamgrid_key[n-1]))n--;steamgrid_key[n]=0;if(steamgrid_key[0])return true;}
    return false;
}
static bool edit_sgdb_key(void){
    SwkbdConfig config;char entered[sizeof(steamgrid_key)]="";
    if(R_FAILED(swkbdCreate(&config,0)))return false;
    swkbdConfigMakePresetPassword(&config);swkbdConfigSetHeaderText(&config,"Clave API de SteamGridDB");swkbdConfigSetSubText(&config,"La clave se guarda en la SD de esta consola.");swkbdConfigSetStringLenMax(&config,128);
    Result result=swkbdShow(&config,entered,sizeof(entered));swkbdClose(&config);
    if(R_FAILED(result)||!entered[0])return false;
    for(const char *p=entered;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_')return false;
    snprintf(steamgrid_key,sizeof(steamgrid_key),"%s",entered);if(!save_sgdb_key()){steamgrid_key[0]=0;return false;}return true;
}
static void activate_steamgrid_menu(void){
    int target=active_game_index();
    if(steamgrid_menu_cursor==0){if(steamgrid_key[0])steamgrid_start_search(games[target].title);else set_message("Primero añade tu clave API con el botón inferior.");}
    else if(steamgrid_menu_cursor==1){SwkbdConfig config;steamgrid_query[0]=0;if(R_SUCCEEDED(swkbdCreate(&config,0))){swkbdConfigMakePresetDefault(&config);swkbdConfigSetHeaderText(&config,"Buscar cualquier juego en SteamGridDB");swkbdConfigSetSubText(&config,"Escribe el nombre del juego que quieres buscar.");swkbdConfigSetStringLenMax(&config,200);Result result=swkbdShow(&config,steamgrid_query,sizeof(steamgrid_query));swkbdClose(&config);if(R_SUCCEEDED(result)&&steamgrid_query[0]){if(steamgrid_key[0])steamgrid_start_search(steamgrid_query);else set_message("Primero añade tu clave API con el botón inferior.");}}}
    else if(steamgrid_menu_cursor==2){int target=find_game_by_id(steamgrid_target_id);char path[192];if(target>=0&&nova_art_store_path(steamgrid_target_id,steamgrid_kind,path,sizeof(path))&&nova_art_store_has(path)){remove(path);if(games[target].art[steamgrid_kind]){SDL_DestroyTexture(games[target].art[steamgrid_kind]);games[target].art[steamgrid_kind]=NULL;}games[target].art_loaded_mask&=~(1u<<steamgrid_kind);set_message("Imagen personalizada eliminada.");}else set_message("Este juego no tiene una imagen personalizada en esta categoría.");}
    else if(steamgrid_menu_cursor==3){if(steamgrid_key[0]){steamgrid_key[0]=0;remove("sdmc:/switch/NovaHome/steamgriddb.key");set_message("Clave guardada eliminada.");}else set_message("No hay una clave guardada.");}
    else if(steamgrid_menu_cursor==4){if(edit_sgdb_key())set_message("Clave guardada en esta consola.");else set_message("No se guardó una clave API.");}
}
static void steamgrid_clear_thumbs(void){for(int i=0;i<12;i++){if(steamgrid_thumbs[i])SDL_DestroyTexture(steamgrid_thumbs[i]);steamgrid_thumbs[i]=NULL;}}
static int find_game_by_id(unsigned long long id){for(int i=0;i<game_count;i++)if(games[i].id==id)return i;return -1;}
static void open_steamgrid(NovaSgdbKind kind){if(!game_count)return;steamgrid_target_id=games[active_game_index()].id;steamgrid_kind=kind;steamgrid_open=1;steamgrid_step=1;steamgrid_cursor=0;steamgrid_menu_cursor=0;ensure_sgdb_key();}
static void steamgrid_start_search(const char *query){if(!steamgrid_key[0]){set_message("Primero introduce tu clave API con el botón inferior.");return;}snprintf(steamgrid_query,sizeof(steamgrid_query),"%s",query?query:"");steamgrid_step=2;if(!nova_sgdb_task_start(&steamgrid_task,NOVA_SGDB_TASK_SEARCH,steamgrid_key,steamgrid_query,steamgrid_target_id,steamgrid_kind,0,NULL)){steamgrid_step=1;set_message(steamgrid_task.error);}}
static void steamgrid_start_art(void){steamgrid_game_id=steamgrid_games[steamgrid_cursor].id;steamgrid_clear_thumbs();steamgrid_step=4;if(!nova_sgdb_task_start(&steamgrid_task,NOVA_SGDB_TASK_ART,steamgrid_key,NULL,steamgrid_target_id,steamgrid_kind,(unsigned)steamgrid_game_id,NULL)){steamgrid_step=3;set_message(steamgrid_task.error);}}
static void steamgrid_start_download(void){if(steamgrid_cursor<0||steamgrid_cursor>=steamgrid_count)return;steamgrid_step=6;if(!nova_sgdb_task_start(&steamgrid_task,NOVA_SGDB_TASK_DOWNLOAD,NULL,steamgrid_results[steamgrid_cursor].url,steamgrid_target_id,steamgrid_kind,0,NULL)){steamgrid_step=5;set_message(steamgrid_task.error);}}
static void steamgrid_poll(void){if(!steamgrid_task.done)return;NovaSgdbTaskType type=steamgrid_task.type;bool ok=steamgrid_task.ok;int count=steamgrid_task.count;char error[sizeof(steamgrid_task.error)];snprintf(error,sizeof(error),"%s",steamgrid_task.error);
    if(type==NOVA_SGDB_TASK_SEARCH){if(ok){memcpy(steamgrid_games,steamgrid_task.entries,sizeof(steamgrid_games));steamgrid_game_count=count;steamgrid_count=count;steamgrid_cursor=0;steamgrid_step=3;}else steamgrid_step=1;}
    else if(type==NOVA_SGDB_TASK_ART){if(ok){memcpy(steamgrid_results,steamgrid_task.entries,sizeof(steamgrid_results));steamgrid_count=count;steamgrid_cursor=0;for(int i=0;i<count;i++){if(steamgrid_results[i].thumb[0]&&strstr(steamgrid_results[i].thumb,"sdmc:/")){SDL_RWops *rw=SDL_RWFromFile(steamgrid_results[i].thumb,"rb");SDL_Surface *s=rw?IMG_Load_RW(rw,1):NULL;if(s){steamgrid_thumbs[i]=SDL_CreateTextureFromSurface(app_renderer,s);SDL_FreeSurface(s);}}}steamgrid_step=5;}else steamgrid_step=3;}
    else if(type==NOVA_SGDB_TASK_DOWNLOAD){if(ok){int target=find_game_by_id(steamgrid_target_id);Game *g=target>=0?&games[target]:NULL;if(g){if(g->art[steamgrid_kind])SDL_DestroyTexture(g->art[steamgrid_kind]);g->art[steamgrid_kind]=NULL;g->art_loaded_mask&=~(1u<<steamgrid_kind);load_custom_art_kind(g,steamgrid_kind);}if(!g||!g->art[steamgrid_kind]){char path[192];if(nova_art_store_path(steamgrid_target_id,(NovaSgdbKind)steamgrid_kind,path,sizeof(path)))remove(path);if(g)g->art_loaded_mask&=~(1u<<steamgrid_kind);steamgrid_step=5;set_message("La descarga llegó, pero Switch no pudo decodificarla. Prueba otra imagen.");}else{steamgrid_clear_thumbs();steamgrid_open=0;static const char *labels[]={"Carátula","Icono","Fondo","Logotipo"};char msg[128];snprintf(msg,sizeof(msg),"%s guardado para este juego.",labels[steamgrid_kind]);set_message(msg);}}else{steamgrid_step=5;set_message(error);}}
    else if(type==NOVA_SGDB_TASK_VALIDATE){steamgrid_step=1;set_message(ok?"Clave API válida.":error);}
    nova_sgdb_task_finish(&steamgrid_task);if(type!=NOVA_SGDB_TASK_DOWNLOAD&&!ok)set_message(error[0]?error:"La solicitud no devolvió resultados.");
}

static void seed_palette(Game *g){
    u32 h=(u32)(g->id^(g->id>>32));g->top=(SDL_Color){(Uint8)(45+(h&127)),(Uint8)(55+((h>>7)&119)),(Uint8)(75+((h>>15)&115)),255};
    g->bottom=(SDL_Color){(Uint8)(15+((h>>4)&58)),(Uint8)(18+((h>>12)&47)),(Uint8)(27+((h>>20)&56)),255};
    g->accent=(SDL_Color){(Uint8)(128+(h&127)),(Uint8)(100+((h>>8)&135)),(Uint8)(120+((h>>16)&135)),255};
}

static void query_play_stats(void){
    if(!game_count)return;
    u64 *ids=calloc((size_t)game_count,sizeof(*ids));if(!ids)return;for(int i=0;i<game_count;i++)ids[i]=games[i].id;
    if(!pdm_service_attempted){pdm_service_attempted=true;pdm_service_ready=R_SUCCEEDED(pdmqryInitialize());}
    if(pdm_service_ready){PdmLastPlayTime *lasts=calloc((size_t)game_count,sizeof(*lasts));if(lasts){s32 got=0;if(R_SUCCEEDED(pdmqryQueryLastPlayTime(false,lasts,ids,game_count,&got))){if(got>game_count)got=game_count;for(s32 j=0;j<got;j++)for(int i=0;i<game_count;i++)if(games[i].id==lasts[j].application_id){games[i].last_play=lasts[j].flag?pdmPlayTimestampToPosix(lasts[j].timestamp_user):0;break;}}free(lasts);}}
    free(ids);
}
static void query_playtime_for_game(Game *g){
    if(!g||g->playtime_loaded)return;
    g->playtime_loaded=true;
    if(!pdm_service_ready)return;
    PdmPlayStatistics stats={0};Result rc=accountUidIsValid(&active_profile_uid)?pdmqryQueryPlayStatisticsByApplicationIdAndUserAccountId(g->id,active_profile_uid,false,&stats):pdmqryQueryPlayStatisticsByApplicationId(g->id,false,&stats);
    if(R_FAILED(rc))rc=pdmqryQueryPlayStatisticsByApplicationId(g->id,false,&stats);
    if(R_SUCCEEDED(rc)){g->play_ns=stats.playtime;g->playtime_valid=true;}
}
static void query_all_playtimes(void){for(int i=0;i<game_count;i++)query_playtime_for_game(&games[i]);}

static void official_icon_cache_path(u64 id,char *path,size_t size){snprintf(path,size,"sdmc:/switch/NovaHome/cache/%016llX_official.jpg",(unsigned long long)id);}
static void cache_official_icon(u64 id,const void *bytes,size_t length){
    static bool cache_ready=false;if(!cache_ready)cache_ready=nova_art_store_init();
    if(!bytes||length<4||!cache_ready)return;
    char path[192];official_icon_cache_path(id,path,sizeof(path));if(nova_art_store_has(path))return;
    FILE *file=fopen(path,"wb");if(!file)return;bool ok=fwrite(bytes,1,length,file)==length;if(fclose(file)!=0)ok=false;if(!ok)remove(path);
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
        Game *g=&games[game_count];memset(g,0,sizeof(*g));g->id=records[i].application_id;size_t title_len=strnlen(entry->name,sizeof(entry->name));if(title_len>=sizeof(g->title))title_len=sizeof(g->title)-1;memcpy(g->title,entry->name,title_len);g->title[title_len]=0;ascii_text(entry->author,g->author,sizeof(g->author));ascii_text(control_data->nacp.display_version,g->version,sizeof(g->version));seed_palette(g);size_t icon_size=(size_t)(actual-sizeof(control_data->nacp));if(icon_size>sizeof(control_data->icon))icon_size=sizeof(control_data->icon);cache_official_icon(g->id,control_data->icon,icon_size);
        game_count++;
    }
    free(records);free(control_data);control_data=NULL;nsExit();query_play_stats();
}

static void update_visible_icons(SDL_Renderer *r){
    if(!game_count)return;
    for(int i=0;i<game_count;i++){
        int distance=abs(i-selected);Game *g=&games[i];
        if(distance>9){for(int j=0;j<NOVA_SGDB_KIND_COUNT;j++)if(g->art[j]){SDL_DestroyTexture(g->art[j]);g->art[j]=NULL;}g->art_loaded_mask=0;}
        if(distance>13&&g->icon){SDL_DestroyTexture(g->icon);g->icon=NULL;g->icon_loaded=false;}
    }
    (void)r;
}
static void load_official_icon(Game *g,SDL_Renderer *r){
    if(!g||g->icon_loaded)return;
    g->icon_loaded=true;char path[192];official_icon_cache_path(g->id,path,sizeof(path));SDL_RWops *rw=SDL_RWFromFile(path,"rb");SDL_Surface *surface=rw?IMG_Load_RW(rw,1):NULL;if(surface){g->icon=SDL_CreateTextureFromSurface(r,surface);SDL_FreeSurface(surface);}
}
static void prefetch_one_icon(SDL_Renderer *r){
    if(!game_count||selected>=main_game_count()||heavy_load_suppressed)return;
    if(icon_prefetch_center!=selected){icon_prefetch_center=selected;icon_prefetch_step=0;}
    if(fabsf((float)selected-carousel_position)>0.04f)return;
    if(frame_tick%6)return;
    if(icon_prefetch_step>10)return;
    int index=selected;if(icon_prefetch_step>0){int distance=(icon_prefetch_step+1)/2;index=selected+(icon_prefetch_step%2?-distance:distance);}icon_prefetch_step++;
    if(index<0||index>=game_count||abs(index-selected)>5)return;
    load_official_icon(&games[index],r);
}
static void prefetch_library_cover_one(void){
    if(!library_open||!game_count)return;
    update_library_scroll();int row=library_scroll_row;
    if(row!=library_prefetch_row){library_prefetch_row=row;library_prefetch_offset=0;int first=row*5,last=first+10;if(last>game_count)last=game_count;int main_count=main_game_count();for(int i=0;i<game_count;i++){bool keep=i<main_count||(i>=first&&i<last);if(!keep){for(int k=0;k<NOVA_SGDB_KIND_COUNT;k++)if(games[i].art[k]){SDL_DestroyTexture(games[i].art[k]);games[i].art[k]=NULL;}games[i].art_loaded_mask=0;if(games[i].icon){SDL_DestroyTexture(games[i].icon);games[i].icon=NULL;games[i].icon_loaded=false;}}}}
    else if(library_cursor!=library_prefetch_cursor)library_prefetch_offset=0;
    library_prefetch_cursor=library_cursor;
    if(heavy_load_suppressed)return;
    int first=row*5,last=first+10;if(last>game_count)last=game_count;
    if(first+library_prefetch_offset>=last)return;
    int count=last-first,index=first+((library_cursor-first+library_prefetch_offset)%count);library_prefetch_offset++;
    Game *g=&games[index];load_custom_art(g,false,false);if(!g->art[NOVA_SGDB_COVER])load_official_icon(g,app_renderer);
}
static void prefetch_main_cover_one(void){
    int count=main_game_count();if(library_open||heavy_load_suppressed||count<2||fabsf((float)selected-carousel_position)>0.04f||frame_tick%8)return;
    int index=main_cover_prefetch_index++%count;if(index!=selected)load_custom_art(&games[index],false,false);
}

static void free_game_textures(void){for(int i=0;i<game_count;i++){if(games[i].icon)SDL_DestroyTexture(games[i].icon);for(int j=0;j<NOVA_SGDB_KIND_COUNT;j++)if(games[i].art[j])SDL_DestroyTexture(games[i].art[j]);}}
static int compare_games(const void *aa,const void *bb){const Game *a=aa,*b=bb;if(sort_mode==SORT_PLAYTIME){if(a->play_ns>b->play_ns)return -1;if(a->play_ns<b->play_ns)return 1;}else if(sort_mode==SORT_RECENT){if(a->last_play>b->last_play)return -1;if(a->last_play<b->last_play)return 1;}int c=strcasecmp(a->title,b->title);return c;}
static void sort_library(SortMode mode){sort_mode=mode;if(mode==SORT_PLAYTIME)query_all_playtimes();if(game_count>1)qsort(games,(size_t)game_count,sizeof(Game),compare_games);selected=0;library_cursor=0;library_scroll_row=0;library_prefetch_row=-1;}
static void load_preferences(void){FILE *f=fopen("sdmc:/switch/NovaHome/preferences.cfg","rb");if(!f)return;char magic[16]={0};int value=-1;if(fscanf(f,"%15s %d",magic,&value)==2&&!strcmp(magic,"NOVA_PREFS_V1")&&value>=SORT_AZ&&value<=SORT_RECENT)sort_mode=value;fclose(f);}
static void save_preferences(void){FILE *f=fopen("sdmc:/switch/NovaHome/preferences.cfg.part","wb");if(!f)return;bool ok=fprintf(f,"NOVA_PREFS_V1 %d\n",sort_mode)>=0;if(fclose(f)!=0)ok=false;if(!ok){remove("sdmc:/switch/NovaHome/preferences.cfg.part");return;}rename("sdmc:/switch/NovaHome/preferences.cfg.part","sdmc:/switch/NovaHome/preferences.cfg");}

static SDL_Color blend_color(SDL_Color a,SDL_Color b,float t){return (SDL_Color){(Uint8)(a.r+(b.r-a.r)*t),(Uint8)(a.g+(b.g-a.g)*t),(Uint8)(a.b+(b.b-a.b)*t),255};}

static int main_game_count(void){return game_count<9?game_count:9;}
static int carousel_count(void){return main_game_count()+1;}
static int active_game_index(void){if(library_open)return library_cursor<game_count?library_cursor:0;return selected<main_game_count()?selected:0;}
static void update_library_scroll(void){
    int row_count=(game_count+4)/5;if(row_count<=2){library_scroll_row=0;return;}
    int cursor_row=library_cursor/5;if(cursor_row<library_scroll_row)library_scroll_row=cursor_row;else if(cursor_row>=library_scroll_row+2)library_scroll_row=cursor_row-1;
    int max_row=row_count-2;if(library_scroll_row>max_row)library_scroll_row=max_row;if(library_scroll_row<0)library_scroll_row=0;
}
static void open_library(void){library_open=1;library_cursor=selected<main_game_count()?selected:0;library_scroll_row=library_cursor/5;if(library_scroll_row>0)library_scroll_row--;library_prefetch_row=-1;focus_area=FOCUS_LIBRARY;}

static void draw_gradient(SDL_Renderer *r,float position){
    Game *first=NULL,*second=NULL;float blend=0.0f;
    int count=main_game_count();
    if(count){if(position<0.0f)position=0.0f;if(position>(float)(count-1))position=(float)(count-1);int index=(int)position;first=&games[index];second=&games[index<count-1?index+1:index];blend=position-index;}
    SDL_Color base_top=first?blend_color(first->top,second->top,blend):(SDL_Color){35,34,54,255};
    SDL_Color base_bottom=first?blend_color(first->bottom,second->bottom,blend):(SDL_Color){12,14,21,255};
    SDL_Color base=blend_color(base_top,base_bottom,0.45f);fill(r,0,0,SCREEN_W,SCREEN_H,base);
    if(!library_open&&selected<main_game_count()&&games[selected].art[NOVA_SGDB_BACKGROUND]){SDL_Rect hero={0,0,SCREEN_W,SCREEN_H};SDL_SetTextureAlphaMod(games[selected].art[NOVA_SGDB_BACKGROUND],235);draw_texture_fit(r,games[selected].art[NOVA_SGDB_BACKGROUND],hero,true);SDL_SetTextureAlphaMod(games[selected].art[NOVA_SGDB_BACKGROUND],255);fill(r,0,0,SCREEN_W,SCREEN_H,(SDL_Color){8,10,16,96});}
}

static void build_card_layout(int anchor,int low,int high,const int *widths,int *centers){
    if(low>high)return;
    centers[anchor]=SCREEN_W/2;
    for(int i=anchor+1;i<=high;i++)centers[i]=centers[i-1]+widths[i-1]/2+CARD_GAP+widths[i]/2;
    for(int i=anchor-1;i>=low;i--)centers[i]=centers[i+1]-widths[i+1]/2-CARD_GAP-widths[i]/2;
}

static void update_carousel_layout(float position){
    int count=carousel_count();if(!count)return;
    if(position<0.0f)position=0.0f;
    if(position>(float)(count-1))position=(float)(count-1);
    int left=(int)floorf(position),right=left<count-1?left+1:left;
    float blend=position-left;
    int low=left-5,high=right+5;if(low<0)low=0;if(high>=count)high=count-1;
    int layout_left[GAME_CAP],layout_right[GAME_CAP];
    for(int i=low;i<=high;i++){
        float emphasis=1.0f-fabsf((float)i-position);
        if(emphasis<0.0f)emphasis=0.0f;
        if(emphasis>1.0f)emphasis=1.0f;
        card_widths[i]=(int)(SMALL_W+(BIG_W-SMALL_W)*emphasis);
    }
    build_card_layout(left,low,high,card_widths,layout_left);
    build_card_layout(right,low,high,card_widths,layout_right);
    for(int i=low;i<=high;i++)card_centers[i]=(int)(layout_left[i]+(layout_right[i]-layout_left[i])*blend);
}

static void draw_card(SDL_Renderer *r,const Game *g,int cx,int y,int w,int h,int chosen){
    int x=cx-w/2;round_box(r,x-3,y-3,w+6,h+6,15,chosen?(SDL_Color){192,174,255,255}:(SDL_Color){255,255,255,38});
    round_box(r,x,y,w,h,12,g->bottom);
    if(g->art[NOVA_SGDB_COVER]){SDL_Rect dst={x,y,w,h};draw_rounded_cover(r,g->art[NOVA_SGDB_COVER],dst);}
    else if(g->icon){SDL_Rect dst={x+6,y+8,w-12,h-16};draw_texture_fit(r,g->icon,dst,false);}
    else {filledCircleRGBA(r,cx,y+h/3,w/4,g->accent.r,g->accent.g,g->accent.b,185);}
    draw_outlined_centered_fit_text(r,cx,y+h-34,w-20,g->title,chosen?16:12);
}
static void draw_library_card(SDL_Renderer *r,int cx,int y,int w,int h,int chosen){
    int x=cx-w/2;round_box(r,x-3,y-3,w+6,h+6,15,chosen?(SDL_Color){192,174,255,255}:(SDL_Color){255,255,255,38});round_box(r,x,y,w,h,12,(SDL_Color){35,29,59,255});
    round_box(r,cx-w/3,y+h/5,w*2/3,h/3,12,(SDL_Color){91,71,143,255});round_box(r,cx-w/3+12,y+h/5+12,w*2/3-24,h/3-24,8,(SDL_Color){24,22,39,255});
    for(int i=0;i<3;i++)round_box(r,cx-w/4+i*(w/5),y+h/2,w/7,h/5,5,(SDL_Color){151,126,213,(Uint8)(220-i*24)});
    draw_outlined_centered_fit_text(r,cx,y+h-45,w-20,"BIBLIOTECA",chosen?16:13);
}

static void draw_settings_menu(SDL_Renderer *r){
    round_box(r,250,82,780,556,20,(SDL_Color){13,14,24,250});purple_outline(r,250,82,780,556,20,(SDL_Color){100,66,166,240});
    txt_weight(r,294,105,settings_page==0?"AJUSTES DE NOVA":settings_page==1?"GESTIÓN DE DATOS":"INFORMACIÓN DE LA CONSOLA",(SDL_Color){211,195,255,255},NOVA_FONT_BOLD);
    if(settings_page==0){const char *labels[]={"Modo avión","Configuración de internet","Gestión de datos","Información de la consola","Cerrar"};char state[96];for(int i=0;i<5;i++){int y=155+i*78;round_box(r,292,y,696,62,13,i==settings_cursor?(SDL_Color){68,50,109,245}:(SDL_Color){255,255,255,12});purple_outline(r,292,y,696,62,13,i==settings_cursor?(SDL_Color){151,113,221,240}:(SDL_Color){91,62,154,160});nova_ui_draw_text_fit(r,318,y+22,labels[i],(SDL_Color){245,244,250,255},i==settings_cursor?NOVA_FONT_BOLD:NOVA_FONT_MEDIUM,16,430);if(i==0){snprintf(state,sizeof(state),"%s",settings_wifi_known?(settings_wifi_enabled?"Desactivado":"Activado"):"No disponible");nova_ui_draw_text_fit(r,755,y+22,state,(SDL_Color){202,189,255,255},NOVA_FONT_SEMIBOLD,14,205);}else if(i==1)nova_ui_draw_text_fit(r,755,y+22,"Abrir configuración",(SDL_Color){202,189,255,255},NOVA_FONT_MEDIUM,12,205);else if(i==2)nova_ui_draw_text_fit(r,755,y+22,"Ver almacenamiento",(SDL_Color){202,189,255,255},NOVA_FONT_MEDIUM,12,205);else if(i==3)nova_ui_draw_text_fit(r,755,y+22,"Firmware y sistema",(SDL_Color){202,189,255,255},NOVA_FONT_MEDIUM,12,205);}txt(r,296,565,"A  Abrir / cambiar        B  Cerrar",(SDL_Color){194,179,255,255});}
    else if(settings_page==1){txt_weight(r,310,165,"Almacenamiento",(SDL_Color){202,189,255,255},NOVA_FONT_SEMIBOLD);txt(r,310,205,settings_storage,(SDL_Color){245,244,250,255});char count[96];snprintf(count,sizeof(count),"Juegos detectados por NOVA: %d",game_count);txt(r,310,250,count,(SDL_Color){245,244,250,255});txt(r,310,305,"La eliminación de juegos se encuentra en Opciones del juego.",(SDL_Color){195,193,207,255});txt(r,310,565,"A / B  Volver",(SDL_Color){194,179,255,255});}
    else{char line[160];snprintf(line,sizeof(line),"Firmware de Nintendo Switch: %s",settings_firmware);txt(r,310,165,line,(SDL_Color){245,244,250,255});snprintf(line,sizeof(line),"Atmosphère: %s",settings_atmosphere);txt(r,310,205,line,(SDL_Color){245,244,250,255});snprintf(line,sizeof(line),"Idioma: %s",settings_language);txt(r,310,245,line,(SDL_Color){245,244,250,255});snprintf(line,sizeof(line),"Fecha y hora: %s",settings_clock);txt(r,310,285,line,(SDL_Color){245,244,250,255});txt(r,310,565,"A / B  Volver",(SDL_Color){194,179,255,255});}
}

static void draw_header(SDL_Renderer *r){
    round_box(r,24,18,310,46,10,(focus_area==FOCUS_TOP&&focus_top==0)?(SDL_Color){82,67,143,230}:(SDL_Color){12,14,21,195});
    purple_outline(r,24,18,310,46,10,(SDL_Color){91,62,154,220});
    if(profile_icon){SDL_Rect p={32,24,34,34};SDL_RenderCopy(r,profile_icon,NULL,&p);}else filledCircleRGBA(r,49,41,14,145,126,241,255);
    if(!profile_icon)txt(r,45,37,"P",(SDL_Color){255,255,255,255});
    char name[32];ascii_text(profile_name,name,sizeof(name));txt(r,74,35,name,(SDL_Color){255,255,255,250});txt(r,74,48,"PERFIL",(SDL_Color){165,168,183,255});
    txt(r,211,39,clock_text,(SDL_Color){255,255,255,255});
    round_box(r,267,35,25,13,4,(SDL_Color){205,207,216,255});fill(r,270,38,battery_level<0?2:20*battery_level/100,7,(SDL_Color){147,131,238,255});txt(r,297,38,battery_text,(SDL_Color){236,236,243,255});
    round_box(r,1004,18,252,46,10,(SDL_Color){12,14,21,195});
    purple_outline(r,1004,18,252,46,10,(SDL_Color){91,62,154,220});
    for(int i=0;i<4;i++){
        int x=1011+i*60;round_box(r,x,25,51,32,7,(focus_area==FOCUS_TOP&&focus_top==i+1)?(SDL_Color){117,94,204,210}:(SDL_Color){255,255,255,15});
        if(i==0){roundedBoxRGBA(r,x+13,34,x+38,49,6,220,222,238,255);lineRGBA(r,x+18,38,x+18,45,54,54,69,255);lineRGBA(r,x+15,41,x+21,41,54,54,69,255);filledCircleRGBA(r,x+32,39,2,126,100,225,255);filledCircleRGBA(r,x+35,44,2,126,100,225,255);}
        else if(i==1){circleRGBA(r,x+25,41,9,220,222,238,255);circleRGBA(r,x+25,41,3,167,147,255,255);lineRGBA(r,x+25,29,x+25,32,220,222,238,255);lineRGBA(r,x+25,50,x+25,53,220,222,238,255);lineRGBA(r,x+13,41,x+16,41,220,222,238,255);lineRGBA(r,x+34,41,x+37,41,220,222,238,255);}
        else if(i==2){lineRGBA(r,x+20,30,x+20,45,220,222,238,255);lineRGBA(r,x+20,30,x+34,27,220,222,238,255);lineRGBA(r,x+34,27,x+34,42,220,222,238,255);filledCircleRGBA(r,x+17,46,4,220,222,238,255);filledCircleRGBA(r,x+31,43,4,220,222,238,255);}
        else {roundedRectangleRGBA(r,x+13,30,x+38,51,4,220,222,238,255);filledCircleRGBA(r,x+20,36,2,167,147,255,255);lineRGBA(r,x+15,47,x+22,40,220,222,238,255);lineRGBA(r,x+22,40,x+28,46,220,222,238,255);lineRGBA(r,x+28,46,x+33,41,220,222,238,255);}
    }
}

static void draw_screen(SDL_Renderer *r,float carousel_pos,int tick){
    if(!library_open)update_carousel_layout(carousel_pos);
    const Game *g=(game_count&&selected<main_game_count())?&games[selected]:(Game*)0;
    if(library_open)fill(r,0,0,SCREEN_W,SCREEN_H,(SDL_Color){17,16,27,255});else draw_gradient(r,carousel_pos);
    draw_header(r);
    const char *sort_names[]={"A-Z","MAS JUGADO","RECIENTE"};round_box(r,26,78,166,28,7,(focus_area==FOCUS_SORT)?(SDL_Color){117,94,204,230}:(SDL_Color){12,14,21,180});purple_outline(r,26,78,166,28,7,(SDL_Color){91,62,154,220});txt_weight(r,37,89,"ORDEN",(SDL_Color){188,178,255,255},NOVA_FONT_MEDIUM);txt_weight(r,78,89,sort_names[sort_mode],(SDL_Color){255,255,255,255},NOVA_FONT_MEDIUM);
    if(library_open){
        update_library_scroll();
        nova_ui_draw_text_centered_fit(r,640,86,"BIBLIOTECA",(SDL_Color){222,210,255,255},NOVA_FONT_BOLD,22,700);
        int first=library_scroll_row*5,last=first+10;if(last>game_count)last=game_count;
        for(int i=first;i<last;i++){int slot=i-first,col=slot%5,row=slot/5,x=74+col*226,y=126+row*244;bool chosen=i==library_cursor;SDL_Rect image={x+25,y+5,158,225};
            round_box(r,image.x-3,image.y-3,image.w+6,image.h+6,12,chosen?(SDL_Color){151,113,221,240}:(SDL_Color){255,255,255,42});
            if(games[i].art[NOVA_SGDB_COVER])draw_rounded_cover(r,games[i].art[NOVA_SGDB_COVER],image);else if(games[i].icon)draw_rounded_cover(r,games[i].icon,image);else{round_box(r,image.x,image.y,image.w,image.h,10,(SDL_Color){32,34,46,255});char initial[2]={games[i].title[0]?games[i].title[0]:'?',0};nova_ui_draw_text_centered(r,image.x+image.w/2,image.y+image.h/2-12,initial,(SDL_Color){169,150,232,255},NOVA_FONT_BOLD,24);}
            fill(r,image.x+2,image.y+184,image.w-4,40,(SDL_Color){8,8,14,196});draw_outlined_centered_fit_text(r,image.x+image.w/2,image.y+196,image.w-14,games[i].title,12);
        }
        nova_ui_draw_text_centered(r,640,680,"A  Iniciar      +  Opciones      B  Volver",(SDL_Color){194,179,255,255},NOVA_FONT_MEDIUM,12);
    }else{
        int count=carousel_count();int first=selected-4,last=selected+4;if(first<0)first=0;if(last>=count)last=count-1;
        for(int i=first;i<=last;i++){
            int w=card_widths[i],h=SMALL_H+(BIG_H-SMALL_H)*(w-SMALL_W)/(BIG_W-SMALL_W);
            int cx=card_centers[i],y=158+(BIG_H-h)/2;
            float emphasis=(float)(w-SMALL_W)/(BIG_W-SMALL_W);
            if(i<main_game_count())draw_card(r,&games[i],cx,y,w,h,emphasis>0.985f);else draw_library_card(r,cx,y,w,h,emphasis>0.985f);
        }
        if(g){round_box(r,315,520,650,176,18,(SDL_Color){15,13,27,205});purple_outline(r,315,520,650,176,18,(SDL_Color){100,66,166,240});
            if(g->art[NOVA_SGDB_LOGO])draw_texture_fit(r,g->art[NOVA_SGDB_LOGO],(SDL_Rect){334,531,286,150},false);else nova_ui_draw_text_centered_fit(r,477,591,g->title,(SDL_Color){235,230,250,255},NOVA_FONT_BOLD,22,270);
            round_box(r,642,530,300,43,11,(focus_area==FOCUS_ACTIONS&&focus_action==0)?(SDL_Color){183,163,255,255}:(SDL_Color){239,235,255,245});purple_outline(r,642,530,300,43,11,(SDL_Color){149,117,211,240});nova_ui_draw_text_centered_fit(r,792,543,"A   INICIAR",(SDL_Color){32,29,47,255},NOVA_FONT_BOLD,18,270);
            round_box(r,642,580,300,40,11,(focus_area==FOCUS_ACTIONS&&focus_action==1)?(SDL_Color){117,94,204,255}:(SDL_Color){255,255,255,22});purple_outline(r,642,580,300,40,11,(SDL_Color){100,66,166,240});nova_ui_draw_text_centered_fit(r,792,591,"+   OPCIONES",(SDL_Color){255,255,255,255},NOVA_FONT_BOLD,16,270);
            nova_ui_draw_text_centered_fit(r,792,628,"TIEMPO DE JUEGO",(SDL_Color){186,169,226,255},NOVA_FONT_SEMIBOLD,12,295);
            char play[64];if(g->playtime_valid){u64 hours=g->play_ns/3600000000000ULL,minutes=(g->play_ns%3600000000000ULL)/60000000000ULL;snprintf(play,sizeof(play),"%llu h %llu min",(unsigned long long)hours,(unsigned long long)minutes);}else snprintf(play,sizeof(play),g->playtime_loaded?"No disponible":"Cargando...");nova_ui_draw_text_centered_fit(r,792,646,play,(SDL_Color){248,244,255,255},NOVA_FONT_BOLD,17,295);
        }else{nova_ui_draw_text_centered_fit(r,640,646,"A - Abrir biblioteca",(SDL_Color){243,235,255,255},NOVA_FONT_SEMIBOLD,17,500);}
    }
    if(show_start_until>tick)nova_ui_draw_text_fit(r,270,666,transient_message[0]?transient_message:"No se pudo iniciar el juego seleccionado.",(SDL_Color){255,255,255,255},NOVA_FONT_MEDIUM,12,740);
    if(options_open){
        const char *main_opts[]={"Iniciar juego","Información","Personalización","Restaurar apariencia","Eliminar juego","Cancelar"};
        const char *art_opts[]={"Cambiar carátula","Cambiar fondo","Cambiar logotipo","Volver a opciones"};
        const char **opts=personalization_open?art_opts:main_opts;int option_count=personalization_open?4:6;
        round_box(r,286,76,708,572,20,(SDL_Color){14,13,24,250});purple_outline(r,286,76,708,572,20,(SDL_Color){100,66,166,240});
        txt_weight(r,330,98,personalization_open?"PERSONALIZACIÓN":"OPCIONES DEL JUEGO",(SDL_Color){211,195,255,255},NOVA_FONT_BOLD);
        nova_ui_draw_text_fit(r,330,123,games[active_game_index()].title,(SDL_Color){176,174,194,255},NOVA_FONT_MEDIUM,12,620);
        for(int i=0;i<option_count;i++){int y=151+i*56;round_box(r,322,y,636,48,12,i==option_cursor?(SDL_Color){68,50,109,245}:(SDL_Color){255,255,255,13});if(i==option_cursor)purple_outline(r,322,y,636,48,12,(SDL_Color){151,113,221,240});txt_weight(r,350,y+16,opts[i],(!personalization_open&&i==4)?(SDL_Color){255,190,196,255}:(SDL_Color){241,240,248,255},i==option_cursor?NOVA_FONT_BOLD:NOVA_FONT_MEDIUM);}
        txt(r,330,615,"A   Elegir                         B   Volver",(SDL_Color){194,179,255,255});
    }
    if(steamgrid_open){round_box(r,120,68,1040,584,18,(SDL_Color){12,14,22,250});
        if(steamgrid_step==1){const char *kinds[]={"CARÁTULA","LOGOTIPO","FONDO"};char heading[96];snprintf(heading,sizeof(heading),"PERSONALIZAR %s",kinds[steamgrid_kind==NOVA_SGDB_COVER?0:steamgrid_kind==NOVA_SGDB_LOGO?1:2]);txt_weight(r,180,105,heading,(SDL_Color){211,195,255,255},NOVA_FONT_BOLD);txt(r,180,140,steamgrid_key[0]?"Clave API configurada en esta consola.":"Configura tu clave una vez para buscar imágenes.",(SDL_Color){190,188,205,255});const char *clear_labels[]={"Borrar carátula aplicada","Borrar logotipo aplicado","Borrar fondo aplicado"};const char *labels[]={"Buscar imágenes de este juego","Buscar otro juego…",clear_labels[steamgrid_kind==NOVA_SGDB_COVER?0:steamgrid_kind==NOVA_SGDB_LOGO?1:2],"Borrar clave guardada","Introducir o cambiar clave API"};const int xs[]={180,650,180,650,415},ys[]={185,185,315,315,445},ws[]={450,450,450,450,450},hs[]={100,100,100,100,76};for(int i=0;i<5;i++){round_box(r,xs[i],ys[i],ws[i],hs[i],15,i==steamgrid_menu_cursor?(SDL_Color){68,50,109,245}:(SDL_Color){255,255,255,12});purple_outline(r,xs[i],ys[i],ws[i],hs[i],15,i==steamgrid_menu_cursor?(SDL_Color){151,113,221,245}:(SDL_Color){91,62,154,190});nova_ui_draw_text_fit(r,xs[i]+26,ys[i]+(hs[i]/2)-9,labels[i],(SDL_Color){245,244,250,255},i==steamgrid_menu_cursor?NOVA_FONT_BOLD:NOVA_FONT_MEDIUM,16,ws[i]-52);}txt(r,180,562,"A  Elegir                                  B  Cerrar",(SDL_Color){194,179,255,255});}
        else if(steamgrid_step==2||steamgrid_step==4||steamgrid_step==6){const char *status=steamgrid_step==2?"Buscando juegos…":steamgrid_step==4?"Cargando imágenes y miniaturas…":"Descargando y aplicando imagen…";txt_weight(r,190,180,"STEAMGRIDDB",(SDL_Color){194,179,255,255},NOVA_FONT_BOLD);txt(r,190,245,status,(SDL_Color){255,255,255,255});txt(r,190,305,"La interfaz seguirá respondiendo mientras se conecta.",(SDL_Color){178,181,195,255});}
        else if(steamgrid_step==3){txt_weight(r,155,96,"ELIGE EL JUEGO CORRECTO",(SDL_Color){211,195,255,255},NOVA_FONT_BOLD);nova_ui_draw_text_fit(r,155,116,steamgrid_query,(SDL_Color){178,174,196,255},NOVA_FONT_MEDIUM,11,700);for(int i=0;i<steamgrid_count&&i<9;i++){int y=145+i*45;round_box(r,150,y,740,40,10,i==steamgrid_cursor?(SDL_Color){65,48,105,245}:(SDL_Color){255,255,255,12});if(i==steamgrid_cursor)purple_outline(r,150,y,740,40,10,(SDL_Color){151,113,221,240});nova_ui_draw_text_fit(r,170,y+12,steamgrid_games[i].name,(SDL_Color){245,244,250,255},i==steamgrid_cursor?NOVA_FONT_BOLD:NOVA_FONT_MEDIUM,15,700);}if(!steamgrid_count)txt(r,170,160,"No se encontraron coincidencias para este título.",(SDL_Color){230,228,238,255});txt(r,155,590,"A  Ver imágenes      B  Volver a la clave",(SDL_Color){194,179,255,255});}
        else if(steamgrid_step==5){const char *kind_names[]={"CARÁTULAS","ICONOS","FONDOS","LOGOTIPOS"};char title[96];snprintf(title,sizeof(title),"%s  ·  %d imágenes",kind_names[steamgrid_kind],steamgrid_count);txt_weight(r,150,88,title,(SDL_Color){211,195,255,255},NOVA_FONT_BOLD);int first=(steamgrid_cursor/6)*6;for(int i=first;i<steamgrid_count&&i<first+6;i++){int col=(i-first)%3,row=(i-first)/3,x=150+col*232,y=126+row*205;round_box(r,x,y,216,188,12,i==steamgrid_cursor?(SDL_Color){59,44,96,255}:(SDL_Color){34,37,48,255});if(i==steamgrid_cursor)purple_outline(r,x,y,216,188,12,(SDL_Color){151,113,221,240});if(steamgrid_thumbs[i])draw_texture_fit(r,steamgrid_thumbs[i],(SDL_Rect){x+10,y+9,196,142},false);else{round_box(r,x+10,y+9,196,142,8,(SDL_Color){50,53,66,255});txt(r,x+24,y+68,"Miniatura no disponible",(SDL_Color){210,210,220,255});}nova_ui_draw_text_fit(r,x+12,y+158,steamgrid_results[i].style[0]?steamgrid_results[i].style:"Imagen de SteamGridDB",(SDL_Color){245,245,250,255},NOVA_FONT_MEDIUM,11,190);}round_box(r,860,126,270,394,14,(SDL_Color){29,32,43,255});purple_outline(r,860,126,270,394,14,(SDL_Color){100,66,166,220});if(steamgrid_count>0&&steamgrid_thumbs[steamgrid_cursor])draw_texture_fit(r,steamgrid_thumbs[steamgrid_cursor],(SDL_Rect){875,142,240,330},false);else txt(r,895,295,steamgrid_count?"Vista previa no disponible":"No hay imágenes en esta categoría.",(SDL_Color){210,210,220,255});if(steamgrid_count>0)nova_ui_draw_text_fit(r,878,486,steamgrid_results[steamgrid_cursor].style[0]?steamgrid_results[steamgrid_cursor].style:"Imagen de SteamGridDB",(SDL_Color){245,245,250,255},NOVA_FONT_MEDIUM,12,230);txt(r,150,570,"A  Aplicar       B  Volver       ← / →  Seleccionar",(SDL_Color){194,179,255,255});}
    }
    if(info_open&&game_count){Game *info=&games[active_game_index()];round_box(r,330,185,620,350,18,(SDL_Color){14,16,24,250});txt_weight(r,370,220,"INFORMACIÓN DEL JUEGO",(SDL_Color){194,179,255,255},NOVA_FONT_MEDIUM);nova_ui_draw_text(r,370,260,info->title,(SDL_Color){255,255,255,255},NOVA_FONT_SEMIBOLD,17);char line[300];snprintf(line,sizeof(line),"Desarrollador: %s",info->author[0]?info->author:"No disponible");txt(r,370,305,line,(SDL_Color){220,219,232,255});snprintf(line,sizeof(line),"Versión: %s",info->version[0]?info->version:"No disponible");txt(r,370,335,line,(SDL_Color){220,219,232,255});snprintf(line,sizeof(line),"ID: %016llX",(unsigned long long)info->id);txt(r,370,365,line,(SDL_Color){220,219,232,255});u64 hours=info->play_ns/3600000000000ULL;snprintf(line,sizeof(line),"Tiempo jugado: %llu h",(unsigned long long)hours);txt(r,370,395,line,(SDL_Color){220,219,232,255});txt(r,370,490,"A / B  Cerrar",(SDL_Color){194,179,255,255});}
    if(confirm_delete_open&&game_count){round_box(r,350,235,580,230,18,(SDL_Color){35,15,23,252});txt_weight(r,390,275,"ELIMINAR JUEGO",(SDL_Color){255,150,160,255},NOVA_FONT_BOLD);nova_ui_draw_text(r,390,315,"Esta acción eliminará el juego y sus datos de la consola.",(SDL_Color){255,255,255,255},NOVA_FONT_MEDIUM,15);txt(r,390,365,"A  Confirmar eliminación",(SDL_Color){255,180,180,255});txt(r,390,405,"B  Cancelar",(SDL_Color){220,219,232,255});}
    if(sort_open){round_box(r,370,195,540,260,16,(SDL_Color){14,16,24,250});txt_weight(r,405,225,"ORDENAR JUEGOS",(SDL_Color){194,179,255,255},NOVA_FONT_MEDIUM);const char *opts[]={"Título · A–Z","Tiempo de juego","Última vez abierto"};for(int i=0;i<3;i++){char row[96];snprintf(row,sizeof(row),"%c %s%s",i==sort_cursor?'>':' ',opts[i],i==sort_mode?"  · ACTUAL":"");txt_weight(r,405,267+i*40,row,i==sort_cursor?(SDL_Color){255,255,255,255}:(SDL_Color){225,225,235,255},i==sort_cursor?NOVA_FONT_BOLD:NOVA_FONT_MEDIUM);}txt(r,405,410,"A   Aplicar       B   Volver",(SDL_Color){194,179,255,255});}
    if(panel_open>=0){const char *names[]={"CONTROLES","AJUSTES","MUSICA","ALBUM / HOMEBREW","PERFIL","AJUSTES NOVA"};SDL_Rect panel={400,215,480,270};if(panel_open==2)nova_music_draw_panel(menu_music,r,panel);else if(panel_open==4){round_box(r,330,115,620,490,20,(SDL_Color){14,16,24,250});purple_outline(r,330,115,620,490,20,(SDL_Color){100,66,166,240});txt_weight(r,375,145,"ELIGE UN PERFIL",(SDL_Color){211,195,255,255},NOVA_FONT_BOLD);if(!profile_count)txt(r,375,205,"No se encontraron perfiles en esta consola.",(SDL_Color){235,232,242,255});for(int i=0;i<profile_count;i++){int y=195+i*48;bool active=profile_uids[i].uid[0]==active_profile_uid.uid[0]&&profile_uids[i].uid[1]==active_profile_uid.uid[1];round_box(r,365,y,550,40,10,i==profile_cursor?(SDL_Color){68,50,109,245}:(SDL_Color){255,255,255,12});if(i==profile_cursor)purple_outline(r,365,y,550,40,10,(SDL_Color){151,113,221,240});char row[96];snprintf(row,sizeof(row),"%.45s%s",profile_names[i],active?"  ·  ACTUAL":"");nova_ui_draw_text_fit(r,390,y+11,row,(SDL_Color){245,244,250,255},i==profile_cursor?NOVA_FONT_BOLD:NOVA_FONT_MEDIUM,15,500);}txt(r,375,565,"A  Seleccionar       B  Cerrar",(SDL_Color){194,179,255,255});}else if(panel_open==5){draw_settings_menu(r);}else{round_box(r,panel.x,panel.y,panel.w,panel.h,16,(SDL_Color){14,16,24,248});txt_weight(r,440,250,names[panel_open],(SDL_Color){194,179,255,255},NOVA_FONT_MEDIUM);txt(r,440,292,"Este panel se conectara en una proxima etapa.",(SDL_Color){255,255,255,255});txt_weight(r,440,390,"A / B  Cerrar",(SDL_Color){194,179,255,255},NOVA_FONT_MEDIUM);}}
}

static void set_message(const char *message){snprintf(transient_message,sizeof(transient_message),"%s",message);show_start_until=frame_tick+300;}
static void activate_game_option(void){
    if(!game_count){options_open=0;return;}
    int target=active_game_index();Game *g=&games[target];
    if(personalization_open){
        switch(option_cursor){
            case 0: options_open=0;personalization_open=0;open_steamgrid(NOVA_SGDB_COVER);break;
            case 1: options_open=0;personalization_open=0;open_steamgrid(NOVA_SGDB_BACKGROUND);break;
            case 2: options_open=0;personalization_open=0;open_steamgrid(NOVA_SGDB_LOGO);break;
            default: personalization_open=0;option_cursor=2;break;
        }
        return;
    }
    switch(option_cursor){
        case 0: options_open=0;launch_game(g);break;
        case 1: options_open=0;query_playtime_for_game(g);info_open=1;break;
        case 2: personalization_open=1;option_cursor=0;break;
        case 3: options_open=0;nova_art_store_restore(g->id);for(int i=0;i<NOVA_SGDB_KIND_COUNT;i++)if(g->art[i]){SDL_DestroyTexture(g->art[i]);g->art[i]=NULL;}g->art_loaded_mask=0;load_custom_art(g,true,true);set_message("Apariencia original restaurada.");break;
        case 4: options_open=0;confirm_delete_open=1;break;
        default: options_open=0;break;
    }
}
static void delete_selected_game(void){
    if(!game_count)return;
    int target=active_game_index();u64 id=games[target].id;
    if(R_SUCCEEDED(nsInitialize())){
        Result rc=nsDeleteApplicationCompletely(id);
        nsExit();
        if(R_SUCCEEDED(rc)){if(games[target].icon)SDL_DestroyTexture(games[target].icon);nova_art_store_restore(games[target].id);for(int i=0;i<NOVA_SGDB_KIND_COUNT;i++)if(games[target].art[i])SDL_DestroyTexture(games[target].art[i]);memmove(&games[target],&games[target+1],(size_t)(game_count-target-1)*sizeof(Game));game_count--;if(library_open){if(game_count==0)library_cursor=0;else if(library_cursor>=game_count)library_cursor=game_count-1;library_prefetch_row=-1;}else{if(game_count==0)selected=0;else{if(selected>=game_count)selected=game_count-1;if(selected>=carousel_count())selected=carousel_count()-1;}carousel_position=(float)selected;}confirm_delete_open=0;set_message("Juego eliminado de la consola.");update_visible_icons(app_renderer);}
        else{confirm_delete_open=0;set_message("La consola rechazó la eliminación. Revisa permisos/datos.");}
    }else{confirm_delete_open=0;set_message("No se pudo acceder al servicio de gestión de juegos.");}
}
static bool activate_header(bool right_shoulder_held){
    if(focus_top==0){open_profile_menu();return false;}
    if(focus_top==1){HidLaControllerSupportArg arg;hidLaCreateControllerSupportArg(&arg);Result rc=hidLaShowControllerSupportForSystem(NULL,&arg,true);if(R_FAILED(rc))set_message("No se pudo abrir el panel de controles.");return false;}
    if(focus_top==2){open_settings_menu();return false;}
    if(focus_top==3){panel_open=2;return false;}
    if(focus_top==4){
        // Holding R while opening Album preserves the Switch's original
        // screenshot viewer instead of redirecting to the Homebrew Menu.
        if(right_shoulder_held){
            Result rc=albumLaShowAllAlbumFilesForHomeMenu();
            if(R_FAILED(rc))set_message("No se pudo abrir el Álbum original de capturas.");
            return false;
        }
        if(envHasNextLoad()){
            const char *hbmenu_path="sdmc:/hbmenu.nro";
            struct stat info;
            if(stat(hbmenu_path,&info)!=0||!S_ISREG(info.st_mode)){
                set_message("No se encontró sdmc:/hbmenu.nro.");
                return false;
            }
            // hbloader forwards argv to the NRO. hbmenu needs argv[0] so
            // libnx can locate its own RomFS and mount assets.zip.
            Result rc=envSetNextLoad(hbmenu_path,hbmenu_path);
            if(R_SUCCEEDED(rc)){nova_music_set_active(menu_music,false);done_requested=1;return true;}
            set_message("El loader no aceptó el cambio a hbmenu.");
            return false;
        }
        set_message("Este entorno no permite cargar otro NRO. R + Álbum abre el visor original.");
    }
    return false;
}
static void launch_game(const Game *game){if(!game)return;nova_music_set_active(menu_music,false);if(R_FAILED(appletRequestLaunchApplication(game->id,NULL))){show_start_until=frame_tick+180;nova_music_set_active(menu_music,true);}}
static void launch_selected(void){if(selected>=main_game_count()){open_library();return;}if(selected>=0&&selected<game_count)launch_game(&games[selected]);}
static void activate_sort_option(void){sort_open=0;sort_library((SortMode)sort_cursor);save_preferences();carousel_position=0.0f;}

int main(int argc,char *argv[]){
    (void)argc;(void)argv;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0)return 1;
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG|IMG_INIT_WEBP);
    SDL_Window *window=SDL_CreateWindow("NOVA HOME",0,0,SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);if(!window){IMG_Quit();SDL_Quit();return 1;}
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer){SDL_DestroyWindow(window);IMG_Quit();SDL_Quit();return 1;}
    SDL_RenderSetLogicalSize(renderer,SCREEN_W,SCREEN_H);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);app_renderer=renderer;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);hidInitializeTouchScreen();
    romfsInit();nova_ui_init(renderer);menu_music=nova_music_create();
    fsdevMountSdmc();has_time_service=R_SUCCEEDED(timeInitialize());has_battery_service=R_SUCCEEDED(psmInitialize());load_preferences();load_profile();load_library();sort_library((SortMode)sort_mode);update_visible_icons(renderer);update_system_status();
    int tick=0,done=0,touching=0,dragging=0,drag_start_x=0,drag_start_selected=0,tap_card=-1;
    while(appletMainLoop()&&!done){
        frame_tick=tick;int selection_changed=0;padUpdate(&pad);u64 down=padGetButtonsDown(&pad),held=padGetButtons(&pad);steamgrid_poll();
        if(down&HidNpadButton_Plus){if(steamgrid_open&&!steamgrid_task.running){steamgrid_open=0;steamgrid_clear_thumbs();}else if(panel_open>=0)panel_open=-1;else if(info_open)info_open=0;else if(confirm_delete_open)confirm_delete_open=0;else if(sort_open)sort_open=0;else if(options_open){options_open=0;personalization_open=0;}else if(!library_open&&selected>=main_game_count())open_library();else{options_open=1;personalization_open=0;option_cursor=0;}}
        if(down&HidNpadButton_Minus)done=1;
        HidAnalogStickState left_stick=padGetStickPos(&pad,0);
        int raw_left=(held&HidNpadButton_Left)||left_stick.x < -16000;
        int raw_right=(held&HidNpadButton_Right)||left_stick.x > 16000;
        int raw_up=(held&HidNpadButton_Up)||left_stick.y > 16000;
        int raw_down=(held&HidNpadButton_Down)||left_stick.y < -16000;
        int nav_left=0,nav_right=0,nav_up=0,nav_down=0;
        int any_navigation=raw_left||raw_right||raw_up||raw_down;
        heavy_load_suppressed=any_navigation;
        if(!any_navigation){held_direction_frames=0;stick_repeat=0;}
        else {
            held_direction_frames++;
            if(down&(HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_Up|HidNpadButton_Down)){
                nav_left=raw_left;nav_right=raw_right;nav_up=raw_up;nav_down=raw_down;
                stick_repeat=held_direction_frames>16?5:8;
            } else if(stick_repeat>0)stick_repeat--;
            else {
                nav_left=raw_left;nav_right=raw_right;nav_up=raw_up;nav_down=raw_down;
                stick_repeat=held_direction_frames>16?5:8;
            }
        }
        if(panel_open==2){if(nav_left)nova_music_previous(menu_music);if(nav_right)nova_music_next(menu_music);if(nav_up)nova_music_adjust_volume(menu_music,5);if(nav_down)nova_music_adjust_volume(menu_music,-5);}
        else if(panel_open==4){if(nav_up&&profile_cursor>0)profile_cursor--;if(nav_down&&profile_cursor<profile_count-1)profile_cursor++;}
        else if(panel_open==5&&settings_page==0){if(nav_up&&settings_cursor>0)settings_cursor--;if(nav_down&&settings_cursor<4)settings_cursor++;}
        else if(steamgrid_open&&!steamgrid_task.running){if(steamgrid_step==1){int row=steamgrid_menu_cursor/2,col=steamgrid_menu_cursor%2;if(nav_left&&col)steamgrid_menu_cursor--;if(nav_right&&!col&&steamgrid_menu_cursor<4)steamgrid_menu_cursor++;if(nav_up&&row>0)steamgrid_menu_cursor-=2;if(nav_down&&row<2&&steamgrid_menu_cursor+2<5)steamgrid_menu_cursor+=2;}else if(steamgrid_step==3){if(nav_up&&steamgrid_cursor>0)steamgrid_cursor--;if(nav_down&&steamgrid_cursor<steamgrid_count-1)steamgrid_cursor++;}else if(steamgrid_step==5&&steamgrid_count>0){if(nav_left&&steamgrid_cursor>0)steamgrid_cursor--;if(nav_right&&steamgrid_cursor<steamgrid_count-1)steamgrid_cursor++;if(nav_up&&steamgrid_cursor>=3)steamgrid_cursor-=3;if(nav_down&&steamgrid_cursor+3<steamgrid_count)steamgrid_cursor+=3;}}
        else if(sort_open){if(nav_up&&sort_cursor>0)sort_cursor--;if(nav_down&&sort_cursor<2)sort_cursor++;}
        else if(options_open){int option_count=personalization_open?4:6;if(nav_up&&option_cursor>0)option_cursor--;if(nav_down&&option_cursor<option_count-1)option_cursor++;}
        else if(library_open&&panel_open<0){
            if(focus_area==FOCUS_TOP){if(nav_left)focus_top=(focus_top+4)%5;if(nav_right)focus_top=(focus_top+1)%5;if(nav_down)focus_area=FOCUS_SORT;}
            else if(focus_area==FOCUS_SORT){if(nav_up)focus_area=FOCUS_TOP;if(nav_down)focus_area=FOCUS_LIBRARY;}
            else {if(nav_left&&library_cursor>0)library_cursor--;if(nav_right&&library_cursor<game_count-1)library_cursor++;if(nav_up){if(library_cursor>=5)library_cursor-=5;else focus_area=FOCUS_SORT;}if(nav_down&&library_cursor+5<game_count)library_cursor+=5;update_library_scroll();}
        }
        else if(panel_open<0&&!info_open&&!confirm_delete_open){
            if(nav_left||nav_right){int dir=nav_left?-1:1;if(focus_area==FOCUS_LIBRARY&&carousel_count()){int next=selected+dir;if(next>=0&&next<carousel_count()){selected=next;selection_changed=1;}}else if(focus_area==FOCUS_TOP)focus_top=(focus_top+dir+5)%5;}
            if(nav_up){if(focus_area==FOCUS_ACTIONS){if(focus_action==1)focus_action=0;else focus_area=FOCUS_LIBRARY;}else if(focus_area==FOCUS_LIBRARY)focus_area=FOCUS_SORT;else if(focus_area==FOCUS_SORT)focus_area=FOCUS_TOP;}
            if(nav_down){if(focus_area==FOCUS_TOP)focus_area=FOCUS_SORT;else if(focus_area==FOCUS_SORT)focus_area=FOCUS_LIBRARY;else if(focus_area==FOCUS_LIBRARY){if(selected<main_game_count())focus_area=FOCUS_ACTIONS;}else if(focus_area==FOCUS_ACTIONS){if(focus_action==0)focus_action=1;else focus_area=FOCUS_LIBRARY;}}
        }
        if(down&HidNpadButton_B){if(steamgrid_open&&!steamgrid_task.running){if(steamgrid_step==5){steamgrid_clear_thumbs();steamgrid_step=3;steamgrid_count=steamgrid_game_count;steamgrid_cursor=0;}else if(steamgrid_step==3)steamgrid_step=1;else{steamgrid_open=0;steamgrid_clear_thumbs();}}else if(confirm_delete_open)confirm_delete_open=0;else if(info_open)info_open=0;else if(options_open&&personalization_open){personalization_open=0;option_cursor=2;}else if(options_open)options_open=0;else if(sort_open)sort_open=0;else if(panel_open==5&&settings_page>0)settings_page=0;else if(panel_open>=0)panel_open=-1;else if(library_open)library_open=0;}
        if(down&HidNpadButton_A){
            if(confirm_delete_open)delete_selected_game();
            else if(steamgrid_open&&!steamgrid_task.running){if(steamgrid_step==1)activate_steamgrid_menu();else if(steamgrid_step==3&&steamgrid_count>0)steamgrid_start_art();else if(steamgrid_step==5)steamgrid_start_download();}
            else if(info_open)info_open=0;
            else if(panel_open==2)nova_music_toggle_pause(menu_music);
            else if(panel_open==4)select_profile_index();
            else if(panel_open==5)activate_settings_option();
            else if(sort_open){activate_sort_option();selection_changed=1;}
            else if(options_open)activate_game_option();
            else if(library_open&&panel_open<0){if(library_cursor>=0&&library_cursor<game_count)launch_game(&games[library_cursor]);}
            else if(panel_open>=0)panel_open=-1;
            else if(focus_area==FOCUS_TOP)activate_header((held&HidNpadButton_R)!=0);
            else if(focus_area==FOCUS_SORT){sort_open=1;sort_cursor=sort_mode;}
            else if(focus_area==FOCUS_ACTIONS&&focus_action==1){options_open=1;personalization_open=0;option_cursor=0;}
            else launch_selected();
        }
        HidTouchScreenState touch={0};hidGetTouchScreenStates(&touch,1);
            if(touch.count>0){int x=touch.touches[0].x,y=touch.touches[0].y;
            if(!touching){touching=1;dragging=0;tap_card=-1;
                if(sort_open){if(x>=370&&x<=910&&y>=267&&y<387){sort_cursor=(y-267)/40;if(sort_cursor<0)sort_cursor=0;if(sort_cursor>2)sort_cursor=2;activate_sort_option();selection_changed=1;}else sort_open=0;}
                else if(panel_open==5){if(settings_page==0&&x>=292&&x<=988&&y>=155&&y<545){settings_cursor=(y-155)/78;activate_settings_option();}else if(settings_page>0)settings_page=0;else panel_open=-1;}
                else if(panel_open==4){if(x>=365&&x<=915&&y>=195&&y<195+profile_count*48){profile_cursor=(y-195)/48;select_profile_index();}else panel_open=-1;}
                else if(steamgrid_open&&!steamgrid_task.running){if(steamgrid_step==1){bool pressed=false;const int xs[]={180,650,180,650,415},ys[]={185,185,315,315,445},ws[]={450,450,450,450,450},hs[]={100,100,100,100,76};for(int i=0;i<5;i++)if(x>=xs[i]&&x<=xs[i]+ws[i]&&y>=ys[i]&&y<ys[i]+hs[i]){steamgrid_menu_cursor=i;pressed=true;break;}if(pressed)activate_steamgrid_menu();}else if(steamgrid_step==3&&y>=145&&y<145+steamgrid_count*45){int row=(y-145)/45;if(row>=0&&row<steamgrid_count){steamgrid_cursor=row;steamgrid_start_art();}}else if(steamgrid_step==5&&y>=120&&y<540){int col=(x-150)/232,row=(y-126)/205,index=(steamgrid_cursor/6)*6+row*3+col;if(col>=0&&col<3&&row>=0&&row<2&&index<steamgrid_count)steamgrid_cursor=index;}}
                else if(confirm_delete_open){if(x>=350&&x<=930&&y>=350&&y<395)delete_selected_game();else confirm_delete_open=0;}
                else if(info_open)info_open=0;
                else if(options_open){int option_count=personalization_open?4:6;if(x>=322&&x<=958&&y>=151&&y<151+option_count*56){option_cursor=(y-151)/56;if(option_cursor<0)option_cursor=0;if(option_cursor>=option_count)option_cursor=option_count-1;activate_game_option();}else options_open=0;}
                else if(panel_open==2){SDL_Rect music_bounds={400,215,480,270};nova_music_handle_touch(menu_music,x,y,music_bounds);}
                else if(library_open&&y>=126&&y<620&&x>=74&&x<1200){int col=(x-74)/226,row=(y-126)/244,index=(library_scroll_row+row)*5+col;if(col>=0&&col<5&&row>=0&&row<2&&index<game_count){library_cursor=index;update_library_scroll();}}
        else if(panel_open>=0)panel_open=-1;
                else if(y<68&&x>990){focus_area=FOCUS_TOP;focus_top=1+(x-1008)/60;if(focus_top<1)focus_top=1;if(focus_top>4)focus_top=4;activate_header((held&HidNpadButton_R)!=0);}
                else if(y<68&&x<340){focus_area=FOCUS_TOP;focus_top=0;activate_header((held&HidNpadButton_R)!=0);}
                else if(y<115&&x<200){focus_area=FOCUS_SORT;sort_open=1;sort_cursor=sort_mode;}
                else if(y>560&&x>670&&x<970){if(selected>=main_game_count())launch_selected();else{focus_area=FOCUS_ACTIONS;focus_action=x>815?1:0;if(focus_action){options_open=1;personalization_open=0;option_cursor=0;}else launch_selected();}}
                else if(y>140&&y<490){drag_start_x=x;drag_start_selected=selected;int count=carousel_count();for(int i=0;i<count;i++){if(abs(i-selected)>4)continue;if(abs(x-card_centers[i])<=card_widths[i]/2){tap_card=i;break;}}}
            } else if(panel_open==2&&y>=465&&y<=485){SDL_Rect music_bounds={400,215,480,270};nova_music_handle_touch(menu_music,x,y,music_bounds);}
            else if(tap_card>=0&&y>140&&y<490){int dx=drag_start_x-x;if(abs(dx)>12)dragging=1;if(dragging){heavy_load_suppressed=true;int next=drag_start_selected+(int)lroundf((float)dx/(SMALL_W+CARD_GAP));if(next<0)next=0;if(next>=carousel_count())next=carousel_count()-1;if(next!=selected){selected=next;selection_changed=1;focus_area=FOCUS_LIBRARY;}}}
        } else if(touching){
            if(panel_open!=2&&tap_card>=0&&!dragging){if(tap_card==selected)launch_selected();else{selected=tap_card;selection_changed=1;}focus_area=FOCUS_LIBRARY;}
            touching=0;dragging=0;tap_card=-1;
        }
        if(done_requested)done=1;
        if(selection_changed){icon_prefetch_center=selected;icon_prefetch_step=0;update_visible_icons(renderer);}
        if(!library_open){float delta=(float)selected-carousel_position;carousel_position+=delta*0.24f;if(fabsf(delta)<0.025f)carousel_position=(float)selected;if(game_count&&selected<main_game_count()&&!heavy_load_suppressed&&fabsf((float)selected-carousel_position)<0.04f){query_playtime_for_game(&games[selected]);preload_selected_art_one(&games[selected]);}}
        if(library_open)prefetch_library_cover_one();else{prefetch_one_icon(renderer);prefetch_main_cover_one();}
        if(tick%60==0)update_system_status();
        nova_music_update(menu_music);
        draw_screen(renderer,carousel_position,tick++);SDL_RenderPresent(renderer);
    }
    if(steamgrid_task.running){threadWaitForExit(&steamgrid_task.thread);threadClose(&steamgrid_task.thread);steamgrid_task.running=false;}
    steamgrid_clear_thumbs();nova_music_destroy(menu_music);free_game_textures();if(profile_icon)SDL_DestroyTexture(profile_icon);if(pdm_service_ready)pdmqryExit();if(has_time_service)timeExit();if(has_battery_service)psmExit();nova_ui_destroy();romfsExit();fsdevUnmountDevice("sdmc");SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);IMG_Quit();SDL_Quit();return 0;
}
