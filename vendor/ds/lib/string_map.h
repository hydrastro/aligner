#ifndef DS_STRING_MAP_H
#define DS_STRING_MAP_H

#include "common.h"

#include "hash_table.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_string_map {
  ds_hash_table_t *table;
} ds_string_map_t;

typedef struct ds_string_set {
  ds_string_map_t *map;
} ds_string_set_t;

ds_string_map_t *FUNC(ds_string_map_create)(void);
ds_string_map_t *FUNC(ds_string_map_create_ctx)(ds_context_t *context);
ds_status_t FUNC(ds_string_map_put)(ds_string_map_t *map, const char *key,
                                     void *value);
ds_status_t FUNC(ds_string_map_get)(ds_string_map_t *map, const char *key,
                                     void **out_value);
ds_status_t FUNC(ds_string_map_remove)(ds_string_map_t *map,
                                        const char *key);
ds_status_t FUNC(ds_string_map_contains)(ds_string_map_t *map,
                                          const char *key);
void FUNC(ds_string_map_destroy)(ds_string_map_t *map);

ds_string_set_t *FUNC(ds_string_set_create)(void);
ds_string_set_t *FUNC(ds_string_set_create_ctx)(ds_context_t *context);
ds_status_t FUNC(ds_string_set_add)(ds_string_set_t *set, const char *key);
ds_status_t FUNC(ds_string_set_contains)(ds_string_set_t *set,
                                          const char *key);
ds_status_t FUNC(ds_string_set_remove)(ds_string_set_t *set,
                                        const char *key);
void FUNC(ds_string_set_destroy)(ds_string_set_t *set);

#ifdef __cplusplus
}
#endif

#endif /* DS_STRING_MAP_H */
