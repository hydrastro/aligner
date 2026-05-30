#include "deque.h"
#include <stdlib.h>

ds_deque_t *FUNC(deque_create)(void) {
  return FUNC(deque_create_alloc)(malloc, free);
}

ds_deque_t *FUNC(deque_create_alloc)(void *(*allocator)(size_t),
                                     void (*deallocator)(void *)) {
  ds_deque_t *deque = (ds_deque_t *)allocator(sizeof(ds_deque_t));
  deque->allocator = allocator;
  deque->deallocator = deallocator;
  deque->nil = (ds_deque_node_t *)allocator(sizeof(ds_deque_node_t));
  deque->nil->next = deque->nil;
  deque->nil->prev = deque->nil;
  deque->head = deque->nil;
  deque->tail = deque->nil;
  deque->size = 0;

#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(deque);
#endif

  return deque;
}

void FUNC(deque_push_front)(ds_deque_t *deque, ds_deque_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  node->next = deque->head;
  node->prev = deque->nil;
  if (deque->head != deque->nil) {
    deque->head->prev = node;
  }
  deque->head = node;
  if (deque->tail == deque->nil) {
    deque->tail = node;
  }
  deque->size += 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

void FUNC(deque_push_back)(ds_deque_t *deque, ds_deque_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  node->next = deque->nil;
  node->prev = deque->tail;
  if (deque->tail != deque->nil) {
    deque->tail->next = node;
  }
  deque->tail = node;
  if (deque->head == deque->nil) {
    deque->head = node;
  }
  deque->size += 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

ds_deque_node_t *FUNC(deque_pop_front)(ds_deque_t *deque) {
  ds_deque_node_t *node;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  if (FUNC(deque_is_empty)(deque)) {
#ifdef DS_THREAD_SAFE
    UNLOCK(deque);
#endif
    return deque->nil;
  }
  node = deque->head;
  deque->head = node->next;
  if (deque->head != deque->nil) {
    deque->head->prev = deque->nil;
  } else {
    deque->tail = deque->nil;
  }
  deque->size -= 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif

  return node;
}

ds_deque_node_t *FUNC(deque_pop_back)(ds_deque_t *deque) {
  ds_deque_node_t *node;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  if (FUNC(deque_is_empty)(deque)) {
#ifdef DS_THREAD_SAFE
    UNLOCK(deque);
#endif
    return deque->nil;
  }
  node = deque->tail;
  deque->tail = node->prev;
  if (deque->tail != deque->nil) {
    deque->tail->next = deque->nil;
  } else {
    deque->head = deque->nil;
  }
  deque->size -= 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif

  return node;
}

ds_deque_node_t *FUNC(deque_peek_front)(ds_deque_t *deque) {
  ds_deque_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif
  result = deque->head;
#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
  return result;
}

ds_deque_node_t *FUNC(deque_peek_back)(ds_deque_t *deque) {
  ds_deque_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif
  result = deque->tail;
#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
  return result;
}

bool FUNC(deque_is_empty)(ds_deque_t *deque) {
  bool empty;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  empty = deque->head == deque->nil;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif

  return empty;
}

void FUNC(deque_destroy)(ds_deque_t *deque,
                         void (*destroy)(ds_deque_node_t *)) {
  ds_deque_node_t *node = deque->head;
  while (node != deque->nil) {
    ds_deque_node_t *next = node->next;
    if (destroy != NULL) {
      destroy(node);
    }
    node = next;
  }
  deque->deallocator(deque->nil);

#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(deque);
#endif

  deque->deallocator(deque);
}

void FUNC(deque_delete)(ds_deque_t *deque) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  deque->head = deque->tail = deque->nil;
  deque->size = 0;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

void FUNC(deque_delete_node)(ds_deque_t *deque, ds_deque_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  if (node == deque->head) {
    deque->head = node->next;
    if (deque->head != deque->nil) {
      deque->head->prev = deque->nil;
    } else {
      deque->tail = deque->nil;
    }
  } else if (node == deque->tail) {
    deque->tail = node->prev;
    if (deque->tail != deque->nil) {
      deque->tail->next = deque->nil;
    } else {
      deque->head = deque->nil;
    }
  } else {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  deque->size--;

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

void FUNC(deque_destroy_node)(ds_deque_t *deque, ds_deque_node_t *node,
                              void (*destroy)(ds_deque_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  FUNC(deque_delete_node)(deque, node);
  if (destroy != NULL) {
    destroy(node);
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

void FUNC(deque_pop_front_destroy)(ds_deque_t *deque,
                                   void (*destroy)(ds_deque_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  if (deque->head != deque->nil) {
    ds_deque_node_t *node = FUNC(deque_pop_front)(deque);
    if (destroy != NULL) {
      destroy(node);
    }
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

void FUNC(deque_pop_back_destroy)(ds_deque_t *deque,
                                  void (*destroy)(ds_deque_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  if (deque->tail != deque->nil) {
    ds_deque_node_t *node = FUNC(deque_pop_back)(deque);
    if (destroy != NULL) {
      destroy(node);
    }
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

ds_deque_node_t *FUNC(deque_search)(ds_deque_t *deque, ds_deque_node_t *node,
                                    int (*compare)(ds_deque_node_t *,
                                                   ds_deque_node_t *)) {
  ds_deque_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  current = deque->head;

  while (current != deque->nil) {
    if (compare(current, node) == 0) {
#ifdef DS_THREAD_SAFE
      UNLOCK(deque);
#endif
      return current;
    }
    current = current->next;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif

  return deque->nil;
}

void FUNC(deque_walk_forward)(ds_deque_t *deque, ds_deque_node_t *start_node,
                              void (*callback)(ds_deque_node_t *)) {
  ds_deque_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  current = (start_node != NULL) ? start_node : deque->head;

  while (current != deque->nil) {
    callback(current);
    current = current->next;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

void FUNC(deque_walk_backwards)(ds_deque_t *deque, ds_deque_node_t *current,
                                void (*callback)(ds_deque_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif

  while (current != deque->nil) {
    callback(current);
    current = current->prev;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif
}

ds_deque_t *
FUNC(deque_clone)(ds_deque_t *deque,
                  ds_deque_node_t *(*clone_node)(ds_deque_node_t *)) {
  ds_deque_t *new_deque;
  ds_deque_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(deque);
#endif
  new_deque = FUNC(deque_create_alloc)(deque->allocator, deque->deallocator);
  current = deque->head;
  while (current != deque->nil) {
    ds_deque_node_t *cloned_node = clone_node(current);
    FUNC(deque_push_back)(new_deque, cloned_node);
    current = current->next;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(deque);
#endif

  return new_deque;
}
