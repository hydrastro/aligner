#ifndef DS_HEAP_H
#define DS_HEAP_H

#include "common.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct heap_node {
  size_t index;
} ds_heap_node_t;

typedef struct heap {
  ds_heap_node_t **data;
  size_t size;
  size_t capacity;
  ds_heap_node_t *nil;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} ds_heap_t;

void swap(ds_heap_node_t **a, ds_heap_node_t **b);
void heapify_up(ds_heap_t *heap, size_t index,
                int (*compare)(ds_heap_node_t *, ds_heap_node_t *));
void heapify_down(ds_heap_t *heap, size_t index,
                  int (*compare)(ds_heap_node_t *, ds_heap_node_t *));
ds_heap_t *FUNC(heap_create)(size_t capacity);
ds_heap_t *FUNC(heap_create_alloc)(size_t capacity, void *(*allocator)(size_t),
                                   void (*deallocator)(void *));
void FUNC(heap_insert)(ds_heap_t *heap, ds_heap_node_t *node,
                       int (*compare)(ds_heap_node_t *, ds_heap_node_t *));
void *FUNC(heap_extract_root)(ds_heap_t *heap,
                              int (*compare)(ds_heap_node_t *,
                                             ds_heap_node_t *));
void *FUNC(heap_peek_root)(ds_heap_t *heap);
bool FUNC(heap_is_empty)(ds_heap_t *heap);
void FUNC(heap_destroy)(ds_heap_t *heap, void (*destroy)(ds_heap_node_t *));
ds_heap_t *FUNC(heap_clone)(ds_heap_t *heap,
                            ds_heap_node_t *(*clone_node)(ds_heap_node_t *));


#ifdef __cplusplus
}
#endif

#endif /* DS_HEAP_H */
