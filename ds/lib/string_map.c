#include "string_map.h"
#include <stdlib.h>
#include <string.h>

static size_t string_map_hash(void *key, void *user);
static int string_map_compare(void *a, void *b, void *user);
static void string_map_destroy_key(void *key, void *user);
static char *string_map_strdup(const char *str);
static void *string_map_key_ptr(const char *key);

static size_t string_map_hash(void *key, void *user) {
  (void)user;
  return hash_func_string_djb2(key);
}

static int string_map_compare(void *a, void *b, void *user) {
  (void)user;
  return strcmp((const char *)a, (const char *)b);
}

static void string_map_destroy_key(void *key, void *user) {
  (void)user;
  free(key);
}

static void *string_map_key_ptr(const char *key) {
  union {
    const char *c;
    void *v;
  } u;
  u.c = key;
  return u.v;
}

static char *string_map_strdup(const char *str) {
  size_t len;
  char *copy;
  if (str == NULL) {
    return NULL;
  }
  len = strlen(str) + 1U;
  copy = (char *)malloc(len);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, str, len);
  return copy;
}

ds_string_map_t *FUNC(ds_string_map_create)(void) {
  return FUNC(ds_string_map_create_ctx)(NULL);
}

ds_string_map_t *FUNC(ds_string_map_create_ctx)(ds_context_t *context) {
  ds_string_map_t *map;
  ds_hash_table_config_t config;

  map = (ds_string_map_t *)malloc(sizeof(*map));
  if (map == NULL) {
    return NULL;
  }
  ds_hash_table_config_init(&config);
  config.capacity = 97U;
  config.mode = HASH_CHAINING;
  config.hash = string_map_hash;
  config.compare = string_map_compare;
  config.destroy_key = string_map_destroy_key;
  config.context = context;
  map->table = FUNC(ds_hash_table_create_config)(&config);
  if (map->table == NULL) {
    free(map);
    return NULL;
  }
  return map;
}

ds_status_t FUNC(ds_string_map_put)(ds_string_map_t *map, const char *key,
                                     void *value) {
  char *copy;
  ds_status_t status;

  if (map == NULL || key == NULL) {
    return DS_ERR_NULL;
  }
  copy = string_map_strdup(key);
  if (copy == NULL) {
    return DS_ERR_ALLOC;
  }
  status = FUNC(ds_hash_table_insert_take_key)(map->table, copy, value);
  if (status != DS_OK) {
    free(copy);
  }
  return status;
}

ds_status_t FUNC(ds_string_map_get)(ds_string_map_t *map, const char *key,
                                     void **out_value) {
  if (map == NULL || key == NULL) {
    return DS_ERR_NULL;
  }
  return FUNC(ds_hash_table_get)(map->table, string_map_key_ptr(key), out_value);
}

ds_status_t FUNC(ds_string_map_remove)(ds_string_map_t *map,
                                        const char *key) {
  if (map == NULL || key == NULL) {
    return DS_ERR_NULL;
  }
  return FUNC(ds_hash_table_remove)(map->table, string_map_key_ptr(key));
}

ds_status_t FUNC(ds_string_map_contains)(ds_string_map_t *map,
                                          const char *key) {
  if (map == NULL || key == NULL) {
    return DS_ERR_NULL;
  }
  return FUNC(ds_hash_table_contains)(map->table, string_map_key_ptr(key));
}

void FUNC(ds_string_map_destroy)(ds_string_map_t *map) {
  if (map == NULL) {
    return;
  }
  FUNC(ds_hash_table_destroy)(map->table);
  free(map);
}

ds_string_set_t *FUNC(ds_string_set_create)(void) {
  return FUNC(ds_string_set_create_ctx)(NULL);
}

ds_string_set_t *FUNC(ds_string_set_create_ctx)(ds_context_t *context) {
  ds_string_set_t *set;
  set = (ds_string_set_t *)malloc(sizeof(*set));
  if (set == NULL) {
    return NULL;
  }
  set->map = FUNC(ds_string_map_create_ctx)(context);
  if (set->map == NULL) {
    free(set);
    return NULL;
  }
  return set;
}

ds_status_t FUNC(ds_string_set_add)(ds_string_set_t *set, const char *key) {
  if (set == NULL) {
    return DS_ERR_NULL;
  }
  return FUNC(ds_string_map_put)(set->map, key, set);
}

ds_status_t FUNC(ds_string_set_contains)(ds_string_set_t *set,
                                          const char *key) {
  if (set == NULL) {
    return DS_ERR_NULL;
  }
  return FUNC(ds_string_map_contains)(set->map, key);
}

ds_status_t FUNC(ds_string_set_remove)(ds_string_set_t *set,
                                        const char *key) {
  if (set == NULL) {
    return DS_ERR_NULL;
  }
  return FUNC(ds_string_map_remove)(set->map, key);
}

void FUNC(ds_string_set_destroy)(ds_string_set_t *set) {
  if (set == NULL) {
    return;
  }
  FUNC(ds_string_map_destroy)(set->map);
  free(set);
}
