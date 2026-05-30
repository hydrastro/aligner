#ifndef DS_LIST_H
#define DS_LIST_H

#include "common.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct list_node {
  struct list_node *next;
} ds_list_node_t;

typedef struct linked_list {
  ds_list_node_t *head;
  ds_list_node_t *tail;
  ds_list_node_t *nil;
  size_t size;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} ds_list_t;

/* Backward compatibility for code written against earlier ds headers. */
typedef ds_list_node_t list_node_t;
typedef ds_list_t list_t;

ds_list_t *FUNC(list_create)(void);
ds_list_t *FUNC(list_create_alloc)(void *(*allocator)(size_t),
                                   void (*deallocator)(void *));
void FUNC(list_append)(ds_list_t *list, ds_list_node_t *node);
void FUNC(list_prepend)(ds_list_t *list, ds_list_node_t *node);
ds_list_node_t *FUNC(list_search)(ds_list_t *list, ds_list_node_t *node,
                                  int (*compare)(ds_list_node_t *,
                                                 ds_list_node_t *));
void FUNC(list_insert_before)(ds_list_t *list, ds_list_node_t *node,
                              ds_list_node_t *next);
void FUNC(list_insert_after)(ds_list_t *list, ds_list_node_t *node,
                             ds_list_node_t *prev);
void FUNC(list_delete_node)(ds_list_t *list, ds_list_node_t *node);
void FUNC(list_delete)(ds_list_t *list);
void FUNC(list_destroy_node)(ds_list_t *list, ds_list_node_t *node,
                             void (*destroy)(ds_list_node_t *));
void FUNC(list_destroy)(ds_list_t *list, void (*destroy)(ds_list_node_t *));
void FUNC(list_walk_forward)(ds_list_t *list, ds_list_node_t *node,
                             void (*callback)(ds_list_node_t *));
void FUNC(list_walk_backwards)(ds_list_t *list, ds_list_node_t *node,
                               void (*callback)(ds_list_node_t *));
bool FUNC(list_is_empty)(ds_list_t *list);
ds_list_t *FUNC(list_clone)(ds_list_t *list,
                            ds_list_node_t *(*clone_node)(ds_list_node_t *));


#ifdef __cplusplus
}
#endif

#endif /* DS_LIST_H */
