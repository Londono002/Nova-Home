#include "nova_art_store.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *kind_name[NOVA_SGDB_KIND_COUNT]={"cover","icon","background","logo"};
bool nova_art_store_init(void){mkdir("sdmc:/switch/NovaHome",0777);if(mkdir("sdmc:/switch/NovaHome/art",0777)!=0&&errno!=EEXIST)return false;if(mkdir("sdmc:/switch/NovaHome/cache",0777)!=0&&errno!=EEXIST)return false;return true;}
bool nova_art_store_path(unsigned long long id,NovaSgdbKind kind,char *path,size_t size){if(kind<0||kind>=NOVA_SGDB_KIND_COUNT)return false;int n=snprintf(path,size,"sdmc:/switch/NovaHome/art/%016llX_%s.img",id,kind_name[kind]);return n>0&&(size_t)n<size;}
bool nova_art_store_thumb_path(unsigned long long id,NovaSgdbKind kind,unsigned image_id,char *path,size_t size){if(kind<0||kind>=NOVA_SGDB_KIND_COUNT)return false;int n=snprintf(path,size,"sdmc:/switch/NovaHome/cache/%016llX_%s_%u.img",id,kind_name[kind],image_id);return n>0&&(size_t)n<size;}
bool nova_art_store_has(const char *path){struct stat info;return stat(path,&info)==0&&S_ISREG(info.st_mode);}
void nova_art_store_restore(unsigned long long id){
    char path[320],prefix[40];for(int k=0;k<NOVA_SGDB_KIND_COUNT;k++)if(nova_art_store_path(id,(NovaSgdbKind)k,path,sizeof(path)))remove(path);
    snprintf(prefix,sizeof(prefix),"%016llX_",id);DIR *dir=opendir("sdmc:/switch/NovaHome/cache");if(!dir)return;struct dirent *entry;while((entry=readdir(dir)))if(!strncmp(entry->d_name,prefix,strlen(prefix))){snprintf(path,sizeof(path),"sdmc:/switch/NovaHome/cache/%s",entry->d_name);remove(path);}closedir(dir);
}
