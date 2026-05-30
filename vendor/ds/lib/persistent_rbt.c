#include "persistent_rbt.h"
#include <stdbool.h>
#include <stdlib.h>

#define PRBT_RED true
#define PRBT_BLACK false

typedef struct ds_prbt_node {
  unsigned long refs;
  bool red;
  struct ds_prbt_node *left;
  struct ds_prbt_node *right;
  void *key;
  void *value;
  size_t subtree_size;
} ds_prbt_node_t;

struct ds_prbt_version {
  unsigned long refs;
  ds_prbt_node_t *root;
  size_t size;
};

struct ds_prbt {
  ds_prbt_config_t config;
  ds_prbt_version_t *root;
};

static void *prbt_clone_key(ds_prbt_t *tree, void *key);
static void *prbt_clone_value(ds_prbt_t *tree, void *value);
static void prbt_destroy_key(ds_prbt_t *tree, void *key);
static void prbt_destroy_value(ds_prbt_t *tree, void *value);
static ds_prbt_node_t *prbt_node_create(ds_prbt_t *tree, void *key,
                                        void *value, bool red);
static size_t prbt_node_size(ds_prbt_node_t *node);
static void prbt_update_size(ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_node_copy(ds_prbt_t *tree, ds_prbt_node_t *node);
static void prbt_node_retain(ds_prbt_node_t *node);
static void prbt_node_release(ds_prbt_t *tree, ds_prbt_node_t *node);
static bool prbt_is_red(ds_prbt_node_t *node);
static void prbt_set_left(ds_prbt_t *tree, ds_prbt_node_t *node,
                          ds_prbt_node_t *left);
static void prbt_set_right(ds_prbt_t *tree, ds_prbt_node_t *node,
                           ds_prbt_node_t *right);
static ds_prbt_node_t *prbt_rotate_left(ds_prbt_t *tree, ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_rotate_right(ds_prbt_t *tree, ds_prbt_node_t *node);
static bool prbt_color_flip(ds_prbt_t *tree, ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_fixup(ds_prbt_t *tree, ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_move_red_left(ds_prbt_t *tree,
                                          ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_move_red_right(ds_prbt_t *tree,
                                           ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_insert_rec(ds_prbt_t *tree, ds_prbt_node_t *node,
                                       void *key, void *value,
                                       bool *inserted, bool *failed);
static ds_prbt_node_t *prbt_delete_min_rec(ds_prbt_t *tree,
                                           ds_prbt_node_t *node,
                                           bool *failed);
static ds_prbt_node_t *prbt_remove_rec(ds_prbt_t *tree, ds_prbt_node_t *node,
                                       void *key, bool *removed,
                                       bool *failed);
static ds_prbt_node_t *prbt_min_node(ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_max_node(ds_prbt_node_t *node);
static ds_prbt_node_t *prbt_search_node(ds_prbt_t *tree,
                                        ds_prbt_node_t *node, void *key);
static ds_prbt_version_t *prbt_version_create(ds_prbt_t *tree,
                                              ds_prbt_node_t *root,
                                              size_t size);
static ds_status_t prbt_visit_node(ds_prbt_t *tree, ds_prbt_node_t *node,
                                   void *lower, void *upper,
                                   ds_prbt_visit_func_t visit, void *user);

void ds_prbt_config_init(ds_prbt_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->compare = NULL;
  config->clone_key = NULL;
  config->clone_value = NULL;
  config->destroy_key = NULL;
  config->destroy_value = NULL;
  config->user = NULL;
  config->context = NULL;
}

static void *prbt_clone_key(ds_prbt_t *tree, void *key) {
  if (tree->config.clone_key != NULL) {
    return tree->config.clone_key(key, tree->config.user);
  }
  return key;
}

static void *prbt_clone_value(ds_prbt_t *tree, void *value) {
  if (tree->config.clone_value != NULL) {
    return tree->config.clone_value(value, tree->config.user);
  }
  return value;
}

static void prbt_destroy_key(ds_prbt_t *tree, void *key) {
  if (tree->config.destroy_key != NULL) {
    tree->config.destroy_key(key, tree->config.user);
  }
}

static void prbt_destroy_value(ds_prbt_t *tree, void *value) {
  if (tree->config.destroy_value != NULL) {
    tree->config.destroy_value(value, tree->config.user);
  }
}

static size_t prbt_node_size(ds_prbt_node_t *node) {
  return node == NULL ? 0U : node->subtree_size;
}

static void prbt_update_size(ds_prbt_node_t *node) {
  if (node != NULL) {
    node->subtree_size = 1U + prbt_node_size(node->left) +
                         prbt_node_size(node->right);
  }
}

static ds_prbt_node_t *prbt_node_create(ds_prbt_t *tree, void *key,
                                        void *value, bool red) {
  ds_prbt_node_t *node;

  node = (ds_prbt_node_t *)ds_context_alloc(tree->config.context,
                                             sizeof(*node));
  if (node == NULL) {
    return NULL;
  }

  node->refs = 1UL;
  node->red = red;
  node->left = NULL;
  node->right = NULL;
  node->key = prbt_clone_key(tree, key);
  if (node->key == NULL && key != NULL && tree->config.clone_key != NULL) {
    ds_context_free(tree->config.context, node);
    return NULL;
  }
  node->subtree_size = 1U;
  node->value = prbt_clone_value(tree, value);
  if (node->value == NULL && value != NULL && tree->config.clone_value != NULL) {
    prbt_destroy_key(tree, node->key);
    ds_context_free(tree->config.context, node);
    return NULL;
  }

  return node;
}

static ds_prbt_node_t *prbt_node_copy(ds_prbt_t *tree, ds_prbt_node_t *node) {
  ds_prbt_node_t *copy;

  if (node == NULL) {
    return NULL;
  }

  copy = prbt_node_create(tree, node->key, node->value, node->red);
  if (copy == NULL) {
    return NULL;
  }
  copy->left = node->left;
  copy->right = node->right;
  copy->subtree_size = node->subtree_size;
  prbt_node_retain(copy->left);
  prbt_node_retain(copy->right);
  return copy;
}

static void prbt_node_retain(ds_prbt_node_t *node) {
  if (node != NULL) {
    node->refs++;
  }
}

static void prbt_node_release(ds_prbt_t *tree, ds_prbt_node_t *node) {
  if (node == NULL) {
    return;
  }
  node->refs--;
  if (node->refs != 0UL) {
    return;
  }
  prbt_node_release(tree, node->left);
  prbt_node_release(tree, node->right);
  prbt_destroy_key(tree, node->key);
  prbt_destroy_value(tree, node->value);
  ds_context_free(tree->config.context, node);
}

static bool prbt_is_red(ds_prbt_node_t *node) {
  return node != NULL && node->red;
}

static void prbt_set_left(ds_prbt_t *tree, ds_prbt_node_t *node,
                          ds_prbt_node_t *left) {
  if (node->left != left) {
    prbt_node_release(tree, node->left);
    node->left = left;
    prbt_update_size(node);
  }
}

static void prbt_set_right(ds_prbt_t *tree, ds_prbt_node_t *node,
                           ds_prbt_node_t *right) {
  if (node->right != right) {
    prbt_node_release(tree, node->right);
    node->right = right;
    prbt_update_size(node);
  }
}

static ds_prbt_node_t *prbt_rotate_left(ds_prbt_t *tree, ds_prbt_node_t *node) {
  ds_prbt_node_t *pivot;
  ds_prbt_node_t *pivot_left;

  pivot = prbt_node_copy(tree, node->right);
  if (pivot == NULL) {
    return NULL;
  }
  pivot_left = pivot->left;
  prbt_node_retain(pivot_left);
  prbt_set_right(tree, node, pivot_left);
  prbt_node_release(tree, pivot->left);
  pivot->left = node;
  pivot->red = node->red;
  node->red = PRBT_RED;
  prbt_update_size(node);
  prbt_update_size(pivot);
  return pivot;
}

static ds_prbt_node_t *prbt_rotate_right(ds_prbt_t *tree, ds_prbt_node_t *node) {
  ds_prbt_node_t *pivot;
  ds_prbt_node_t *pivot_right;

  pivot = prbt_node_copy(tree, node->left);
  if (pivot == NULL) {
    return NULL;
  }
  pivot_right = pivot->right;
  prbt_node_retain(pivot_right);
  prbt_set_left(tree, node, pivot_right);
  prbt_node_release(tree, pivot->right);
  pivot->right = node;
  pivot->red = node->red;
  node->red = PRBT_RED;
  prbt_update_size(node);
  prbt_update_size(pivot);
  return pivot;
}

static bool prbt_color_flip(ds_prbt_t *tree, ds_prbt_node_t *node) {
  ds_prbt_node_t *left;
  ds_prbt_node_t *right;

  left = prbt_node_copy(tree, node->left);
  if (left == NULL) {
    return false;
  }
  right = prbt_node_copy(tree, node->right);
  if (right == NULL) {
    prbt_node_release(tree, left);
    return false;
  }
  left->red = !left->red;
  right->red = !right->red;
  node->red = !node->red;
  prbt_set_left(tree, node, left);
  prbt_set_right(tree, node, right);
  return true;
}

static ds_prbt_node_t *prbt_fixup(ds_prbt_t *tree, ds_prbt_node_t *node) {
  ds_prbt_node_t *next;

  if (node == NULL) {
    return NULL;
  }
  if (prbt_is_red(node->right) && !prbt_is_red(node->left)) {
    next = prbt_rotate_left(tree, node);
    if (next == NULL) {
      prbt_node_release(tree, node);
      return NULL;
    }
    node = next;
  }
  if (prbt_is_red(node->left) && prbt_is_red(node->left->left)) {
    next = prbt_rotate_right(tree, node);
    if (next == NULL) {
      prbt_node_release(tree, node);
      return NULL;
    }
    node = next;
  }
  if (prbt_is_red(node->left) && prbt_is_red(node->right)) {
    if (!prbt_color_flip(tree, node)) {
      prbt_node_release(tree, node);
      return NULL;
    }
  }
  prbt_update_size(node);
  return node;
}

static ds_prbt_node_t *prbt_move_red_left(ds_prbt_t *tree,
                                          ds_prbt_node_t *node) {
  ds_prbt_node_t *right;
  ds_prbt_node_t *next;

  if (!prbt_color_flip(tree, node)) {
    prbt_node_release(tree, node);
    return NULL;
  }
  if (node->right != NULL && prbt_is_red(node->right->left)) {
    right = prbt_node_copy(tree, node->right);
    if (right == NULL) {
      prbt_node_release(tree, node);
      return NULL;
    }
    next = prbt_rotate_right(tree, right);
    if (next == NULL) {
      prbt_node_release(tree, right);
      prbt_node_release(tree, node);
      return NULL;
    }
    prbt_set_right(tree, node, next);
    next = prbt_rotate_left(tree, node);
    if (next == NULL) {
      prbt_node_release(tree, node);
      return NULL;
    }
    node = next;
    if (!prbt_color_flip(tree, node)) {
      prbt_node_release(tree, node);
      return NULL;
    }
  }
  return node;
}

static ds_prbt_node_t *prbt_move_red_right(ds_prbt_t *tree,
                                           ds_prbt_node_t *node) {
  ds_prbt_node_t *next;

  if (!prbt_color_flip(tree, node)) {
    prbt_node_release(tree, node);
    return NULL;
  }
  if (node->left != NULL && prbt_is_red(node->left->left)) {
    next = prbt_rotate_right(tree, node);
    if (next == NULL) {
      prbt_node_release(tree, node);
      return NULL;
    }
    node = next;
    if (!prbt_color_flip(tree, node)) {
      prbt_node_release(tree, node);
      return NULL;
    }
  }
  return node;
}

static ds_prbt_node_t *prbt_insert_rec(ds_prbt_t *tree, ds_prbt_node_t *node,
                                       void *key, void *value,
                                       bool *inserted, bool *failed) {
  ds_prbt_node_t *copy;
  ds_prbt_node_t *child;
  int cmp;

  if (node == NULL) {
    *inserted = true;
    return prbt_node_create(tree, key, value, PRBT_RED);
  }

  copy = prbt_node_copy(tree, node);
  if (copy == NULL) {
    *failed = true;
    return NULL;
  }

  cmp = tree->config.compare(key, copy->key, tree->config.user);
  if (cmp < 0) {
    child = prbt_insert_rec(tree, copy->left, key, value, inserted, failed);
    if (*failed || child == NULL) {
      prbt_node_release(tree, copy);
      *failed = true;
      return NULL;
    }
    prbt_set_left(tree, copy, child);
  } else if (cmp > 0) {
    child = prbt_insert_rec(tree, copy->right, key, value, inserted, failed);
    if (*failed || child == NULL) {
      prbt_node_release(tree, copy);
      *failed = true;
      return NULL;
    }
    prbt_set_right(tree, copy, child);
  } else {
    prbt_destroy_value(tree, copy->value);
    copy->value = prbt_clone_value(tree, value);
    if (copy->value == NULL && value != NULL && tree->config.clone_value != NULL) {
      prbt_node_release(tree, copy);
      *failed = true;
      return NULL;
    }
  }

  copy = prbt_fixup(tree, copy);
  if (copy == NULL) {
    *failed = true;
  }
  return copy;
}

static ds_prbt_node_t *prbt_min_node(ds_prbt_node_t *node) {
  while (node != NULL && node->left != NULL) {
    node = node->left;
  }
  return node;
}

static ds_prbt_node_t *prbt_max_node(ds_prbt_node_t *node) {
  while (node != NULL && node->right != NULL) {
    node = node->right;
  }
  return node;
}

static ds_prbt_node_t *prbt_delete_min_rec(ds_prbt_t *tree,
                                           ds_prbt_node_t *node,
                                           bool *failed) {
  ds_prbt_node_t *copy;
  ds_prbt_node_t *child;

  if (node == NULL) {
    return NULL;
  }
  if (node->left == NULL) {
    return NULL;
  }

  copy = prbt_node_copy(tree, node);
  if (copy == NULL) {
    *failed = true;
    return NULL;
  }
  if (!prbt_is_red(copy->left) && !prbt_is_red(copy->left->left)) {
    copy = prbt_move_red_left(tree, copy);
    if (copy == NULL) {
      *failed = true;
      return NULL;
    }
  }

  child = prbt_delete_min_rec(tree, copy->left, failed);
  if (*failed) {
    prbt_node_release(tree, copy);
    return NULL;
  }
  prbt_set_left(tree, copy, child);
  copy = prbt_fixup(tree, copy);
  if (copy == NULL) {
    *failed = true;
  }
  return copy;
}

static ds_prbt_node_t *prbt_remove_rec(ds_prbt_t *tree, ds_prbt_node_t *node,
                                       void *key, bool *removed,
                                       bool *failed) {
  ds_prbt_node_t *copy;
  ds_prbt_node_t *child;
  ds_prbt_node_t *min;
  void *new_key;
  void *new_value;
  int cmp;

  if (node == NULL) {
    return NULL;
  }

  copy = prbt_node_copy(tree, node);
  if (copy == NULL) {
    *failed = true;
    return NULL;
  }

  cmp = tree->config.compare(key, copy->key, tree->config.user);
  if (cmp < 0) {
    if (!prbt_is_red(copy->left) && copy->left != NULL &&
        !prbt_is_red(copy->left->left)) {
      copy = prbt_move_red_left(tree, copy);
      if (copy == NULL) {
        *failed = true;
        return NULL;
      }
    }
    child = prbt_remove_rec(tree, copy->left, key, removed, failed);
    if (*failed) {
      prbt_node_release(tree, copy);
      return NULL;
    }
    prbt_set_left(tree, copy, child);
  } else {
    if (prbt_is_red(copy->left)) {
      copy = prbt_rotate_right(tree, copy);
      if (copy == NULL) {
        *failed = true;
        return NULL;
      }
    }
    cmp = tree->config.compare(key, copy->key, tree->config.user);
    if (cmp == 0 && copy->right == NULL) {
      prbt_node_release(tree, copy);
      *removed = true;
      return NULL;
    }
    if (copy->right != NULL) {
      if (!prbt_is_red(copy->right) && !prbt_is_red(copy->right->left)) {
        copy = prbt_move_red_right(tree, copy);
        if (copy == NULL) {
          *failed = true;
          return NULL;
        }
      }
      cmp = tree->config.compare(key, copy->key, tree->config.user);
      if (cmp == 0) {
        min = prbt_min_node(copy->right);
        new_key = prbt_clone_key(tree, min->key);
        if (new_key == NULL && min->key != NULL && tree->config.clone_key != NULL) {
          prbt_node_release(tree, copy);
          *failed = true;
          return NULL;
        }
        new_value = prbt_clone_value(tree, min->value);
        if (new_value == NULL && min->value != NULL &&
            tree->config.clone_value != NULL) {
          prbt_destroy_key(tree, new_key);
          prbt_node_release(tree, copy);
          *failed = true;
          return NULL;
        }
        prbt_destroy_key(tree, copy->key);
        prbt_destroy_value(tree, copy->value);
        copy->key = new_key;
        copy->value = new_value;
        child = prbt_delete_min_rec(tree, copy->right, failed);
        if (*failed) {
          prbt_node_release(tree, copy);
          return NULL;
        }
        prbt_set_right(tree, copy, child);
        *removed = true;
      } else {
        child = prbt_remove_rec(tree, copy->right, key, removed, failed);
        if (*failed) {
          prbt_node_release(tree, copy);
          return NULL;
        }
        prbt_set_right(tree, copy, child);
      }
    }
  }

  copy = prbt_fixup(tree, copy);
  if (copy == NULL) {
    *failed = true;
  }
  return copy;
}

static ds_prbt_node_t *prbt_search_node(ds_prbt_t *tree,
                                        ds_prbt_node_t *node, void *key) {
  int cmp;

  while (node != NULL) {
    cmp = tree->config.compare(key, node->key, tree->config.user);
    if (cmp == 0) {
      return node;
    }
    if (cmp < 0) {
      node = node->left;
    } else {
      node = node->right;
    }
  }
  return NULL;
}

static ds_prbt_version_t *prbt_version_create(ds_prbt_t *tree,
                                              ds_prbt_node_t *root,
                                              size_t size) {
  ds_prbt_version_t *version;

  version = (ds_prbt_version_t *)ds_context_alloc(tree->config.context,
                                                   sizeof(*version));
  if (version == NULL) {
    return NULL;
  }
  version->refs = 1UL;
  version->root = root;
  version->size = size;
  return version;
}

static ds_status_t prbt_visit_node(ds_prbt_t *tree, ds_prbt_node_t *node,
                                   void *lower, void *upper,
                                   ds_prbt_visit_func_t visit, void *user) {
  int cmp_lower;
  int cmp_upper;
  ds_status_t status;

  if (node == NULL) {
    return DS_OK;
  }

  cmp_lower = lower == NULL ? 1 : tree->config.compare(node->key, lower,
                                                       tree->config.user);
  cmp_upper = upper == NULL ? -1 : tree->config.compare(node->key, upper,
                                                        tree->config.user);

  if (lower == NULL || cmp_lower > 0) {
    status = prbt_visit_node(tree, node->left, lower, upper, visit, user);
    if (status != DS_OK) {
      return status;
    }
  }

  if ((lower == NULL || cmp_lower >= 0) && (upper == NULL || cmp_upper <= 0)) {
    if (!visit(node->key, node->value, user)) {
      return DS_STOP;
    }
  }

  if (upper == NULL || cmp_upper < 0) {
    status = prbt_visit_node(tree, node->right, lower, upper, visit, user);
    if (status != DS_OK) {
      return status;
    }
  }

  return DS_OK;
}

static ds_prbt_node_t *prbt_floor_node(ds_prbt_t *tree,
                                            ds_prbt_node_t *node, void *key) {
  ds_prbt_node_t *candidate;
  int cmp;

  candidate = NULL;
  while (node != NULL) {
    cmp = tree->config.compare(key, node->key, tree->config.user);
    if (cmp == 0) {
      return node;
    }
    if (cmp < 0) {
      node = node->left;
    } else {
      candidate = node;
      node = node->right;
    }
  }
  return candidate;
}

static ds_prbt_node_t *prbt_ceiling_node(ds_prbt_t *tree,
                                              ds_prbt_node_t *node, void *key) {
  ds_prbt_node_t *candidate;
  int cmp;

  candidate = NULL;
  while (node != NULL) {
    cmp = tree->config.compare(key, node->key, tree->config.user);
    if (cmp == 0) {
      return node;
    }
    if (cmp < 0) {
      candidate = node;
      node = node->left;
    } else {
      node = node->right;
    }
  }
  return candidate;
}


static ds_prbt_node_t *prbt_predecessor_node(ds_prbt_t *tree,
                                             ds_prbt_node_t *node, void *key) {
  ds_prbt_node_t *candidate;
  int cmp;

  candidate = NULL;
  while (node != NULL) {
    cmp = tree->config.compare(key, node->key, tree->config.user);
    if (cmp <= 0) {
      node = node->left;
    } else {
      candidate = node;
      node = node->right;
    }
  }
  return candidate;
}

static ds_prbt_node_t *prbt_successor_node(ds_prbt_t *tree,
                                           ds_prbt_node_t *node, void *key) {
  ds_prbt_node_t *candidate;
  int cmp;

  candidate = NULL;
  while (node != NULL) {
    cmp = tree->config.compare(key, node->key, tree->config.user);
    if (cmp < 0) {
      candidate = node;
      node = node->left;
    } else {
      node = node->right;
    }
  }
  return candidate;
}

static bool prbt_validate_node(ds_prbt_t *tree, ds_prbt_node_t *node,
                               void *min_key, void *max_key,
                               size_t *out_size, size_t black_count,
                               size_t *expected_black_count) {
  size_t left_size;
  size_t right_size;
  int cmp;

  if (node == NULL) {
    if (*expected_black_count == (size_t)-1) {
      *expected_black_count = black_count;
      *out_size = 0U;
      return true;
    }
    *out_size = 0U;
    return black_count == *expected_black_count;
  }

  if (min_key != NULL &&
      tree->config.compare(node->key, min_key, tree->config.user) <= 0) {
    return false;
  }
  if (max_key != NULL &&
      tree->config.compare(node->key, max_key, tree->config.user) >= 0) {
    return false;
  }
  if (prbt_is_red(node) && (prbt_is_red(node->left) || prbt_is_red(node->right))) {
    return false;
  }
  if (prbt_is_red(node->right)) {
    return false;
  }
  if (!prbt_is_red(node)) {
    black_count++;
  }
  if (!prbt_validate_node(tree, node->left, min_key, node->key, &left_size,
                          black_count, expected_black_count)) {
    return false;
  }
  if (!prbt_validate_node(tree, node->right, node->key, max_key, &right_size,
                          black_count, expected_black_count)) {
    return false;
  }
  if (node->subtree_size != left_size + right_size + 1U) {
    return false;
  }
  if (node->left != NULL) {
    cmp = tree->config.compare(node->left->key, node->key, tree->config.user);
    if (cmp >= 0) {
      return false;
    }
  }
  if (node->right != NULL) {
    cmp = tree->config.compare(node->right->key, node->key, tree->config.user);
    if (cmp <= 0) {
      return false;
    }
  }
  *out_size = node->subtree_size;
  return true;
}

static size_t prbt_rank_node(ds_prbt_t *tree, ds_prbt_node_t *node,
                             void *key) {
  size_t rank;
  int cmp;

  rank = 0U;
  while (node != NULL) {
    cmp = tree->config.compare(key, node->key, tree->config.user);
    if (cmp <= 0) {
      node = node->left;
    } else {
      rank += prbt_node_size(node->left) + 1U;
      node = node->right;
    }
  }
  return rank;
}

static ds_prbt_node_t *prbt_select_node(ds_prbt_node_t *node, size_t rank) {
  size_t left_size;

  while (node != NULL) {
    left_size = prbt_node_size(node->left);
    if (rank < left_size) {
      node = node->left;
    } else if (rank > left_size) {
      rank -= left_size + 1U;
      node = node->right;
    } else {
      return node;
    }
  }
  return NULL;
}

ds_prbt_t *FUNC(ds_prbt_create)(const ds_prbt_config_t *config) {
  ds_prbt_t *tree;

  if (config == NULL || config->compare == NULL) {
    return NULL;
  }
  if ((config->destroy_key != NULL && config->clone_key == NULL) ||
      (config->destroy_value != NULL && config->clone_value == NULL)) {
    return NULL;
  }

  tree = (ds_prbt_t *)ds_context_alloc(
      config->context != NULL ? config->context : ds_default_context(),
      sizeof(*tree));
  if (tree == NULL) {
    return NULL;
  }
  tree->config = *config;
  if (tree->config.context == NULL) {
    tree->config.context = ds_default_context();
  }
  tree->root = prbt_version_create(tree, NULL, 0U);
  if (tree->root == NULL) {
    ds_context_free(tree->config.context, tree);
    return NULL;
  }
  return tree;
}

void FUNC(ds_prbt_destroy)(ds_prbt_t *tree) {
  if (tree == NULL) {
    return;
  }
  FUNC(ds_prbt_version_release)(tree, tree->root);
  ds_context_free(tree->config.context, tree);
}

ds_prbt_version_t *FUNC(ds_prbt_root)(ds_prbt_t *tree) {
  if (tree == NULL) {
    return NULL;
  }
  return tree->root;
}

ds_prbt_version_t *FUNC(ds_prbt_insert)(ds_prbt_t *tree,
                                         ds_prbt_version_t *base, void *key,
                                         void *value) {
  ds_prbt_version_t *version;
  ds_prbt_node_t *root;
  bool inserted;
  bool failed;

  if (tree == NULL || base == NULL) {
    return NULL;
  }

  inserted = false;
  failed = false;
  root = prbt_insert_rec(tree, base->root, key, value, &inserted, &failed);
  if (failed || root == NULL) {
    prbt_node_release(tree, root);
    return NULL;
  }
  root->red = PRBT_BLACK;

  version = prbt_version_create(tree, root, base->size + (inserted ? 1U : 0U));
  if (version == NULL) {
    prbt_node_release(tree, root);
    return NULL;
  }
  return version;
}

ds_status_t FUNC(ds_prbt_get)(ds_prbt_t *tree, ds_prbt_version_t *version,
                               void *key, void **out_value) {
  ds_prbt_node_t *node;

  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (tree == NULL || version == NULL || out_value == NULL) {
    return DS_ERR_NULL;
  }

  node = prbt_search_node(tree, version->root, key);
  if (node == NULL) {
    return DS_NOT_FOUND;
  }
  *out_value = node->value;
  return DS_OK;
}

ds_prbt_version_t *FUNC(ds_prbt_remove)(ds_prbt_t *tree,
                                         ds_prbt_version_t *base, void *key) {
  ds_prbt_version_t *version;
  ds_prbt_node_t *root;
  bool removed;
  bool failed;

  if (tree == NULL || base == NULL) {
    return NULL;
  }
  if (prbt_search_node(tree, base->root, key) == NULL) {
    return NULL;
  }

  removed = false;
  failed = false;
  root = prbt_remove_rec(tree, base->root, key, &removed, &failed);
  if (failed || !removed) {
    prbt_node_release(tree, root);
    return NULL;
  }
  if (root != NULL) {
    root->red = PRBT_BLACK;
  }

  version = prbt_version_create(tree, root, base->size - 1U);
  if (version == NULL) {
    prbt_node_release(tree, root);
    return NULL;
  }
  return version;
}


ds_prbt_version_t *FUNC(ds_prbt_delete_min)(ds_prbt_t *tree,
                                             ds_prbt_version_t *base) {
  ds_prbt_node_t *node;

  if (tree == NULL || base == NULL || base->root == NULL) {
    return NULL;
  }
  node = prbt_min_node(base->root);
  return FUNC(ds_prbt_remove)(tree, base, node->key);
}

ds_prbt_version_t *FUNC(ds_prbt_delete_max)(ds_prbt_t *tree,
                                             ds_prbt_version_t *base) {
  ds_prbt_node_t *node;

  if (tree == NULL || base == NULL || base->root == NULL) {
    return NULL;
  }
  node = prbt_max_node(base->root);
  return FUNC(ds_prbt_remove)(tree, base, node->key);
}

ds_status_t FUNC(ds_prbt_visit)(ds_prbt_t *tree, ds_prbt_version_t *version,
                                 ds_prbt_visit_func_t visit, void *user) {
  if (tree == NULL || version == NULL || visit == NULL) {
    return DS_ERR_NULL;
  }
  return prbt_visit_node(tree, version->root, NULL, NULL, visit, user);
}

ds_status_t FUNC(ds_prbt_visit_range)(ds_prbt_t *tree,
                                       ds_prbt_version_t *version,
                                       void *lower, void *upper,
                                       ds_prbt_visit_func_t visit, void *user) {
  if (tree == NULL || version == NULL || visit == NULL) {
    return DS_ERR_NULL;
  }
  if (lower != NULL && upper != NULL &&
      tree->config.compare(lower, upper, tree->config.user) > 0) {
    return DS_ERR_RANGE;
  }
  return prbt_visit_node(tree, version->root, lower, upper, visit, user);
}

ds_status_t FUNC(ds_prbt_floor)(ds_prbt_t *tree, ds_prbt_version_t *version,
                                 void *key, void **out_key,
                                 void **out_value) {
  ds_prbt_node_t *node;

  if (out_key != NULL) {
    *out_key = NULL;
  }
  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (tree == NULL || version == NULL) {
    return DS_ERR_NULL;
  }
  node = prbt_floor_node(tree, version->root, key);
  if (node == NULL) {
    return DS_NOT_FOUND;
  }
  if (out_key != NULL) {
    *out_key = node->key;
  }
  if (out_value != NULL) {
    *out_value = node->value;
  }
  return DS_OK;
}

ds_status_t FUNC(ds_prbt_ceiling)(ds_prbt_t *tree, ds_prbt_version_t *version,
                                   void *key, void **out_key,
                                   void **out_value) {
  ds_prbt_node_t *node;

  if (out_key != NULL) {
    *out_key = NULL;
  }
  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (tree == NULL || version == NULL) {
    return DS_ERR_NULL;
  }
  node = prbt_ceiling_node(tree, version->root, key);
  if (node == NULL) {
    return DS_NOT_FOUND;
  }
  if (out_key != NULL) {
    *out_key = node->key;
  }
  if (out_value != NULL) {
    *out_value = node->value;
  }
  return DS_OK;
}


ds_status_t FUNC(ds_prbt_predecessor)(ds_prbt_t *tree,
                                       ds_prbt_version_t *version, void *key,
                                       void **out_key, void **out_value) {
  ds_prbt_node_t *node;

  if (out_key != NULL) {
    *out_key = NULL;
  }
  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (tree == NULL || version == NULL || out_key == NULL || out_value == NULL) {
    return DS_ERR_NULL;
  }
  node = prbt_predecessor_node(tree, version->root, key);
  if (node == NULL) {
    return DS_NOT_FOUND;
  }
  *out_key = node->key;
  *out_value = node->value;
  return DS_OK;
}

ds_status_t FUNC(ds_prbt_successor)(ds_prbt_t *tree,
                                     ds_prbt_version_t *version, void *key,
                                     void **out_key, void **out_value) {
  ds_prbt_node_t *node;

  if (out_key != NULL) {
    *out_key = NULL;
  }
  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (tree == NULL || version == NULL || out_key == NULL || out_value == NULL) {
    return DS_ERR_NULL;
  }
  node = prbt_successor_node(tree, version->root, key);
  if (node == NULL) {
    return DS_NOT_FOUND;
  }
  *out_key = node->key;
  *out_value = node->value;
  return DS_OK;
}

ds_status_t FUNC(ds_prbt_rank)(ds_prbt_t *tree, ds_prbt_version_t *version,
                                void *key, size_t *out_rank) {
  if (out_rank != NULL) {
    *out_rank = 0U;
  }
  if (tree == NULL || version == NULL || out_rank == NULL) {
    return DS_ERR_NULL;
  }
  *out_rank = prbt_rank_node(tree, version->root, key);
  return DS_OK;
}

ds_status_t FUNC(ds_prbt_select)(ds_prbt_t *tree, ds_prbt_version_t *version,
                                  size_t rank, void **out_key,
                                  void **out_value) {
  ds_prbt_node_t *node;

  if (out_key != NULL) {
    *out_key = NULL;
  }
  if (out_value != NULL) {
    *out_value = NULL;
  }
  if (tree == NULL || version == NULL) {
    return DS_ERR_NULL;
  }
  if (rank >= version->size) {
    return DS_ERR_RANGE;
  }
  node = prbt_select_node(version->root, rank);
  if (node == NULL) {
    return DS_ERR_INTERNAL;
  }
  if (out_key != NULL) {
    *out_key = node->key;
  }
  if (out_value != NULL) {
    *out_value = node->value;
  }
  return DS_OK;
}

size_t FUNC(ds_prbt_size)(ds_prbt_version_t *version) {
  if (version == NULL) {
    return 0U;
  }
  return version->size;
}


ds_status_t FUNC(ds_prbt_validate)(ds_prbt_t *tree,
                                    ds_prbt_version_t *version) {
  size_t size;
  size_t black_count;

  if (tree == NULL || version == NULL) {
    return DS_ERR_NULL;
  }
  if (version->root != NULL && prbt_is_red(version->root)) {
    return DS_ERR_STATE;
  }
  size = 0U;
  black_count = (size_t)-1;
  if (!prbt_validate_node(tree, version->root, NULL, NULL, &size, 0U,
                          &black_count)) {
    return DS_ERR_STATE;
  }
  return size == version->size ? DS_OK : DS_ERR_STATE;
}

void FUNC(ds_prbt_version_retain)(ds_prbt_version_t *version) {
  if (version != NULL) {
    version->refs++;
  }
}

void FUNC(ds_prbt_version_release)(ds_prbt_t *tree,
                                   ds_prbt_version_t *version) {
  if (tree == NULL || version == NULL) {
    return;
  }
  version->refs--;
  if (version->refs != 0UL) {
    return;
  }
  prbt_node_release(tree, version->root);
  ds_context_free(tree->config.context, version);
}
