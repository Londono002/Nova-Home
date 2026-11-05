#pragma once
#include "nova_sgdb.h"
#include <stdbool.h>
#include <stddef.h>
bool nova_art_store_init(void);
bool nova_art_store_path(unsigned long long title_id, NovaSgdbKind kind, char *path, size_t path_size);
bool nova_art_store_thumb_path(unsigned long long title_id, NovaSgdbKind kind, unsigned image_id, char *path, size_t path_size);
bool nova_art_store_has(const char *path);
void nova_art_store_restore(unsigned long long title_id);
