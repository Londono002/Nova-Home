#include "nova_sgdb_http.h"

#include <switch.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define HTTP_LIMIT (32u * 1024u * 1024u)
typedef struct { unsigned char *data; size_t size, capacity; } ByteBuffer;

static void error_text(char *dst,size_t size,const char *value){if(dst&&size)snprintf(dst,size,"%s",value);}
static bool allowed_host(const char *host){size_t n=strlen(host);return !strcmp(host,"www.steamgriddb.com")||!strcmp(host,"steamgriddb.com")||!strcmp(host,"s3.amazonaws.com")||(n>16&&!strcmp(host+n-16,".steamgriddb.com"));}
static bool append(ByteBuffer *b,const void *data,size_t size){
    if(size>HTTP_LIMIT||b->size>HTTP_LIMIT-size)return false;
    if(b->size+size>b->capacity){size_t cap=b->capacity?b->capacity:4096;while(cap<b->size+size)cap*=2;if(cap>HTTP_LIMIT)cap=HTTP_LIMIT;unsigned char *next=realloc(b->data,cap+1);if(!next)return false;b->data=next;b->capacity=cap;}
    memcpy(b->data+b->size,data,size);b->size+=size;b->data[b->size]=0;return true;
}
static bool chunk_decode(const unsigned char *src,size_t size,ByteBuffer *out){
    size_t pos=0;
    while(pos<size){size_t end=pos;while(end+1<size&&!(src[end]=='\r'&&src[end+1]=='\n'))end++;if(end+1>=size)return false;
        char hex[24];size_t n=end-pos;if(!n||n>=sizeof(hex))return false;memcpy(hex,src+pos,n);hex[n]=0;char *semi=strchr(hex,';');if(semi)*semi=0;size_t count=(size_t)strtoul(hex,NULL,16);pos=end+2;
        if(!count){out->data=realloc(out->data,out->size+1);if(!out->data)return false;out->data[out->size]=0;return true;}
        if(count>size-pos||pos+count+2>size||src[pos+count]!='\r'||src[pos+count+1]!='\n'||!append(out,src+pos,count))return false;
        pos+=count+2;
    }
    return false;
}
static bool parse_response(ByteBuffer *raw,NovaHttpResponse *out,char *error,size_t error_size){
    size_t split=0;bool found=false;for(size_t i=0;i+3<raw->size;i++)if(!memcmp(raw->data+i,"\r\n\r\n",4)){split=i;found=true;break;}
    if(!found||raw->size<12){error_text(error,error_size,"Respuesta HTTP inválida.");return false;}
    char status_line[64]={0};size_t line_len=0;while(line_len<raw->size&&line_len+1<sizeof(status_line)&&raw->data[line_len]!='\r')line_len++;memcpy(status_line,raw->data,line_len);if(sscanf(status_line,"HTTP/%*s %d",&out->status)!=1){error_text(error,error_size,"Estado HTTP inválido.");return false;}
    size_t body_start=split+4;bool chunked=false;size_t content_length=(size_t)-1;
    size_t pos=0;while(pos<split){size_t end=pos;while(end+1<split&&!(raw->data[end]=='\r'&&raw->data[end+1]=='\n'))end++;if(end==pos)break;size_t len=end-pos;
        if(len>9&&!strncasecmp((char*)raw->data+pos,"Location:",9)){size_t start=pos+9;while(start<end&&isspace(raw->data[start]))start++;size_t n=end-start;if(n>=sizeof(out->location))n=sizeof(out->location)-1;memcpy(out->location,raw->data+start,n);out->location[n]=0;}
        else if(len>18&&!strncasecmp((char*)raw->data+pos,"Transfer-Encoding:",18)){for(size_t i=pos+18;i+7<=end;i++)if(!strncasecmp((char*)raw->data+i,"chunked",7)){chunked=true;break;}}
        else if(len>15&&!strncasecmp((char*)raw->data+pos,"Content-Length:",15)){char value[24]={0};size_t start=pos+15;while(start<end&&isspace(raw->data[start]))start++;size_t n=end-start;if(n>=sizeof(value))n=sizeof(value)-1;memcpy(value,raw->data+start,n);content_length=(size_t)strtoull(value,NULL,10);}
        pos=end+2;
    }
    ByteBuffer body={0};size_t available=raw->size-body_start;
    if(chunked){if(!chunk_decode(raw->data+body_start,available,&body)){free(body.data);error_text(error,error_size,"No se pudo procesar una respuesta fragmentada.");return false;}}
    else{if(content_length!=(size_t)-1&&available<content_length){error_text(error,error_size,"La descarga HTTP llegó incompleta.");return false;}size_t n=content_length==(size_t)-1?available:content_length;if(!append(&body,raw->data+body_start,n)){error_text(error,error_size,"La respuesta supera el límite de tamaño.");return false;}}
    out->data=body.data;out->size=body.size;return true;
}

bool nova_sgdb_http_get(const char *host,const char *path,const char *api_key,NovaHttpResponse *response,char *error,size_t error_size){
    memset(response,0,sizeof(*response));
    if(!allowed_host(host)||(api_key&&strcmp(host,"www.steamgriddb.com"))||!path||path[0]!='/'){error_text(error,error_size,"Dominio de SteamGridDB no permitido.");return false;}
    for(const char *p=path;*p;p++)if((unsigned char)*p<0x21||*p=='\\'){error_text(error,error_size,"Ruta HTTP inválida.");return false;}
    if(R_FAILED(socketInitializeDefault())){error_text(error,error_size,"No se pudo iniciar la red de Switch.");return false;}
    bool ssl_ready=false,context_ready=false,connection_ready=false;int fd=-1;SslContext context={0};SslConnection connection={0};
    if(R_FAILED(sslInitialize(1))){error_text(error,error_size,"No se pudo iniciar TLS del sistema.");goto fail;}ssl_ready=true;
    if(R_FAILED(sslCreateContext(&context,SslVersion_Auto))){error_text(error,error_size,"No se pudo crear el contexto TLS.");goto fail;}context_ready=true;
    if(R_FAILED(sslContextCreateConnection(&context,&connection))){error_text(error,error_size,"No se pudo crear HTTPS.");goto fail;}connection_ready=true;
    struct addrinfo hints={0},*addresses=NULL;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(host,"443",&hints,&addresses)!=0){error_text(error,error_size,"No se pudo resolver SteamGridDB.");goto fail;}
    for(struct addrinfo *it=addresses;it;it=it->ai_next){fd=socket(it->ai_family,it->ai_socktype,it->ai_protocol);if(fd<0)continue;struct timeval timeout={8,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));if(connect(fd,it->ai_addr,it->ai_addrlen)==0)break;close(fd);fd=-1;}freeaddrinfo(addresses);
    if(fd<0){error_text(error,error_size,"No se pudo conectar. Comprueba el Wi-Fi de la consola.");goto fail;}
    Result rc=sslConnectionSetOption(&connection,SslOptionType_DoNotCloseSocket,true);
    if(R_SUCCEEDED(rc)){int assigned=socketSslConnectionSetSocketDescriptor(&connection,fd);if(assigned==-1&&errno!=ENOENT){error_text(error,error_size,"No se pudo asociar TLS al socket.");goto fail;}}
    if(R_SUCCEEDED(rc))rc=sslConnectionSetHostName(&connection,host,(u32)strlen(host));
    if(R_SUCCEEDED(rc))rc=sslConnectionSetVerifyOption(&connection,SslVerifyOption_PeerCa|SslVerifyOption_HostName|SslVerifyOption_DateCheck);
    if(R_FAILED(rc)||R_FAILED(sslConnectionDoHandshake(&connection,NULL,NULL,NULL,0))){error_text(error,error_size,"Falló la conexión TLS verificada.");goto fail;}
    char request[2048];int request_size=api_key?snprintf(request,sizeof(request),"GET %s HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\nUser-Agent: NovaHome/0.2\r\n\r\n",path,host,api_key):snprintf(request,sizeof(request),"GET %s HTTP/1.1\r\nHost: %s\r\nAccept: image/*\r\nAccept-Encoding: identity\r\nConnection: close\r\nUser-Agent: NovaHome/0.2\r\n\r\n",path,host);
    if(request_size<=0||request_size>=(int)sizeof(request)){error_text(error,error_size,"La solicitud es demasiado larga.");goto fail;}
    size_t sent=0;while(sent<(size_t)request_size){u32 count=0;if(R_FAILED(sslConnectionWrite(&connection,request+sent,(u32)(request_size-sent),&count))||!count){error_text(error,error_size,"Falló el envío de la solicitud HTTPS.");goto fail;}sent+=count;}
    ByteBuffer raw={0};unsigned char chunk[8192];for(;;){u32 count=0;Result read_rc=sslConnectionRead(&connection,chunk,sizeof(chunk),&count);if(R_FAILED(read_rc)||!count)break;if(!append(&raw,chunk,count)){free(raw.data);error_text(error,error_size,"La respuesta supera 32 MB.");goto fail;}}
    if(!parse_response(&raw,response,error,error_size)){free(raw.data);goto fail;}free(raw.data);
    if(connection_ready)sslConnectionClose(&connection);
    if(fd>=0)close(fd);
    if(context_ready)sslContextClose(&context);
    if(ssl_ready)sslExit();
    socketExit();return true;
fail:
    if(connection_ready)sslConnectionClose(&connection);
    if(fd>=0)close(fd);
    if(context_ready)sslContextClose(&context);
    if(ssl_ready)sslExit();
    socketExit();return false;
}

void nova_sgdb_http_free(NovaHttpResponse *response){if(response){free(response->data);memset(response,0,sizeof(*response));}}
