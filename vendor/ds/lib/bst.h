#ifndef DS_BST_H
#define DS_BST_H

#include "common.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bst_node {
  struct bst_node *left;
  struct bst_node *right;
  struct bst_node *parent;
} ds_bst_node_t;

typedef struct bst {
  ds_bst_node_t *root;
  ds_bst_node_t *nil;
  size_t size;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} ds_bst_t;

ds_bst_t *FUNC(bst_create)(void);
ds_bst_t *FUNC(bst_create_alloc)(void *(*allocator)(size_t),
                                 void (*deallocator)(void *));
void FUNC(bst_insert)(ds_bst_t *tree, ds_bst_node_t *node,
                      int (*compare)(ds_bst_node_t *, ds_bst_node_t *));
ds_bst_node_t *FUNC(bst_search)(ds_bst_t *tree, ds_bst_node_t *data,
                                int (*compare)(ds_bst_node_t *,
                                               ds_bst_node_t *));
ds_bst_node_t *FUNC(bst_minimum)(ds_bst_t *tree, ds_bst_node_t *node);
ds_bst_node_t *FUNC(bst_maximum)(ds_bst_t *tree, ds_bst_node_t *node);
void FUNC(bst_transplant)(ds_bst_t *tree, ds_bst_node_t *u, ds_bst_node_t *v);
void FUNC(bst_delete_node)(ds_bst_t *tree, ds_bst_node_t *node);
void FUNC(bst_destroy_node)(ds_bst_t *tree, ds_bst_node_t *node,
                            void (*destroy)(ds_bst_node_t *));
void FUNC(bst_destroy_recursive)(ds_bst_t *tree, ds_bst_node_t *root,
                                 void (*destroy)(ds_bst_node_t *));
void FUNC(bst_destroy_tree)(ds_bst_t *tree, void (*destroy)(ds_bst_node_t *));
void FUNC(bst_delete_tree)(ds_bst_t *tree);
void FUNC(bst_inorder_walk_helper)(ds_bst_t *tree, ds_bst_node_t *node,
                                   void (*callback)(void *));
void FUNC(bst_inorder_walk)(ds_bst_t *tree, ds_bst_node_t *node,
                            void (*callback)(void *));
void FUNC(bst_inorder_walk_tree)(ds_bst_t *tree, void (*callback)(void *));
void FUNC(bst_preorder_walk_helper)(ds_bst_t *tree, ds_bst_node_t *node,
                                    void (*callback)(void *));
void FUNC(bst_preorder_walk)(ds_bst_t *tree, ds_bst_node_t *node,
                             void (*callback)(void *));
void FUNC(bst_preorder_walk_tree)(ds_bst_t *tree, void (*callback)(void *));
void FUNC(bst_postorder_walk_helper)(ds_bst_t *tree, ds_bst_node_t *node,
                                     void (*callback)(void *));
void FUNC(bst_postorder_walk)(ds_bst_t *tree, ds_bst_node_t *node,
                              void (*callback)(void *));
void FUNC(bst_postorder_walk_tree)(ds_bst_t *tree, void (*callback)(void *));
ds_bst_node_t *FUNC(bst_successor)(ds_bst_t *tree, ds_bst_node_t *node_x);
ds_bst_node_t *FUNC(bst_predecessor)(ds_bst_t *tree, ds_bst_node_t *node_x);

ds_bst_node_t *
    FUNC(bst_clone_recursive)(ds_bst_t *tree, ds_bst_t *new_tree,
                              ds_bst_node_t *node,
                              ds_bst_node_t *(*clone_node)(ds_bst_node_t *));
ds_bst_t *FUNC(bst_clone)(ds_bst_t *tree,
                          ds_bst_node_t *(*clone_node)(ds_bst_node_t *));


#ifdef __cplusplus
}
#endif

#endif /* DS_BST_H */
