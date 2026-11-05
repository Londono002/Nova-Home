#include <switch.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define W 1280
#define H 720

typedef enum { PAGE_OVERVIEW, PAGE_FILES, PAGE_RECOVERY } Page;
typedef enum { ACTION_NONE, ACTION_ENABLE, ACTION_DISABLE, ACTION_RESTART } PendingAction;

static SDL_Renderer *renderer;
static TTF_Font *font_regular, *font_medium, *font_semibold, *font_bold;
static bool fonts_ready;
static Page page=PAGE_OVERVIEW;
static PendingAction pending=ACTION_NONE;
static int cursor=0;
static bool simulated_active=false;
static bool simulated_restart_pending=false;
static bool simulated_target_active=false;
static int notice_frames=0;
static char notice[160]="Prototipo local: la SD y el HOME no se modifican.";

static void box(int x,int y,int w,int h,int radius,SDL_Color c){roundedBoxRGBA(renderer,x,y,x+w,y+h,radius,c.r,c.g,c.b,c.a);}
static void outline(int x,int y,int w,int h,int radius,SDL_Color c){roundedRectangleRGBA(renderer,x,y,x+w,y+h,radius,c.r,c.g,c.b,c.a);}
static void text_font(TTF_Font *font,int x,int y,const char *value,SDL_Color c){
    if(!value||!*value)return;
    SDL_Surface *s=font?TTF_RenderUTF8_Blended(font,value,c):NULL;
    if(s){SDL_Texture *t=SDL_CreateTextureFromSurface(renderer,s);SDL_Rect d={x,y,s->w,s->h};if(t)SDL_RenderCopy(renderer,t,NULL,&d);if(t)SDL_DestroyTexture(t);SDL_FreeSurface(s);return;}
    char fallback[192];size_t out=0;
    for(size_t i=0;value[i]&&out+1<sizeof(fallback);i++){
        unsigned char ch=(unsigned char)value[i];
        if(ch<0x80)fallback[out++]=(char)ch;
        else {fallback[out++]='?';while(((unsigned char)value[i+1]&0xC0)==0x80)i++;}
    }
    fallback[out]=0;stringRGBA(renderer,x,y,fallback,c.r,c.g,c.b,c.a);
}
static void text(int x,int y,const char *v,SDL_Color c){text_font(font_regular,x,y,v,c);}
static void medium(int x,int y,const char *v,SDL_Color c){text_font(font_medium,x,y,v,c);}
static void semibold(int x,int y,const char *v,SDL_Color c){text_font(font_semibold,x,y,v,c);}
static void bold(int x,int y,const char *v,SDL_Color c){text_font(font_bold,x,y,v,c);}
static void set_notice(const char *message){snprintf(notice,sizeof(notice),"%s",message);notice_frames=300;}

static void draw_header(void){
    box(48,34,1184,70,16,(SDL_Color){22,22,34,245});outline(48,34,1184,70,16,(SDL_Color){99,75,153,210});
    box(67,49,38,38,11,(SDL_Color){116,88,194,255});bold(79,58,"N",(SDL_Color){255,255,255,255});
    bold(120,52,"NOVA MANAGER",(SDL_Color){246,243,255,255});text(120,76,"Administración segura de NOVA HOME",(SDL_Color){165,158,187,255});
    box(970,53,238,32,16,(SDL_Color){72,51,112,255});semibold(992,61,fonts_ready?"PROTOTIPO · SIN CAMBIOS EN SD":"FUENTE INTEGRADA",(SDL_Color){223,210,255,255});
}
static void draw_nav(void){
    const char *labels[]={"Resumen","Archivos","Recuperación"};
    for(int i=0;i<3;i++){int x=50+i*188;SDL_Color bg=page==i?(SDL_Color){79,58,126,255}:(SDL_Color){25,25,36,230};box(x,125,174,46,12,bg);outline(x,125,174,46,12,page==i?(SDL_Color){158,123,225,255}:(SDL_Color){76,65,96,210});semibold(x+20,139,labels[i],(SDL_Color){239,235,248,255});}
}
static void draw_status_card(void){
    box(50,193,354,220,18,(SDL_Color){20,21,32,245});outline(50,193,354,220,18,(SDL_Color){94,71,146,230});
    text(76,217,"ESTADO SIMULADO",(SDL_Color){177,164,213,255});
    SDL_Color state=simulated_active?(SDL_Color){115,217,163,255}:(SDL_Color){191,187,206,255};
    semibold(76,253,simulated_active?"NOVA HOME · ACTIVA":"HOME ORIGINAL · ACTIVO",state);
    text(76,290,simulated_restart_pending?"Hay un cambio esperando reinicio simulado.":"No hay cambios pendientes.",(SDL_Color){198,195,211,255});
    box(76,337,302,49,12,(SDL_Color){255,255,255,13});
    medium(94,353,simulated_restart_pending?"Reinicio requerido":"Integración real: aún no instalada",(SDL_Color){224,216,242,255});
}
static void draw_action_card(void){
    box(426,193,804,220,18,(SDL_Color){20,21,32,245});outline(426,193,804,220,18,(SDL_Color){94,71,146,230});
    semibold(454,217,"ACCIONES DE DEMOSTRACIÓN",(SDL_Color){223,214,246,255});
    const char *labels[]={"Comprobar archivos","Activar NOVA","Desactivar NOVA","Aplicar reinicio simulado"};
    for(int i=0;i<4;i++){int x=454+(i%2)*365,y=259+(i/2)*62;bool selected=cursor==i;box(x,y,340,48,11,selected?(SDL_Color){78,58,121,255}:(SDL_Color){37,37,51,245});outline(x,y,340,48,11,selected?(SDL_Color){167,131,233,255}:(SDL_Color){82,68,108,210});semibold(x+17,y+15,labels[i],(SDL_Color){242,239,249,255});}
}
static void draw_explanation(void){
    box(50,435,1180,206,18,(SDL_Color){20,21,32,245});outline(50,435,1180,206,18,(SDL_Color){76,61,112,220});
    semibold(76,458,"SEPARACIÓN DE COMPONENTES",(SDL_Color){223,214,246,255});
    text(76,495,"NOVA Interface",(SDL_Color){205,184,255,255});text(302,495,"Interfaz principal NRO, actualmente independiente.",(SDL_Color){201,198,213,255});
    text(76,530,"NOVA Integration",(SDL_Color){205,184,255,255});text(302,530,"Paquete futuro de integración con HOME; aún sin implementación.",(SDL_Color){201,198,213,255});
    text(76,565,"NOVA Manager",(SDL_Color){205,184,255,255});text(302,565,"Este NRO administra; en esta compilación solo simula acciones.",(SDL_Color){201,198,213,255});
    box(76,598,1126,26,9,(SDL_Color){94,53,62,150});medium(89,604,"No se escribe en atmosphere/contents, no se reemplaza hbmenu y no se reinicia la consola.",(SDL_Color){255,210,213,255});
}
static void draw_files(void){
    box(50,193,1180,448,18,(SDL_Color){20,21,32,245});outline(50,193,1180,448,18,(SDL_Color){94,71,146,230});
    semibold(78,220,"VALIDACIÓN DE ARCHIVOS · ESTADO DE DISEÑO",(SDL_Color){223,214,246,255});
    const char *items[]={"NOVA Manager (aplicación independiente)","NOVA Interface (aplicación NRO)","Paquete de integración firmado/versionado", "Respaldo verificable de cualquier archivo previo", "Compatibilidad con versión de Atmosphère y firmware"};
    for(int i=0;i<5;i++){int y=270+i*59;box(78,y,1124,45,11,(SDL_Color){255,255,255,10});medium(98,y+14,items[i],(SDL_Color){230,227,238,255});semibold(913,y+14,"PENDIENTE DE DISEÑO REAL",(SDL_Color){222,190,136,255});}
    text(80,584,"No se inspecciona la SD desde este prototipo. La comprobación real requiere definir formatos, hashes y reglas de compatibilidad.",(SDL_Color){181,176,196,255});
}
static void draw_recovery(void){
    box(50,193,1180,448,18,(SDL_Color){20,21,32,245});outline(50,193,1180,448,18,(SDL_Color){94,71,146,230});
    semibold(78,220,"RECUPERACIÓN · PLAN PRELIMINAR",(SDL_Color){223,214,246,255});
    const char *steps[]={"Mantener intacto el HOME de Nintendo y no reemplazar archivos del sistema.","Conservar una copia exacta de los archivos que una futura integración administre.","Validar la copia y la integración antes de preparar cualquier activación.","Si falta una condición, cancelar sin modificar nada.","Ofrecer una ruta manual de recuperación desde PC si HOME no inicia."};
    for(int i=0;i<5;i++){int y=270+i*54;box(78,y,1124,41,10,(SDL_Color){255,255,255,10});bold(97,y+11,"•",(SDL_Color){189,156,245,255});medium(121,y+11,steps[i],(SDL_Color){230,227,238,255});}
    box(78,560,1124,48,12,(SDL_Color){94,53,62,150});semibold(96,576,"Este plan no es todavía una guía de instalación: falta validar el mecanismo real con documentación y pruebas.",(SDL_Color){255,210,213,255});
}
static void draw_confirm(void){
    if(pending==ACTION_NONE)return;
    box(0,0,W,H,0,(SDL_Color){0,0,0,170});
    box(300,246,680,230,18,(SDL_Color){25,24,37,255});outline(300,246,680,230,18,(SDL_Color){155,117,216,255});
    bold(344,273,"CONFIRMACIÓN DE SIMULACIÓN",(SDL_Color){245,239,255,255});
    const char *msg=pending==ACTION_ENABLE?"Simular activación de NOVA HOME?":pending==ACTION_DISABLE?"Simular restauración del HOME original?":"Aplicar el cambio solo en esta demostración?";
    medium(344,323,msg,(SDL_Color){225,220,236,255});text(344,355,"No se escribirá en la SD ni se reiniciará el sistema.",(SDL_Color){192,185,207,255});
    box(344,399,250,48,11,(SDL_Color){90,65,143,255});semibold(427,414,"A · CONFIRMAR",(SDL_Color){255,255,255,255});
    box(624,399,300,48,11,(SDL_Color){255,255,255,18});semibold(705,414,"B · CANCELAR",(SDL_Color){239,235,247,255});
}
static void draw(void){
    SDL_SetRenderDrawColor(renderer,12,12,19,255);SDL_RenderClear(renderer);draw_header();draw_nav();
    if(page==PAGE_OVERVIEW){draw_status_card();draw_action_card();draw_explanation();}
    else if(page==PAGE_FILES)draw_files();else draw_recovery();
    if(notice_frames>0){box(50,657,1180,40,11,(SDL_Color){44,39,61,245});text(68,670,notice,(SDL_Color){224,215,246,255});notice_frames--;}
    bold(52,704,"A  Seleccionar      B  Volver/cancelar      +  Salir",(SDL_Color){171,163,195,255});draw_confirm();SDL_RenderPresent(renderer);
}
static void confirm_action(void){
    if(pending==ACTION_ENABLE){simulated_target_active=true;simulated_restart_pending=true;set_notice("Simulación: activación preparada; reinicio pendiente.");}
    else if(pending==ACTION_DISABLE){simulated_target_active=false;simulated_restart_pending=true;set_notice("Simulación: restauración preparada; reinicio pendiente.");}
    else if(pending==ACTION_RESTART){if(simulated_restart_pending)simulated_active=simulated_target_active;simulated_restart_pending=false;set_notice("Simulación aplicada en memoria. No hubo reinicio real.");}
    pending=ACTION_NONE;
}
int main(void){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0)return 1;
    if(TTF_Init()<0){SDL_Quit();return 1;}
    SDL_Window *window=SDL_CreateWindow("NOVA Manager",0,0,W,H,SDL_WINDOW_SHOWN);if(!window){TTF_Quit();SDL_Quit();return 1;}
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer){SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return 1;}
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);SDL_RenderSetLogicalSize(renderer,W,H);
    font_regular=TTF_OpenFont("romfs:/fonts/Manrope-Regular.ttf",14);font_medium=TTF_OpenFont("romfs:/fonts/Manrope-Medium.ttf",14);font_semibold=TTF_OpenFont("romfs:/fonts/Manrope-SemiBold.ttf",15);font_bold=TTF_OpenFont("romfs:/fonts/Manrope-Bold.ttf",18);
    fonts_ready=font_regular&&font_medium&&font_semibold&&font_bold;
    if(!fonts_ready)set_notice("No se pudieron abrir todas las fuentes Manrope; se usa fuente integrada.");
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);hidInitializeTouchScreen();
    while(appletMainLoop()){
        padUpdate(&pad);u64 down=padGetButtonsDown(&pad);
        if(down&HidNpadButton_Plus)break;
        if(pending!=ACTION_NONE){if(down&HidNpadButton_A)confirm_action();else if(down&HidNpadButton_B)pending=ACTION_NONE;}
        else if(down&HidNpadButton_B){if(page!=PAGE_OVERVIEW)page=PAGE_OVERVIEW;else set_notice("Manager en modo simulación.");}
        else if(down&HidNpadButton_Left){if(page>0)page--;cursor=0;}
        else if(down&HidNpadButton_Right){if(page<PAGE_RECOVERY)page++;cursor=0;}
        else if(page==PAGE_OVERVIEW&&(down&HidNpadButton_Up)&&cursor>0)cursor--;
        else if(page==PAGE_OVERVIEW&&(down&HidNpadButton_Down)&&cursor<3)cursor++;
        else if(down&HidNpadButton_A&&page==PAGE_OVERVIEW){if(cursor==0)set_notice("Simulación: no se accedió a la SD ni se validaron archivos reales.");else if(cursor==1)pending=ACTION_ENABLE;else if(cursor==2)pending=ACTION_DISABLE;else pending=ACTION_RESTART;}
        draw();
    }
    if(font_regular)TTF_CloseFont(font_regular);
    if(font_medium)TTF_CloseFont(font_medium);
    if(font_semibold)TTF_CloseFont(font_semibold);
    if(font_bold)TTF_CloseFont(font_bold);
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return 0;
}
