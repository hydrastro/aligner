#ifndef DS_HASH_TABLE_H
#define DS_HASH_TABLE_H

#include "common.h"
#include "context.h"
#include "iter.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HASH_TABLE_RESIZE_FACTOR
#define HASH_TABLE_RESIZE_FACTOR 0.75
#endif

typedef enum {
  HASH_CHAINING,
  HASH_LINEAR_PROBING,
  HASH_QUADRATIC_PROBING,
  HASH_CUSTOM_PROBING
} ds_hash_table_mode_t;

struct hash_node;
struct hash_table;
struct hash_bucket_store;
struct ds_hash_table_config;

typedef struct hash_node ds_hash_node_t;
typedef struct hash_table ds_hash_table_t;
typedef struct hash_bucket_store ds_hash_bucket_store_t;
typedef struct ds_hash_table_config ds_hash_table_config_t;

typedef size_t (*ds_hash_probing_func_t)(size_t base_index, size_t iteration,
                                         size_t capacity);
typedef void *(*ds_hash_bucket_create_func_t)(ds_hash_table_t *table);
typedef ds_hash_node_t *(*ds_hash_bucket_search_func_t)(
    ds_hash_table_t *table, void *bucket, void *key,
    int (*compare)(void *, void *));
typedef void (*ds_hash_bucket_insert_func_t)(
    ds_hash_table_t *table, void **bucket, ds_hash_node_t *node,
    int (*compare)(void *, void *));
typedef ds_hash_node_t *(*ds_hash_bucket_remove_func_t)(
    ds_hash_table_t *table, void **bucket, void *key,
    int (*compare)(void *, void *));
typedef void (*ds_hash_bucket_destroy_func_t)(
    ds_hash_table_t *table, void *bucket, void (*destroy)(ds_hash_node_t *));
typedef void (*ds_hash_bucket_apply_func_t)(
    ds_hash_table_t *table, void *bucket,
    void (*callback)(ds_hash_node_t *, void *), void *args);

typedef size_t (*ds_hash_func_t)(void *key, void *user);
typedef int (*ds_hash_compare_func_t)(void *a, void *b, void *user);
typedef void *(*ds_hash_clone_func_t)(void *ptr, void *user);
typedef void (*ds_hash_destroy_func_t)(void *ptr, void *user);

struct hash_node {
  void *key;
  void *value;
  struct hash_node *next;
  struct hash_node *list_next;
  struct hash_node *list_prev;
};

struct ds_hash_table_config {
  size_t capacity;
  ds_hash_table_mode_t mode;
  ds_hash_probing_func_t probing_func;
  const ds_hash_bucket_store_t *bucket_store;
  ds_hash_func_t hash;
  ds_hash_compare_func_t compare;
  ds_hash_clone_func_t clone_key;
  ds_hash_clone_func_t clone_value;
  ds_hash_destroy_func_t destroy_key;
  ds_hash_destroy_func_t destroy_value;
  void *user;
  ds_context_t *context;
};

struct hash_bucket_store {
  ds_hash_bucket_create_func_t create;
  ds_hash_bucket_search_func_t search;
  ds_hash_bucket_insert_func_t insert;
  ds_hash_bucket_remove_func_t remove;
  ds_hash_bucket_destroy_func_t destroy;
  ds_hash_bucket_apply_func_t apply;
};

struct hash_table {
  union {
    void **buckets;
    ds_hash_node_t *entries;
  } store;
  size_t size;
  size_t capacity;
  void *nil;
  void *tombstone;
  ds_hash_table_mode_t mode;
  ds_hash_probing_func_t probing_func;
  const ds_hash_bucket_store_t *bucket_store;
  ds_hash_node_t *last_node;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
  ds_context_t *context;
  ds_hash_func_t config_hash;
  ds_hash_compare_func_t config_compare;
  ds_hash_clone_func_t config_clone_key;
  ds_hash_clone_func_t config_clone_value;
  ds_hash_destroy_func_t config_destroy_key;
  ds_hash_destroy_func_t config_destroy_value;
  void *config_user;
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
};


void ds_hash_table_config_init(ds_hash_table_config_t *config);
ds_hash_table_t *FUNC(ds_hash_table_create_config)(
    const ds_hash_table_config_t *config);
ds_status_t FUNC(ds_hash_table_insert)(ds_hash_table_t *table, void *key,
                                        void *value);
ds_status_t FUNC(ds_hash_table_insert_take_key)(ds_hash_table_t *table,
                                                 void *key, void *value);
ds_status_t FUNC(ds_hash_table_get)(ds_hash_table_t *table, void *key,
                                     void **out_value);
ds_status_t FUNC(ds_hash_table_remove)(ds_hash_table_t *table,
                                        void *key);
ds_status_t FUNC(ds_hash_table_contains)(ds_hash_table_t *table,
                                          void *key);
void FUNC(ds_hash_table_destroy)(ds_hash_table_t *table);
ds_hash_table_t *FUNC(ds_hash_table_clone)(ds_hash_table_t *table);
ds_status_t FUNC(ds_hash_table_iter_init)(ds_hash_table_t *table,
                                           ds_iter_t *iter);

ds_hash_node_t *hash_node_create(ds_hash_table_t *table, void *key,
                                 void *value);

size_t hash_func_string_djb2(void *key);
size_t hash_func_int(void *key);
size_t hash_func_size_t(void *key);
size_t hash_func_pointer(void *key);
size_t hash_func_double(void *key);
size_t hash_func_default(void *key);

size_t next_prime_capacity(size_t current_capacity);

size_t quadratic_probing(size_t base_index, size_t iteration, size_t capacity);
size_t linear_probing(size_t base_index, size_t iteration, size_t capacity);

const ds_hash_bucket_store_t *FUNC(hash_table_linked_list_bucket_store)(void);

ds_hash_table_t *FUNC(hash_table_create)(size_t capacity,
                                         ds_hash_table_mode_t mode,
                                         ds_hash_probing_func_t probing_func);
ds_hash_table_t *FUNC(hash_table_create_chain)(
    size_t capacity, const ds_hash_bucket_store_t *bucket_store);
ds_hash_table_t *FUNC(hash_table_create_alloc)(
    size_t capacity, ds_hash_table_mode_t mode,
    ds_hash_probing_func_t probing_func, void *(*allocator)(size_t),
    void (*deallocator)(void *));
ds_hash_table_t *FUNC(hash_table_create_chain_alloc)(
    size_t capacity, const ds_hash_bucket_store_t *bucket_store,
    void *(*allocator)(size_t), void (*deallocator)(void *));
void FUNC(hash_table_insert)(ds_hash_table_t *table, void *key, void *value,
                             size_t (*hash_func)(void *),
                             int (*compare)(void *, void *));
void FUNC(hash_table_resize)(ds_hash_table_t *table, size_t new_capacity,
                             size_t (*hash_func)(void *),
                             int (*compare)(void *, void *));
void *FUNC(hash_table_lookup)(ds_hash_table_t *table, void *key,
                              size_t (*hash_func)(void *),
                              int (*compare)(void *, void *));
void FUNC(hash_table_remove)(ds_hash_table_t *table, void *key,
                             size_t (*hash_func)(void *),
                             int (*compare)(void *, void *),
                             void (*destroy)(ds_hash_node_t *));
void FUNC(hash_table_for_each)(ds_hash_table_t *table,
                               void (*callback)(ds_hash_node_t *, void *),
                               void *args);
bool FUNC(hash_table_is_empty)(ds_hash_table_t *table);
void FUNC(hash_table_destroy)(ds_hash_table_t *table,
                              void (*destroy)(ds_hash_node_t *));
ds_hash_table_t *FUNC(hash_table_clone)(ds_hash_table_t *table,
                                        void *(*clone_key)(void *),
                                        void *(*clone_value)(void *));
ds_hash_table_t *FUNC(hash_table_clone_with)(
    ds_hash_table_t *table, void *(*clone_key)(void *),
    void *(*clone_value)(void *), size_t (*hash_func)(void *),
    int (*compare)(void *, void *));


#ifdef __cplusplus
}
#endif

#endif /* DS_HASH_TABLE_H */
