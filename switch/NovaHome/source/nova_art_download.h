#pragma once
#include <stdbool.h>
#include <stddef.h>
bool nova_art_download(const char *url, const char *destination, size_t max_bytes,
                       char *error, size_t error_size);
