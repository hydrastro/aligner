#include "btree.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

ds_btree_internal_node_t *FUNC(btree_create_node)(ds_btree_t *tree,
                                                  bool is_leaf) {
  ds_btree_internal_node_t *node = (ds_btree_internal_node_t *)tree->allocator(
      sizeof(ds_btree_internal_node_t));
  node->num_keys = 0;
  node->is_leaf = is_leaf;
  node->data = (ds_btree_node_t **)tree->allocator(
      (2 * (unsigned int)tree->degree - 1) * sizeof(ds_btree_node_t *));
  node->children = (ds_btree_internal_node_t **)tree->allocator(
      2 * (unsigned int)tree->degree * sizeof(ds_btree_internal_node_t *));
  memset(node->children, 0,
         2 * (unsigned int)tree->degree * sizeof(ds_btree_internal_node_t *));
  return node;
}

ds_btree_t *FUNC(btree_create)(int degree) {
  return FUNC(btree_create_alloc)(degree, malloc, free);
}

ds_btree_t *FUNC(btree_create_alloc)(int degree, void *(*allocator)(size_t),
                                     void (*deallocator)(void *)) {
  ds_btree_t *tree = (ds_btree_t *)allocator(sizeof(ds_btree_t));
  tree->allocator = allocator;
  tree->deallocator = deallocator;
  tree->degree = degree;
  tree->root = FUNC(btree_create_node)(tree, true);
  tree->root->parent = NULL;
  tree->nil = (ds_btree_node_t *)allocator(sizeof(ds_btree_node_t));
  tree->size = 0;

#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(tree);
#endif

  return tree;
}

void FUNC(btree_split_child)(ds_btree_t *tree, ds_btree_internal_node_t *parent,
                             int index) {
  int i;
  ds_btree_internal_node_t *child;
  ds_btree_internal_node_t *new_node;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  child = parent->children[index];
  new_node = FUNC(btree_create_node)(tree, child->is_leaf);
  new_node->num_keys = tree->degree - 1;
  new_node->parent = parent;

  for (i = 0; i < tree->degree - 1; i++) {
    new_node->data[i] = child->data[i + tree->degree];
    new_node->data[i]->internal = new_node;
  }
  if (!child->is_leaf) {
    for (i = 0; i < tree->degree; i++) {
      new_node->children[i] = child->children[i + tree->degree];
    }
  }
  child->num_keys = tree->degree - 1;
  for (i = parent->num_keys; i >= index + 1; i--) {
    parent->children[i + 1] = parent->children[i];
  }
  parent->children[index + 1] = new_node;

  for (i = parent->num_keys - 1; i >= index; i--) {
    parent->data[i + 1] = parent->data[i];
    parent->data[i + 1]->internal = parent;
  }
  parent->data[index] = child->data[tree->degree - 1];
  parent->data[index]->internal = parent;
  parent->num_keys++;
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_insert_non_full)(
    ds_btree_t *tree, ds_btree_internal_node_t *node, ds_btree_node_t *data,
    int (*compare)(ds_btree_node_t *, ds_btree_node_t *)) {
  int i;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  i = node->num_keys - 1;

  if (node->is_leaf) {
    while (i >= 0 && compare(data, node->data[i]) < 0) {
      node->data[i + 1] = node->data[i];
      i--;
    }
    node->data[i + 1] = data;
    data->internal = node;
    node->num_keys++;
  } else {
    while (i >= 0 && compare(data, node->data[i]) < 0) {
      i--;
    }
    i++;
    if (node->children[i]->num_keys == 2 * tree->degree - 1) {
      FUNC(btree_split_child)(tree, node, i);
      if (compare(data, node->data[i]) > 0) {
        i++;
      }
    }
    FUNC(btree_insert_non_full)(tree, node->children[i], data, compare);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_insert)(ds_btree_t *tree, ds_btree_node_t *data,
                        int (*compare)(ds_btree_node_t *, ds_btree_node_t *)) {
  ds_btree_internal_node_t *new_root;
  ds_btree_internal_node_t *root;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  data->internal = NULL;
  root = tree->root;
  if (root->num_keys == 2 * tree->degree - 1) {
    new_root = FUNC(btree_create_node)(tree, false);
    root->parent = new_root;
    new_root->parent = NULL;
    new_root->children[0] = root;
    FUNC(btree_split_child)(tree, new_root, 0);
    tree->root = new_root;
    FUNC(btree_insert_non_full)(tree, new_root, data, compare);
  } else {
    FUNC(btree_insert_non_full)(tree, root, data, compare);
  }
  tree->size += 1;

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_btree_node_t *
FUNC(btree_search_node)(ds_btree_t *tree, ds_btree_internal_node_t *node,
                        ds_btree_node_t *key,
                        int (*compare)(ds_btree_node_t *, ds_btree_node_t *)) {
  int i;
  ds_btree_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  i = 0;
  while (i < node->num_keys && compare(key, node->data[i]) > 0) {
    i++;
  }

  if (i < node->num_keys && compare(key, node->data[i]) == 0) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return node->data[i];
  }

  if (node->is_leaf) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return tree->nil;
  }
  result = FUNC(btree_search_node)(tree, node->children[i], key, compare);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
  return result;
}

ds_btree_node_t *FUNC(btree_search)(ds_btree_t *tree, ds_btree_node_t *key,
                                    int (*compare)(ds_btree_node_t *,
                                                   ds_btree_node_t *)) {
  ds_btree_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif

  result = FUNC(btree_search_node)(tree, tree->root, key, compare);

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return result;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void *FUNC(btree_local_minimum)(ds_btree_t *tree,
                                ds_btree_internal_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif

  while (!node->is_leaf) {
    node = node->children[0];
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return node->data[0];
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void *FUNC(btree_local_maximum)(ds_btree_t *tree,
                                ds_btree_internal_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif

  while (!node->is_leaf) {
    node = node->children[node->num_keys];
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return node->data[node->num_keys - 1];
}
#pragma GCC diagnostic pop

void *FUNC(btree_minimum)(ds_btree_t *tree) {
  return FUNC(btree_local_minimum)(tree, tree->root);
}

void *FUNC(btree_maximum)(ds_btree_t *tree) {
  return FUNC(btree_local_maximum)(tree, tree->root);
}

void FUNC(btree_rebalance)(ds_btree_t *tree, ds_btree_internal_node_t *node) {
  int i, j;
  ds_btree_internal_node_t *parent;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  parent = node->parent;
  i = 0;
  if (parent == NULL) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return;
  }
  while (i <= parent->num_keys && parent->children[i] != node) {
    i++;
  }

  if (parent->num_keys - 1 > i &&
      parent->children[i + 1]->num_keys > tree->degree - 1) {
    node->data[node->num_keys] = parent->data[i];
    node->data[node->num_keys]->internal = node;
    node->num_keys++;
    parent->data[i] = parent->children[i + 1]->data[0];
    parent->data[i]->internal = parent;
    for (j = 1; j < parent->children[i + 1]->num_keys; j++) {
      parent->children[i + 1]->data[j - 1] = parent->children[i + 1]->data[j];
    }
    parent->children[i + 1]->num_keys--;
  } else if (i != 0 && parent->children[i - 1]->num_keys > tree->degree - 1) {
    for (j = node->num_keys; j > 0; j--) {
      node->data[j] = node->data[j - 1];
    }
    node->data[0] = parent->data[i];
    node->data[0]->internal = node;
    node->num_keys++;
    parent->data[i] =
        parent->children[i - 1]->data[parent->children[i - 1]->num_keys - 1];
    parent->data[i]->internal = parent;
    parent->children[i - 1]->num_keys--;
  } else {
    ds_btree_internal_node_t *recipient, *next;

    if (i != 0) {
      recipient = parent->children[i - 1];
      next = node;
      j = i - 1;
    } else {
      recipient = node;
      next = parent->children[i + 1];
      j = i;
    }
    recipient->data[recipient->num_keys] = parent->data[j];
    recipient->data[recipient->num_keys]->internal = recipient;
    recipient->num_keys++;
    for (j = 0; j < next->num_keys; j++) {
      recipient->data[recipient->num_keys] = next->data[j];
      recipient->data[recipient->num_keys]->internal = recipient;
      recipient->num_keys++;
    }
    tree->deallocator(next->children);
    tree->deallocator(next->data);
    tree->deallocator(next);

    for (j = i + 1; j < parent->num_keys; j++) {
      parent->data[j - 1] = parent->data[j];
    }

    for (j = i + 2; j <= parent->num_keys; j++) {
      parent->children[j - 1] = parent->children[j];
    }
    parent->num_keys--;
    if (parent == tree->root && parent->num_keys == 0) {
      tree->root = recipient;
      tree->root->parent = NULL;
      tree->deallocator(parent->children);
      tree->deallocator(parent->data);
      tree->deallocator(parent);
    } else if (parent->num_keys < tree->degree - 1) {
      FUNC(btree_rebalance)(tree, parent);
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_delete_node)(ds_btree_t *tree, ds_btree_node_t *key) {
  int i;
  bool deleted;
  ds_btree_internal_node_t *node;
  ds_btree_internal_node_t *borrowed_internal;
  ds_btree_node_t *borrowed;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  node = key->internal;
  deleted = false;
  if (node == NULL) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return;
  }
  i = 0;
  while (i < node->num_keys && node->data[i] != key) {
    i++;
  }
  if (node->is_leaf) {
    for (i = i + 1; i < node->num_keys; i++) {
      node->data[i - 1] = node->data[i];
    }
    node->num_keys--;
    if (node->num_keys < tree->degree - 1) {
      FUNC(btree_rebalance)(tree, node);
    }
    deleted = true;
  } else {
    if (node->children[i + 1]->num_keys > tree->degree - 1) {
      borrowed = FUNC(btree_local_minimum)(tree, node->children[i + 1]);
    } else {
      borrowed = FUNC(btree_local_maximum)(tree, node->children[i]);
    }
    node->data[i] = borrowed;

    borrowed_internal = borrowed->internal;
    i = 0;
    while (i < borrowed_internal->num_keys &&
           borrowed_internal->data[i] != borrowed) {
      i++;
    }

    for (i = i + 1; i < borrowed_internal->num_keys; i++) {
      borrowed_internal->data[i - 1] = borrowed_internal->data[i];
    }
    borrowed_internal->num_keys--;

    borrowed->internal = node;
    if (borrowed_internal->num_keys < tree->degree - 1) {
      FUNC(btree_rebalance)(tree, borrowed_internal);
    }
    deleted = true;
  }
  if (deleted && tree->size != 0U) {
    tree->size -= 1U;
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_destroy_node)(ds_btree_t *tree, ds_btree_node_t *node,
                              void (*destroy)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_delete_node)(tree, node);
  if (destroy != NULL) {
    destroy(node);
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_destroy_recursive)(ds_btree_t *tree,
                                   ds_btree_internal_node_t *node,
                                   void (*destroy)(ds_btree_node_t *)) {
  int i;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  if (node == NULL) {
#ifdef DS_THREAD_SAFE
    UNLOCK(tree);
#endif
    return;
  }
  if (!node->is_leaf) {
    for (i = 0; i <= node->num_keys; i++) {
      FUNC(btree_destroy_recursive)(tree, node->children[i], destroy);
    }
  }
  for (i = 0; i < node->num_keys; i++) {
    destroy(node->data[i]);
  }
  tree->deallocator(node->children);
  tree->deallocator(node->data);
  tree->deallocator(node);

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_destroy_tree)(ds_btree_t *tree,
                              void (*destroy)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_destroy_recursive)(tree, tree->root, destroy);
  tree->deallocator(tree->nil);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
  LOCK_DESTROY(tree);
#endif
  tree->deallocator(tree);
}

void FUNC(btree_inorder_walk_helper)(ds_btree_t *tree,
                                     ds_btree_internal_node_t *node,
                                     void (*callback)(ds_btree_node_t *)) {
  int i;
  if (node == NULL) {
    return;
  }
  for (i = 0; i < node->num_keys; i++) {
    if (!node->is_leaf) {
      FUNC(btree_inorder_walk_helper)(tree, node->children[i], callback);
    }
    callback(node->data[i]);
  }
  if (!node->is_leaf) {
    FUNC(btree_inorder_walk_helper)(tree, node->children[i], callback);
  }
}

void FUNC(btree_inorder_walk)(ds_btree_t *tree, ds_btree_internal_node_t *node,
                              void (*callback)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_inorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_inorder_walk_tree)(ds_btree_t *tree,
                                   void (*callback)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_inorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_preorder_walk_helper)(ds_btree_t *tree,
                                      ds_btree_internal_node_t *node,
                                      void (*callback)(ds_btree_node_t *)) {
  int i;
  if (node == NULL) {
    return;
  }
  for (i = 0; i < node->num_keys; i++) {
    callback(node->data[i]);
  }
  if (!node->is_leaf) {
    for (i = 0; i <= node->num_keys; i++) {
      FUNC(btree_preorder_walk_helper)(tree, node->children[i], callback);
    }
  }
}

void FUNC(btree_preorder_walk)(ds_btree_t *tree, ds_btree_internal_node_t *node,
                               void (*callback)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_preorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_preorder_walk_tree)(ds_btree_t *tree,
                                    void (*callback)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_preorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_postorder_walk_helper)(ds_btree_t *tree,
                                       ds_btree_internal_node_t *node,
                                       void (*callback)(ds_btree_node_t *)) {
  int i;
  if (node == NULL) {
    return;
  }
  if (!node->is_leaf) {
    for (i = 0; i <= node->num_keys; i++) {
      FUNC(btree_postorder_walk_helper)(tree, node->children[i], callback);
    }
  }
  for (i = 0; i < node->num_keys; i++) {
    callback(node->data[i]);
  }
}

void FUNC(btree_postorder_walk)(ds_btree_t *tree,
                                ds_btree_internal_node_t *node,
                                void (*callback)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_postorder_walk_helper)(tree, node, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

void FUNC(btree_postorder_walk_tree)(ds_btree_t *tree,
                                     void (*callback)(ds_btree_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  FUNC(btree_postorder_walk_helper)(tree, tree->root, callback);
#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif
}

ds_btree_internal_node_t *
FUNC(btree_clone_recursive)(ds_btree_t *tree, ds_btree_t *new_tree,
                            ds_btree_internal_node_t *node,
                            ds_btree_node_t *(*clone_node)(ds_btree_node_t *)) {

  int i;
  ds_btree_internal_node_t *new_node =
      FUNC(btree_create_node)(new_tree, node->is_leaf);
  new_node->num_keys = node->num_keys;

  for (i = 0; i < node->num_keys; i++) {
    new_node->data[i] = clone_node(node->data[i]);
    new_node->data[i]->internal = new_node;
  }

  if (!node->is_leaf) {
    for (i = 0; i <= node->num_keys; i++) {
      new_node->children[i] = FUNC(btree_clone_recursive)(
          tree, new_tree, node->children[i], clone_node);
      if (new_node->children[i]) {
        new_node->children[i]->parent = new_node;
      }
    }
  }

  return new_node;
}

ds_btree_t *
FUNC(btree_clone)(ds_btree_t *tree,
                  ds_btree_node_t *(*clone_node)(ds_btree_node_t *)) {
  ds_btree_t *new_tree;
#ifdef DS_THREAD_SAFE
  LOCK(tree);
#endif
  new_tree = (ds_btree_t *)tree->allocator(sizeof(ds_btree_t));
  new_tree->allocator = tree->allocator;
  new_tree->deallocator = tree->deallocator;
  new_tree->degree = tree->degree;
  new_tree->nil = (ds_btree_node_t *)tree->allocator(sizeof(ds_btree_node_t));

#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(new_tree);
#endif

  new_tree->root =
      FUNC(btree_clone_recursive)(tree, new_tree, tree->root, clone_node);
  new_tree->size = tree->size;

#ifdef DS_THREAD_SAFE
  UNLOCK(tree);
#endif

  return new_tree;
}
