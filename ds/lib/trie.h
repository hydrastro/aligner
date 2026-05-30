#ifndef DS_TRIE_H
#define DS_TRIE_H

#include "common.h"
#include "context.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct trie_node {
  size_t data_slice;
  struct trie_node *parent;
  void *children;
  bool is_terminal;
  void *terminal_data;
} ds_trie_node_t;

typedef struct trie {
  ds_trie_node_t *root;
  size_t num_splits;
  ds_trie_node_t *(*store_search)(void *, size_t);
  void *(*store_create)(struct trie *, size_t);
  void (*store_insert)(void *, ds_trie_node_t *);
  void (*store_remove)(void *, ds_trie_node_t *);
  void (*store_destroy_entry)(void *, ds_trie_node_t *);
  void (*store_destroy)(void *);
  size_t (*store_get_size)(void *);
  void (*store_apply)(struct trie *, void *,
                      void (*)(struct trie *, ds_trie_node_t *, va_list *),
                      va_list *);
  void *(*store_clone)(struct trie *, void *, ds_trie_node_t *,
                       void *(*clone_data)(void *));
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
  ds_context_t *context;
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} ds_trie_t;

typedef struct ds_trie_store_ops {
  ds_trie_node_t *(*search)(void *store, size_t slice);
  void *(*create)(struct trie *trie, size_t size);
  void (*insert)(void *store, ds_trie_node_t *node);
  void (*remove)(void *store, ds_trie_node_t *node);
  void (*destroy_entry)(void *store, ds_trie_node_t *node);
  void (*destroy)(void *store);
  size_t (*get_size)(void *store);
  void (*apply)(struct trie *trie, void *store,
                void (*f)(struct trie *, ds_trie_node_t *, va_list *),
                va_list *args);
  void *(*clone)(struct trie *trie, void *store,
                 ds_trie_node_t *parent_node, void *(*clone_data)(void *));
} ds_trie_store_ops_t;

typedef struct ds_trie_config {
  size_t num_splits;
  const ds_trie_store_ops_t *store_ops;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
  ds_context_t *context;
} ds_trie_config_t;


const ds_trie_store_ops_t *FUNC(ds_trie_hash_store_ops)(void);
void ds_trie_config_init(ds_trie_config_t *config);
ds_trie_t *FUNC(ds_trie_create_config)(const ds_trie_config_t *config);
ds_trie_t *FUNC(ds_trie_create_hash)(size_t num_splits);
ds_status_t FUNC(ds_trie_insert)(ds_trie_t *trie, void *data,
                                  size_t (*get_slice)(void *, size_t),
                                  bool (*has_slice)(void *, size_t));
ds_status_t FUNC(ds_trie_get)(ds_trie_t *trie, void *data,
                               size_t (*get_slice)(void *, size_t),
                               bool (*has_slice)(void *, size_t),
                               ds_trie_node_t **out_node);
ds_status_t FUNC(ds_trie_remove)(ds_trie_t *trie, void *data,
                                  size_t (*get_slice)(void *, size_t),
                                  bool (*has_slice)(void *, size_t));
void FUNC(ds_trie_destroy)(ds_trie_t *trie);

ds_trie_node_t *FUNC(trie_create_node)(ds_trie_t *trie);

ds_trie_t *FUNC(trie_create_hash)(size_t num_splits);
ds_trie_t *FUNC(trie_create_hash_alloc)(size_t num_splits,
                                        void *(*allocator)(size_t),
                                        void (*deallocator)(void *));
ds_trie_node_t *FUNC(trie_hash_table_store_search)(void *store,
                                                   size_t slice);
void *FUNC(trie_hash_table_store_create)(struct trie *trie, size_t size);
void FUNC(trie_hash_table_store_insert)(void *store, ds_trie_node_t *node);
void FUNC(trie_hash_table_store_remove)(void *store, ds_trie_node_t *node);
void FUNC(trie_hash_table_store_destroy_entry)(void *store,
                                               ds_trie_node_t *node);
void FUNC(trie_hash_table_store_destroy)(void *store);
size_t FUNC(trie_hash_table_store_get_size)(void *store);
void FUNC(trie_hash_table_store_apply)(
    struct trie *trie, void *store,
    void (*f)(struct trie *, ds_trie_node_t *, va_list *), va_list *args);
void *FUNC(trie_hash_table_store_clone)(struct trie *trie, void *store,
                                        ds_trie_node_t *parent_node,
                                        void *(*clone_data)(void *));
ds_trie_t *FUNC(trie_create)(
    size_t num_splits, ds_trie_node_t *(*store_search)(void *, size_t),
    void *(*store_create)(struct trie *, size_t),
    void (*store_insert)(void *, ds_trie_node_t *),
    void (*store_remove)(void *, ds_trie_node_t *),
    void (*store_destroy_entry)(void *, ds_trie_node_t *),
    void (*store_destroy)(void *), size_t (*store_get_size)(void *),
    void (*store_apply)(struct trie *, void *,
                        void (*)(struct trie *, ds_trie_node_t *, va_list *),
                        va_list *),
    void *(*store_clone)(struct trie *, void *, ds_trie_node_t *,
                        void *(*clone_data)(void *)));
ds_trie_t *FUNC(trie_create_alloc)(
    size_t num_splits, ds_trie_node_t *(*store_search)(void *, size_t),
    void *(*store_create)(struct trie *, size_t),
    void (*store_insert)(void *, ds_trie_node_t *),
    void (*store_remove)(void *, ds_trie_node_t *),
    void (*store_destroy_entry)(void *, ds_trie_node_t *),
    void (*store_destroy)(void *), size_t (*store_get_size)(void *),
    void (*store_apply)(struct trie *, void *,
                        void (*)(struct trie *, ds_trie_node_t *, va_list *),
                        va_list *),
    void *(*store_clone)(struct trie *, void *, ds_trie_node_t *,
                        void *(*clone_data)(void *)),
    void *(*allocator)(size_t), void (*deallocator)(void *));
void FUNC(trie_insert)(ds_trie_t *trie, void *data,
                       size_t (*get_slice)(void *, size_t),
                       bool (*has_slice)(void *, size_t));
ds_trie_node_t *FUNC(trie_search)(ds_trie_t *trie, void *data,
                                  size_t (*get_slice)(void *, size_t),
                                  bool (*has_slice)(void *, size_t));
void FUNC(trie_delete_node)(ds_trie_t *trie, ds_trie_node_t *node);
void FUNC(trie_destroy_node)(ds_trie_t *trie, ds_trie_node_t *node,
                             void (*destroy)(ds_trie_t *, ds_trie_node_t *,
                                             va_list *));
void FUNC(trie_destroy_callback)(ds_trie_t *trie, ds_trie_node_t *node,
                                 va_list *args);
void FUNC(trie_delete_trie)(ds_trie_t *trie);
void FUNC(trie_destroy_trie)(ds_trie_t *trie,
                             void (*destroy)(ds_trie_t *, ds_trie_node_t *,
                                             va_list *));
void FUNC(trie_apply)(ds_trie_t *trie, ds_trie_node_t *node,
                      void (*f)(ds_trie_t *, ds_trie_node_t *, va_list *), ...);
ds_trie_node_t *FUNC(trie_clone_node)(ds_trie_t *trie, ds_trie_node_t *node,
                                      ds_trie_node_t *parent_node,
                                      void *(*clone_data)(void *));
ds_trie_t *FUNC(trie_clone)(ds_trie_t *trie, void *(*clone_data)(void *));


#ifdef __cplusplus
}
#endif

#endif /* DS_TRIE_H */
