#include "trie.h"
#include "hash_table.h"
#include <stdbool.h>
#include <stdlib.h>

struct trie_hash_apply_context {
  ds_trie_t *trie;
  void (*callback)(ds_trie_t *, ds_trie_node_t *, va_list *);
  va_list *args;
};

static int trie_hash_table_compare_slice(void *a, void *b) {
  size_t slice_a;
  size_t slice_b;
  slice_a = (size_t)a;
  slice_b = (size_t)b;
  if (slice_a < slice_b) {
    return -1;
  }
  if (slice_a > slice_b) {
    return 1;
  }
  return 0;
}

static size_t trie_hash_table_hash_slice(void *key) {
  size_t slice;
  slice = (size_t)key;
  return hash_func_size_t(&slice);
}

static void trie_hash_table_apply_node(ds_hash_node_t *hash_node,
                                       void *args) {
  struct trie_hash_apply_context *context;
  ds_trie_node_t *node;
  context = (struct trie_hash_apply_context *)args;
  node = (ds_trie_node_t *)hash_node->value;

  if (context->args != NULL) {
    va_list args_copy1;
    va_list args_copy2;
    DS_VA_COPY(args_copy1, *context->args);
    DS_VA_COPY(args_copy2, *context->args);
    FUNC(trie_hash_table_store_apply)(context->trie, node->children,
                                      context->callback, &args_copy1);
    context->callback(context->trie, node, &args_copy2);
    va_end(args_copy1);
    va_end(args_copy2);
  } else {
    FUNC(trie_hash_table_store_apply)(context->trie, node->children,
                                      context->callback, NULL);
    context->callback(context->trie, node, NULL);
  }
}

ds_trie_node_t *FUNC(trie_create_node)(ds_trie_t *trie) {
  ds_trie_node_t *node;
  node = (ds_trie_node_t *)trie->allocator(sizeof(ds_trie_node_t));
  node->children = trie->store_create(trie, trie->num_splits);
  node->data_slice = 0;
  node->is_terminal = false;
  node->terminal_data = NULL;
  node->parent = NULL;
  return node;
}

ds_trie_t *FUNC(trie_create_hash)(size_t num_splits) {
  return FUNC(trie_create_hash_alloc)(num_splits, malloc, free);
}

ds_trie_t *FUNC(trie_create_hash_alloc)(size_t num_splits,
                                        void *(*allocator)(size_t),
                                        void (*deallocator)(void *)) {
  return FUNC(trie_create_alloc)(
      num_splits, FUNC(trie_hash_table_store_search),
      FUNC(trie_hash_table_store_create), FUNC(trie_hash_table_store_insert),
      FUNC(trie_hash_table_store_remove),
      FUNC(trie_hash_table_store_destroy_entry),
      FUNC(trie_hash_table_store_destroy),
      FUNC(trie_hash_table_store_get_size), FUNC(trie_hash_table_store_apply),
      FUNC(trie_hash_table_store_clone), allocator, deallocator);
}

ds_trie_node_t *FUNC(trie_hash_table_store_search)(void *store,
                                                   size_t slice) {
  ds_hash_table_t *table;
  ds_trie_node_t *result;
  table = (ds_hash_table_t *)store;
  result = (ds_trie_node_t *)FUNC(hash_table_lookup)(
      table, (void *)slice, trie_hash_table_hash_slice,
      trie_hash_table_compare_slice);
  if (result == table->nil) {
    return NULL;
  }
  return result;
}

void *FUNC(trie_hash_table_store_create)(ds_trie_t *trie, size_t size) {
  return (void *)FUNC(hash_table_create_alloc)(size, HASH_CHAINING, NULL,
                                               trie->allocator,
                                               trie->deallocator);
}

void FUNC(trie_hash_table_store_insert)(void *store, ds_trie_node_t *node) {
  ds_hash_table_t *table;
  table = (ds_hash_table_t *)store;
  FUNC(hash_table_insert)(table, (void *)node->data_slice, (void *)node,
                          trie_hash_table_hash_slice,
                          trie_hash_table_compare_slice);
}

void FUNC(trie_hash_table_store_remove)(void *store, ds_trie_node_t *node) {
  FUNC(trie_hash_table_store_destroy_entry)(store, node);
}

void FUNC(trie_hash_table_store_destroy_entry)(void *store,
                                               ds_trie_node_t *node) {
  ds_hash_table_t *table;
  table = (ds_hash_table_t *)store;
  FUNC(hash_table_remove)(table, (void *)node->data_slice,
                          trie_hash_table_hash_slice,
                          trie_hash_table_compare_slice, NULL);
}

void FUNC(trie_hash_table_store_destroy)(void *store) {
  ds_hash_table_t *table;
  table = (ds_hash_table_t *)store;
  FUNC(hash_table_destroy)(table, NULL);
}

size_t FUNC(trie_hash_table_store_get_size)(void *store) {
  ds_hash_table_t *table;
  table = (ds_hash_table_t *)store;
  return table->size;
}

void FUNC(trie_hash_table_store_apply)(
    ds_trie_t *trie, void *store,
    void (*f)(ds_trie_t *, ds_trie_node_t *, va_list *), va_list *args) {
  ds_hash_table_t *table;
  struct trie_hash_apply_context context;
  table = (ds_hash_table_t *)store;
  if (table == NULL) {
    return;
  }
  context.trie = trie;
  context.callback = f;
  context.args = args;
  FUNC(hash_table_for_each)(table, trie_hash_table_apply_node, &context);
}

void *FUNC(trie_hash_table_store_clone)(ds_trie_t *trie, void *store,
                                        ds_trie_node_t *parent_node,
                                        void *(*clone_data)(void *)) {
  ds_hash_table_t *table;
  ds_hash_table_t *new_table;
  ds_hash_node_t *cur;
  ds_hash_node_t *temp;
  ds_trie_node_t *node;
  ds_trie_node_t *new_node;
  table = (ds_hash_table_t *)store;
  new_table = FUNC(hash_table_create_alloc)(
      table->capacity, table->mode, table->probing_func, table->allocator,
      table->deallocator);
  cur = table->last_node;

  while (cur != NULL) {
    temp = cur->list_prev;
    node = (ds_trie_node_t *)cur->value;
    new_node = FUNC(trie_clone_node)(trie, node, parent_node, clone_data);
    FUNC(trie_hash_table_store_insert)(new_table, new_node);
    cur = temp;
  }

  return (void *)new_table;
}

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
                         void *(*clone_data)(void *))) {
  return FUNC(trie_create_alloc)(
      num_splits, store_search, store_create, store_insert, store_remove,
      store_destroy_entry, store_destroy, store_get_size, store_apply,
      store_clone, malloc, free);
}

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
    void *(*allocator)(size_t), void (*deallocator)(void *)) {
  ds_trie_t *trie;
  trie = (ds_trie_t *)allocator(sizeof(ds_trie_t));
  trie->allocator = allocator;
  trie->deallocator = deallocator;
  trie->context = ds_default_context();
  trie->num_splits = num_splits;
  trie->store_search = store_search;
  trie->store_create = store_create;
  trie->store_insert = store_insert;
  trie->store_remove = store_remove;
  trie->store_destroy_entry = store_destroy_entry;
  trie->store_destroy = store_destroy;
  trie->store_get_size = store_get_size;
  trie->store_apply = store_apply;
  trie->store_clone = store_clone;
  trie->root = FUNC(trie_create_node)(trie);
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(trie);
#endif
  return trie;
}

void FUNC(trie_insert)(ds_trie_t *trie, void *data,
                       size_t (*get_slice)(void *, size_t),
                       bool (*has_slice)(void *, size_t)) {
  ds_trie_node_t *current_node;
  ds_trie_node_t *result;
  ds_trie_node_t *new_node;
  size_t current_slice;
  size_t i;
#ifdef DS_THREAD_SAFE
  LOCK(trie);
#endif
  current_node = trie->root;

  for (i = 0; has_slice(data, i); i++) {
    current_slice = get_slice(data, i);
    result = trie->store_search(current_node->children, current_slice);

    if (result != NULL) {
      current_node = result;
    } else {
      new_node = FUNC(trie_create_node)(trie);
      new_node->data_slice = current_slice;
      new_node->parent = current_node;
      trie->store_insert(current_node->children, new_node);
      current_node = new_node;
    }
  }

  current_node->is_terminal = true;
  current_node->terminal_data = data;
#ifdef DS_THREAD_SAFE
  UNLOCK(trie);
#endif
}

ds_trie_node_t *FUNC(trie_search)(ds_trie_t *trie, void *data,
                                  size_t (*get_slice)(void *, size_t),
                                  bool (*has_slice)(void *, size_t)) {
  ds_trie_node_t *current_node;
  ds_trie_node_t *result;
  size_t current_slice;
  size_t i;
#ifdef DS_THREAD_SAFE
  LOCK(trie);
#endif
  if (trie->root == NULL) {
#ifdef DS_THREAD_SAFE
    UNLOCK(trie);
#endif
    return NULL;
  }
  current_node = trie->root;

  for (i = 0; has_slice(data, i); i++) {
    current_slice = get_slice(data, i);
    result = trie->store_search(current_node->children, current_slice);

    if (result == NULL) {
#ifdef DS_THREAD_SAFE
      UNLOCK(trie);
#endif
      return NULL;
    }
    current_node = result;
  }
  result = current_node->is_terminal ? current_node : NULL;

#ifdef DS_THREAD_SAFE
  UNLOCK(trie);
#endif
  return result;
}

void FUNC(trie_delete_node)(ds_trie_t *trie, ds_trie_node_t *node) {
  ds_trie_node_t *cur;
  ds_trie_node_t *temp;
#ifdef DS_THREAD_SAFE
  LOCK(trie);
#endif
  if (!node->is_terminal) {
#ifdef DS_THREAD_SAFE
    UNLOCK(trie);
#endif
    return;
  }
  node->is_terminal = false;
  cur = node;
  while (cur != NULL && !cur->is_terminal) {
    if (trie->store_get_size(cur->children) == 0 && cur->parent != NULL) {
      trie->store_destroy_entry(cur->parent->children, cur);
      if (cur != trie->root) {
        temp = cur->parent;
        trie->store_destroy(cur->children);
        trie->deallocator(cur);
        cur = temp;
        continue;
      }
    }
    cur = cur->parent;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(trie);
#endif
}

void FUNC(trie_destroy_node)(ds_trie_t *trie, ds_trie_node_t *node,
                             void (*destroy)(ds_trie_t *, ds_trie_node_t *,
                                             va_list *)) {
#ifdef DS_THREAD_SAFE
  LOCK(trie);
#endif
  if (!node->is_terminal) {
#ifdef DS_THREAD_SAFE
    UNLOCK(trie);
#endif
    return;
  }
  destroy(trie, node, NULL);
  FUNC(trie_delete_node)(trie, node);
#ifdef DS_THREAD_SAFE
  UNLOCK(trie);
#endif
}

void FUNC(trie_destroy_callback)(ds_trie_t *trie, ds_trie_node_t *node,
                                 va_list *args) {
  (void)args;
  trie->store_destroy(node->children);
  trie->deallocator(node);
}

void FUNC(trie_delete_trie)(ds_trie_t *trie) {
  trie->store_apply(trie, trie->root->children, FUNC(trie_destroy_callback),
                    NULL);
  trie->store_destroy(trie->root->children);
}

void FUNC(trie_destroy_trie)(ds_trie_t *trie,
                             void (*destroy)(ds_trie_t *, ds_trie_node_t *,
                                             va_list *)) {
  if (destroy != NULL) {
    trie->store_apply(trie, trie->root->children, destroy, NULL);
  }
  trie->store_apply(trie, trie->root->children, FUNC(trie_destroy_callback),
                    NULL);
  trie->store_destroy(trie->root->children);
  if (destroy != NULL) {
    destroy(trie, trie->root, NULL);
  }
  trie->deallocator(trie->root);
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(trie);
#endif
  trie->deallocator(trie);
}

void FUNC(trie_apply)(ds_trie_t *trie, ds_trie_node_t *node,
                      void (*f)(ds_trie_t *, ds_trie_node_t *, va_list *),
                      ...) {
  va_list args;
  va_start(args, f);

  trie->store_apply(trie, node->children, f, &args);
  va_end(args);
}

ds_trie_node_t *FUNC(trie_clone_node)(ds_trie_t *trie, ds_trie_node_t *node,
                                      ds_trie_node_t *parent_node,
                                      void *(*clone_data)(void *)) {
  ds_trie_node_t *new_node;
  new_node = (ds_trie_node_t *)trie->allocator(sizeof(ds_trie_node_t));
  new_node->data_slice = node->data_slice;
  new_node->is_terminal = node->is_terminal;
  if (clone_data != NULL && node->terminal_data != NULL) {
    new_node->terminal_data = clone_data(node->terminal_data);
  } else {
    new_node->terminal_data = node->terminal_data;
  }
  new_node->parent = parent_node;
  new_node->children = trie->store_clone(trie, node->children, new_node,
                                         clone_data);
  return new_node;
}

ds_trie_t *FUNC(trie_clone)(ds_trie_t *trie, void *(*clone_data)(void *)) {
  ds_trie_t *new_trie;
#ifdef DS_THREAD_SAFE
  LOCK(trie);
#endif
  new_trie = (ds_trie_t *)trie->allocator(sizeof(ds_trie_t));
  new_trie->num_splits = trie->num_splits;
  new_trie->store_search = trie->store_search;
  new_trie->store_create = trie->store_create;
  new_trie->store_insert = trie->store_insert;
  new_trie->store_remove = trie->store_remove;
  new_trie->store_destroy_entry = trie->store_destroy_entry;
  new_trie->store_destroy = trie->store_destroy;
  new_trie->store_get_size = trie->store_get_size;
  new_trie->store_apply = trie->store_apply;
  new_trie->store_clone = trie->store_clone;
  new_trie->allocator = trie->allocator;
  new_trie->deallocator = trie->deallocator;
  new_trie->context = trie->context;
  new_trie->root = FUNC(trie_clone_node)(trie, trie->root, NULL, clone_data);
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(new_trie);
  UNLOCK(trie);
#endif
  return new_trie;
}


static const ds_trie_store_ops_t ds_trie_hash_ops = {
    FUNC(trie_hash_table_store_search),        FUNC(trie_hash_table_store_create),
    FUNC(trie_hash_table_store_insert),        FUNC(trie_hash_table_store_remove),
    FUNC(trie_hash_table_store_destroy_entry), FUNC(trie_hash_table_store_destroy),
    FUNC(trie_hash_table_store_get_size),      FUNC(trie_hash_table_store_apply),
    FUNC(trie_hash_table_store_clone)};

const ds_trie_store_ops_t *FUNC(ds_trie_hash_store_ops)(void) {
  return &ds_trie_hash_ops;
}

void ds_trie_config_init(ds_trie_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->num_splits = 256U;
  config->store_ops = NULL;
  config->allocator = NULL;
  config->deallocator = NULL;
  config->context = NULL;
}

ds_trie_t *FUNC(ds_trie_create_config)(const ds_trie_config_t *config) {
  const ds_trie_store_ops_t *ops;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
  ds_trie_t *trie;

  if (config == NULL) {
    return NULL;
  }
  ops = config->store_ops != NULL ? config->store_ops : FUNC(ds_trie_hash_store_ops)();
  allocator = config->allocator != NULL ? config->allocator : malloc;
  deallocator = config->deallocator != NULL ? config->deallocator : free;

  trie = FUNC(trie_create_alloc)(config->num_splits, ops->search, ops->create,
                                 ops->insert, ops->remove, ops->destroy_entry,
                                 ops->destroy, ops->get_size, ops->apply,
                                 ops->clone, allocator, deallocator);
  if (trie != NULL) {
    trie->context = config->context != NULL ? config->context : ds_default_context();
  }
  return trie;
}

ds_trie_t *FUNC(ds_trie_create_hash)(size_t num_splits) {
  ds_trie_config_t config;
  ds_trie_config_init(&config);
  config.num_splits = num_splits;
  config.store_ops = FUNC(ds_trie_hash_store_ops)();
  return FUNC(ds_trie_create_config)(&config);
}

ds_status_t FUNC(ds_trie_insert)(ds_trie_t *trie, void *data,
                                  size_t (*get_slice)(void *, size_t),
                                  bool (*has_slice)(void *, size_t)) {
  if (trie == NULL || get_slice == NULL || has_slice == NULL) {
    return DS_CONTEXT_ERROR(trie != NULL ? trie->context : NULL, DS_ERR_NULL,
                            "ds.trie/null_argument", "trie",
                            "trie insert received NULL argument");
  }
  FUNC(trie_insert)(trie, data, get_slice, has_slice);
  return DS_OK;
}

ds_status_t FUNC(ds_trie_get)(ds_trie_t *trie, void *data,
                               size_t (*get_slice)(void *, size_t),
                               bool (*has_slice)(void *, size_t),
                               ds_trie_node_t **out_node) {
  ds_trie_node_t *node;
  if (out_node != NULL) {
    *out_node = NULL;
  }
  if (trie == NULL || get_slice == NULL || has_slice == NULL ||
      out_node == NULL) {
    return DS_CONTEXT_ERROR(trie != NULL ? trie->context : NULL, DS_ERR_NULL,
                            "ds.trie/null_argument", "trie",
                            "trie get received NULL argument");
  }
  node = FUNC(trie_search)(trie, data, get_slice, has_slice);
  if (node == NULL) {
    return DS_NOT_FOUND;
  }
  *out_node = node;
  return DS_OK;
}

ds_status_t FUNC(ds_trie_remove)(ds_trie_t *trie, void *data,
                                  size_t (*get_slice)(void *, size_t),
                                  bool (*has_slice)(void *, size_t)) {
  ds_trie_node_t *node;
  ds_status_t status;
  status = FUNC(ds_trie_get)(trie, data, get_slice, has_slice, &node);
  if (status != DS_OK) {
    return status;
  }
  FUNC(trie_delete_node)(trie, node);
  return DS_OK;
}

void FUNC(ds_trie_destroy)(ds_trie_t *trie) {
  if (trie == NULL) {
    return;
  }
  FUNC(trie_destroy_trie)(trie, NULL);
}
