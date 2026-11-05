#pragma once
#include "nova_sgdb.h"
#include <switch.h>

typedef enum { NOVA_SGDB_TASK_IDLE, NOVA_SGDB_TASK_SEARCH, NOVA_SGDB_TASK_ART, NOVA_SGDB_TASK_DOWNLOAD, NOVA_SGDB_TASK_VALIDATE } NovaSgdbTaskType;
typedef struct {
    Thread thread;
    volatile bool running, done;
    NovaSgdbTaskType type;
    char key[160], query[512], destination[256], error[192];
    unsigned long long title_id;
    NovaSgdbKind kind;
    unsigned game_id;
    NovaSgdbEntry entries[12];
    int count;
    bool ok;
} NovaSgdbTask;
bool nova_sgdb_task_start(NovaSgdbTask *task, NovaSgdbTaskType type, const char *key, const char *query, unsigned long long title_id, NovaSgdbKind kind, unsigned game_id, const char *destination);
bool nova_sgdb_task_finish(NovaSgdbTask *task);
void nova_sgdb_task_close(NovaSgdbTask *task);
