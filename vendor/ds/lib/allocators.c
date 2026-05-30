#include "allocators.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DS_DEBUG_ALLOC_MAGIC 0x4453414cUL

typedef struct ds_debug_header {
  unsigned long magic;
  size_t size;
} ds_debug_header_t;

static const ds_diagnostic_desc_t DS_DEBUG_ALLOC_BAD_FREE = {
    "ds.allocators/debug_bad_free", DS_DIAG_ERROR, "allocators",
    "debug allocator rejected a pointer with an invalid allocation header"};

static size_t ds_align_up(size_t value, size_t alignment) {
  size_t remainder;

  if (alignment == 0U) {
    return value;
  }
  remainder = value % alignment;
  if (remainder == 0U) {
    return value;
  }
  return value + (alignment - remainder);
}

static void *arena_alloc(size_t size, void *user) {
  ds_arena_t *arena;
  size_t alignment;
  size_t start;
  size_t next;

  arena = (ds_arena_t *)user;
  if (arena == NULL || size == 0U) {
    return NULL;
  }
  alignment = sizeof(void *);
  start = ds_align_up(arena->offset, alignment);
  next = start + size;
  if (next < start || next > arena->capacity) {
    return NULL;
  }
  arena->offset = next;
  return arena->buffer + start;
}

static void *arena_calloc(size_t count, size_t size, void *user) {
  void *ptr;
  size_t total;

  if (size != 0U && count > ((size_t)-1) / size) {
    return NULL;
  }
  total = count * size;
  ptr = arena_alloc(total, user);
  if (ptr != NULL) {
    memset(ptr, 0, total);
  }
  return ptr;
}

static void *arena_realloc(void *ptr, size_t size, void *user) {
  (void)ptr;
  if (ptr != NULL) {
    return NULL;
  }
  return arena_alloc(size, user);
}

static void arena_free(void *ptr, void *user) {
  (void)ptr;
  (void)user;
}

void ds_arena_init(ds_arena_t *arena, void *buffer, size_t capacity) {
  if (arena == NULL) {
    return;
  }
  arena->buffer = (unsigned char *)buffer;
  arena->capacity = capacity;
  arena->offset = 0U;
  arena->owns_buffer = 0;
}

ds_status_t ds_arena_create(ds_arena_t *arena, size_t capacity,
                            ds_context_t *context) {
  void *buffer;

  if (arena == NULL) {
    return DS_ERR_NULL;
  }
  buffer = ds_context_alloc(context, capacity);
  if (buffer == NULL && capacity != 0U) {
    ds_arena_init(arena, NULL, 0U);
    return DS_ERR_ALLOC;
  }
  ds_arena_init(arena, buffer, capacity);
  arena->owns_buffer = 1;
  return DS_OK;
}

void ds_arena_destroy(ds_arena_t *arena, ds_context_t *context) {
  if (arena == NULL) {
    return;
  }
  if (arena->owns_buffer) {
    ds_context_free(context, arena->buffer);
  }
  ds_arena_init(arena, NULL, 0U);
}

void ds_arena_reset(ds_arena_t *arena) {
  if (arena != NULL) {
    arena->offset = 0U;
  }
}

size_t ds_arena_used(const ds_arena_t *arena) {
  return arena == NULL ? 0U : arena->offset;
}

size_t ds_arena_remaining(const ds_arena_t *arena) {
  if (arena == NULL || arena->offset > arena->capacity) {
    return 0U;
  }
  return arena->capacity - arena->offset;
}

void ds_context_use_arena(ds_context_t *context, ds_arena_t *arena) {
  if (context == NULL) {
    return;
  }
  context->alloc = arena_alloc;
  context->calloc = arena_calloc;
  context->realloc = arena_realloc;
  context->free = arena_free;
  context->user = arena;
}

static void pool_build_free_list(ds_pool_t *pool) {
  size_t i;
  unsigned char *block;
  ds_pool_free_node_t *node;

  pool->free_list = NULL;
  for (i = 0U; i < pool->block_count; i++) {
    block = pool->buffer + (i * pool->block_size);
    node = (ds_pool_free_node_t *)block;
    node->next = pool->free_list;
    pool->free_list = node;
  }
}

void ds_pool_init(ds_pool_t *pool, void *buffer, size_t block_size,
                  size_t block_count) {
  size_t min_block_size;

  if (pool == NULL) {
    return;
  }
  min_block_size = sizeof(ds_pool_free_node_t);
  if (block_size < min_block_size) {
    block_size = min_block_size;
  }
  block_size = ds_align_up(block_size, sizeof(void *));
  pool->buffer = (unsigned char *)buffer;
  pool->block_size = block_size;
  pool->block_count = block_count;
  pool->available_count = block_count;
  pool->owns_buffer = 0;
  pool_build_free_list(pool);
}

ds_status_t ds_pool_create(ds_pool_t *pool, size_t block_size,
                           size_t block_count, ds_context_t *context) {
  void *buffer;
  size_t aligned_block_size;
  size_t total;

  if (pool == NULL) {
    return DS_ERR_NULL;
  }
  aligned_block_size = ds_align_up(block_size < sizeof(ds_pool_free_node_t)
                                       ? sizeof(ds_pool_free_node_t)
                                       : block_size,
                                   sizeof(void *));
  if (aligned_block_size != 0U &&
      block_count > ((size_t)-1) / aligned_block_size) {
    return DS_ERR_OVERFLOW;
  }
  total = aligned_block_size * block_count;
  buffer = ds_context_alloc(context, total);
  if (buffer == NULL && total != 0U) {
    return DS_ERR_ALLOC;
  }
  ds_pool_init(pool, buffer, aligned_block_size, block_count);
  pool->owns_buffer = 1;
  return DS_OK;
}

void ds_pool_destroy(ds_pool_t *pool, ds_context_t *context) {
  if (pool == NULL) {
    return;
  }
  if (pool->owns_buffer) {
    ds_context_free(context, pool->buffer);
  }
  pool->buffer = NULL;
  pool->block_size = 0U;
  pool->block_count = 0U;
  pool->free_list = NULL;
  pool->available_count = 0U;
  pool->owns_buffer = 0;
}

size_t ds_pool_available(const ds_pool_t *pool) {
  return pool == NULL ? 0U : pool->available_count;
}

static int pool_owns_pointer(const ds_pool_t *pool, const void *ptr) {
  const unsigned char *bytes;
  size_t offset;

  if (pool == NULL || pool->buffer == NULL || ptr == NULL ||
      pool->block_size == 0U) {
    return 0;
  }
  bytes = (const unsigned char *)ptr;
  if (bytes < pool->buffer ||
      bytes >= pool->buffer + (pool->block_size * pool->block_count)) {
    return 0;
  }
  offset = (size_t)(bytes - pool->buffer);
  return offset % pool->block_size == 0U;
}

static int pool_is_free_pointer(const ds_pool_t *pool, const void *ptr) {
  const ds_pool_free_node_t *node;

  if (pool == NULL || ptr == NULL) {
    return 0;
  }
  node = pool->free_list;
  while (node != NULL) {
    if ((const void *)node == ptr) {
      return 1;
    }
    node = node->next;
  }
  return 0;
}

static void *pool_alloc(size_t size, void *user) {
  ds_pool_t *pool;
  ds_pool_free_node_t *node;

  pool = (ds_pool_t *)user;
  if (pool == NULL || size > pool->block_size || pool->free_list == NULL) {
    return NULL;
  }
  node = pool->free_list;
  pool->free_list = node->next;
  if (pool->available_count != 0U) {
    pool->available_count--;
  }
  return node;
}

static void *pool_calloc(size_t count, size_t size, void *user) {
  void *ptr;
  size_t total;

  if (size != 0U && count > ((size_t)-1) / size) {
    return NULL;
  }
  total = count * size;
  ptr = pool_alloc(total, user);
  if (ptr != NULL) {
    memset(ptr, 0, total);
  }
  return ptr;
}

static void *pool_realloc(void *ptr, size_t size, void *user) {
  if (ptr == NULL) {
    return pool_alloc(size, user);
  }
  return NULL;
}

static void pool_free(void *ptr, void *user) {
  ds_pool_t *pool;
  ds_pool_free_node_t *node;

  if (ptr == NULL || user == NULL) {
    return;
  }
  pool = (ds_pool_t *)user;
  if (!pool_owns_pointer(pool, ptr) || pool_is_free_pointer(pool, ptr)) {
    return;
  }
  node = (ds_pool_free_node_t *)ptr;
  node->next = pool->free_list;
  pool->free_list = node;
  pool->available_count++;
}

void ds_context_use_pool(ds_context_t *context, ds_pool_t *pool) {
  if (context == NULL) {
    return;
  }
  context->alloc = pool_alloc;
  context->calloc = pool_calloc;
  context->realloc = pool_realloc;
  context->free = pool_free;
  context->user = pool;
}

static void *debug_alloc(size_t size, void *user) {
  ds_debug_allocator_t *debug;
  ds_debug_header_t *header;
  size_t total;

  debug = (ds_debug_allocator_t *)user;
  if (debug == NULL) {
    return NULL;
  }
  if (size > ((size_t)-1) - sizeof(*header)) {
    debug->stats.failed_allocations++;
    return NULL;
  }
  total = size + sizeof(*header);
  header = (ds_debug_header_t *)ds_context_alloc(debug->backing, total);
  if (header == NULL) {
    debug->stats.failed_allocations++;
    return NULL;
  }
  header->magic = DS_DEBUG_ALLOC_MAGIC;
  header->size = size;
  debug->stats.allocations++;
  debug->stats.bytes_live += size;
  debug->stats.bytes_total += size;
  if (debug->stats.bytes_live > debug->stats.bytes_peak) {
    debug->stats.bytes_peak = debug->stats.bytes_live;
  }
  return (void *)(header + 1);
}

static void *debug_calloc(size_t count, size_t size, void *user) {
  void *ptr;
  size_t total;

  if (size != 0U && count > ((size_t)-1) / size) {
    return NULL;
  }
  total = count * size;
  ptr = debug_alloc(total, user);
  if (ptr != NULL) {
    memset(ptr, 0, total);
  }
  return ptr;
}

static void debug_free(void *ptr, void *user) {
  ds_debug_allocator_t *debug;
  ds_debug_header_t *header;

  if (ptr == NULL || user == NULL) {
    return;
  }
  debug = (ds_debug_allocator_t *)user;
  header = ((ds_debug_header_t *)ptr) - 1;
  if (header->magic != DS_DEBUG_ALLOC_MAGIC) {
    debug->stats.failed_allocations++;
    ds_context_emit_diagnostic(debug->backing, &DS_DEBUG_ALLOC_BAD_FREE, NULL,
                               0U, __FILE__, (unsigned long)__LINE__,
                               DS_FUNCTION);
    return;
  }
  header->magic = 0UL;
  if (debug->stats.bytes_live >= header->size) {
    debug->stats.bytes_live -= header->size;
  } else {
    debug->stats.bytes_live = 0U;
  }
  debug->stats.frees++;
  ds_context_free(debug->backing, header);
}

static void *debug_realloc(void *ptr, size_t size, void *user) {
  ds_debug_allocator_t *debug;
  ds_debug_header_t *old_header;
  void *next;
  size_t copy_size;

  if (ptr == NULL) {
    return debug_alloc(size, user);
  }
  if (size == 0U) {
    debug_free(ptr, user);
    return NULL;
  }
  debug = (ds_debug_allocator_t *)user;
  old_header = ((ds_debug_header_t *)ptr) - 1;
  if (debug == NULL || old_header->magic != DS_DEBUG_ALLOC_MAGIC) {
    if (debug != NULL) {
      debug->stats.failed_allocations++;
      ds_context_emit_diagnostic(debug->backing, &DS_DEBUG_ALLOC_BAD_FREE,
                                 NULL, 0U, __FILE__,
                                 (unsigned long)__LINE__, DS_FUNCTION);
    }
    return NULL;
  }
  next = debug_alloc(size, user);
  if (next == NULL) {
    return NULL;
  }
  copy_size = old_header->size < size ? old_header->size : size;
  memcpy(next, ptr, copy_size);
  debug_free(ptr, user);
  debug->stats.reallocations++;
  return next;
}

void ds_debug_allocator_init(ds_debug_allocator_t *debug,
                             ds_context_t *backing) {
  if (debug == NULL) {
    return;
  }
  debug->backing = backing != NULL ? backing : ds_default_context();
  memset(&debug->stats, 0, sizeof(debug->stats));
}

void ds_context_use_debug_allocator(ds_context_t *context,
                                    ds_debug_allocator_t *debug) {
  if (context == NULL) {
    return;
  }
  context->alloc = debug_alloc;
  context->calloc = debug_calloc;
  context->realloc = debug_realloc;
  context->free = debug_free;
  context->user = debug;
}

void ds_debug_allocator_get_stats(const ds_debug_allocator_t *debug,
                                  ds_debug_allocator_stats_t *out_stats) {
  if (out_stats == NULL) {
    return;
  }
  if (debug != NULL) {
    *out_stats = debug->stats;
  } else {
    memset(out_stats, 0, sizeof(*out_stats));
  }
}
