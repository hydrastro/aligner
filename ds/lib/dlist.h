#ifndef DS_DLIST_H
#define DS_DLIST_H

#include "common.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dlist_node {
  struct dlist_node *next;
  struct dlist_node *prev;
} ds_dlist_node_t;

typedef struct dlist {
  size_t size;
  ds_dlist_node_t *head;
  ds_dlist_node_t *tail;
  ds_dlist_node_t *nil;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} ds_dlist_t;

ds_dlist_t *FUNC(dlist_create)(void);
ds_dlist_t *FUNC(dlist_create_alloc)(void *(*allocator)(size_t),
                                     void (*deallocator)(void *));
void FUNC(dlist_append)(ds_dlist_t *list, ds_dlist_node_t *node);

void FUNC(dlist_prepend)(ds_dlist_t *list, ds_dlist_node_t *node);
ds_dlist_node_t *FUNC(dlist_search)(ds_dlist_t *list, ds_dlist_node_t *node,
                                    int (*compare)(ds_dlist_node_t *,
                                                   ds_dlist_node_t *));
void FUNC(dlist_insert_before)(ds_dlist_t *list, ds_dlist_node_t *node,
                               ds_dlist_node_t *next);
void FUNC(dlist_insert_after)(ds_dlist_t *list, ds_dlist_node_t *node,
                              ds_dlist_node_t *prev);
void FUNC(dlist_delete_node)(ds_dlist_t *list, ds_dlist_node_t *node);
void FUNC(dlist_delete)(ds_dlist_t *list);
void FUNC(dlist_destroy_node)(ds_dlist_t *list, ds_dlist_node_t *node,
                              void (*destroy)(ds_dlist_node_t *));
void FUNC(dlist_destroy)(ds_dlist_t *list, void (*destroy)(ds_dlist_node_t *));
void FUNC(dlist_walk_forward)(ds_dlist_t *list, ds_dlist_node_t *node,
                              void (*callback)(ds_dlist_node_t *));
void FUNC(dlist_walk_backwards)(ds_dlist_t *list, ds_dlist_node_t *node,
                                void (*callback)(ds_dlist_node_t *));
bool FUNC(dlist_is_empty)(ds_dlist_t *);
ds_dlist_t *
    FUNC(dlist_clone)(ds_dlist_t *list,
                      ds_dlist_node_t *(*clone_node)(ds_dlist_node_t *));


#ifdef __cplusplus
}
#endif

#endif /* DS_DLIST_H */
