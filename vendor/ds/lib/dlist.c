#include "dlist.h"
#include <stdio.h>
#include <stdlib.h>

ds_dlist_t *FUNC(dlist_create)(void) {
  return FUNC(dlist_create_alloc)(malloc, free);
}

ds_dlist_t *FUNC(dlist_create_alloc)(void *(*allocator)(size_t),
                                     void (*deallocator)(void *)) {
  ds_dlist_t *list = (ds_dlist_t *)allocator(sizeof(ds_dlist_t));
  list->allocator = allocator;
  list->deallocator = deallocator;
  list->nil = (ds_dlist_node_t *)allocator(sizeof(ds_dlist_node_t));
  list->head = list->nil;
  list->tail = list->nil;
  list->nil->prev = list->nil;
  list->nil->next = list->nil;
  list->size = 0;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(list);
#endif
  return list;
}

void FUNC(dlist_append)(ds_dlist_t *list, ds_dlist_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  FUNC(dlist_insert_after)(list, node, list->tail);
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_prepend)(ds_dlist_t *list, ds_dlist_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  FUNC(dlist_insert_before)(list, node, list->head);
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

ds_dlist_node_t *FUNC(dlist_search)(ds_dlist_t *list, ds_dlist_node_t *node,
                                    int (*compare)(ds_dlist_node_t *,
                                                   ds_dlist_node_t *)) {
  ds_dlist_node_t *head;
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  head = list->head;
  while (head != list->nil && compare(head, node) != 0) {
    head = head->next;
  }
  if (head == list->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(list);
#endif

    return list->nil;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
  return head;
}

void FUNC(dlist_insert_before)(ds_dlist_t *list, ds_dlist_node_t *node,
                               ds_dlist_node_t *next) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  node->next = next;
  node->prev = next->prev;
  next->prev->next = node;
  next->prev = node;

  if (next == list->head) {
    list->head = node;
  }

  if (list->tail == list->nil) {
    list->tail = node;
  }
  list->size += 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_insert_after)(ds_dlist_t *list, ds_dlist_node_t *node,
                              ds_dlist_node_t *prev) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  node->next = prev->next;
  node->prev = prev;
  prev->next->prev = node;
  prev->next = node;

  if (prev == list->tail) {
    list->tail = node;
  }

  if (list->head == list->nil) {
    list->head = node;
  }
  list->size += 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_delete_node)(ds_dlist_t *list, ds_dlist_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  node->prev->next = node->next;
  node->next->prev = node->prev;

  if (node == list->head) {
    list->head = node->next;
  }

  if (node == list->tail) {
    list->tail = node->prev;
  }
  list->size -= 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_delete)(ds_dlist_t *list) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  list->head = list->tail = list->nil;
  list->size = 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_destroy_node)(ds_dlist_t *list, ds_dlist_node_t *node,
                              void (*destroy)(ds_dlist_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  FUNC(dlist_delete_node)(list, node);
  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_destroy)(ds_dlist_t *list, void (*destroy)(ds_dlist_node_t *)) {
  ds_dlist_node_t *node = list->head;
  while (node != list->nil) {
    ds_dlist_node_t *next = node->next;
    if (destroy != NULL) {
      destroy(node);
    }
    node = next;
  }
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(list);
#endif
  list->deallocator(list->nil);
  list->deallocator(list);
}

void FUNC(dlist_walk_forward)(ds_dlist_t *list, ds_dlist_node_t *node,
                              void (*callback)(ds_dlist_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  while (node != list->nil) {
    callback(CAST(node, void));
    node = node->next;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

void FUNC(dlist_walk_backwards)(ds_dlist_t *list, ds_dlist_node_t *node,
                                void (*callback)(ds_dlist_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  while (node != list->nil) {
    callback(CAST(node, void));
    node = node->prev;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
}

bool FUNC(dlist_is_empty)(ds_dlist_t *list) {
  bool result;
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  result = list->size == 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif
  return result;
}

ds_dlist_t *
FUNC(dlist_clone)(ds_dlist_t *list,
                  ds_dlist_node_t *(*clone_node)(ds_dlist_node_t *)) {
  ds_dlist_t *new_list;
  ds_dlist_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(list);
#endif
  new_list = FUNC(dlist_create_alloc)(list->allocator, list->deallocator);
  current = list->head;
  while (current != list->nil) {
    ds_dlist_node_t *cloned_node = clone_node(current);
    FUNC(dlist_append)(new_list, cloned_node);
    current = current->next;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(list);
#endif

  return new_list;
}
