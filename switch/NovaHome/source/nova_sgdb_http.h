#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct { unsigned char *data; size_t size; int status; char location[1024]; } NovaHttpResponse;
bool nova_sgdb_http_get(const char *host, const char *path, const char *api_key,
                        NovaHttpResponse *response, char *error, size_t error_size);
void nova_sgdb_http_free(NovaHttpResponse *response);
