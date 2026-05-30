#include "heap.h"
#include <stdio.h>
#include <stdlib.h>

void swap(ds_heap_node_t **a, ds_heap_node_t **b) {
  ds_heap_node_t *temp;
  size_t index;
  index = (size_t)(*a)->index;
  (*a)->index = (size_t)(*b)->index;
  (*b)->index = index;
  temp = *a;
  *a = *b;
  *b = temp;
}

void heapify_up(ds_heap_t *heap, size_t index,
                int (*compare)(ds_heap_node_t *, ds_heap_node_t *)) {
  size_t parent;
#ifdef DS_THREAD_SAFE
  LOCK(heap);
#endif
  parent = (index - 1) / 2;
  while (index > 0 && compare(heap->data[index], heap->data[parent]) < 0) {
    swap(&heap->data[index], &heap->data[parent]);
    index = parent;
    parent = (index - 1) / 2;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(heap);
#endif
}

void heapify_down(ds_heap_t *heap, size_t index,
                  int (*compare)(ds_heap_node_t *, ds_heap_node_t *)) {
  size_t left, right, smallest;
#ifdef DS_THREAD_SAFE
  LOCK(heap);
#endif
  while (1) {
    left = 2 * index + 1;
    right = 2 * index + 2;
    smallest = index;

    if (left < heap->size &&
        compare(heap->data[left], heap->data[smallest]) < 0) {
      smallest = left;
    }
    if (right < heap->size &&
        compare(heap->data[right], heap->data[smallest]) < 0) {
      smallest = right;
    }
    if (smallest != index) {
      swap(&heap->data[index], &heap->data[smallest]);
      index = smallest;
    } else {
      break;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(heap);
#endif
}

ds_heap_t *FUNC(heap_create)(size_t capacity) {
  return FUNC(heap_create_alloc)(capacity, malloc, free);
}

ds_heap_t *FUNC(heap_create_alloc)(size_t capacity, void *(*allocator)(size_t),
                                   void (*deallocator)(void *)) {
  ds_heap_t *heap = (ds_heap_t *)allocator(sizeof(ds_heap_t));
  heap->allocator = allocator;
  heap->deallocator = deallocator;
  heap->data = (capacity == 0U)
                   ? NULL
                   : (ds_heap_node_t **)allocator(capacity *
                                                  sizeof(ds_heap_node_t *));
  heap->size = 0;
  heap->capacity = capacity;
  heap->nil = (ds_heap_node_t *)allocator(sizeof(ds_heap_node_t));
  heap->nil->index = (size_t)-1;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(heap);
#endif
  return heap;
}

void FUNC(heap_insert)(ds_heap_t *heap, ds_heap_node_t *node,
                       int (*compare)(ds_heap_node_t *, ds_heap_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(heap);
#endif
  if (heap->size >= heap->capacity) {
    size_t new_capacity;
    ds_heap_node_t **new_data;
    size_t i;
    new_capacity = heap->capacity == 0U ? 1U : heap->capacity * 2U;
    new_data = (ds_heap_node_t **)heap->allocator(new_capacity *
                                                  sizeof(*new_data));
    if (new_data == NULL) {
#ifdef DS_THREAD_SAFE
      UNLOCK(heap);
#endif
      return;
    }
    for (i = 0U; i < heap->size; i++) {
      new_data[i] = heap->data[i];
    }
    if (heap->data != NULL) {
      heap->deallocator(heap->data);
    }
    heap->data = new_data;
    heap->capacity = new_capacity;
  }
  node->index = heap->size;
  heap->data[heap->size] = node;
  heapify_up(heap, heap->size, compare);
  heap->size++;
#ifdef DS_THREAD_SAFE
  UNLOCK(heap);
#endif
}

void *FUNC(heap_extract_root)(ds_heap_t *heap,
                              int (*compare)(ds_heap_node_t *,
                                             ds_heap_node_t *)) {
  void *root;
#ifdef DS_THREAD_SAFE
  LOCK(heap);
#endif
  if (FUNC(heap_is_empty)(heap)) {
#ifdef DS_THREAD_SAFE
    UNLOCK(heap);
#endif
    return heap->nil;
  }
  root = heap->data[0];
  heap->data[0] = heap->data[heap->size - 1];
  heap->size--;
  if (heap->size != 0U) {
    heap->data[0]->index = 0U;
    heapify_down(heap, (size_t)0, compare);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(heap);
#endif
  return root;
}

void *FUNC(heap_peek_root)(ds_heap_t *heap) {
#ifdef DS_THREAD_SAFE
  LOCK(heap);
#endif
  if (FUNC(heap_is_empty)(heap)) {
#ifdef DS_THREAD_SAFE
    UNLOCK(heap);
#endif
    return heap->nil;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(heap);
#endif
  return heap->data[0];
}

bool FUNC(heap_is_empty)(ds_heap_t *heap) { return heap->size == 0; }

void FUNC(heap_destroy)(ds_heap_t *heap, void (*destroy)(ds_heap_node_t *)) {
  size_t i;
  if (destroy != NULL) {
    for (i = 0; i < heap->size; ++i) {
      destroy(heap->data[i]);
    }
  }
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(heap);
#endif
  heap->deallocator(heap->nil);
  if (heap->data != NULL) {
    heap->deallocator(heap->data);
  }
  heap->deallocator(heap);
}

ds_heap_t *FUNC(heap_clone)(ds_heap_t *heap,
                            ds_heap_node_t *(*clone_node)(ds_heap_node_t *)) {
  size_t i;
  ds_heap_t *new_heap;
#ifdef DS_THREAD_SAFE
  LOCK(heap);
#endif
  new_heap = FUNC(heap_create_alloc)(heap->capacity, heap->allocator,
                                     heap->deallocator);
  new_heap->size = heap->size;
  for (i = 0; i < heap->size; ++i) {
    ds_heap_node_t *original_node = heap->data[i];
    ds_heap_node_t *cloned_node = clone_node(original_node);
    new_heap->data[i] = cloned_node;
    cloned_node->index = i;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(heap);
#endif

  return new_heap;
}
