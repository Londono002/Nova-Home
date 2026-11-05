#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    NOVA_SGDB_COVER = 0,
    NOVA_SGDB_ICON,
    NOVA_SGDB_BACKGROUND,
    NOVA_SGDB_LOGO,
    NOVA_SGDB_KIND_COUNT
} NovaSgdbKind;
typedef struct { unsigned id; char name[128]; char url[768]; char thumb[768]; char style[64]; } NovaSgdbEntry;

int nova_sgdb_search(const char *key, const char *query, NovaSgdbEntry *out, int capacity, char *error, size_t error_size);
int nova_sgdb_art(const char *key, NovaSgdbKind kind, unsigned game_id, NovaSgdbEntry *out, int capacity, char *error, size_t error_size);
