#include "nova_sgdb_task.h"
#include "nova_sgdb.h"
#include "nova_art_download.h"
#include "nova_art_store.h"
#include <stdio.h>
#include <string.h>

static void task_main(void *arg) {
    NovaSgdbTask *t = arg;
    t->ok = false; t->count = 0; t->error[0] = 0;
    if (t->type == NOVA_SGDB_TASK_SEARCH) {
        t->count = nova_sgdb_search(t->key, t->query, t->entries, 12, t->error, sizeof(t->error));
        t->ok = t->count >= 0;
    } else if (t->type == NOVA_SGDB_TASK_ART) {
        t->count = nova_sgdb_art(t->key, t->kind, t->game_id, t->entries, 12, t->error, sizeof(t->error));
        t->ok = t->count >= 0;
        if (t->ok && t->count) {
            nova_art_store_init();
            for (int i = 0; i < t->count; i++) {
                char path[256], download_error[192] = "";
                if (!nova_art_store_thumb_path(t->title_id, t->kind, t->entries[i].id, path, sizeof(path))) continue;
                if (!nova_art_store_has(path) && t->entries[i].thumb[0])
                    (void)nova_art_download(t->entries[i].thumb, path, 1024u * 1024u, download_error, sizeof(download_error));
                if (nova_art_store_has(path)) snprintf(t->entries[i].thumb, sizeof(t->entries[i].thumb), "%s", path);
            }
        }
    } else if (t->type == NOVA_SGDB_TASK_DOWNLOAD) {
        t->ok = nova_art_store_init() && nova_art_store_path(t->title_id, t->kind, t->destination, sizeof(t->destination)) &&
            nova_art_download(t->query, t->destination, 28u * 1024u * 1024u, t->error, sizeof(t->error));
    } else if (t->type == NOVA_SGDB_TASK_VALIDATE) {
        t->count = nova_sgdb_search(t->key, "Mario", t->entries, 1, t->error, sizeof(t->error));
        t->ok = t->count >= 0;
    }
    if (!t->ok && !t->error[0]) snprintf(t->error, sizeof(t->error), "No se pudo completar la solicitud de SteamGridDB.");
    t->running = false; t->done = true;
}

bool nova_sgdb_task_start(NovaSgdbTask *t, NovaSgdbTaskType type, const char *key, const char *query, unsigned long long title_id, NovaSgdbKind kind, unsigned game_id, const char *destination) {
    if (!t || t->running) return false;
    if (t->done) nova_sgdb_task_close(t);
    memset(t, 0, sizeof(*t)); t->type = type; t->title_id = title_id; t->kind = kind; t->game_id = game_id;
    if (key) snprintf(t->key, sizeof(t->key), "%s", key);
    if (query) snprintf(t->query, sizeof(t->query), "%s", query);
    if (destination) snprintf(t->destination, sizeof(t->destination), "%s", destination);
    t->running = true;
    Result rc = threadCreate(&t->thread, task_main, t, NULL, 0x8000, 0x2B, -2);
    if (R_FAILED(rc)) { t->running = false; snprintf(t->error, sizeof(t->error), "No se pudo iniciar la tarea de red."); return false; }
    rc = threadStart(&t->thread);
    if (R_FAILED(rc)) { threadClose(&t->thread); t->running = false; snprintf(t->error, sizeof(t->error), "No se pudo iniciar la tarea de red."); return false; }
    return true;
}

bool nova_sgdb_task_finish(NovaSgdbTask *t) {
    if (!t || !t->done) return false;
    threadWaitForExit(&t->thread); threadClose(&t->thread); t->done = false;
    return true;
}
void nova_sgdb_task_close(NovaSgdbTask *t) { if (t && t->done) nova_sgdb_task_finish(t); }
