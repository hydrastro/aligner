#include "bst.h"
#include <stdlib.h>

ds_bst_t *FUNC(bst_create)(void) {
  return FUNC(bst_create_alloc)(malloc, free);
}

ds_bst_t *FUNC(bst_create_alloc)(void *(*allocator)(size_t),
                                 void (*deallocator)(void *)) {
  ds_bst_t *tree = (ds_bst_t *)allocator(sizeof(ds_bst_t));
  tree->allocator = allocator;
  tree->deallocator = deallocator;
  tree->nil = (ds_bst_node_t *)allocator(sizeof(ds_bst_node_t));
  tree->nil->left = tree->nil;
  tree->nil->right = tree->nil;
  tree->nil->parent = tree->nil;
  tree->root = tree->nil;
  tree->size = 0;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(tree);
#endif

  return tree;
}

void FUNC(bst_insert)(ds_bst_t *tree, ds_bst_node_t *node,
                      int (*compare)(ds_bst_node_t *, ds_bst_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif

  if (tree->root == tree->nil) {
    tree->root = node;
    node->parent = node->left = node->right = tree->nil;
  } else {
    ds_bst_node_t *current = tree->root;
    ds_bst_node_t *parent = tree->nil;
    while (current != tree->nil) {
      parent = current;
      if (compare(node, current) < 0) {
        current = current->left;
      } else {
        current = current->right;
      }
    }
    if (compare(node, parent) < 0) {
      parent->left = node;
    } else {
      parent->right = node;
    }
    node->parent = parent;
    node->left = node->right = tree->nil;
  }
  tree->size += 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_bst_node_t *FUNC(bst_search)(ds_bst_t *tree, ds_bst_node_t *data,
                                int (*compare)(ds_bst_node_t *,
                                               ds_bst_node_t *)) {
  ds_bst_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  current = tree->root;
  while (current != tree->nil && compare(data, current) != 0) {
    if (compare(data, current) < 0) {
      current = current->left;
    } else {
      current = current->right;
    }
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return current;
}

ds_bst_node_t *FUNC(bst_minimum)(ds_bst_t *tree, ds_bst_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node != tree->nil && node->left != tree->nil) {
    node = node->left;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node;
}

ds_bst_node_t *FUNC(bst_maximum)(ds_bst_t *tree, ds_bst_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  while (node != tree->nil && node->right != tree->nil) {
    node = node->right;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node;
}

void FUNC(bst_transplant)(ds_bst_t *tree, ds_bst_node_t *u, ds_bst_node_t *v) {
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
void FUNC(bst_delete_node)(ds_bst_t *tree, ds_bst_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node->left == tree->nil) {
    FUNC(bst_transplant)(tree, node, node->right);
  } else if (node->right == tree->nil) {
    FUNC(bst_transplant)(tree, node, node->left);
  } else {
    ds_bst_node_t *successor = FUNC(bst_minimum)(tree, node->right);
    if (successor->parent != node) {
      FUNC(bst_transplant)(tree, successor, successor->right);
      successor->right = node->right;
      successor->right->parent = successor;
    }
    FUNC(bst_transplant)(tree, node, successor);
    successor->left = node->left;
    successor->left->parent = successor;
  }
  tree->size -= 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(bst_delete_tree)(ds_bst_t *tree) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  tree->root = tree->nil;
  tree->size = 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(bst_destroy_node)(ds_bst_t *tree, ds_bst_node_t *node,
                            void (*destroy)(ds_bst_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node == tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return;
  }

  FUNC(bst_delete_node)(tree, node);
  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}
void FUNC(bst_destroy_recursive)(ds_bst_t *tree, ds_bst_node_t *node,
                                 void (*destroy)(ds_bst_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node == tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif

    return;
  }

  FUNC(bst_destroy_recursive)(tree, node->left, destroy);
  FUNC(bst_destroy_recursive)(tree, node->right, destroy);

  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}
void FUNC(bst_destroy_tree)(ds_bst_t *tree, void (*destroy)(ds_bst_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif

  FUNC(bst_destroy_recursive)(tree, tree->root, destroy);

  tree->deallocator(tree->nil);

#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(tree);
#endif

  tree->deallocator(tree);
}

void FUNC(bst_inorder_walk_helper)(ds_bst_t *tree, ds_bst_node_t *node,
                                   void (*callback)(void *)) {
  if (node != tree->nil) {
    FUNC(bst_inorder_walk_helper)(tree, node->left, callback);
    callback(node);
    FUNC(bst_inorder_walk_helper)(tree, node->right, callback);
  }
}

void FUNC(bst_inorder_walk)(ds_bst_t *tree, ds_bst_node_t *node,
                            void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(bst_inorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(bst_inorder_walk_tree)(ds_bst_t *tree, void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(bst_inorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(bst_preorder_walk_helper)(ds_bst_t *tree, ds_bst_node_t *node,
                                    void (*callback)(void *)) {
  if (node != tree->nil) {
    callback(node);
    FUNC(bst_preorder_walk_helper)(tree, node->left, callback);
    FUNC(bst_preorder_walk_helper)(tree, node->right, callback);
  }
}

void FUNC(bst_preorder_walk)(ds_bst_t *tree, ds_bst_node_t *node,
                             void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(bst_preorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(bst_preorder_walk_tree)(ds_bst_t *tree, void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(bst_preorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(bst_postorder_walk_helper)(ds_bst_t *tree, ds_bst_node_t *node,
                                     void (*callback)(void *)) {
  if (node != tree->nil) {
    FUNC(bst_postorder_walk_helper)(tree, node->left, callback);
    FUNC(bst_postorder_walk_helper)(tree, node->right, callback);
    callback(node);
  }
}
void FUNC(bst_postorder_walk)(ds_bst_t *tree, ds_bst_node_t *node,
                              void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(bst_postorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}
void FUNC(bst_postorder_walk_tree)(ds_bst_t *tree, void (*callback)(void *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(bst_postorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_bst_node_t *FUNC(bst_successor)(ds_bst_t *tree, ds_bst_node_t *node_x) {
  ds_bst_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node_x->right != tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return FUNC(bst_minimum)(tree, node_x->right);
  }
  node_y = node_x->parent;
  while (node_y != tree->nil && node_x == node_y->right) {
    node_x = node_y;
    node_y = node_y->parent;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_y;
}

ds_bst_node_t *FUNC(bst_predecessor)(ds_bst_t *tree, ds_bst_node_t *node_x) {
  ds_bst_node_t *node_y;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node_x->left != tree->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return FUNC(bst_maximum)(tree, node_x->left);
  }
  node_y = node_x->parent;
  while (node_y != tree->nil && node_x == node_y->left) {
    node_x = node_y;
    node_y = node_y->parent;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return node_y;
}

ds_bst_node_t *
FUNC(bst_clone_recursive)(ds_bst_t *tree, ds_bst_t *new_tree,
                          ds_bst_node_t *node,
                          ds_bst_node_t *(*clone_node)(ds_bst_node_t *)) {
  ds_bst_node_t *new_node;
  if (node == tree->nil) {
    return new_tree->nil;
  }

  new_node = clone_node(node);
  new_node->left =
      FUNC(bst_clone_recursive)(tree, new_tree, node->left, clone_node);
  new_node->right =
      FUNC(bst_clone_recursive)(tree, new_tree, node->right, clone_node);
  if (new_node->left != new_tree->nil) {
    new_node->left->parent = new_node;
  }
  if (new_node->right != new_tree->nil) {
    new_node->right->parent = new_node;
  }

  return new_node;
}

ds_bst_t *FUNC(bst_clone)(ds_bst_t *tree,
                          ds_bst_node_t *(*clone_node)(ds_bst_node_t *)) {
  ds_bst_t *new_tree;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  new_tree = FUNC(bst_create_alloc)(tree->allocator, tree->deallocator);
  new_tree->root =
      FUNC(bst_clone_recursive)(tree, new_tree, tree->root, clone_node);
  new_tree->size = tree->size;

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return new_tree;
}
