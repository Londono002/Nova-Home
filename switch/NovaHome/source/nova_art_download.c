#include "nova_art_download.h"
#include "nova_sgdb_http.h"
#include <switch.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static void set_error(char *dst,size_t cap,const char *text){if(dst&&cap)snprintf(dst,cap,"%s",text);}
static bool parse_url(const char *url,char *host,size_t host_size,const char **path){
    if(strncmp(url,"https://",8))return false;
    const char *start=url+8,*slash=strchr(start,'/');
    if(!slash||slash==start||(size_t)(slash-start)>=host_size)return false;
    memcpy(host,start,(size_t)(slash-start));host[slash-start]=0;for(char *p=host;*p;p++)*p=(char)tolower((unsigned char)*p);
    size_t n=strlen(host);bool allowed=!strcmp(host,"steamgriddb.com")||!strcmp(host,"s3.amazonaws.com")||(n>16&&!strcmp(host+n-16,".steamgriddb.com"));if(!allowed||strchr(host,'@')||strchr(host,':'))return false;
    for(const char *p=slash;*p;p++)if((unsigned char)*p<0x21||*p=='\\')return false;
    *path=slash;return true;
}
static bool valid_image(const unsigned char *p,size_t n){return (n>=8&&!memcmp(p,"\x89PNG\r\n\x1a\n",8))||(n>=3&&p[0]==0xff&&p[1]==0xd8&&p[2]==0xff)||(n>=12&&!memcmp(p,"RIFF",4)&&!memcmp(p+8,"WEBP",4))||(n>=6&&(!memcmp(p,"GIF87a",6)||!memcmp(p,"GIF89a",6)));}
static bool fetch_image_response(const char *host,const char *path,NovaHttpResponse *response,char *error,size_t error_size){
    for(int attempt=0;attempt<3;attempt++){
        bool received=nova_sgdb_http_get(host,path,NULL,response,error,error_size);
        bool retry=!received||response->status==429||response->status==408||response->status>=500;
        if(!retry)return true;
        nova_sgdb_http_free(response);
        if(attempt<2)svcSleepThread((u64)(250+attempt*500)*1000000ULL);
    }
    return false;
}

bool nova_art_download(const char *url,const char *destination,size_t max_bytes,char *error,size_t error_size){
    char current[1200];snprintf(current,sizeof(current),"%s",url);
    NovaHttpResponse response={0};char host[256];const char *path=NULL;bool loaded=false;
    for(int redirects=0;redirects<=3;redirects++){
        if(!parse_url(current,host,sizeof(host),&path)){set_error(error,error_size,"SteamGridDB devolvió una URL de imagen no permitida.");return false;}
        if(!fetch_image_response(host,path,&response,error,error_size))return false;
        if(response.status==301||response.status==302||response.status==303||response.status==307||response.status==308){
            if(!response.location[0]||redirects==3){nova_sgdb_http_free(&response);set_error(error,error_size,"La imagen devolvió una redirección no válida.");return false;}
            char next[1200];if(!strncmp(response.location,"https://",8))snprintf(next,sizeof(next),"%s",response.location);else if(response.location[0]=='/')snprintf(next,sizeof(next),"https://%s%s",host,response.location);else{nova_sgdb_http_free(&response);set_error(error,error_size,"La redirección de la imagen no es segura.");return false;}
            nova_sgdb_http_free(&response);snprintf(current,sizeof(current),"%s",next);continue;
        }
        loaded=true;break;
    }
    if(!loaded)return false;
    if(response.status!=200){char message[96];snprintf(message,sizeof(message),"SteamGridDB devolvió HTTP %d al descargar la imagen.",response.status);nova_sgdb_http_free(&response);set_error(error,error_size,message);return false;}
    if(!response.data||response.size<16||response.size>max_bytes||!valid_image(response.data,response.size)){nova_sgdb_http_free(&response);set_error(error,error_size,"El archivo recibido no es una imagen válida o excede el límite.");return false;}
    char temp[512];if(snprintf(temp,sizeof(temp),"%s.part",destination)>=(int)sizeof(temp)){nova_sgdb_http_free(&response);set_error(error,error_size,"Ruta de almacenamiento demasiado larga.");return false;}
    FILE *file=fopen(temp,"wb");if(!file){nova_sgdb_http_free(&response);set_error(error,error_size,"No se pudo abrir el almacenamiento de la SD.");return false;}
    bool written=fwrite(response.data,1,response.size,file)==response.size;int closed=fclose(file);nova_sgdb_http_free(&response);
    if(!written||closed!=0){remove(temp);set_error(error,error_size,"La escritura de la imagen en la SD falló.");return false;}
    char backup[560];if(snprintf(backup,sizeof(backup),"%s.bak",destination)>=(int)sizeof(backup)){remove(temp);set_error(error,error_size,"Ruta de respaldo demasiado larga.");return false;}
    bool had_destination=access(destination,F_OK)==0;
    if(had_destination){remove(backup);if(rename(destination,backup)!=0){remove(temp);set_error(error,error_size,"No se pudo reemplazar la imagen existente.");return false;}}
    if(rename(temp,destination)!=0){if(had_destination)(void)rename(backup,destination);remove(temp);set_error(error,error_size,"No se pudo guardar la imagen descargada.");return false;}
    if(had_destination)remove(backup);
    return true;
}
