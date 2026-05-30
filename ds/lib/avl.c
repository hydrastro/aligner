#include "avl.h"
#include <stdio.h>
#include <stdlib.h>

ds_avl_t *FUNC(avl_create)(void) {
  return FUNC(avl_create_alloc)(malloc, free);
}

ds_avl_t *FUNC(avl_create_alloc)(void *(*allocator)(size_t),
                                 void (*deallocator)(void *)) {
  ds_avl_t *tree = (ds_avl_t *)allocator(sizeof(ds_avl_t));
  tree->allocator = allocator;
  tree->deallocator = deallocator;
  tree->nil = (ds_avl_node_t *)allocator(sizeof(ds_avl_node_t));
  tree->nil->height = 0;
  tree->nil->left = tree->nil;
  tree->nil->right = tree->nil;
  tree->nil->parent = tree->nil;
  tree->root = tree->nil;
  tree->root->left = tree->nil;
  tree->root->right = tree->nil;
  tree->root->parent = tree->nil;
  tree->size = 0;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(tree);
#endif
  return tree;
}

void FUNC(avl_inorder_walk_helper)(ds_avl_t *tree, ds_avl_node_t *node,
                                   void (*callback)(ds_avl_node_t *)) {
  if (node != tree->nil) {
    FUNC(avl_inorder_walk)(tree, node->left, callback);
    callback(node);
    FUNC(avl_inorder_walk)(tree, node->right, callback);
  }
}

void FUNC(avl_inorder_walk)(ds_avl_t *tree, ds_avl_node_t *node,
                            void (*callback)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_inorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_inorder_walk_tree)(ds_avl_t *tree,
                                 void (*callback)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_inorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}
void FUNC(avl_preorder_walk_helper)(ds_avl_t *tree, ds_avl_node_t *node,
                                    void (*callback)(ds_avl_node_t *)) {
  if (node != tree->nil) {
    callback(node);
    FUNC(avl_preorder_walk)(tree, node->left, callback);
    FUNC(avl_preorder_walk)(tree, node->right, callback);
  }
}
void FUNC(avl_preorder_walk)(ds_avl_t *tree, ds_avl_node_t *node,
                             void (*callback)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_preorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_preorder_walk_tree)(ds_avl_t *tree,
                                  void (*callback)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_preorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_postorder_walk_helper)(ds_avl_t *tree, ds_avl_node_t *node,
                                     void (*callback)(ds_avl_node_t *)) {
  if (node != tree->nil) {
    FUNC(avl_postorder_walk)(tree, node->left, callback);
    FUNC(avl_postorder_walk)(tree, node->right, callback);
    callback(node);
  }
}

void FUNC(avl_postorder_walk)(ds_avl_t *tree, ds_avl_node_t *node,
                              void (*callback)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_postorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_postorder_walk_tree)(ds_avl_t *tree,
                                   void (*callback)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_postorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_avl_node_t *FUNC(avl_search)(ds_avl_t *tree, ds_avl_node_t *data,
                                int (*compare)(ds_avl_node_t *,
                                               ds_avl_node_t *)) {
  ds_avl_node_t *node;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node = tree->root;
  while (node != tree->nil) {
    int cmp = compare(data, node);
    if (cmp == 0) {
#ifdef DS_THREAD_SAFE
      UNLOCK(tree);
#endif
      return node;
    } else if (cmp < 0) {
      node = node->left;
    } else {
      node = node->right;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return tree->nil;
}

void FUNC(avl_destroy_node)(ds_avl_t *tree, ds_avl_node_t *node,
                            void (*destroy)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_delete_node)(tree, node);
  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_destroy_recursive)(ds_avl_t *tree, ds_avl_node_t *node,
                                 void (*destroy)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node != tree->nil) {
    FUNC(avl_destroy_recursive)(tree, node->left, destroy);
    FUNC(avl_destroy_recursive)(tree, node->right, destroy);
    tree->size -= 1;
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_destroy_tree)(ds_avl_t *tree, void (*destroy)(ds_avl_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_destroy_recursive)(tree, tree->root, destroy);
  tree->root = tree->nil;
  tree->deallocator(tree->nil);
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(tree);
#endif
  tree->deallocator(tree);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int FUNC(avl_get_height)(ds_avl_t *tree, ds_avl_node_t *node) {
  int result;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  result = node ? node->height : 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return result;
}
#pragma GCC diagnostic pop

int FUNC(avl_get_balance)(ds_avl_t *tree, ds_avl_node_t *node) {
  int result;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  result = node ? FUNC(avl_get_height)(tree, node->left) -
                      FUNC(avl_get_height)(tree, node->right)
                : 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return result;
}

void FUNC(avl_update_height)(ds_avl_t *tree, ds_avl_node_t *node) {
  int left_height, right_height;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  left_height = FUNC(avl_get_height)(tree, node->left);
  right_height = FUNC(avl_get_height)(tree, node->right);
  node->height = 1 + (left_height > right_height ? left_height : right_height);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_avl_node_t *FUNC(avl_left_rotate)(ds_avl_t *tree, ds_avl_node_t *node) {
  ds_avl_node_t *right;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  right = node->right;
  node->right = right->left;
  if (right->left != tree->nil) {
    right->left->parent = node;
  }
  right->parent = node->parent;
  right->left = node;
  node->parent = right;
  FUNC(avl_update_height)(tree, node);
  FUNC(avl_update_height)(tree, right);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return right;
}

ds_avl_node_t *FUNC(avl_right_rotate)(ds_avl_t *tree, ds_avl_node_t *node) {
  ds_avl_node_t *left;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  left = node->left;
  node->left = left->right;
  if (left->right != tree->nil) {
    left->right->parent = node;
  }
  left->parent = node->parent;
  left->right = node;
  node->parent = left;
  FUNC(avl_update_height)(tree, node);
  FUNC(avl_update_height)(tree, left);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return left;
}

ds_avl_node_t *FUNC(avl_balance)(ds_avl_t *tree, ds_avl_node_t *node) {
  int balance;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(avl_update_height)(tree, node);
  balance = FUNC(avl_get_balance)(tree, node);

  if (balance > 1) {
    if (FUNC(avl_get_balance)(tree, node->left) < 0) {
      node->left = FUNC(avl_left_rotate)(tree, node->left);
    }
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return FUNC(avl_right_rotate)(tree, node);
  } else if (balance < -1) {
    if (FUNC(avl_get_balance)(tree, node->right) > 0) {
      node->right = FUNC(avl_right_rotate)(tree, node->right);
    }
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return FUNC(avl_left_rotate)(tree, node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node;
}

void FUNC(avl_insert)(ds_avl_t *tree, ds_avl_node_t *data,
                      int (*compare)(ds_avl_node_t *, ds_avl_node_t *)) {
  int cmp;
  ds_avl_node_t *n, *current, *parent;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  current = tree->root;
  parent = tree->nil;

  while (current != tree->nil) {
    parent = current;

    cmp = compare(data, current);
    if (cmp < 0) {
      current = current->left;
    } else if (cmp > 0) {
      current = current->right;
    } else {
#ifdef DS_THREAD_SAFE
      UNLOCK(tree);
#endif
      return;
    }
  }

  data->parent = parent;
  data->left = tree->nil;
  data->right = tree->nil;
  data->height = 1;

  if (parent == tree->nil) {
    tree->root = data;
  } else if (compare(data, parent) < 0) {
    parent->left = data;
  } else {
    parent->right = data;
  }
  for (n = parent; n != tree->nil;) {
    ds_avl_node_t *old_parent;
    bool was_left;
    old_parent = n->parent;
    was_left = old_parent != tree->nil && n == old_parent->left;
    n = FUNC(avl_balance)(tree, n);
    if (old_parent == tree->nil) {
      tree->root = n;
    } else if (was_left) {
      old_parent->left = n;
    } else {
      old_parent->right = n;
    }
    n->parent = old_parent;
    n = old_parent;
  }
  tree->size += 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_avl_node_t *FUNC(avl_min_node)(ds_avl_t *tree, ds_avl_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node->left != tree->nil) {
    node = node->left;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node;
}

void FUNC(avl_transplant)(ds_avl_t *tree, ds_avl_node_t *u, ds_avl_node_t *v) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (u->parent == tree->nil) {
    tree->root = v;
  } else if (u == u->parent->left) {
    u->parent->left = v;
  } else {
    u->parent->right = v;
  }
  if (v != tree->nil) {
    v->parent = u->parent;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_delete_node)(ds_avl_t *tree, ds_avl_node_t *data) {
  ds_avl_node_t *x, *y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  y = data;
  if (data->left == tree->nil) {
    x = data->right;
    FUNC(avl_transplant)(tree, data, data->right);
  } else if (data->right == tree->nil) {
    x = data->left;
    FUNC(avl_transplant)(tree, data, data->left);
  } else {
    y = FUNC(avl_min_node)(tree, data->right);
    if (y->parent != data) {
      FUNC(avl_transplant)(tree, y, y->right);
      y->right = data->right;
      y->right->parent = y;
    }
    FUNC(avl_transplant)(tree, data, y);
    y->left = data->left;
    y->left->parent = y;
    x = y;
  }

  while (x != tree->nil) {
    x = FUNC(avl_balance)(tree, x);
    x = x->parent;
  }
  tree->size -= 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(avl_delete_tree)(ds_avl_t *tree) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  tree->root = tree->nil;
  tree->size = 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_avl_node_t *
FUNC(avl_clone_recursive)(ds_avl_t *tree, ds_avl_t *new_tree,
                          ds_avl_node_t *node,
                          ds_avl_node_t *(*clone_node)(ds_avl_node_t *)) {
  ds_avl_node_t *new_node;
  if (node == tree->nil) {
    return new_tree->nil;
  }

  new_node = clone_node(node);
  new_node->left =
      FUNC(avl_clone_recursive)(tree, new_tree, node->left, clone_node);
  new_node->right =
      FUNC(avl_clone_recursive)(tree, new_tree, node->right, clone_node);
  new_node->height = node->height;
  if (new_node->left != new_tree->nil) {
    new_node->left->parent = new_node;
  }
  if (new_node->right != new_tree->nil) {
    new_node->right->parent = new_node;
  }

  return new_node;
}

ds_avl_t *FUNC(avl_clone)(ds_avl_t *tree,
                          ds_avl_node_t *(*clone_node)(ds_avl_node_t *)) {
  ds_avl_t *new_tree;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  new_tree = FUNC(avl_create_alloc)(tree->allocator, tree->deallocator);
  new_tree->root =
      FUNC(avl_clone_recursive)(tree, new_tree, tree->root, clone_node);
  new_tree->size = tree->size;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return new_tree;
}
