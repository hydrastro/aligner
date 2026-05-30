#include "queue.h"
#include <stdlib.h>

ds_queue_t *FUNC(queue_create)(void) {
  return FUNC(queue_create_alloc)(malloc, free);
}

ds_queue_t *FUNC(queue_create_alloc)(void *(*allocator)(size_t),
                                     void (*deallocator)(void *)) {
  ds_queue_t *queue = (ds_queue_t *)allocator(sizeof(ds_queue_t));
  queue->allocator = allocator;
  queue->deallocator = deallocator;
  queue->nil = (ds_queue_node_t *)allocator(sizeof(ds_queue_node_t));
  queue->nil->next = queue->nil;
  queue->head = queue->nil;
  queue->tail = queue->nil;
  queue->size = 0;

#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(queue);
#endif

  return queue;
}

void FUNC(queue_enqueue)(ds_queue_t *queue, ds_queue_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  node->next = queue->nil;
  if (queue->head == queue->nil) {
    queue->head = node;
    queue->tail = node;
  } else {
    queue->tail->next = node;
    queue->tail = node;
  }
  queue->size += 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

ds_queue_node_t *FUNC(queue_dequeue)(ds_queue_t *queue) {
  ds_queue_node_t *node;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  if (queue->head == queue->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(queue);
#endif
    return queue->nil;
  }
  node = queue->head;
  queue->head = node->next;
  if (queue->head == queue->nil) {
    queue->tail = queue->nil;
  }
  queue->size -= 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif

  return node;
}

ds_queue_node_t *FUNC(queue_peek)(ds_queue_t *queue) {
  ds_queue_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif
  result = queue->head;

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
  return result;
}

ds_queue_node_t *FUNC(queue_peek_tail)(ds_queue_t *queue) {
  ds_queue_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif
  result = queue->tail;
#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
  return result;
}

bool FUNC(queue_is_empty)(ds_queue_t *queue) {
  bool empty;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  empty = queue->head == queue->nil;

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif

  return empty;
}

void FUNC(queue_destroy)(ds_queue_t *queue,
                         void (*destroy)(ds_queue_node_t *)) {
  ds_queue_node_t *node = queue->head;
  while (node != queue->nil) {
    ds_queue_node_t *next = node->next;
    if (destroy != NULL) {
      destroy(node);
    }
    node = next;
  }
  queue->deallocator(queue->nil);

#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(queue);
#endif

  queue->deallocator(queue);
}

void FUNC(queue_delete)(ds_queue_t *queue) {
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif
  queue->head = queue->tail = queue->nil;
  queue->size = 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

void FUNC(queue_delete_node)(ds_queue_t *queue, ds_queue_node_t *node) {
  bool removed;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  removed = false;
  if (queue->head == node) {
    queue->head = node->next;

    if (queue->head == queue->nil) {
      queue->tail = queue->nil;
    }
    removed = true;
  } else {
    ds_queue_node_t *prev = queue->head;
    while (prev->next != node && prev->next != queue->nil) {
      prev = prev->next;
    }

    if (prev->next == node) {
      prev->next = node->next;

      if (node == queue->tail) {
        queue->tail = prev;
      }
      removed = true;
    }
  }

  if (removed) {
    queue->size--;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

void FUNC(queue_destroy_node)(ds_queue_t *queue, ds_queue_node_t *node,
                              void (*destroy)(ds_queue_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  FUNC(queue_delete_node)(queue, node);
  if (destroy != NULL) {
    destroy(node);
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

void FUNC(queue_pop_destroy)(ds_queue_t *queue,
                             void (*destroy)(ds_queue_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  if (queue->head != queue->nil) {
    ds_queue_node_t *node = FUNC(queue_dequeue)(queue);
    if (destroy != NULL) {
      destroy(node);
    }
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

ds_queue_node_t *FUNC(queue_search)(ds_queue_t *queue, ds_queue_node_t *node,
                                    int (*compare)(ds_queue_node_t *,
                                                   ds_queue_node_t *)) {
  ds_queue_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  current = queue->head;

  while (current != queue->nil) {
    if (compare(current, node) == 0) {
#ifdef DS_THREAD_SAFE
      UNLOCK(queue);
#endif
      return current;
    }
    current = current->next;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif

  return queue->nil;
}

void FUNC(queue_walk_forward)(ds_queue_t *queue, ds_queue_node_t *start_node,
                              void (*callback)(ds_queue_node_t *)) {
  ds_queue_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif

  current = start_node != NULL ? start_node : queue->head;

  while (current != queue->nil) {
    callback(current);
    current = current->next;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

void FUNC(queue_walk_backwards)(ds_queue_t *queue, ds_queue_node_t *current,
                                void (*callback)(ds_queue_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif
  if (current == queue->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(queue);
#endif
    return;
  }
  FUNC(queue_walk_backwards)(queue, current->next, callback);
  callback(current);
#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif
}

ds_queue_t *
FUNC(queue_clone)(ds_queue_t *queue,
                  ds_queue_node_t *(*clone_node)(ds_queue_node_t *)) {
  ds_queue_t *new_queue;
  ds_queue_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(queue);
#endif
  new_queue = FUNC(queue_create_alloc)(queue->allocator, queue->deallocator);
  current = queue->head;
  while (current != queue->nil) {
    ds_queue_node_t *cloned_node = clone_node(current);
    FUNC(queue_enqueue)(new_queue, cloned_node);
    current = current->next;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(queue);
#endif

  return new_queue;
}
