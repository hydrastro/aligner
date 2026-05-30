#ifndef DS_QUEUE_H
#define DS_QUEUE_H

#include "common.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct queue_node {
  struct queue_node *next;
} ds_queue_node_t;

typedef struct queue {
  ds_queue_node_t *head;
  ds_queue_node_t *tail;
  ds_queue_node_t *nil;
  size_t size;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} ds_queue_t;

ds_queue_t *FUNC(queue_create)(void);
ds_queue_t *FUNC(queue_create_alloc)(void *(*allocator)(size_t),
                                     void (*deallocator)(void *));
void FUNC(queue_enqueue)(ds_queue_t *queue, ds_queue_node_t *node);
ds_queue_node_t *FUNC(queue_dequeue)(ds_queue_t *queue);
ds_queue_node_t *FUNC(queue_peek)(ds_queue_t *queue);
ds_queue_node_t *FUNC(queue_peek_tail)(ds_queue_t *queue);
bool FUNC(queue_is_empty)(ds_queue_t *queue);
void FUNC(queue_destroy)(ds_queue_t *queue, void (*destroy)(ds_queue_node_t *));

void FUNC(queue_delete)(ds_queue_t *queue);
void FUNC(queue_delete_node)(ds_queue_t *queue, ds_queue_node_t *node);
void FUNC(queue_destroy_node)(ds_queue_t *queue, ds_queue_node_t *node,
                              void (*destroy)(ds_queue_node_t *));
void FUNC(queue_pop_destroy)(ds_queue_t *queue,
                             void (*destroy_node)(ds_queue_node_t *));
ds_queue_node_t *FUNC(queue_search)(ds_queue_t *queue, ds_queue_node_t *node,
                                    int (*compare)(ds_queue_node_t *,
                                                   ds_queue_node_t *));
void FUNC(queue_walk_forward)(ds_queue_t *queue, ds_queue_node_t *node,
                              void (*callback)(ds_queue_node_t *));
void FUNC(queue_walk_backwards)(ds_queue_t *queue, ds_queue_node_t *node,
                                void (*callback)(ds_queue_node_t *));
ds_queue_t *
    FUNC(queue_clone)(ds_queue_t *queue,
                      ds_queue_node_t *(*clone_node)(ds_queue_node_t *));


#ifdef __cplusplus
}
#endif

#endif /* DS_QUEUE_H */
