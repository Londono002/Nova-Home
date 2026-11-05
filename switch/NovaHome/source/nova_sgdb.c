#include "nova_sgdb.h"
#include "nova_sgdb_http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_HOST "www.steamgriddb.com"

static void set_error(char *dst,size_t cap,const char *text){if(dst&&cap)snprintf(dst,cap,"%s",text);}
static const char *find_bytes(const char *data,size_t size,const char *needle){size_t n=strlen(needle);if(n>size)return NULL;for(size_t i=0;i+n<=size;i++)if(!memcmp(data+i,needle,n))return data+i;return NULL;}
static bool json_success(const char *data,size_t size){const char *p=find_bytes(data,size,"\"success\"");if(!p)return false;p+=9;const char *end=data+size;while(p<end&&isspace((unsigned char)*p))p++;if(p==end||*p++!=':')return false;while(p<end&&isspace((unsigned char)*p))p++;return (size_t)(end-p)>=4&&!memcmp(p,"true",4);}
static char *encode_query(const char *input){size_t n=strlen(input);char *out=malloc(n*3+1);if(!out)return NULL;char *p=out;static const char h[]="0123456789ABCDEF";for(const unsigned char *s=(const unsigned char*)input;*s;s++){if(isalnum(*s)||*s=='-'||*s=='_'||*s=='.'||*s=='~')*p++=(char)*s;else{*p++='%';*p++=h[*s>>4];*p++=h[*s&15];}}*p=0;return out;}
static bool json_value_string(const char *obj,size_t len,const char *field,char *out,size_t cap){char key[48];snprintf(key,sizeof(key),"\"%s\"",field);const char *p=obj,*end=obj+len;while(p<end){const char *hit=find_bytes(p,(size_t)(end-p),key);if(!hit)return false;p=hit+strlen(key);while(p<end&&isspace((unsigned char)*p))p++;if(p==end||*p!=':')continue;p++;while(p<end&&isspace((unsigned char)*p))p++;if(p==end||*p!='\"')return false;p++;size_t n=0;while(p<end&&*p!='\"'){if(*p=='\\'&&p+1<end)p++;if(n+1<cap)out[n++]=*p;p++;}if(!cap)return false;out[n]=0;return p<end;}return false;}
static unsigned json_value_id(const char *obj,size_t len){const char *hit=find_bytes(obj,len,"\"id\"");if(!hit)return 0;const char *end=obj+len,*p=hit+4;while(p<end&&isspace((unsigned char)*p))p++;if(p==end||*p++!=':')return 0;while(p<end&&isspace((unsigned char)*p))p++;if(p<end&&*p=='\"')p++;return (unsigned)strtoul(p,NULL,10);}
static int parse_data(const unsigned char *json,size_t length,NovaSgdbEntry *out,int capacity,bool games,char *error,size_t error_size){
    const char *data=find_bytes((const char*)json,length,"\"data\"");if(!data){set_error(error,error_size,"La respuesta no contiene el campo data.");return -1;}size_t remaining=length-(size_t)(data-(const char*)json);const char *p=find_bytes(data,remaining,"[");if(!p){set_error(error,error_size,"La lista de resultados está vacía o malformada.");return -1;}p++;
    const char *end=(const char*)json+length;int count=0;
    while(p<end&&count<capacity){while(p<end&&(*p==','||isspace((unsigned char)*p)))p++;if(p==end||*p==']')break;if(*p!='{'){p++;continue;}const char *start=p;int depth=0;bool quoted=false,escape=false;do{char c=*p++;if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='\"')quoted=false;}else if(c=='\"')quoted=true;else if(c=='{')depth++;else if(c=='}')depth--;}while(p<end&&depth>0);if(depth){set_error(error,error_size,"Un elemento JSON está incompleto.");return -1;}size_t n=(size_t)(p-start);NovaSgdbEntry entry={0};entry.id=json_value_id(start,n);
        if(games)json_value_string(start,n,"name",entry.name,sizeof(entry.name));else{json_value_string(start,n,"url",entry.url,sizeof(entry.url));json_value_string(start,n,"thumb",entry.thumb,sizeof(entry.thumb));json_value_string(start,n,"style",entry.style,sizeof(entry.style));if(!entry.style[0])json_value_string(start,n,"name",entry.style,sizeof(entry.style));if(!entry.thumb[0])snprintf(entry.thumb,sizeof(entry.thumb),"%s",entry.url);}
        if((games&&entry.id&&entry.name[0])||(!games&&entry.url[0]))out[count++]=entry;
    }
    return count;
}
static int request_entries(const char *key,const char *path,NovaSgdbEntry *out,int capacity,bool games,char *error,size_t error_size){
    NovaHttpResponse response={0};if(!nova_sgdb_http_get(API_HOST,path,key,&response,error,error_size))return -1;
    if(response.status==401||response.status==403){set_error(error,error_size,"SteamGridDB rechazó la clave API.");nova_sgdb_http_free(&response);return -1;}
    if(response.status==429){set_error(error,error_size,"SteamGridDB limitó las solicitudes. Espera y vuelve a intentarlo.");nova_sgdb_http_free(&response);return -1;}
    if(response.status!=200){set_error(error,error_size,"SteamGridDB respondió con un error HTTP.");nova_sgdb_http_free(&response);return -1;}
    if(!response.data||!response.size){set_error(error,error_size,"SteamGridDB devolvió una respuesta vacía.");nova_sgdb_http_free(&response);return -1;}
    if(!json_success((const char*)response.data,response.size)){set_error(error,error_size,"SteamGridDB indicó que la solicitud no tuvo éxito.");nova_sgdb_http_free(&response);return -1;}
    int count=parse_data(response.data,response.size,out,capacity,games,error,error_size);nova_sgdb_http_free(&response);return count;
}

int nova_sgdb_search(const char *key,const char *query,NovaSgdbEntry *out,int capacity,char *error,size_t error_size){
    if(!key||!*key||!query||!*query||!out||capacity<1){set_error(error,error_size,"Falta la clave API o el nombre del juego.");return -1;}
    for(const char *p=key;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_'){set_error(error,error_size,"La clave API contiene caracteres inválidos.");return -1;}
    char *encoded=encode_query(query);if(!encoded){set_error(error,error_size,"No hay memoria para la búsqueda.");return -1;}size_t n=strlen(encoded)+40;char *path=malloc(n);if(!path){free(encoded);set_error(error,error_size,"No hay memoria para la búsqueda.");return -1;}snprintf(path,n,"/api/v2/search/autocomplete/%s",encoded);free(encoded);int count=request_entries(key,path,out,capacity,true,error,error_size);free(path);return count;
}

int nova_sgdb_art(const char *key,NovaSgdbKind kind,unsigned game_id,NovaSgdbEntry *out,int capacity,char *error,size_t error_size){
    static const char *endpoint[NOVA_SGDB_KIND_COUNT]={"grids","icons","heroes","logos"};if(kind<0||kind>=NOVA_SGDB_KIND_COUNT||!out||capacity<1){set_error(error,error_size,"La categoría de imagen no es válida.");return -1;}
    const char *filters=kind==NOVA_SGDB_COVER?"?dimensions=600x900":kind==NOVA_SGDB_BACKGROUND?"?dimensions=1920x620,1600x650&types=static":"";
    char path[192];snprintf(path,sizeof(path),"/api/v2/%s/game/%u%s",endpoint[kind],game_id,filters);return request_entries(key,path,out,capacity,false,error,error_size);
}
