#include "hash_table.h"
#include <stdlib.h>
#include <string.h>

static const size_t prime_capacities[] = {
    5,
    11,
    23,
    47,
    97,
    197,
    397,
    797,
    1597,
    3203,
    6421,
    12853,
    25717,
    51437,
    102877,
    205759,
    411527,
    823117,
    1646237,
    3292489,
    6584983,
    13169977,
    26339969,
    52679969,
    105359939,
    210719881,
    421439783,
    842879579,
    1685759167,
    3371518343,
    6743036717,
    13486073473,
    26972146961,
    53944293929,
    107888587883,
    215777175787,
    431554351609,
    863108703229,
    1726217406467,
    3452434812973,
    6904869625999,
    13809739252051,
    27619478504183,
    55238957008387,
    110477914016779,
    220955828033581,
    441911656067171,
    883823312134381,
    1767646624268779,
    3535293248537579,
    7070586497075177,
    14141172994150357,
    28282345988300791,
    56564691976601587,
    113129383953203213,
    226258767906406483,
    452517535812813007,
    905035071625626043,
    1810070143251252131,
    3620140286502504283,
    7240280573005008577};

struct hash_resize_context {
  ds_hash_table_t *new_table;
  size_t (*hash_func)(void *);
  int (*compare)(void *, void *);
};

static size_t hash_table_prime_capacity_at_least(size_t capacity) {
  size_t i;
  for (i = 0; i < sizeof(prime_capacities) / sizeof(prime_capacities[0]); i++) {
    if (prime_capacities[i] >= capacity) {
      return prime_capacities[i];
    }
  }
  return capacity * 2;
}

static bool hash_table_uses_probing(ds_hash_table_mode_t mode) {
  return mode == HASH_LINEAR_PROBING || mode == HASH_QUADRATIC_PROBING ||
         mode == HASH_CUSTOM_PROBING;
}

static void hash_table_link_node(ds_hash_table_t *table, ds_hash_node_t *node) {
  node->list_next = NULL;
  node->list_prev = table->last_node;
  if (table->last_node != NULL) {
    table->last_node->list_next = node;
  }
  table->last_node = node;
}

static void hash_table_unlink_node(ds_hash_table_t *table,
                                   ds_hash_node_t *node) {
  if (table->last_node == node) {
    table->last_node = node->list_prev;
  }
  if (node->list_prev != NULL) {
    node->list_prev->list_next = node->list_next;
  }
  if (node->list_next != NULL) {
    node->list_next->list_prev = node->list_prev;
  }
  node->list_next = NULL;
  node->list_prev = NULL;
}

static void *hash_table_list_bucket_create(ds_hash_table_t *table) {
  (void)table;
  return NULL;
}

static int hash_table_compare_keys(ds_hash_table_t *table, void *a, void *b,
                                   int (*compare)(void *, void *)) {
  if (compare != NULL) {
    return compare(a, b);
  }
  if (table != NULL && table->config_compare != NULL) {
    return table->config_compare(a, b, table->config_user);
  }
  return a == b ? 0 : 1;
}

static ds_hash_node_t *hash_table_list_bucket_search(
    ds_hash_table_t *table, void *bucket, void *key,
    int (*compare)(void *, void *)) {
  ds_hash_node_t *current;
  (void)table;
  current = (ds_hash_node_t *)bucket;
  while (current != NULL) {
    if (hash_table_compare_keys(table, current->key, key, compare) == 0) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static void hash_table_list_bucket_insert(ds_hash_table_t *table,
                                          void **bucket,
                                          ds_hash_node_t *node,
                                          int (*compare)(void *, void *)) {
  (void)table;
  (void)compare;
  node->next = (ds_hash_node_t *)*bucket;
  *bucket = node;
}

static ds_hash_node_t *hash_table_list_bucket_remove(
    ds_hash_table_t *table, void **bucket, void *key,
    int (*compare)(void *, void *)) {
  ds_hash_node_t *current;
  ds_hash_node_t *prev;
  (void)table;
  current = (ds_hash_node_t *)*bucket;
  prev = NULL;
  while (current != NULL) {
    if (hash_table_compare_keys(table, current->key, key, compare) == 0) {
      if (prev == NULL) {
        *bucket = current->next;
      } else {
        prev->next = current->next;
      }
      current->next = NULL;
      return current;
    }
    prev = current;
    current = current->next;
  }
  return NULL;
}

static void hash_table_list_bucket_destroy(ds_hash_table_t *table,
                                           void *bucket,
                                           void (*destroy)(ds_hash_node_t *)) {
  ds_hash_node_t *current;
  ds_hash_node_t *next;
  current = (ds_hash_node_t *)bucket;
  while (current != NULL) {
    next = current->next;
    if (destroy != NULL) {
      destroy(current);
    }
    table->deallocator(current);
    current = next;
  }
}

static void hash_table_list_bucket_apply(ds_hash_table_t *table, void *bucket,
                                         void (*callback)(ds_hash_node_t *,
                                                          void *),
                                         void *args) {
  ds_hash_node_t *current;
  ds_hash_node_t *next;
  (void)table;
  current = (ds_hash_node_t *)bucket;
  while (current != NULL) {
    next = current->next;
    callback(current, args);
    current = next;
  }
}

static const ds_hash_bucket_store_t hash_table_list_bucket_store = {
    hash_table_list_bucket_create,  hash_table_list_bucket_search,
    hash_table_list_bucket_insert,  hash_table_list_bucket_remove,
    hash_table_list_bucket_destroy, hash_table_list_bucket_apply};

static void hash_table_create_buckets(ds_hash_table_t *table) {
  size_t i;
  table->store.buckets =
      (void **)table->allocator(table->capacity * sizeof(void *));
  for (i = 0; i < table->capacity; i++) {
    table->store.buckets[i] = table->bucket_store->create(table);
  }
}

static void hash_table_create_entries(ds_hash_table_t *table) {
  size_t i;
  table->store.entries = (ds_hash_node_t *)table->allocator(
      table->capacity * sizeof(ds_hash_node_t));
  memset(table->store.entries, 0, table->capacity * sizeof(ds_hash_node_t));
  for (i = 0; i < table->capacity; i++) {
    table->store.entries[i].key = table->nil;
  }
}

static void hash_table_destroy_store(ds_hash_table_t *table,
                                     void (*destroy)(ds_hash_node_t *)) {
  size_t i;
  if (table->mode == HASH_CHAINING) {
    for (i = 0; i < table->capacity; i++) {
      table->bucket_store->destroy(table, table->store.buckets[i], destroy);
    }
    table->deallocator(table->store.buckets);
  } else if (hash_table_uses_probing(table->mode)) {
    for (i = 0; i < table->capacity; i++) {
      if (table->store.entries[i].key != table->nil &&
          table->store.entries[i].key != table->tombstone) {
        if (destroy != NULL) {
          destroy(&table->store.entries[i]);
        }
      }
    }
    table->deallocator(table->store.entries);
  }
}

static ds_hash_table_t *hash_table_create_alloc_internal(
    size_t capacity, ds_hash_table_mode_t mode,
    ds_hash_probing_func_t probing_func,
    const ds_hash_bucket_store_t *bucket_store, void *(*allocator)(size_t),
    void (*deallocator)(void *)) {
  ds_hash_table_t *table;
  table = (ds_hash_table_t *)allocator(sizeof(ds_hash_table_t));
  table->allocator = allocator;
  table->deallocator = deallocator;
  table->capacity = hash_table_prime_capacity_at_least(capacity);
  table->size = 0;
  table->nil = allocator(sizeof(void *));
  table->tombstone = allocator(sizeof(void *));
  table->mode = mode;
  table->last_node = NULL;
  table->bucket_store = NULL;
  table->context = ds_default_context();
  table->config_hash = NULL;
  table->config_compare = NULL;
  table->config_clone_key = NULL;
  table->config_clone_value = NULL;
  table->config_destroy_key = NULL;
  table->config_destroy_value = NULL;
  table->config_user = NULL;

  if (mode == HASH_CHAINING) {
    if (bucket_store == NULL) {
      table->bucket_store = FUNC(hash_table_linked_list_bucket_store)();
    } else {
      table->bucket_store = bucket_store;
    }
    hash_table_create_buckets(table);
  } else if (hash_table_uses_probing(mode)) {
    hash_table_create_entries(table);
  }

  if (probing_func != NULL) {
    table->probing_func = probing_func;
  } else if (mode == HASH_LINEAR_PROBING || mode == HASH_CUSTOM_PROBING) {
    table->probing_func = linear_probing;
  } else if (mode == HASH_QUADRATIC_PROBING) {
    table->probing_func = quadratic_probing;
  } else {
    table->probing_func = NULL;
  }

#ifdef DS_THREAD_SAFE
  LOCK_INIT(table);
#endif

  return table;
}

static void hash_table_reinsert_node(ds_hash_node_t *node, void *args) {
  struct hash_resize_context *context;
  context = (struct hash_resize_context *)args;
  FUNC(hash_table_insert)(context->new_table, node->key, node->value,
                          context->hash_func, context->compare);
}

size_t quadratic_probing(size_t base_index, size_t iteration, size_t capacity) {
  const size_t c1 = 1;
  const size_t c2 = 1;
  return (base_index + c1 * iteration + c2 * iteration * iteration) % capacity;
}

size_t linear_probing(size_t base_index, size_t iteration, size_t capacity) {
  return (base_index + iteration) % capacity;
}

size_t hash_func_string_djb2(void *key) {
  char *str;
  size_t hash;
  size_t c;
  str = (char *)key;
  hash = 5381;
  while ((c = (size_t)*str++) != 0) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

size_t hash_func_int(void *key) {
  unsigned int num;
  num = (unsigned int)*(int *)key;
  num = ((num >> 16) ^ num) * 0x45d9f3bu;
  num = ((num >> 16) ^ num) * 0x45d9f3bu;
  num = (num >> 16) ^ num;
  return (size_t)num;
}

size_t hash_func_size_t(void *key) {
  size_t num;
  num = *(size_t *)key;
  num = (~num) + (num << 21);
  num = num ^ (num >> 24);
  num = (num + (num << 3)) + (num << 8);
  num = num ^ (num >> 14);
  num = (num + (num << 2)) + (num << 4);
  num = num ^ (num >> 28);
  num = num + (num << 31);
  return num;
}

size_t hash_func_pointer(void *key) { return (size_t)key; }

size_t hash_func_double(void *key) {
  double num;
  size_t hash;
  unsigned char *bytes;
  size_t i;
  num = *(double *)key;
  hash = 0;
  bytes = (unsigned char *)&num;
  for (i = 0; i < sizeof(double); i++) {
    hash = (hash * 31) + bytes[i];
  }
  return hash;
}

size_t hash_func_default(void *key) { return (size_t)key; }

ds_hash_node_t *hash_node_create(ds_hash_table_t *table, void *key,
                                 void *value) {
  ds_hash_node_t *node;
  node = (ds_hash_node_t *)table->allocator(sizeof(ds_hash_node_t));
  node->key = key;
  node->value = value;
  node->next = NULL;
  node->list_next = NULL;
  node->list_prev = NULL;
  return node;
}

size_t next_prime_capacity(size_t current_capacity) {
  size_t i;
  for (i = 0; i < sizeof(prime_capacities) / sizeof(prime_capacities[0]); i++) {
    if (prime_capacities[i] > current_capacity) {
      return prime_capacities[i];
    }
  }
  return current_capacity * 2;
}

const ds_hash_bucket_store_t *FUNC(hash_table_linked_list_bucket_store)(void) {
  return &hash_table_list_bucket_store;
}

ds_hash_table_t *FUNC(hash_table_create)(size_t capacity,
                                         ds_hash_table_mode_t mode,
                                         ds_hash_probing_func_t probing_func) {
  return FUNC(hash_table_create_alloc)(capacity, mode, probing_func, malloc,
                                       free);
}

ds_hash_table_t *FUNC(hash_table_create_chain)(
    size_t capacity, const ds_hash_bucket_store_t *bucket_store) {
  return FUNC(hash_table_create_chain_alloc)(capacity, bucket_store, malloc,
                                             free);
}

ds_hash_table_t *FUNC(hash_table_create_alloc)(
    size_t capacity, ds_hash_table_mode_t mode,
    ds_hash_probing_func_t probing_func, void *(*allocator)(size_t),
    void (*deallocator)(void *)) {
  return hash_table_create_alloc_internal(capacity, mode, probing_func, NULL,
                                          allocator, deallocator);
}

ds_hash_table_t *FUNC(hash_table_create_chain_alloc)(
    size_t capacity, const ds_hash_bucket_store_t *bucket_store,
    void *(*allocator)(size_t), void (*deallocator)(void *)) {
  return hash_table_create_alloc_internal(capacity, HASH_CHAINING, NULL,
                                          bucket_store, allocator,
                                          deallocator);
}

static void hash_table_resize_unlocked(ds_hash_table_t *table,
                                       size_t new_capacity,
                                       size_t (*hash_func)(void *),
                                       int (*compare)(void *, void *)) {
  ds_hash_table_t *new_table;
  struct hash_resize_context context;
  void *old_nil;
  void *old_tombstone;
  size_t i;
  new_table = hash_table_create_alloc_internal(
      new_capacity, table->mode, table->probing_func, table->bucket_store,
      table->allocator, table->deallocator);
  context.new_table = new_table;
  context.hash_func = hash_func;
  context.compare = compare;

  if (table->mode == HASH_CHAINING) {
    for (i = 0; i < table->capacity; i++) {
      table->bucket_store->apply(table, table->store.buckets[i],
                                 hash_table_reinsert_node, &context);
    }
  } else if (hash_table_uses_probing(table->mode)) {
    for (i = 0; i < table->capacity; i++) {
      if (table->store.entries[i].key != table->nil &&
          table->store.entries[i].key != table->tombstone) {
        FUNC(hash_table_insert)
        (new_table, table->store.entries[i].key, table->store.entries[i].value,
         hash_func, compare);
      }
    }
  }

  hash_table_destroy_store(table, NULL);
  old_nil = table->nil;
  old_tombstone = table->tombstone;

  table->capacity = new_table->capacity;
  table->size = new_table->size;
  table->store = new_table->store;
  table->nil = new_table->nil;
  table->tombstone = new_table->tombstone;
  table->probing_func = new_table->probing_func;
  table->bucket_store = new_table->bucket_store;
  table->last_node = new_table->last_node;

  table->deallocator(old_nil);
  table->deallocator(old_tombstone);
  table->deallocator(new_table);
}

void FUNC(hash_table_resize)(ds_hash_table_t *table, size_t new_capacity,
                             size_t (*hash_func)(void *),
                             int (*compare)(void *, void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  hash_table_resize_unlocked(table, new_capacity, hash_func, compare);
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
}

void FUNC(hash_table_insert)(ds_hash_table_t *table, void *key, void *value,
                             size_t (*hash_func)(void *),
                             int (*compare)(void *, void *)) {
  size_t base_index;
  size_t index;
  size_t iteration;
  size_t tombstone_index;
  ds_hash_node_t *new_node;
  ds_hash_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  if ((double)table->size / (double)table->capacity >
      HASH_TABLE_RESIZE_FACTOR) {
    hash_table_resize_unlocked(table, next_prime_capacity(table->capacity),
                               hash_func, compare);
  }

  base_index = hash_func(key) % table->capacity;
  index = base_index;
  iteration = 0;
  tombstone_index = (size_t)-1;

  if (table->mode == HASH_CHAINING) {
    current = table->bucket_store->search(table, table->store.buckets[index],
                                          key, compare);
    if (current != NULL) {
      current->value = value;
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return;
    }
    new_node = hash_node_create(table, key, value);
    hash_table_link_node(table, new_node);
    table->bucket_store->insert(table, &table->store.buckets[index], new_node,
                                compare);
  } else if (hash_table_uses_probing(table->mode)) {
    while (table->store.entries[index].key != table->nil) {
      if (table->store.entries[index].key == table->tombstone) {
        if (tombstone_index == (size_t)-1) {
          tombstone_index = index;
        }
      } else if (compare(table->store.entries[index].key, key) == 0) {
        table->store.entries[index].value = value;
#ifdef DS_THREAD_SAFE
        UNLOCK(table);
#endif
        return;
      }
      iteration++;
      index = table->probing_func(base_index, iteration, table->capacity);
    }

    if (tombstone_index != (size_t)-1) {
      index = tombstone_index;
    }

    table->store.entries[index].key = key;
    table->store.entries[index].value = value;
    table->store.entries[index].next = NULL;
    hash_table_link_node(table, &table->store.entries[index]);
  }

  table->size++;

#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
}

void *FUNC(hash_table_lookup)(ds_hash_table_t *table, void *key,
                              size_t (*hash_func)(void *),
                              int (*compare)(void *, void *)) {
  size_t base_index;
  size_t index;
  size_t iteration;
  ds_hash_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  base_index = hash_func(key) % table->capacity;
  index = base_index;
  iteration = 0;

  if (table->mode == HASH_CHAINING) {
    current = table->bucket_store->search(table, table->store.buckets[index],
                                          key, compare);
    if (current != NULL) {
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return current->value;
    }
  } else if (hash_table_uses_probing(table->mode)) {
    while (table->store.entries[index].key != table->nil) {
      if (table->store.entries[index].key != table->tombstone &&
          compare(table->store.entries[index].key, key) == 0) {
#ifdef DS_THREAD_SAFE
        UNLOCK(table);
#endif
        return table->store.entries[index].value;
      }
      iteration++;
      index = table->probing_func(base_index, iteration, table->capacity);
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif

  return table->nil;
}

void FUNC(hash_table_remove)(ds_hash_table_t *table, void *key,
                             size_t (*hash_func)(void *),
                             int (*compare)(void *, void *),
                             void (*destroy)(ds_hash_node_t *)) {
  size_t base_index;
  size_t index;
  size_t iteration;
  ds_hash_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  base_index = hash_func(key) % table->capacity;
  index = base_index;
  iteration = 0;

  if (table->mode == HASH_CHAINING) {
    current = table->bucket_store->remove(table, &table->store.buckets[index],
                                          key, compare);
    if (current != NULL) {
      hash_table_unlink_node(table, current);
      table->size--;
      if (destroy != NULL) {
        destroy(current);
      }
      table->deallocator(current);
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return;
    }
  } else if (hash_table_uses_probing(table->mode)) {
    while (table->store.entries[index].key != table->nil) {
      if (table->store.entries[index].key != table->tombstone &&
          compare(table->store.entries[index].key, key) == 0) {
        if (destroy != NULL) {
          destroy(&table->store.entries[index]);
        }
        hash_table_unlink_node(table, &table->store.entries[index]);
        table->store.entries[index].key = table->tombstone;
        table->store.entries[index].value = NULL;
        table->store.entries[index].next = NULL;
        table->size--;
#ifdef DS_THREAD_SAFE
        UNLOCK(table);
#endif
        return;
      }
      iteration++;
      index = table->probing_func(base_index, iteration, table->capacity);
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
}

void FUNC(hash_table_for_each)(ds_hash_table_t *table,
                               void (*callback)(ds_hash_node_t *, void *),
                               void *args) {
  ds_hash_node_t *current;
  ds_hash_node_t *prev;
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  current = table->last_node;
  while (current != NULL) {
    prev = current->list_prev;
    callback(current, args);
    current = prev;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
}

bool FUNC(hash_table_is_empty)(ds_hash_table_t *table) {
  return table->size == 0;
}

void FUNC(hash_table_destroy)(ds_hash_table_t *table,
                              void (*destroy)(ds_hash_node_t *)) {
  hash_table_destroy_store(table, destroy);
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(table);
#endif
  table->deallocator(table->nil);
  table->deallocator(table->tombstone);
  table->deallocator(table);
}

ds_hash_table_t *FUNC(hash_table_clone)(ds_hash_table_t *table,
                                        void *(*clone_key)(void *),
                                        void *(*clone_value)(void *)) {
  ds_hash_table_t *new_table;
  void *key;
  void *value;
  size_t i;
  ds_hash_node_t *current;
  ds_hash_node_t *new_node;
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  if (table->mode == HASH_CHAINING &&
      table->bucket_store != FUNC(hash_table_linked_list_bucket_store)()) {
#ifdef DS_THREAD_SAFE
    UNLOCK(table);
#endif
    return NULL;
  }

  new_table = hash_table_create_alloc_internal(
      table->capacity, table->mode, table->probing_func, table->bucket_store,
      table->allocator, table->deallocator);

  if (table->mode == HASH_CHAINING) {
    for (i = 0; i < table->capacity; i++) {
      current = (ds_hash_node_t *)table->store.buckets[i];
      while (current != NULL) {
        key = clone_key != NULL ? clone_key(current->key) : current->key;
        value = clone_value != NULL ? clone_value(current->value)
                                    : current->value;
        new_node = hash_node_create(new_table, key, value);
        hash_table_link_node(new_table, new_node);
        new_table->bucket_store->insert(new_table, &new_table->store.buckets[i],
                                        new_node, NULL);
        new_table->size++;
        current = current->next;
      }
    }
  } else if (hash_table_uses_probing(table->mode)) {
    for (i = 0; i < table->capacity; i++) {
      if (table->store.entries[i].key != table->nil &&
          table->store.entries[i].key != table->tombstone) {
        key = clone_key != NULL ? clone_key(table->store.entries[i].key)
                                : table->store.entries[i].key;
        value = clone_value != NULL ? clone_value(table->store.entries[i].value)
                                    : table->store.entries[i].value;
        new_table->store.entries[i].key = key;
        new_table->store.entries[i].value = value;
        hash_table_link_node(new_table, &new_table->store.entries[i]);
        new_table->size++;
      }
    }
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif

  return new_table;
}

ds_hash_table_t *FUNC(hash_table_clone_with)(
    ds_hash_table_t *table, void *(*clone_key)(void *),
    void *(*clone_value)(void *), size_t (*hash_func)(void *),
    int (*compare)(void *, void *)) {
  ds_hash_table_t *new_table;
  ds_hash_node_t *current;
  void *key;
  void *value;
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  if (hash_func == NULL || compare == NULL) {
#ifdef DS_THREAD_SAFE
    UNLOCK(table);
#endif
    return NULL;
  }
  new_table = hash_table_create_alloc_internal(
      table->capacity, table->mode, table->probing_func, table->bucket_store,
      table->allocator, table->deallocator);
  current = table->last_node;
  while (current != NULL) {
    key = clone_key != NULL ? clone_key(current->key) : current->key;
    value = clone_value != NULL ? clone_value(current->value) : current->value;
    FUNC(hash_table_insert)(new_table, key, value, hash_func, compare);
    current = current->list_prev;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
  return new_table;
}


static size_t hash_table_config_hash(ds_hash_table_t *table, void *key) {
  if (table->config_hash != NULL) {
    return table->config_hash(key, table->config_user);
  }
  return hash_func_pointer((void *)key);
}

static int hash_table_config_compare(ds_hash_table_t *table, void *a,
                                     void *b) {
  if (table->config_compare != NULL) {
    return table->config_compare(a, b, table->config_user);
  }
  return a == b ? 0 : 1;
}

static void hash_table_config_destroy_node(ds_hash_table_t *table,
                                           ds_hash_node_t *node) {
  if (node == NULL) {
    return;
  }
  if (table->config_destroy_key != NULL && node->key != table->nil &&
      node->key != table->tombstone) {
    table->config_destroy_key(node->key, table->config_user);
  }
  if (table->config_destroy_value != NULL && node->value != NULL) {
    table->config_destroy_value(node->value, table->config_user);
  }
}

void ds_hash_table_config_init(ds_hash_table_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->capacity = 17U;
  config->mode = HASH_CHAINING;
  config->probing_func = NULL;
  config->bucket_store = NULL;
  config->hash = NULL;
  config->compare = NULL;
  config->clone_key = NULL;
  config->clone_value = NULL;
  config->destroy_key = NULL;
  config->destroy_value = NULL;
  config->user = NULL;
  config->context = NULL;
}

ds_hash_table_t *FUNC(ds_hash_table_create_config)(
    const ds_hash_table_config_t *config) {
  ds_hash_table_t *table;
  ds_context_t *context;

  if (config == NULL) {
    return NULL;
  }
  context = config->context != NULL ? config->context : ds_default_context();
  table = hash_table_create_alloc_internal(config->capacity, config->mode,
                                           config->probing_func,
                                           config->bucket_store, malloc, free);
  if (table == NULL) {
    DS_CONTEXT_ERROR(context, DS_ERR_ALLOC, "ds.hash_table/create_failed",
                     "hash_table", "failed to create hash table");
    return NULL;
  }
  table->context = context;
  table->config_hash = config->hash;
  table->config_compare = config->compare;
  table->config_clone_key = config->clone_key;
  table->config_clone_value = config->clone_value;
  table->config_destroy_key = config->destroy_key;
  table->config_destroy_value = config->destroy_value;
  table->config_user = config->user;
  return table;
}

static bool ds_hash_table_resize_config_unlocked(ds_hash_table_t *table,
                                                 size_t new_capacity) {
  ds_hash_table_t *new_table;
  ds_hash_node_t *current;
  ds_hash_node_t *next;
  size_t base_index;
  size_t index;
  size_t iteration;
  size_t i;
  void *old_nil;
  void *old_tombstone;

  new_table = hash_table_create_alloc_internal(
      new_capacity, table->mode, table->probing_func, table->bucket_store,
      table->allocator, table->deallocator);
  if (new_table == NULL) {
    return false;
  }
  new_table->context = table->context;
  new_table->config_hash = table->config_hash;
  new_table->config_compare = table->config_compare;
  new_table->config_clone_key = table->config_clone_key;
  new_table->config_clone_value = table->config_clone_value;
  new_table->config_destroy_key = table->config_destroy_key;
  new_table->config_destroy_value = table->config_destroy_value;
  new_table->config_user = table->config_user;

  current = table->last_node;
  while (current != NULL) {
    next = current->list_prev;
    base_index = hash_table_config_hash(new_table, current->key) %
                 new_table->capacity;
    index = base_index;
    iteration = 0U;
    if (new_table->mode == HASH_CHAINING) {
      ds_hash_node_t *new_node;
      new_node = hash_node_create(new_table, current->key, current->value);
      if (new_node == NULL) {
        hash_table_destroy_store(new_table, NULL);
        new_table->deallocator(new_table->nil);
        new_table->deallocator(new_table->tombstone);
        new_table->deallocator(new_table);
        return false;
      }
      hash_table_link_node(new_table, new_node);
      new_table->bucket_store->insert(new_table, &new_table->store.buckets[index],
                                      new_node, NULL);
      new_table->size++;
    } else if (hash_table_uses_probing(new_table->mode)) {
      while (new_table->store.entries[index].key != new_table->nil) {
        iteration++;
        index = new_table->probing_func(base_index, iteration,
                                        new_table->capacity);
      }
      new_table->store.entries[index].key = current->key;
      new_table->store.entries[index].value = current->value;
      new_table->store.entries[index].next = NULL;
      hash_table_link_node(new_table, &new_table->store.entries[index]);
      new_table->size++;
    }
    current = next;
  }

  hash_table_destroy_store(table, NULL);
  old_nil = table->nil;
  old_tombstone = table->tombstone;

  table->capacity = new_table->capacity;
  table->size = new_table->size;
  table->store = new_table->store;
  table->nil = new_table->nil;
  table->tombstone = new_table->tombstone;
  table->probing_func = new_table->probing_func;
  table->bucket_store = new_table->bucket_store;
  table->last_node = new_table->last_node;

  table->deallocator(old_nil);
  table->deallocator(old_tombstone);
  table->deallocator(new_table);
  (void)i;
  return true;
}

static ds_status_t ds_hash_table_insert_internal(ds_hash_table_t *table,
                                                      void *key, void *value,
                                                      int destroy_key_on_update) {
  size_t base_index;
  size_t index;
  size_t iteration;
  size_t tombstone_index;
  ds_hash_node_t *new_node;
  ds_hash_node_t *current;

  if (table == NULL) {
    return DS_CONTEXT_ERROR(NULL, DS_ERR_NULL, "ds.hash_table/null_table",
                            "hash_table", "hash table pointer is NULL");
  }
  if (table->config_hash == NULL || table->config_compare == NULL) {
    return DS_CONTEXT_ERROR(table->context, DS_ERR_INVALID,
                            "ds.hash_table/missing_callbacks", "hash_table",
                            "configured hash table requires hash and compare callbacks");
  }

#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  if ((double)table->size / (double)table->capacity >
      HASH_TABLE_RESIZE_FACTOR) {
    if (!ds_hash_table_resize_config_unlocked(table,
                                             next_prime_capacity(table->capacity))) {
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return DS_CONTEXT_ERROR(table->context, DS_ERR_ALLOC,
                              "ds.hash_table/resize_failed", "hash_table",
                              "failed to resize hash table");
    }
  }

  base_index = hash_table_config_hash(table, key) % table->capacity;
  index = base_index;
  iteration = 0U;
  tombstone_index = (size_t)-1;

  if (table->mode == HASH_CHAINING) {
    current = table->bucket_store->search(table, table->store.buckets[index],
                                          key, NULL);
    if (current != NULL) {
      if (table->config_destroy_value != NULL) {
        table->config_destroy_value(current->value, table->config_user);
      }
      if (destroy_key_on_update && table->config_destroy_key != NULL) {
        table->config_destroy_key(key, table->config_user);
      }
      current->value = value;
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return DS_OK;
    }
    new_node = hash_node_create(table, key, value);
    if (new_node == NULL) {
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return DS_CONTEXT_ERROR(table->context, DS_ERR_ALLOC,
                              "ds.hash_table/node_alloc_failed", "hash_table",
                              "failed to allocate hash node");
    }
    hash_table_link_node(table, new_node);
    table->bucket_store->insert(table, &table->store.buckets[index], new_node,
                                NULL);
  } else if (hash_table_uses_probing(table->mode)) {
    while (table->store.entries[index].key != table->nil) {
      if (table->store.entries[index].key == table->tombstone) {
        if (tombstone_index == (size_t)-1) {
          tombstone_index = index;
        }
      } else if (hash_table_config_compare(table, table->store.entries[index].key,
                                           key) == 0) {
        if (table->config_destroy_value != NULL) {
          table->config_destroy_value(table->store.entries[index].value,
                                      table->config_user);
        }
        if (destroy_key_on_update && table->config_destroy_key != NULL) {
          table->config_destroy_key(key, table->config_user);
        }
        table->store.entries[index].value = value;
#ifdef DS_THREAD_SAFE
        UNLOCK(table);
#endif
        return DS_OK;
      }
      iteration++;
      index = table->probing_func(base_index, iteration, table->capacity);
    }
    if (tombstone_index != (size_t)-1) {
      index = tombstone_index;
    }
    table->store.entries[index].key = key;
    table->store.entries[index].value = value;
    table->store.entries[index].next = NULL;
    hash_table_link_node(table, &table->store.entries[index]);
  } else {
#ifdef DS_THREAD_SAFE
    UNLOCK(table);
#endif
    return DS_CONTEXT_ERROR(table->context, DS_ERR_INVALID,
                            "ds.hash_table/invalid_mode", "hash_table",
                            "invalid hash table mode");
  }

  table->size++;
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
  return DS_OK;
}

ds_status_t FUNC(ds_hash_table_insert)(ds_hash_table_t *table, void *key,
                                        void *value) {
  return ds_hash_table_insert_internal(table, key, value, 0);
}

ds_status_t FUNC(ds_hash_table_insert_take_key)(ds_hash_table_t *table,
                                                 void *key, void *value) {
  return ds_hash_table_insert_internal(table, key, value, 1);
}

ds_status_t FUNC(ds_hash_table_get)(ds_hash_table_t *table, void *key,
                                     void **out_value) {
  size_t base_index;
  size_t index;
  size_t iteration;
  ds_hash_node_t *current;

  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (table == NULL || out_value == NULL) {
    return DS_CONTEXT_ERROR(table != NULL ? table->context : NULL, DS_ERR_NULL,
                            "ds.hash_table/null_argument", "hash_table",
                            "hash table or output pointer is NULL");
  }
  if (table->config_hash == NULL || table->config_compare == NULL) {
    return DS_CONTEXT_ERROR(table->context, DS_ERR_INVALID,
                            "ds.hash_table/missing_callbacks", "hash_table",
                            "configured hash table requires hash and compare callbacks");
  }

#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  base_index = hash_table_config_hash(table, key) % table->capacity;
  index = base_index;
  iteration = 0U;
  if (table->mode == HASH_CHAINING) {
    current = table->bucket_store->search(table, table->store.buckets[index],
                                          (void *)key, NULL);
    if (current != NULL) {
      *out_value = current->value;
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return DS_OK;
    }
  } else if (hash_table_uses_probing(table->mode)) {
    while (table->store.entries[index].key != table->nil) {
      if (table->store.entries[index].key != table->tombstone &&
          hash_table_config_compare(table, table->store.entries[index].key,
                                    key) == 0) {
        *out_value = table->store.entries[index].value;
#ifdef DS_THREAD_SAFE
        UNLOCK(table);
#endif
        return DS_OK;
      }
      iteration++;
      index = table->probing_func(base_index, iteration, table->capacity);
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
  return DS_NOT_FOUND;
}

ds_status_t FUNC(ds_hash_table_contains)(ds_hash_table_t *table,
                                          void *key) {
  void *value;
  return FUNC(ds_hash_table_get)(table, key, &value);
}

ds_status_t FUNC(ds_hash_table_remove)(ds_hash_table_t *table,
                                        void *key) {
  size_t base_index;
  size_t index;
  size_t iteration;
  ds_hash_node_t *current;

  if (table == NULL) {
    return DS_CONTEXT_ERROR(NULL, DS_ERR_NULL, "ds.hash_table/null_table",
                            "hash_table", "hash table pointer is NULL");
  }
#ifdef DS_THREAD_SAFE
  LOCK(table);
#endif
  base_index = hash_table_config_hash(table, key) % table->capacity;
  index = base_index;
  iteration = 0U;
  if (table->mode == HASH_CHAINING) {
    current = table->bucket_store->remove(table, &table->store.buckets[index],
                                          (void *)key, NULL);
    if (current != NULL) {
      hash_table_unlink_node(table, current);
      table->size--;
      hash_table_config_destroy_node(table, current);
      table->deallocator(current);
#ifdef DS_THREAD_SAFE
      UNLOCK(table);
#endif
      return DS_OK;
    }
  } else if (hash_table_uses_probing(table->mode)) {
    while (table->store.entries[index].key != table->nil) {
      if (table->store.entries[index].key != table->tombstone &&
          hash_table_config_compare(table, table->store.entries[index].key,
                                    key) == 0) {
        hash_table_config_destroy_node(table, &table->store.entries[index]);
        hash_table_unlink_node(table, &table->store.entries[index]);
        table->store.entries[index].key = table->tombstone;
        table->store.entries[index].value = NULL;
        table->store.entries[index].next = NULL;
        table->size--;
#ifdef DS_THREAD_SAFE
        UNLOCK(table);
#endif
        return DS_OK;
      }
      iteration++;
      index = table->probing_func(base_index, iteration, table->capacity);
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(table);
#endif
  return DS_NOT_FOUND;
}

void FUNC(ds_hash_table_destroy)(ds_hash_table_t *table) {
  ds_hash_node_t *current;
  ds_hash_node_t *prev;
  if (table == NULL) {
    return;
  }
  current = table->last_node;
  while (current != NULL) {
    prev = current->list_prev;
    hash_table_config_destroy_node(table, current);
    current = prev;
  }
  hash_table_destroy_store(table, NULL);
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(table);
#endif
  table->deallocator(table->nil);
  table->deallocator(table->tombstone);
  table->deallocator(table);
}

ds_hash_table_t *FUNC(ds_hash_table_clone)(ds_hash_table_t *table) {
  ds_hash_table_config_t config;
  ds_hash_table_t *clone;
  ds_hash_node_t *current;
  void *key;
  void *value;
  ds_status_t status;

  if (table == NULL) {
    return NULL;
  }
  ds_hash_table_config_init(&config);
  config.capacity = table->capacity;
  config.mode = table->mode;
  config.probing_func = table->probing_func;
  config.bucket_store = table->bucket_store;
  config.hash = table->config_hash;
  config.compare = table->config_compare;
  config.clone_key = table->config_clone_key;
  config.clone_value = table->config_clone_value;
  config.destroy_key = table->config_destroy_key;
  config.destroy_value = table->config_destroy_value;
  config.user = table->config_user;
  config.context = table->context;
  clone = FUNC(ds_hash_table_create_config)(&config);
  if (clone == NULL) {
    return NULL;
  }

  current = table->last_node;
  while (current != NULL) {
    key = table->config_clone_key != NULL
              ? table->config_clone_key(current->key, table->config_user)
              : current->key;
    value = table->config_clone_value != NULL
                ? table->config_clone_value(current->value, table->config_user)
                : current->value;
    status = FUNC(ds_hash_table_insert)(clone, key, value);
    if (DS_FAILED(status)) {
      FUNC(ds_hash_table_destroy)(clone);
      return NULL;
    }
    current = current->list_prev;
  }
  return clone;
}


static void hash_table_iter_destroy(ds_iter_t *iter) {
#ifdef DS_THREAD_SAFE
  ds_hash_table_t *table;
#endif
  if (iter == NULL || iter->container == NULL) {
    return;
  }
#ifdef DS_THREAD_SAFE
  table = (ds_hash_table_t *)iter->container;
  UNLOCK(table);
#endif
  iter->container = NULL;
  iter->state = NULL;
  iter->key = NULL;
  iter->value = NULL;
  iter->node = NULL;
}

static ds_status_t hash_table_iter_next(ds_iter_t *iter) {
  ds_hash_node_t *current;
  if (iter == NULL) {
    return DS_ERR_NULL;
  }
  current = (ds_hash_node_t *)iter->state;
  if (current == NULL) {
    iter->key = NULL;
    iter->value = NULL;
    iter->node = NULL;
    hash_table_iter_destroy(iter);
    return DS_STOP;
  }
  iter->node = current;
  iter->key = current->key;
  iter->value = current->value;
  iter->state = current->list_prev;
  return DS_OK;
}

ds_status_t FUNC(ds_hash_table_iter_init)(ds_hash_table_t *table,
                                           ds_iter_t *iter) {
  if (iter == NULL) {
    return DS_ERR_NULL;
  }
  iter->container = table;
#ifdef DS_THREAD_SAFE
  if (table != NULL) {
    LOCK(table);
  }
#endif
  iter->state = table != NULL ? table->last_node : NULL;
  iter->key = NULL;
  iter->value = NULL;
  iter->node = NULL;
  iter->next = hash_table_iter_next;
  iter->destroy = hash_table_iter_destroy;
  if (table == NULL) {
    return DS_ERR_NULL;
  }
  return DS_OK;
}
