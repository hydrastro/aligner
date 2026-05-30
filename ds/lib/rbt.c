#include "rbt.h"
#include <stdio.h>
#include <stdlib.h>

void FUNC(rbt_set_parent)(ds_rbt_node_t *node, ds_rbt_node_t *parent) {
  node->parent_color = RBT_GET_COLOR_FROM_NODE(node) + (unsigned long)parent;
}

void FUNC(rbt_set_parent_color)(ds_rbt_node_t *node, ds_rbt_node_t *parent,
                                int color) {
  node->parent_color = (unsigned long)parent + (long unsigned int)color;
}

ds_rbt_t *FUNC(rbt_create)(void) {
  return FUNC(rbt_create_alloc)(malloc, free);
}

ds_rbt_t *FUNC(rbt_create_alloc)(void *(*allocator)(size_t),
                                 void (*deallocator)(void *)) {
  ds_rbt_t *tree;
  tree = (ds_rbt_t *)allocator(sizeof(ds_rbt_t));
  tree->allocator = allocator;
  tree->deallocator = deallocator;
  tree->nil = (ds_rbt_node_t *)allocator(sizeof(ds_rbt_node_t));
  tree->nil->parent_color = RBT_BLACK;
  tree->root = tree->nil;
  FUNC(rbt_set_parent_color)(tree->root, tree->nil, RBT_BLACK);
  tree->root->left = tree->nil;
  tree->root->right = tree->nil;
  tree->size = 0;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(tree);
#endif

  return tree;
}

ds_rbt_node_t *FUNC(rbt_minimum)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node_x->left != tree->nil) {
    node_x = node_x->left;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_x;
}

ds_rbt_node_t *FUNC(rbt_maximum)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node_x->right != tree->nil) {
    node_x = node_x->right;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_x;
}

ds_rbt_node_t *FUNC(rbt_successor)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
  ds_rbt_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node_x->right != tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return FUNC(rbt_minimum)(tree, node_x->right);
  }
  node_y = RBT_GET_PARENT_FROM_NODE(node_x);
  while (node_y != tree->nil && node_x == node_y->right) {
    node_x = node_y;
    node_y = RBT_GET_PARENT_FROM_NODE(node_y);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_y;
}

ds_rbt_node_t *FUNC(rbt_predecessor)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
  ds_rbt_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node_x->left != tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return FUNC(rbt_maximum)(tree, node_x->left);
  }
  node_y = RBT_GET_PARENT_FROM_NODE(node_x);
  while (node_y != tree->nil && node_x == node_y->left) {
    node_x = node_y;
    node_y = RBT_GET_PARENT_FROM_NODE(node_y);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_y;
}

ds_rbt_node_t *
FUNC(rbt_search)(ds_rbt_t *tree, ds_rbt_node_t *node_x, ds_rbt_node_t *data,
                 int (*compare)(ds_rbt_node_t *, ds_rbt_node_t *)) {
  int cmp;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node_x != tree->nil) {
    cmp = compare(data, node_x);
    if (cmp == 0) {
#ifdef DS_THREAD_SAFE
      UNLOCK(tree);
#endif
      return node_x;
    } else if (cmp < 0) {
      node_x = node_x->left;
    } else {
      node_x = node_x->right;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return tree->nil;
}

ds_rbt_node_t *
FUNC(rbt_bigger_than)(ds_rbt_t *tree, ds_rbt_node_t *node_x, ds_rbt_node_t *key,
                      int (*compare)(ds_rbt_node_t *, ds_rbt_node_t *)) {
  int cmp;
  ds_rbt_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node_y = tree->nil;
  while (node_x != tree->nil) {
    cmp = compare(key, node_x);
    if (cmp < 0) {
      node_y = node_x;
      node_x = node_x->left;
    } else {
      node_x = node_x->right;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_y;
}

ds_rbt_node_t *FUNC(rbt_smaller_than)(ds_rbt_t *tree, ds_rbt_node_t *node_x,
                                      ds_rbt_node_t *key,
                                      int (*compare)(ds_rbt_node_t *,
                                                     ds_rbt_node_t *)) {
  int cmp;
  ds_rbt_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node_y = tree->nil;
  while (node_x != tree->nil) {
    cmp = compare(key, node_x);
    if (cmp > 0) {
      node_y = node_x;
      node_x = node_x->right;
    } else {
      node_x = node_x->left;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_y;
}

void FUNC(rbt_left_rotate)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
  ds_rbt_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node_y = node_x->right;
  node_x->right = node_y->left;
  if (node_y->left != tree->nil) {
    FUNC(rbt_set_parent)(node_y->left, node_x);
  }
  FUNC(rbt_set_parent)(node_y, RBT_GET_PARENT_FROM_NODE(node_x));
  if (RBT_GET_PARENT_FROM_NODE(node_x) == tree->nil) {
    tree->root = node_y;
  } else if (node_x == RBT_GET_PARENT_FROM_NODE(node_x)->left) {
    RBT_GET_PARENT_FROM_NODE(node_x)->left = node_y;
  } else {
    RBT_GET_PARENT_FROM_NODE(node_x)->right = node_y;
  }
  node_y->left = node_x;
  FUNC(rbt_set_parent)(node_x, node_y);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_right_rotate)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
  ds_rbt_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node_y = node_x->left;
  node_x->left = node_y->right;
  if (node_y->right != tree->nil) {
    FUNC(rbt_set_parent)(node_y->right, node_x);
  }
  FUNC(rbt_set_parent)(node_y, RBT_GET_PARENT_FROM_NODE(node_x));
  if (RBT_GET_PARENT_FROM_NODE(node_x) == tree->nil) {
    tree->root = node_y;
  } else if (node_x == RBT_GET_PARENT_FROM_NODE(node_x)->right) {
    RBT_GET_PARENT_FROM_NODE(node_x)->right = node_y;
  } else {
    RBT_GET_PARENT_FROM_NODE(node_x)->left = node_y;
  }
  node_y->right = node_x;
  FUNC(rbt_set_parent)(node_x, node_y);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_insert_fixup)(ds_rbt_t *tree, ds_rbt_node_t *node_z) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (RBT_IS_RED_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z))) {

    if (RBT_GET_PARENT_FROM_NODE(node_z) ==
        RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z))->left) {
      ds_rbt_node_t *node_y;
      node_y =
          RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z))->right;
      if (RBT_IS_RED_FROM_NODE(node_y)

      ) {
        RBT_SET_BLACK_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z));
        RBT_SET_BLACK_FROM_NODE(node_y);
        RBT_SET_RED_FROM_NODE(
            RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z)));
        node_z = RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z));
      } else {
        if (node_z == RBT_GET_PARENT_FROM_NODE(node_z)->right) {
          node_z = RBT_GET_PARENT_FROM_NODE(node_z);
          FUNC(rbt_left_rotate)(tree, node_z);
        }
        RBT_SET_BLACK_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z));
        RBT_SET_RED_FROM_NODE(
            RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z)));
        FUNC(rbt_right_rotate)
        (tree, RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z)));
      }
    } else {
      ds_rbt_node_t *node_y;
      node_y = RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z))->left;
      if (RBT_IS_RED_FROM_NODE(node_y)) {
        RBT_SET_BLACK_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z));
        RBT_SET_BLACK_FROM_NODE(node_y);
        RBT_SET_RED_FROM_NODE(
            RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z)));
        node_z = RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z));
      } else {
        if (node_z == RBT_GET_PARENT_FROM_NODE(node_z)->left) {
          node_z = RBT_GET_PARENT_FROM_NODE(node_z);
          FUNC(rbt_right_rotate)(tree, node_z);
        }
        RBT_SET_BLACK_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z));
        RBT_SET_RED_FROM_NODE(
            RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z)));
        FUNC(rbt_left_rotate)
        (tree, RBT_GET_PARENT_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_z)));
      }
    }
  }
  RBT_SET_BLACK_FROM_NODE(tree->root);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_insert)(ds_rbt_t *tree, ds_rbt_node_t *node_z,
                      int (*compare)(ds_rbt_node_t *, ds_rbt_node_t *)) {
  ds_rbt_node_t *node_y;
  ds_rbt_node_t *node_x;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node_y = tree->nil;
  node_x = tree->root;
  while (node_x != tree->nil) {
    node_y = node_x;
    if (compare(node_z, node_x) < 0) {
      node_x = node_x->left;
    } else {
      node_x = node_x->right;
    }
  }
  FUNC(rbt_set_parent)(node_z, node_y);
  if (node_y == tree->nil) {
    tree->root = node_z;
  } else if (compare(node_z, node_y) < 0) {
    node_y->left = node_z;
  } else {
    node_y->right = node_z;
  }
  node_z->left = tree->nil;
  node_z->right = tree->nil;
  RBT_SET_RED_FROM_NODE(node_z);
  FUNC(rbt_insert_fixup)(tree, node_z);
  tree->size += 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_inorder_walk_helper)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                   void (*callback)(void *)) {
  if (node != tree->nil) {
    FUNC(rbt_inorder_walk_helper)(tree, node->left, callback);
    callback(node);
    FUNC(rbt_inorder_walk_helper)(tree, node->right, callback);
  }
}

void FUNC(rbt_inorder_walk)(ds_rbt_t *tree, ds_rbt_node_t *node,
                            void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_inorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_inorder_walk_tree)(ds_rbt_t *tree, void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_inorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_preorder_walk_helper)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                    void (*callback)(void *)) {
  if (node != tree->nil) {
    callback(node);
    FUNC(rbt_preorder_walk_helper)(tree, node->left, callback);
    FUNC(rbt_preorder_walk_helper)(tree, node->right, callback);
  }
}

void FUNC(rbt_preorder_walk)(ds_rbt_t *tree, ds_rbt_node_t *node,
                             void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_preorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_preorder_walk_tree)(ds_rbt_t *tree, void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_preorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_postorder_walk_helper)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                     void (*callback)(void *)) {
  if (node != tree->nil) {
    FUNC(rbt_postorder_walk_helper)(tree, node->left, callback);
    FUNC(rbt_postorder_walk_helper)(tree, node->right, callback);
    callback(node);
  }
}

void FUNC(rbt_postorder_walk)(ds_rbt_t *tree, ds_rbt_node_t *node,
                              void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_postorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_postorder_walk_tree)(ds_rbt_t *tree, void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_postorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_transplant)(ds_rbt_t *tree, ds_rbt_node_t *node_u,
                          ds_rbt_node_t *node_v) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (RBT_GET_PARENT_FROM_NODE(node_u) == tree->nil) {
    tree->root = node_v;
  } else if (node_u == RBT_GET_PARENT_FROM_NODE(node_u)->left) {
    RBT_GET_PARENT_FROM_NODE(node_u)->left = node_v;
  } else {
    RBT_GET_PARENT_FROM_NODE(node_u)->right = node_v;
  }
  FUNC(rbt_set_parent)(node_v, RBT_GET_PARENT_FROM_NODE(node_u));
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_delete_fixup)(ds_rbt_t *tree, ds_rbt_node_t *node_x) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node_x != tree->root && RBT_IS_BLACK_FROM_NODE(node_x)) {
    if (node_x == RBT_GET_PARENT_FROM_NODE(node_x)->left) {
      ds_rbt_node_t *node_w;
      node_w = RBT_GET_PARENT_FROM_NODE(node_x)->right;
      if (RBT_IS_RED_FROM_NODE(node_w)) {
        RBT_SET_BLACK_FROM_NODE(node_w);
        RBT_SET_RED_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_x));
        FUNC(rbt_left_rotate)(tree, RBT_GET_PARENT_FROM_NODE(node_x));
        node_w = RBT_GET_PARENT_FROM_NODE(node_x)->right;
      }
      if (RBT_IS_BLACK_FROM_NODE(node_w->left) &&
          RBT_IS_BLACK_FROM_NODE(node_w->right)) {
        RBT_SET_RED_FROM_NODE(node_w);
        node_x = RBT_GET_PARENT_FROM_NODE(node_x);
      } else {
        if (RBT_IS_BLACK_FROM_NODE(node_w->right)) {
          RBT_SET_BLACK_FROM_NODE(node_w->left);
          RBT_SET_RED_FROM_NODE(node_w);
          FUNC(rbt_right_rotate)(tree, node_w);
          node_w = RBT_GET_PARENT_FROM_NODE(node_x)->right;
        }
        RBT_SET_COLOR_FROM_NODE(
            node_w, RBT_GET_COLOR_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_x)));
        RBT_SET_BLACK_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_x));
        RBT_SET_BLACK_FROM_NODE(node_w->right);
        FUNC(rbt_left_rotate)(tree, RBT_GET_PARENT_FROM_NODE(node_x));
        node_x = tree->root;
      }
    } else {
      ds_rbt_node_t *node_w;
      node_w = RBT_GET_PARENT_FROM_NODE(node_x)->left;
      if (RBT_IS_RED_FROM_NODE(node_w)) {
        RBT_SET_BLACK_FROM_NODE(node_w);
        RBT_SET_RED_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_x));
        FUNC(rbt_right_rotate)(tree, RBT_GET_PARENT_FROM_NODE(node_x));
        node_w = RBT_GET_PARENT_FROM_NODE(node_x)->left;
      }
      if (RBT_IS_BLACK_FROM_NODE(node_w->right) &&
          RBT_IS_BLACK_FROM_NODE(node_w->left)) {
        RBT_SET_RED_FROM_NODE(node_w);
        node_x = RBT_GET_PARENT_FROM_NODE(node_x);
      } else {
        if (RBT_IS_BLACK_FROM_NODE(node_w->left)) {
          RBT_SET_BLACK_FROM_NODE(node_w->right);
          RBT_SET_RED_FROM_NODE(node_w);
          FUNC(rbt_left_rotate)(tree, node_w);
          node_w = RBT_GET_PARENT_FROM_NODE(node_x)->left;
        }
        RBT_SET_COLOR_FROM_NODE(
            node_w, RBT_GET_COLOR_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_x)));
        RBT_SET_BLACK_FROM_NODE(RBT_GET_PARENT_FROM_NODE(node_x));
        RBT_SET_BLACK_FROM_NODE(node_w->left);
        FUNC(rbt_right_rotate)(tree, RBT_GET_PARENT_FROM_NODE(node_x));
        node_x = tree->root;
      }
    }
  }
  RBT_SET_BLACK_FROM_NODE(node_x);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_delete_node)(ds_rbt_t *tree, ds_rbt_node_t *node_z) {
  ds_rbt_node_t *node_y;
  ds_rbt_node_t *node_x;
  unsigned int color;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node_y = node_z;
  color = RBT_GET_COLOR_FROM_NODE(node_y);
  if (node_z->left == tree->nil) {
    node_x = node_z->right;
    FUNC(rbt_transplant)(tree, node_z, node_z->right);
  } else if (node_z->right == tree->nil) {
    node_x = node_z->left;
    FUNC(rbt_transplant)(tree, node_z, node_z->left);
  } else {
    node_y = FUNC(rbt_minimum)(tree, node_z->right);
    color = RBT_GET_COLOR_FROM_NODE(node_y);
    node_x = node_y->right;
    if (RBT_GET_PARENT_FROM_NODE(node_y) == node_z) {
      FUNC(rbt_set_parent)(node_x, node_y);
    } else {
      FUNC(rbt_transplant)(tree, node_y, node_y->right);
      node_y->right = node_z->right;
      FUNC(rbt_set_parent)(node_y->right, node_y);
    }
    FUNC(rbt_transplant)(tree, node_z, node_y);
    node_y->left = node_z->left;
    FUNC(rbt_set_parent)(node_y->left, node_y);
    RBT_SET_COLOR_FROM_NODE(node_y, RBT_GET_COLOR_FROM_NODE(node_z));
  }
  if (color == RBT_BLACK) {
    FUNC(rbt_delete_fixup)(tree, node_x);
  }
  tree->size -= 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_delete_tree)(ds_rbt_t *tree) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  tree->root = tree->nil;
  tree->size = 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_destroy_node)(ds_rbt_t *tree, ds_rbt_node_t *node,
                            void (*destroy)(ds_rbt_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(rbt_delete_node)(tree, node);
  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_destroy_recursive)(ds_rbt_t *tree, ds_rbt_node_t *node,
                                 void (*destroy)(ds_rbt_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node == tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return;
  }
  FUNC(rbt_destroy_recursive)(tree, node->left, destroy);
  FUNC(rbt_destroy_recursive)(tree, node->right, destroy);
  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(rbt_destroy_tree)(ds_rbt_t *tree, void (*destroy)(ds_rbt_node_t *)) {
  FUNC(rbt_destroy_recursive)(tree, tree->root, destroy);
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(tree);
#endif
  tree->deallocator(tree->nil);
  tree->deallocator(tree);
}

ds_rbt_node_t *
FUNC(rbt_clone_recursive)(ds_rbt_t *tree, ds_rbt_t *new_tree,
                          ds_rbt_node_t *node,
                          ds_rbt_node_t *(*clone_node)(ds_rbt_node_t *)) {
  ds_rbt_node_t *new_node;
  if (node == tree->nil) {
    return new_tree->nil;
  }

  new_node = clone_node(node);
  new_node->left =
      FUNC(rbt_clone_recursive)(tree, new_tree, node->left, clone_node);
  new_node->right =
      FUNC(rbt_clone_recursive)(tree, new_tree, node->right, clone_node);

  return new_node;
}

ds_rbt_t *FUNC(rbt_clone)(ds_rbt_t *tree,
                          ds_rbt_node_t *(*clone_node)(ds_rbt_node_t *)) {
  ds_rbt_t *new_tree;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  new_tree = FUNC(rbt_create_alloc)(tree->allocator, tree->deallocator);
  new_tree->root =
      FUNC(rbt_clone_recursive)(tree, new_tree, tree->root, clone_node);
  new_tree->size = tree->size;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return new_tree;
}
