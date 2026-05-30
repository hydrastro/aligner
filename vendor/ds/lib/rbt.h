#ifndef DS_RBT_H
#define DS_RBT_H

#include "common.h"
#include <stddef.h>

#ifdef DS_THREAD_SAFE
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rbt_node {
  unsigned long parent_color;
  struct rbt_node *right;
  struct rbt_node *left;
} __attribute__((aligned(sizeof(long)))) ds_rbt_node_t;

#define RBT_RED 0
#define RBT_BLACK 1

#define RBT_GET_PARENT_FROM_COLOR(color)                                       \
  ((ds_rbt_node_t *)((color) & (long unsigned int)~3))
#define RBT_GET_COLOR_FROM_COLOR(color) ((color) & 1)
#define RBT_IS_RED_FROM_COLOR(color)                                           \
  (RBT_GET_COLOR_FROM_COLOR(color) == RBT_RED)
#define RBT_IS_BLACK_FROM_COLOR(color)                                         \
  (RBT_GET_COLOR_FROM_COLOR(color) == RBT_BLACK)
#define RBT_SET_RED_FROM_COLOR(color) ((color) &= (long unsigned int)~1)
#define RBT_SET_BLACK_FROM_COLOR(color) ((color) |= (long unsigned int)1)

#define RBT_GET_PARENT_FROM_NODE(node)                                         \
  (RBT_GET_PARENT_FROM_COLOR((node)->parent_color))
#define RBT_GET_COLOR_FROM_NODE(node)                                          \
  (RBT_GET_COLOR_FROM_COLOR((node)->parent_color))
#define RBT_IS_RED_FROM_NODE(node)                                             \
  (RBT_IS_RED_FROM_COLOR(RBT_GET_COLOR_FROM_NODE(node)))
#define RBT_IS_BLACK_FROM_NODE(node)                                           \
  (RBT_IS_BLACK_FROM_COLOR(RBT_GET_COLOR_FROM_NODE(node)))
#define RBT_SET_RED_FROM_NODE(node)                                            \
  (RBT_SET_RED_FROM_COLOR((node)->parent_color))
#define RBT_SET_BLACK_FROM_NODE(node)                                          \
  (RBT_SET_BLACK_FROM_COLOR((node)->parent_color))
#define RBT_SET_COLOR_FROM_NODE(node, color)                                   \
  ((node)->parent_color = (unsigned long)RBT_GET_PARENT_FROM_NODE(node) |      \
                          ((long unsigned int)color))

typedef struct rbt {
  ds_rbt_node_t *root;
  ds_rbt_node_t *nil;
  size_t size;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
#ifdef DS_THREAD_SAFE
  mutex_t lock;
  bool is_thread_safe;
#endif
} __attribute__((aligned(sizeof(long)))) ds_rbt_t;

void FUNC(rbt_set_parent)(ds_rbt_node_t *node, ds_rbt_node_t *parent);
void FUNC(rbt_set_parent_color)(ds_rbt_node_t *node, ds_rbt_node_t *parent,
                                int color);
ds_rbt_t *FUNC(rbt_create)(void);
ds_rbt_t *FUNC(rbt_create_alloc)(void *(*allocator)(size_t),
                                 void (*deallocator)(void *));
ds_rbt_node_t *FUNC(rbt_minimum)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
ds_rbt_node_t *FUNC(rbt_maximum)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
ds_rbt_node_t *FUNC(rbt_successor)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
ds_rbt_node_t *FUNC(rbt_predecessor)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
ds_rbt_node_t *
    FUNC(rbt_search)(ds_rbt_t *tree, ds_rbt_node_t *node_x, ds_rbt_node_t *data,
                     int (*compare)(ds_rbt_node_t *, ds_rbt_node_t *));

ds_rbt_node_t *FUNC(rbt_bigger_than)(ds_rbt_t *tree, ds_rbt_node_t *node_x,
                                     ds_rbt_node_t *key,
                                     int (*compare)(ds_rbt_node_t *,
                                                    ds_rbt_node_t *));
ds_rbt_node_t *FUNC(rbt_smaller_than)(ds_rbt_t *tree, ds_rbt_node_t *node_x,
                                      ds_rbt_node_t *key,
                                      int (*compare)(ds_rbt_node_t *,
                                                     ds_rbt_node_t *));
void FUNC(rbt_left_rotate)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
void FUNC(rbt_right_rotate)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
void FUNC(rbt_insert_fixup)(ds_rbt_t *tree, ds_rbt_node_t *node_z);
void FUNC(rbt_insert)(ds_rbt_t *tree, ds_rbt_node_t *node_z,
                      int (*compare)(ds_rbt_node_t *, ds_rbt_node_t *));
void FUNC(rbt_inorder_walk_helper)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                   void (*callback)(void *));
void FUNC(rbt_inorder_walk)(ds_rbt_t *tree, ds_rbt_node_t *node,
                            void (*callback)(void *));
void FUNC(rbt_inorder_walk_tree)(ds_rbt_t *tree, void (*callback)(void *));
void FUNC(rbt_preorder_walk_helper)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                    void (*callback)(void *));
void FUNC(rbt_preorder_walk)(ds_rbt_t *tree, ds_rbt_node_t *node,
                             void (*callback)(void *));
void FUNC(rbt_preorder_walk_tree)(ds_rbt_t *tree, void (*callback)(void *));
void FUNC(rbt_postorder_walk_helper)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                     void (*callback)(void *));
void FUNC(rbt_postorder_walk)(ds_rbt_t *tree, ds_rbt_node_t *node,
                              void (*callback)(void *));
void FUNC(rbt_postorder_walk_tree)(ds_rbt_t *tree, void (*callback)(void *));
void FUNC(rbt_transplant)(ds_rbt_t *tree, ds_rbt_node_t *node_u,
                          ds_rbt_node_t *node_v);
void FUNC(rbt_delete_fixup)(ds_rbt_t *tree, ds_rbt_node_t *node_x);
void FUNC(rbt_delete_node)(ds_rbt_t *tree, ds_rbt_node_t *node_z);
void FUNC(rbt_delete_tree)(ds_rbt_t *tree);
void FUNC(rbt_destroy_node)(ds_rbt_t *tree, ds_rbt_node_t *node,
                            void (*destroy)(ds_rbt_node_t *));
void FUNC(rbt_destroy_recursive)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                 void (*destroy)(ds_rbt_node_t *));
void FUNC(rbt_destroy_tree)(ds_rbt_t *tree, void (*destroy)(ds_rbt_node_t *));
ds_rbt_node_t *
    FUNC(rbt_clone_recursive)(ds_rbt_t *tree, ds_rbt_t *new_tree,
                              ds_rbt_node_t *node,
                              ds_rbt_node_t *(*clone_node)(ds_rbt_node_t *));
ds_rbt_t *FUNC(rbt_clone)(ds_rbt_t *tree,
                          ds_rbt_node_t *(*clone_node)(ds_rbt_node_t *));


#ifdef __cplusplus
}
#endif

#endif /* DS_RBT_H */
