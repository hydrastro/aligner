#ifndef DS_ALLOCATORS_H
#define DS_ALLOCATORS_H

#include "context.h"
#include "status.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_arena {
  unsigned char *buffer;
  size_t capacity;
  size_t offset;
  int owns_buffer;
} ds_arena_t;

typedef struct ds_pool_free_node {
  struct ds_pool_free_node *next;
} ds_pool_free_node_t;

typedef struct ds_pool {
  unsigned char *buffer;
  size_t block_size;
  size_t block_count;
  ds_pool_free_node_t *free_list;
  size_t available_count;
  int owns_buffer;
} ds_pool_t;

typedef struct ds_debug_allocator_stats {
  size_t allocations;
  size_t frees;
  size_t reallocations;
  size_t failed_allocations;
  size_t bytes_live;
  size_t bytes_peak;
  size_t bytes_total;
} ds_debug_allocator_stats_t;

typedef struct ds_debug_allocator {
  ds_context_t *backing;
  ds_debug_allocator_stats_t stats;
} ds_debug_allocator_t;

void ds_arena_init(ds_arena_t *arena, void *buffer, size_t capacity);
ds_status_t ds_arena_create(ds_arena_t *arena, size_t capacity,
                            ds_context_t *context);
void ds_arena_destroy(ds_arena_t *arena, ds_context_t *context);
void ds_arena_reset(ds_arena_t *arena);
size_t ds_arena_used(const ds_arena_t *arena);
size_t ds_arena_remaining(const ds_arena_t *arena);
void ds_context_use_arena(ds_context_t *context, ds_arena_t *arena);

void ds_pool_init(ds_pool_t *pool, void *buffer, size_t block_size,
                  size_t block_count);
ds_status_t ds_pool_create(ds_pool_t *pool, size_t block_size,
                           size_t block_count, ds_context_t *context);
void ds_pool_destroy(ds_pool_t *pool, ds_context_t *context);
size_t ds_pool_available(const ds_pool_t *pool);
void ds_context_use_pool(ds_context_t *context, ds_pool_t *pool);

void ds_debug_allocator_init(ds_debug_allocator_t *debug,
                             ds_context_t *backing);
void ds_context_use_debug_allocator(ds_context_t *context,
                                    ds_debug_allocator_t *debug);
void ds_debug_allocator_get_stats(const ds_debug_allocator_t *debug,
                                  ds_debug_allocator_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* DS_ALLOCATORS_H */
