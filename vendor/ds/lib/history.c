#include "history.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ds_history_operation_node;
struct ds_history_checkpoint_node;
struct ds_history_branch_node;

struct ds_history_operation_node {
  ds_history_operation_t operation;
  struct ds_history_operation_node *next;
};

struct ds_history_checkpoint_node {
  unsigned long time;
  void *state;
  struct ds_history_checkpoint_node *next;
};

struct ds_history_branch {
  ds_history_t *history;
  unsigned long id;
  unsigned long fork_time;
  char *name;
  ds_history_branch_t *parent;
  struct ds_history_operation_node *operations;
  struct ds_history_checkpoint_node *checkpoints;
};

struct ds_history_branch_node {
  ds_history_branch_t *branch;
  struct ds_history_branch_node *next;
};

struct ds_history {
  ds_history_ops_t ops;
  void *config;
  void *clone_data;
  size_t checkpoint_interval;
  unsigned long next_operation_id;
  unsigned long next_branch_id;
  size_t transaction_depth;
  bool transaction_dirty;
  ds_history_branch_t *main_branch;
  struct ds_history_branch_node *branches;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
  mutex_t lock;
  bool is_thread_safe;
};

static void *history_default_allocator(size_t size);
static void history_default_deallocator(void *ptr);
static char *history_strdup(ds_history_t *history, const char *str);
static ds_history_branch_t *history_branch_alloc(ds_history_t *history,
                                                 ds_history_branch_t *parent,
                                                 unsigned long fork_time,
                                                 const char *name);
static void history_branch_destroy(ds_history_branch_t *branch);
static bool history_branch_register(ds_history_t *history,
                                    ds_history_branch_t *branch);
static void history_destroy_operation_list(ds_history_t *history,
                                           struct ds_history_operation_node *op);
static void history_destroy_checkpoint_list(
    ds_history_t *history, struct ds_history_checkpoint_node *checkpoint);
static void history_destroy_checkpoints_from(ds_history_branch_t *branch,
                                             unsigned long time);
static void history_destroy_all_checkpoints(ds_history_branch_t *branch);
static void history_destroy_all_branch_checkpoints(ds_history_t *history);
static void history_note_edit(ds_history_branch_t *branch, unsigned long edit_time);
static void history_invalidate_descendants(ds_history_branch_t *branch,
                                           unsigned long edit_time);
static void history_invalidate_after_edit(ds_history_branch_t *branch,
                                          unsigned long edit_time);
static struct ds_history_checkpoint_node *history_find_checkpoint(
    ds_history_branch_t *branch, unsigned long time);
static struct ds_history_checkpoint_node *history_find_exact_checkpoint(
    ds_history_branch_t *branch, unsigned long time);
static bool history_store_checkpoint(ds_history_branch_t *branch,
                                     unsigned long time, void *state);
static int history_replay_branch_ops(ds_history_branch_t *branch, void *state,
                                     unsigned long lower_time,
                                     bool lower_is_checkpoint,
                                     unsigned long upper_time,
                                     size_t *replayed_count);
static void *history_materialize(ds_history_branch_t *branch,
                                 unsigned long time,
                                 size_t *replayed_count);
static unsigned long history_branch_local_head_time(ds_history_branch_t *branch);
static bool history_branch_has_descendant(ds_history_t *history,
                                          ds_history_branch_t *ancestor,
                                          ds_history_branch_t *branch);
static bool history_time_before_fork(ds_history_branch_t *branch,
                                     unsigned long time);
static unsigned long history_branch_insert_with_id(ds_history_branch_t *branch,
                                                   unsigned long id,
                                                   unsigned long time, int kind,
                                                   void *payload);
static size_t history_branch_operation_count(ds_history_branch_t *branch);

static void *history_default_allocator(size_t size) { return malloc(size); }

static void history_default_deallocator(void *ptr) { free(ptr); }

static char *history_strdup(ds_history_t *history, const char *str) {
  size_t len;
  char *copy;

  if (str == NULL) {
    return NULL;
  }

  len = strlen(str) + 1U;
  copy = (char *)history->allocator(len);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, str, len);
  return copy;
}

static ds_history_branch_t *history_branch_alloc(ds_history_t *history,
                                                 ds_history_branch_t *parent,
                                                 unsigned long fork_time,
                                                 const char *name) {
  ds_history_branch_t *branch;

  branch = (ds_history_branch_t *)history->allocator(sizeof(*branch));
  if (branch == NULL) {
    return NULL;
  }

  branch->history = history;
  branch->id = history->next_branch_id++;
  branch->fork_time = fork_time;
  branch->name = history_strdup(history, name);
  branch->parent = parent;
  branch->operations = NULL;
  branch->checkpoints = NULL;

  if (name != NULL && branch->name == NULL) {
    history->deallocator(branch);
    return NULL;
  }

  return branch;
}

static bool history_branch_register(ds_history_t *history,
                                    ds_history_branch_t *branch) {
  struct ds_history_branch_node *node;

  node = (struct ds_history_branch_node *)history->allocator(sizeof(*node));
  if (node == NULL) {
    return false;
  }

  node->branch = branch;
  node->next = history->branches;
  history->branches = node;
  return true;
}

static void history_destroy_operation_list(ds_history_t *history,
                                           struct ds_history_operation_node *op) {
  struct ds_history_operation_node *next;

  while (op != NULL) {
    next = op->next;
    if (history->ops.destroy_payload != NULL) {
      history->ops.destroy_payload(op->operation.kind, op->operation.payload);
    }
    history->deallocator(op);
    op = next;
  }
}

static void history_destroy_checkpoint_list(
    ds_history_t *history, struct ds_history_checkpoint_node *checkpoint) {
  struct ds_history_checkpoint_node *next;

  while (checkpoint != NULL) {
    next = checkpoint->next;
    if (history->ops.destroy_state != NULL) {
      history->ops.destroy_state(checkpoint->state);
    }
    history->deallocator(checkpoint);
    checkpoint = next;
  }
}

static void history_branch_destroy(ds_history_branch_t *branch) {
  ds_history_t *history;

  if (branch == NULL) {
    return;
  }

  history = branch->history;
  history_destroy_operation_list(history, branch->operations);
  history_destroy_checkpoint_list(history, branch->checkpoints);
  if (branch->name != NULL) {
    history->deallocator(branch->name);
  }
  history->deallocator(branch);
}

static void history_destroy_checkpoints_from(ds_history_branch_t *branch,
                                             unsigned long time) {
  ds_history_t *history;
  struct ds_history_checkpoint_node *checkpoint;
  struct ds_history_checkpoint_node *prev;
  struct ds_history_checkpoint_node *next;

  history = branch->history;
  checkpoint = branch->checkpoints;
  prev = NULL;

  while (checkpoint != NULL) {
    next = checkpoint->next;
    if (checkpoint->time >= time) {
      if (prev == NULL) {
        branch->checkpoints = next;
      } else {
        prev->next = next;
      }
      if (history->ops.destroy_state != NULL) {
        history->ops.destroy_state(checkpoint->state);
      }
      history->deallocator(checkpoint);
    } else {
      prev = checkpoint;
    }
    checkpoint = next;
  }
}

static void history_destroy_all_checkpoints(ds_history_branch_t *branch) {
  ds_history_t *history;

  history = branch->history;
  history_destroy_checkpoint_list(history, branch->checkpoints);
  branch->checkpoints = NULL;
}

static void history_destroy_all_branch_checkpoints(ds_history_t *history) {
  struct ds_history_branch_node *node;

  node = history->branches;
  while (node != NULL) {
    history_destroy_all_checkpoints(node->branch);
    node = node->next;
  }
}

static bool history_branch_has_descendant(ds_history_t *history,
                                          ds_history_branch_t *ancestor,
                                          ds_history_branch_t *branch) {
  ds_history_branch_t *walk;

  (void)history;
  walk = branch->parent;
  while (walk != NULL) {
    if (walk == ancestor) {
      return true;
    }
    walk = walk->parent;
  }

  return false;
}

static void history_invalidate_descendants(ds_history_branch_t *branch,
                                           unsigned long edit_time) {
  ds_history_t *history;
  struct ds_history_branch_node *node;
  ds_history_branch_t *child;

  history = branch->history;
  node = history->branches;

  while (node != NULL) {
    child = node->branch;
    if (child != branch && history_branch_has_descendant(history, branch, child) &&
        child->fork_time >= edit_time) {
      history_destroy_all_checkpoints(child);
    }
    node = node->next;
  }
}

static void history_invalidate_after_edit(ds_history_branch_t *branch,
                                          unsigned long edit_time) {
  history_destroy_checkpoints_from(branch, edit_time);
  history_invalidate_descendants(branch, edit_time);
}

static void history_note_edit(ds_history_branch_t *branch, unsigned long edit_time) {
  ds_history_t *history;

  history = branch->history;
  if (history->transaction_depth != 0U) {
    if (!history->transaction_dirty) {
      history_destroy_all_branch_checkpoints(history);
    }
    history->transaction_dirty = true;
  } else {
    history_invalidate_after_edit(branch, edit_time);
  }
}

static struct ds_history_checkpoint_node *history_find_checkpoint(
    ds_history_branch_t *branch, unsigned long time) {
  struct ds_history_checkpoint_node *checkpoint;
  struct ds_history_checkpoint_node *best;

  checkpoint = branch->checkpoints;
  best = NULL;

  while (checkpoint != NULL) {
    if (checkpoint->time <= time &&
        (best == NULL || checkpoint->time > best->time)) {
      best = checkpoint;
    }
    checkpoint = checkpoint->next;
  }

  return best;
}

static struct ds_history_checkpoint_node *history_find_exact_checkpoint(
    ds_history_branch_t *branch, unsigned long time) {
  struct ds_history_checkpoint_node *checkpoint;

  checkpoint = branch->checkpoints;
  while (checkpoint != NULL) {
    if (checkpoint->time == time) {
      return checkpoint;
    }
    checkpoint = checkpoint->next;
  }

  return NULL;
}

static bool history_store_checkpoint(ds_history_branch_t *branch,
                                     unsigned long time, void *state) {
  ds_history_t *history;
  struct ds_history_checkpoint_node *checkpoint;
  struct ds_history_checkpoint_node *prev;
  struct ds_history_checkpoint_node *walk;
  void *copy;

  history = branch->history;
  if (history->ops.clone_state == NULL) {
    return false;
  }

  copy = history->ops.clone_state(state, history->clone_data);
  if (copy == NULL) {
    return false;
  }

  checkpoint = history_find_exact_checkpoint(branch, time);
  if (checkpoint != NULL) {
    if (history->ops.destroy_state != NULL) {
      history->ops.destroy_state(checkpoint->state);
    }
    checkpoint->state = copy;
    return true;
  }

  checkpoint = (struct ds_history_checkpoint_node *)history->allocator(
      sizeof(*checkpoint));
  if (checkpoint == NULL) {
    if (history->ops.destroy_state != NULL) {
      history->ops.destroy_state(copy);
    }
    return false;
  }

  checkpoint->time = time;
  checkpoint->state = copy;
  checkpoint->next = NULL;

  prev = NULL;
  walk = branch->checkpoints;
  while (walk != NULL && walk->time < time) {
    prev = walk;
    walk = walk->next;
  }

  checkpoint->next = walk;
  if (prev == NULL) {
    branch->checkpoints = checkpoint;
  } else {
    prev->next = checkpoint;
  }

  return true;
}

static int history_replay_branch_ops(ds_history_branch_t *branch, void *state,
                                     unsigned long lower_time,
                                     bool lower_is_checkpoint,
                                     unsigned long upper_time,
                                     size_t *replayed_count) {
  struct ds_history_operation_node *op;
  bool after_lower;
  int rc;

  op = branch->operations;
  while (op != NULL) {
    if (lower_is_checkpoint) {
      after_lower = op->operation.time > lower_time;
    } else {
      after_lower = op->operation.time >= lower_time;
    }

    if (after_lower && op->operation.time <= upper_time) {
      rc = branch->history->ops.apply(state, &op->operation);
      if (rc != 0) {
        return rc;
      }
      if (replayed_count != NULL) {
        (*replayed_count)++;
      }
    }
    op = op->next;
  }

  return 0;
}

static bool history_time_before_fork(ds_history_branch_t *branch,
                                     unsigned long time) {
  return branch->parent != NULL && time < branch->fork_time;
}

static void *history_materialize(ds_history_branch_t *branch,
                                 unsigned long time,
                                 size_t *replayed_count) {
  ds_history_t *history;
  struct ds_history_checkpoint_node *checkpoint;
  void *state;
  unsigned long lower_time;
  bool lower_is_checkpoint;
  int rc;

  history = branch->history;

  if (history_time_before_fork(branch, time)) {
    return history_materialize(branch->parent, time, replayed_count);
  }

  checkpoint = history_find_checkpoint(branch, time);
  if (checkpoint != NULL && history->ops.clone_state != NULL) {
    state = history->ops.clone_state(checkpoint->state, history->clone_data);
    if (state == NULL) {
      return NULL;
    }
    lower_time = checkpoint->time;
    lower_is_checkpoint = true;
  } else {
    if (branch->parent != NULL) {
      state = history_materialize(branch->parent, branch->fork_time,
                                  replayed_count);
    } else {
      state = history->ops.create_state(history->config);
    }
    if (state == NULL) {
      return NULL;
    }
    lower_time = branch->fork_time;
    lower_is_checkpoint = false;
  }

  rc = history_replay_branch_ops(branch, state, lower_time, lower_is_checkpoint,
                                 time, replayed_count);
  if (rc != 0) {
    if (history->ops.destroy_state != NULL) {
      history->ops.destroy_state(state);
    }
    return NULL;
  }

  return state;
}

static unsigned long history_branch_local_head_time(ds_history_branch_t *branch) {
  struct ds_history_operation_node *op;
  unsigned long head;

  head = branch->fork_time;
  op = branch->operations;
  while (op != NULL) {
    if (op->operation.time > head) {
      head = op->operation.time;
    }
    op = op->next;
  }

  return head;
}

ds_history_t *FUNC(ds_history_create)(const ds_history_ops_t *ops,
                                      void *config) {
  return FUNC(ds_history_create_alloc)(ops, config, history_default_allocator,
                                       history_default_deallocator);
}

ds_history_t *FUNC(ds_history_create_alloc)(const ds_history_ops_t *ops,
                                            void *config,
                                            void *(*allocator)(size_t),
                                            void (*deallocator)(void *)) {
  ds_history_t *history;
  ds_history_branch_t *main_branch;

  if (ops == NULL || ops->create_state == NULL || ops->clone_state == NULL ||
      ops->destroy_state == NULL || ops->apply == NULL) {
    return NULL;
  }

  if (allocator == NULL) {
    allocator = history_default_allocator;
  }
  if (deallocator == NULL) {
    deallocator = history_default_deallocator;
  }

  history = (ds_history_t *)allocator(sizeof(*history));
  if (history == NULL) {
    return NULL;
  }

  history->ops = *ops;
  history->config = config;
  history->clone_data = NULL;
  history->checkpoint_interval = 32U;
  history->next_operation_id = 1UL;
  history->next_branch_id = 1UL;
  history->transaction_depth = 0U;
  history->transaction_dirty = false;
  history->main_branch = NULL;
  history->branches = NULL;
  history->allocator = allocator;
  history->deallocator = deallocator;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(history);
#else
  history->is_thread_safe = false;
#endif

  main_branch = history_branch_alloc(history, NULL, 0UL, "main");
  if (main_branch == NULL) {
    if (history->is_thread_safe) {
      LOCK_DESTROY(history);
    }
    deallocator(history);
    return NULL;
  }

  if (!history_branch_register(history, main_branch)) {
    history_branch_destroy(main_branch);
    if (history->is_thread_safe) {
      LOCK_DESTROY(history);
    }
    deallocator(history);
    return NULL;
  }

  history->main_branch = main_branch;
  return history;
}

void FUNC(ds_history_destroy)(ds_history_t *history) {
  struct ds_history_branch_node *node;
  struct ds_history_branch_node *next;

  if (history == NULL) {
    return;
  }

  LOCK(history);
  node = history->branches;
  while (node != NULL) {
    next = node->next;
    history_branch_destroy(node->branch);
    history->deallocator(node);
    node = next;
  }
  history->branches = NULL;
  UNLOCK(history);
  if (history->is_thread_safe) {
    LOCK_DESTROY(history);
  }
  history->deallocator(history);
}

void FUNC(ds_history_set_clone_data)(ds_history_t *history, void *clone_data) {
  if (history == NULL) {
    return;
  }

  LOCK(history);
  history->clone_data = clone_data;
  UNLOCK(history);
}

void FUNC(ds_history_set_checkpoint_interval)(ds_history_t *history,
                                              size_t interval) {
  if (history == NULL) {
    return;
  }

  LOCK(history);
  history->checkpoint_interval = interval;
  UNLOCK(history);
}


void FUNC(ds_history_transaction_begin)(ds_history_t *history) {
  if (history == NULL) {
    return;
  }
  LOCK(history);
  history->transaction_depth++;
  UNLOCK(history);
}

void FUNC(ds_history_transaction_commit)(ds_history_t *history) {
  if (history == NULL) {
    return;
  }
  LOCK(history);
  if (history->transaction_depth != 0U) {
    history->transaction_depth--;
    if (history->transaction_depth == 0U) {
      history->transaction_dirty = false;
    }
  }
  UNLOCK(history);
}

bool FUNC(ds_history_in_transaction)(ds_history_t *history) {
  bool result;
  if (history == NULL) {
    return false;
  }
  LOCK(history);
  result = history->transaction_depth != 0U;
  UNLOCK(history);
  return result;
}

const ds_history_ops_t *FUNC(ds_history_get_ops)(ds_history_t *history) {
  if (history == NULL) {
    return NULL;
  }
  return &history->ops;
}

void *FUNC(ds_history_get_config)(ds_history_t *history) {
  if (history == NULL) {
    return NULL;
  }
  return history->config;
}

ds_history_branch_t *FUNC(ds_history_main)(ds_history_t *history) {
  if (history == NULL) {
    return NULL;
  }
  return history->main_branch;
}

ds_history_branch_t *FUNC(ds_history_branch_create)(ds_history_t *history,
                                                    ds_history_branch_t *parent,
                                                    unsigned long fork_time,
                                                    const char *name) {
  ds_history_branch_t *branch;

  if (history == NULL) {
    return NULL;
  }

  LOCK(history);
  if (parent == NULL) {
    parent = history->main_branch;
  }
  if (parent == NULL || parent->history != history) {
    UNLOCK(history);
    return NULL;
  }

  branch = history_branch_alloc(history, parent, fork_time, name);
  if (branch == NULL) {
    UNLOCK(history);
    return NULL;
  }

  if (!history_branch_register(history, branch)) {
    history_branch_destroy(branch);
    UNLOCK(history);
    return NULL;
  }

  UNLOCK(history);
  return branch;
}

const char *FUNC(ds_history_branch_name)(ds_history_branch_t *branch) {
  if (branch == NULL) {
    return NULL;
  }
  return branch->name;
}

unsigned long FUNC(ds_history_branch_id)(ds_history_branch_t *branch) {
  if (branch == NULL) {
    return 0UL;
  }
  return branch->id;
}

unsigned long FUNC(ds_history_branch_fork_time)(ds_history_branch_t *branch) {
  if (branch == NULL) {
    return 0UL;
  }
  return branch->fork_time;
}

unsigned long FUNC(ds_history_branch_head_time)(ds_history_branch_t *branch) {
  if (branch == NULL) {
    return 0UL;
  }
  return history_branch_local_head_time(branch);
}

ds_history_branch_t *FUNC(ds_history_branch_parent)(ds_history_branch_t *branch) {
  if (branch == NULL) {
    return NULL;
  }
  return branch->parent;
}

ds_history_branch_t *FUNC(ds_history_branch_find_name)(ds_history_t *history,
                                                        const char *name) {
  struct ds_history_branch_node *node;

  if (history == NULL || name == NULL) {
    return NULL;
  }
  LOCK(history);
  node = history->branches;
  while (node != NULL) {
    if (node->branch->name != NULL && strcmp(node->branch->name, name) == 0) {
      UNLOCK(history);
      return node->branch;
    }
    node = node->next;
  }
  UNLOCK(history);
  return NULL;
}

ds_history_operation_t *FUNC(ds_history_branch_find_op)(
    ds_history_branch_t *branch, unsigned long op_id) {
  struct ds_history_operation_node *op;

  if (branch == NULL) {
    return NULL;
  }

  op = branch->operations;
  while (op != NULL) {
    if (op->operation.id == op_id) {
      return &op->operation;
    }
    op = op->next;
  }

  return NULL;
}

static unsigned long history_branch_insert_with_id(ds_history_branch_t *branch,
                                                   unsigned long id,
                                                   unsigned long time, int kind,
                                                   void *payload) {
  ds_history_t *history;
  struct ds_history_operation_node *node;
  struct ds_history_operation_node *walk;
  struct ds_history_operation_node *prev;

  if (branch == NULL || id == 0UL) {
    return 0UL;
  }

  history = branch->history;

  if (time < branch->fork_time) {
    return 0UL;
  }

  node = (struct ds_history_operation_node *)history->allocator(sizeof(*node));
  if (node == NULL) {
    return 0UL;
  }

  node->operation.id = id;
  node->operation.time = time;
  node->operation.kind = kind;
  node->operation.payload = payload;
  node->next = NULL;

  prev = NULL;
  walk = branch->operations;
  while (walk != NULL &&
         (walk->operation.time < time ||
          (walk->operation.time == time &&
           walk->operation.id < node->operation.id))) {
    prev = walk;
    walk = walk->next;
  }

  node->next = walk;
  if (prev == NULL) {
    branch->operations = node;
  } else {
    prev->next = node;
  }

  if (history->next_operation_id <= id) {
    history->next_operation_id = id + 1UL;
  }
  history_note_edit(branch, time);
  return node->operation.id;
}

unsigned long FUNC(ds_history_branch_insert_at)(ds_history_branch_t *branch,
                                                unsigned long time, int kind,
                                                void *payload) {
  ds_history_t *history;
  unsigned long id;

  if (branch == NULL) {
    return 0UL;
  }

  history = branch->history;
  LOCK(history);
  id = history->next_operation_id++;
  if (history_branch_insert_with_id(branch, id, time, kind, payload) == 0UL) {
    if (history->next_operation_id == id + 1UL) {
      history->next_operation_id = id;
    }
    UNLOCK(history);
    return 0UL;
  }
  UNLOCK(history);
  return id;
}

unsigned long FUNC(ds_history_branch_append)(ds_history_branch_t *branch,
                                             int kind, void *payload) {
  ds_history_t *history;
  unsigned long time;
  unsigned long id;

  if (branch == NULL) {
    return 0UL;
  }

  history = branch->history;
  LOCK(history);
  time = history_branch_local_head_time(branch);
  if (time != ~0UL) {
    time++;
  }
  id = history->next_operation_id++;
  if (history_branch_insert_with_id(branch, id, time, kind, payload) == 0UL) {
    if (history->next_operation_id == id + 1UL) {
      history->next_operation_id = id;
    }
    UNLOCK(history);
    return 0UL;
  }
  UNLOCK(history);
  return id;
}

bool FUNC(ds_history_branch_delete_op)(ds_history_branch_t *branch,
                                       unsigned long op_id) {
  ds_history_t *history;
  struct ds_history_operation_node *op;
  struct ds_history_operation_node *prev;
  unsigned long edit_time;

  if (branch == NULL) {
    return false;
  }

  history = branch->history;
  LOCK(history);

  op = branch->operations;
  prev = NULL;
  while (op != NULL && op->operation.id != op_id) {
    prev = op;
    op = op->next;
  }

  if (op == NULL) {
    UNLOCK(history);
    return false;
  }

  edit_time = op->operation.time;
  if (prev == NULL) {
    branch->operations = op->next;
  } else {
    prev->next = op->next;
  }

  if (history->ops.destroy_payload != NULL) {
    history->ops.destroy_payload(op->operation.kind, op->operation.payload);
  }
  history->deallocator(op);

  history_note_edit(branch, edit_time);
  UNLOCK(history);
  return true;
}

size_t FUNC(ds_history_branch_merge_with)(
    ds_history_branch_t *target, ds_history_branch_t *source,
    unsigned long from_time, unsigned long through_time, unsigned int flags,
    ds_history_merge_policy_func_t policy, void *policy_user) {
  ds_history_t *history;
  struct ds_history_operation_node *op;
  void *payload;
  unsigned long time;
  unsigned long append_time;
  int kind;
  size_t merged;
  unsigned int action;

  if (target == NULL || source == NULL || target == source ||
      target->history != source->history || from_time > through_time) {
    return 0U;
  }

  history = target->history;
  merged = 0U;
  LOCK(history);
  append_time = history_branch_local_head_time(target);
  if (append_time < target->fork_time) {
    append_time = target->fork_time;
  }

  op = source->operations;
  while (op != NULL) {
    if (op->operation.time >= from_time && op->operation.time <= through_time) {
      if (history->ops.clone_payload != NULL) {
        payload = history->ops.clone_payload(op->operation.kind,
                                             op->operation.payload,
                                             history->clone_data);
        if (payload == NULL && op->operation.payload != NULL) {
          break;
        }
      } else {
        if (history->ops.destroy_payload != NULL &&
            op->operation.payload != NULL) {
          break;
        }
        payload = op->operation.payload;
      }

      if ((flags & DS_HISTORY_MERGE_APPEND_AFTER_HEAD) != 0U) {
        append_time++;
        time = append_time;
      } else {
        time = op->operation.time;
      }
      kind = op->operation.kind;
      action = DS_HISTORY_MERGE_TAKE;
      if (policy != NULL) {
        action = policy(target, source, &op->operation, &time, &kind, &payload,
                        policy_user);
      }
      if (action == DS_HISTORY_MERGE_STOP || action == DS_HISTORY_MERGE_SKIP) {
        if (history->ops.destroy_payload != NULL &&
            payload != op->operation.payload) {
          history->ops.destroy_payload(kind, payload);
        }
        if (action == DS_HISTORY_MERGE_STOP) {
          break;
        }
        op = op->next;
        continue;
      }

      if (history_branch_insert_with_id(target, history->next_operation_id++,
                                        time, kind, payload) == 0UL) {
        if (history->ops.destroy_payload != NULL &&
            payload != op->operation.payload) {
          history->ops.destroy_payload(kind, payload);
        }
        break;
      }
      history_note_edit(target, time);
      merged++;
    }
    op = op->next;
  }
  UNLOCK(history);
  return merged;
}

size_t FUNC(ds_history_branch_merge)(ds_history_branch_t *target,
                                      ds_history_branch_t *source,
                                      unsigned long from_time,
                                      unsigned long through_time,
                                      unsigned int flags) {
  return FUNC(ds_history_branch_merge_with)(target, source, from_time,
                                            through_time, flags, NULL, NULL);
}

void *FUNC(ds_history_branch_snapshot_at)(ds_history_branch_t *branch,
                                          unsigned long time) {
  ds_history_t *history;
  void *state;
  size_t replayed_count;

  if (branch == NULL) {
    return NULL;
  }

  history = branch->history;
  LOCK(history);
  replayed_count = 0U;
  state = history_materialize(branch, time, &replayed_count);
  if (state != NULL && history->checkpoint_interval != 0U &&
      replayed_count >= history->checkpoint_interval) {
    (void)history_store_checkpoint(branch, time, state);
  }
  UNLOCK(history);
  return state;
}

void *FUNC(ds_history_branch_snapshot_head)(ds_history_branch_t *branch) {
  if (branch == NULL) {
    return NULL;
  }
  return FUNC(ds_history_branch_snapshot_at)(branch,
                                             FUNC(ds_history_branch_head_time)(branch));
}


void FUNC(ds_history_snapshot_destroy)(ds_history_t *history, void *snapshot) {
  if (history == NULL || snapshot == NULL) {
    return;
  }

  history->ops.destroy_state(snapshot);
}

void *FUNC(ds_history_branch_query_at)(ds_history_branch_t *branch,
                                       unsigned long time,
                                       const ds_history_query_t *query) {
  ds_history_t *history;
  void *state;
  void *result;
  size_t replayed_count;

  if (branch == NULL || query == NULL) {
    return NULL;
  }

  history = branch->history;
  if (history->ops.query == NULL) {
    return NULL;
  }

  LOCK(history);
  replayed_count = 0U;
  state = history_materialize(branch, time, &replayed_count);
  if (state == NULL) {
    UNLOCK(history);
    return NULL;
  }

  if (history->checkpoint_interval != 0U &&
      replayed_count >= history->checkpoint_interval) {
    (void)history_store_checkpoint(branch, time, state);
  }

  result = history->ops.query(state, query);
  history->ops.destroy_state(state);
  UNLOCK(history);
  return result;
}

void *FUNC(ds_history_branch_query_head)(ds_history_branch_t *branch,
                                         const ds_history_query_t *query) {
  if (branch == NULL) {
    return NULL;
  }
  return FUNC(ds_history_branch_query_at)(branch,
                                          FUNC(ds_history_branch_head_time)(branch),
                                          query);
}


size_t FUNC(ds_history_branch_export_ops)(ds_history_branch_t *branch,
                                           ds_history_export_op_func_t writer,
                                           void *user) {
  struct ds_history_operation_node *op;
  size_t count;

  if (branch == NULL || writer == NULL) {
    return 0U;
  }
  count = 0U;
  op = branch->operations;
  while (op != NULL) {
    if (!writer(&op->operation, user)) {
      break;
    }
    count++;
    op = op->next;
  }
  return count;
}


static size_t history_branch_operation_count(ds_history_branch_t *branch) {
  struct ds_history_operation_node *op;
  size_t count;
  count = 0U;
  if (branch == NULL) {
    return 0U;
  }
  op = branch->operations;
  while (op != NULL) {
    count++;
    op = op->next;
  }
  return count;
}

bool FUNC(ds_history_write_ulong)(ds_history_write_func_t write, void *io_user,
                                   unsigned long value) {
  unsigned char bytes[8];
  size_t i;

  if (write == NULL) {
    return false;
  }
  for (i = 0U; i < 8U; i++) {
    bytes[i] = (unsigned char)(value & 0xffUL);
    value >>= 8;
  }
  return write(bytes, sizeof(bytes), io_user);
}

bool FUNC(ds_history_read_ulong)(ds_history_read_func_t read, void *io_user,
                                  unsigned long *out_value) {
  unsigned char bytes[8];
  unsigned long value;
  size_t i;
  size_t ulong_bytes;

  if (read == NULL || out_value == NULL) {
    return false;
  }
  if (!read(bytes, sizeof(bytes), io_user)) {
    return false;
  }
  value = 0UL;
  ulong_bytes = sizeof(unsigned long);
  for (i = 0U; i < 8U; i++) {
    if (i >= ulong_bytes) {
      if (bytes[i] != 0U) {
        return false;
      }
    } else {
      value |= ((unsigned long)bytes[i]) << (i * 8U);
    }
  }
  *out_value = value;
  return true;
}

bool FUNC(ds_history_write_int)(ds_history_write_func_t write, void *io_user,
                                 int value) {
  unsigned long encoded;
  long signed_value;

  signed_value = (long)value;
  if (signed_value < 0L) {
    encoded = ((unsigned long)(-(signed_value + 1L)) * 2UL) + 1UL;
  } else {
    encoded = ((unsigned long)signed_value) * 2UL;
  }
  return FUNC(ds_history_write_ulong)(write, io_user, encoded);
}

bool FUNC(ds_history_read_int)(ds_history_read_func_t read, void *io_user,
                                int *out_value) {
  unsigned long encoded;
  unsigned long magnitude;
  long signed_value;

  if (out_value == NULL) {
    return false;
  }
  if (!FUNC(ds_history_read_ulong)(read, io_user, &encoded)) {
    return false;
  }
  magnitude = encoded / 2UL;
  if ((encoded & 1UL) != 0UL) {
    if (magnitude > (unsigned long)LONG_MAX) {
      return false;
    }
    signed_value = -((long)magnitude) - 1L;
  } else {
    if (magnitude > (unsigned long)INT_MAX) {
      return false;
    }
    signed_value = (long)magnitude;
  }
  if (signed_value < (long)INT_MIN || signed_value > (long)INT_MAX) {
    return false;
  }
  *out_value = (int)signed_value;
  return true;
}

bool FUNC(ds_history_write_size)(ds_history_write_func_t write, void *io_user,
                                  size_t value) {
  if (value > (size_t)ULONG_MAX) {
    return false;
  }
  return FUNC(ds_history_write_ulong)(write, io_user, (unsigned long)value);
}

bool FUNC(ds_history_read_size)(ds_history_read_func_t read, void *io_user,
                                 size_t *out_value) {
  unsigned long value;

  if (out_value == NULL) {
    return false;
  }
  if (!FUNC(ds_history_read_ulong)(read, io_user, &value)) {
    return false;
  }
  *out_value = (size_t)value;
  return true;
}

size_t FUNC(ds_history_branch_serialize_ops)(
    ds_history_branch_t *branch, ds_history_write_func_t write,
    ds_history_payload_write_func_t write_payload, void *io_user,
    void *payload_user) {
  struct ds_history_operation_node *op;
  size_t count;
  size_t written;

  if (branch == NULL || write == NULL || write_payload == NULL) {
    return 0U;
  }

  count = history_branch_operation_count(branch);
  if (!write(&count, sizeof(count), io_user)) {
    return 0U;
  }

  written = 0U;
  op = branch->operations;
  while (op != NULL) {
    if (!write(&op->operation.id, sizeof(op->operation.id), io_user) ||
        !write(&op->operation.time, sizeof(op->operation.time), io_user) ||
        !write(&op->operation.kind, sizeof(op->operation.kind), io_user) ||
        !write_payload(&op->operation, write, io_user, payload_user)) {
      break;
    }
    written++;
    op = op->next;
  }
  return written;
}

size_t FUNC(ds_history_branch_deserialize_ops)(
    ds_history_branch_t *branch, ds_history_read_func_t read,
    ds_history_payload_read_func_t read_payload, void *io_user,
    void *payload_user) {
  ds_history_t *history;
  size_t count;
  size_t imported;
  unsigned long id;
  unsigned long time;
  int kind;
  void *payload;

  if (branch == NULL || read == NULL || read_payload == NULL) {
    return 0U;
  }
  if (!read(&count, sizeof(count), io_user)) {
    return 0U;
  }

  history = branch->history;
  imported = 0U;
  LOCK(history);
  while (imported < count) {
    if (!read(&id, sizeof(id), io_user) || !read(&time, sizeof(time), io_user) ||
        !read(&kind, sizeof(kind), io_user)) {
      break;
    }
    payload = read_payload(id, time, kind, read, io_user, payload_user);
    if (payload == NULL && read_payload != NULL) {
      break;
    }
    if (history_branch_insert_with_id(branch, id, time, kind, payload) == 0UL) {
      if (history->ops.destroy_payload != NULL) {
        history->ops.destroy_payload(kind, payload);
      }
      break;
    }
    imported++;
  }
  UNLOCK(history);
  return imported;
}

static size_t history_branch_count(ds_history_t *history) {
  struct ds_history_branch_node *node;
  size_t count;

  count = 0U;
  node = history->branches;
  while (node != NULL) {
    count++;
    node = node->next;
  }
  return count;
}

static ds_history_branch_t *history_find_branch_by_id(ds_history_t *history,
                                                       unsigned long id) {
  struct ds_history_branch_node *node;

  node = history->branches;
  while (node != NULL) {
    if (node->branch->id == id) {
      return node->branch;
    }
    node = node->next;
  }
  return NULL;
}

static bool history_write_branch_ops(
    ds_history_branch_t *branch, ds_history_write_func_t write,
    ds_history_payload_write_func_t write_payload, void *io_user,
    void *payload_user) {
  struct ds_history_operation_node *op;
  size_t count;

  count = history_branch_operation_count(branch);
  if (!write(&count, sizeof(count), io_user)) {
    return false;
  }
  op = branch->operations;
  while (op != NULL) {
    if (!write(&op->operation.id, sizeof(op->operation.id), io_user) ||
        !write(&op->operation.time, sizeof(op->operation.time), io_user) ||
        !write(&op->operation.kind, sizeof(op->operation.kind), io_user) ||
        !write_payload(&op->operation, write, io_user, payload_user)) {
      return false;
    }
    op = op->next;
  }
  return true;
}

static bool history_serialize_branch_recursive(
    ds_history_branch_t *branch, ds_history_write_func_t write,
    ds_history_payload_write_func_t write_payload, void *io_user,
    void *payload_user) {
  ds_history_t *history;
  struct ds_history_branch_node *node;
  unsigned long parent_id;
  size_t name_len;

  history = branch->history;
  parent_id = branch->parent == NULL ? 0UL : branch->parent->id;
  name_len = branch->name == NULL ? 0U : strlen(branch->name);

  if (!write(&branch->id, sizeof(branch->id), io_user) ||
      !write(&parent_id, sizeof(parent_id), io_user) ||
      !write(&branch->fork_time, sizeof(branch->fork_time), io_user) ||
      !write(&name_len, sizeof(name_len), io_user)) {
    return false;
  }
  if (name_len != 0U && !write(branch->name, name_len, io_user)) {
    return false;
  }
  if (!history_write_branch_ops(branch, write, write_payload, io_user,
                                payload_user)) {
    return false;
  }

  node = history->branches;
  while (node != NULL) {
    if (node->branch->parent == branch) {
      if (!history_serialize_branch_recursive(node->branch, write,
                                              write_payload, io_user,
                                              payload_user)) {
        return false;
      }
    }
    node = node->next;
  }
  return true;
}

static char *history_read_name(ds_history_t *history, ds_history_read_func_t read,
                               void *io_user, size_t name_len) {
  char *name;

  if (name_len == 0U) {
    return NULL;
  }
  name = (char *)history->allocator(name_len + 1U);
  if (name == NULL) {
    return NULL;
  }
  if (!read(name, name_len, io_user)) {
    history->deallocator(name);
    return NULL;
  }
  name[name_len] = '\0';
  return name;
}

static bool history_read_branch_ops(
    ds_history_branch_t *branch, ds_history_read_func_t read,
    ds_history_payload_read_func_t read_payload, void *io_user,
    void *payload_user) {
  ds_history_t *history;
  size_t count;
  size_t i;
  unsigned long id;
  unsigned long time;
  int kind;
  void *payload;

  history = branch->history;
  if (!read(&count, sizeof(count), io_user)) {
    return false;
  }
  for (i = 0U; i < count; i++) {
    if (!read(&id, sizeof(id), io_user) || !read(&time, sizeof(time), io_user) ||
        !read(&kind, sizeof(kind), io_user)) {
      return false;
    }
    payload = read_payload(id, time, kind, read, io_user, payload_user);
    if (history_branch_insert_with_id(branch, id, time, kind, payload) == 0UL) {
      if (history->ops.destroy_payload != NULL) {
        history->ops.destroy_payload(kind, payload);
      }
      return false;
    }
  }
  return true;
}

bool FUNC(ds_history_serialize)(ds_history_t *history,
                                 ds_history_write_func_t write,
                                 ds_history_payload_write_func_t write_payload,
                                 void *io_user, void *payload_user) {
  static const unsigned char magic[8] = {'D', 'S', 'H', 'I', 'S', 'T', '3', 0};
  size_t branch_count;
  bool ok;

  if (history == NULL || write == NULL || write_payload == NULL) {
    return false;
  }

  LOCK(history);
  branch_count = history_branch_count(history);
  ok = write(magic, sizeof(magic), io_user) &&
       write(&branch_count, sizeof(branch_count), io_user) &&
       write(&history->next_operation_id, sizeof(history->next_operation_id),
             io_user) &&
       write(&history->next_branch_id, sizeof(history->next_branch_id),
             io_user) &&
       write(&history->checkpoint_interval, sizeof(history->checkpoint_interval),
             io_user) &&
       history_serialize_branch_recursive(history->main_branch, write,
                                          write_payload, io_user, payload_user);
  UNLOCK(history);
  return ok;
}

ds_history_t *FUNC(ds_history_deserialize)(
    const ds_history_ops_t *ops, void *config, ds_history_read_func_t read,
    ds_history_payload_read_func_t read_payload, void *io_user,
    void *payload_user) {
  static const unsigned char expected_magic[8] = {'D', 'S', 'H', 'I', 'S', 'T',
                                                  '3', 0};
  unsigned char magic[8];
  ds_history_t *history;
  ds_history_branch_t *branch;
  ds_history_branch_t *parent;
  size_t branch_count;
  size_t i;
  unsigned long next_operation_id;
  unsigned long next_branch_id;
  size_t checkpoint_interval;
  unsigned long id;
  unsigned long parent_id;
  unsigned long fork_time;
  size_t name_len;
  char *name;

  if (ops == NULL || read == NULL || read_payload == NULL) {
    return NULL;
  }
  if (!read(magic, sizeof(magic), io_user) ||
      memcmp(magic, expected_magic, sizeof(magic)) != 0 ||
      !read(&branch_count, sizeof(branch_count), io_user) ||
      !read(&next_operation_id, sizeof(next_operation_id), io_user) ||
      !read(&next_branch_id, sizeof(next_branch_id), io_user) ||
      !read(&checkpoint_interval, sizeof(checkpoint_interval), io_user)) {
    return NULL;
  }
  if (branch_count == 0U) {
    return NULL;
  }

  history = FUNC(ds_history_create)(ops, config);
  if (history == NULL) {
    return NULL;
  }
  history->checkpoint_interval = checkpoint_interval;

  for (i = 0U; i < branch_count; i++) {
    if (!read(&id, sizeof(id), io_user) ||
        !read(&parent_id, sizeof(parent_id), io_user) ||
        !read(&fork_time, sizeof(fork_time), io_user) ||
        !read(&name_len, sizeof(name_len), io_user)) {
      FUNC(ds_history_destroy)(history);
      return NULL;
    }
    name = history_read_name(history, read, io_user, name_len);
    if (name_len != 0U && name == NULL) {
      FUNC(ds_history_destroy)(history);
      return NULL;
    }

    if (parent_id == 0UL) {
      branch = history->main_branch;
      if (branch == NULL) {
        if (name != NULL) {
          history->deallocator(name);
        }
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
      if (branch->name != NULL) {
        history->deallocator(branch->name);
      }
      branch->name = name;
      branch->id = id;
      branch->fork_time = fork_time;
    } else {
      parent = history_find_branch_by_id(history, parent_id);
      if (parent == NULL) {
        if (name != NULL) {
          history->deallocator(name);
        }
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
      branch = history_branch_alloc(history, parent, fork_time, name);
      if (name != NULL) {
        history->deallocator(name);
      }
      if (branch == NULL) {
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
      branch->id = id;
      if (!history_branch_register(history, branch)) {
        history_branch_destroy(branch);
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
    }

    if (history->next_branch_id <= id) {
      history->next_branch_id = id + 1UL;
    }
    if (!history_read_branch_ops(branch, read, read_payload, io_user,
                                 payload_user)) {
      FUNC(ds_history_destroy)(history);
      return NULL;
    }
  }

  if (history->next_operation_id < next_operation_id) {
    history->next_operation_id = next_operation_id;
  }
  if (history->next_branch_id < next_branch_id) {
    history->next_branch_id = next_branch_id;
  }
  return history;
}

static bool history_write_branch_ops_portable(
    ds_history_branch_t *branch, ds_history_write_func_t write,
    ds_history_payload_write_func_t write_payload, void *io_user,
    void *payload_user) {
  struct ds_history_operation_node *op;
  size_t count;

  count = history_branch_operation_count(branch);
  if (!FUNC(ds_history_write_size)(write, io_user, count)) {
    return false;
  }
  op = branch->operations;
  while (op != NULL) {
    if (!FUNC(ds_history_write_ulong)(write, io_user, op->operation.id) ||
        !FUNC(ds_history_write_ulong)(write, io_user, op->operation.time) ||
        !FUNC(ds_history_write_int)(write, io_user, op->operation.kind) ||
        !write_payload(&op->operation, write, io_user, payload_user)) {
      return false;
    }
    op = op->next;
  }
  return true;
}

static bool history_serialize_branch_recursive_portable(
    ds_history_branch_t *branch, ds_history_write_func_t write,
    ds_history_payload_write_func_t write_payload, void *io_user,
    void *payload_user) {
  ds_history_t *history;
  struct ds_history_branch_node *node;
  unsigned long parent_id;
  size_t name_len;

  history = branch->history;
  parent_id = branch->parent == NULL ? 0UL : branch->parent->id;
  name_len = branch->name == NULL ? 0U : strlen(branch->name);

  if (!FUNC(ds_history_write_ulong)(write, io_user, branch->id) ||
      !FUNC(ds_history_write_ulong)(write, io_user, parent_id) ||
      !FUNC(ds_history_write_ulong)(write, io_user, branch->fork_time) ||
      !FUNC(ds_history_write_size)(write, io_user, name_len)) {
    return false;
  }
  if (name_len != 0U && !write(branch->name, name_len, io_user)) {
    return false;
  }
  if (!history_write_branch_ops_portable(branch, write, write_payload, io_user,
                                         payload_user)) {
    return false;
  }

  node = history->branches;
  while (node != NULL) {
    if (node->branch->parent == branch) {
      if (!history_serialize_branch_recursive_portable(node->branch, write,
                                                       write_payload, io_user,
                                                       payload_user)) {
        return false;
      }
    }
    node = node->next;
  }
  return true;
}

static bool history_read_branch_ops_portable(
    ds_history_branch_t *branch, ds_history_read_func_t read,
    ds_history_payload_read_func_t read_payload, void *io_user,
    void *payload_user) {
  ds_history_t *history;
  size_t count;
  size_t i;
  unsigned long id;
  unsigned long time;
  int kind;
  void *payload;

  history = branch->history;
  if (!FUNC(ds_history_read_size)(read, io_user, &count)) {
    return false;
  }
  for (i = 0U; i < count; i++) {
    if (!FUNC(ds_history_read_ulong)(read, io_user, &id) ||
        !FUNC(ds_history_read_ulong)(read, io_user, &time) ||
        !FUNC(ds_history_read_int)(read, io_user, &kind)) {
      return false;
    }
    payload = read_payload(id, time, kind, read, io_user, payload_user);
    if (history_branch_insert_with_id(branch, id, time, kind, payload) == 0UL) {
      if (history->ops.destroy_payload != NULL) {
        history->ops.destroy_payload(kind, payload);
      }
      return false;
    }
  }
  return true;
}

static void *history_archive_default_allocator(size_t size) {
  return malloc(size);
}

static void history_archive_default_deallocator(void *ptr) { free(ptr); }

void FUNC(ds_history_archive_init)(ds_history_archive_t *archive) {
  FUNC(ds_history_archive_init_alloc)(archive, history_archive_default_allocator,
                                      history_archive_default_deallocator);
}

void FUNC(ds_history_archive_init_alloc)(ds_history_archive_t *archive,
                                          void *(*allocator)(size_t),
                                          void (*deallocator)(void *)) {
  if (archive == NULL) {
    return;
  }
  archive->data = NULL;
  archive->size = 0U;
  archive->capacity = 0U;
  archive->offset = 0U;
  archive->allocator = allocator != NULL ? allocator : history_archive_default_allocator;
  archive->deallocator = deallocator != NULL ? deallocator : history_archive_default_deallocator;
}

void FUNC(ds_history_archive_destroy)(ds_history_archive_t *archive) {
  if (archive == NULL) {
    return;
  }
  if (archive->data != NULL) {
    archive->deallocator(archive->data);
  }
  FUNC(ds_history_archive_init_alloc)(archive, archive->allocator,
                                      archive->deallocator);
}

void FUNC(ds_history_archive_clear)(ds_history_archive_t *archive) {
  if (archive == NULL) {
    return;
  }
  archive->size = 0U;
  archive->offset = 0U;
}

void FUNC(ds_history_archive_rewind)(ds_history_archive_t *archive) {
  if (archive != NULL) {
    archive->offset = 0U;
  }
}

const unsigned char *FUNC(ds_history_archive_data)(
    const ds_history_archive_t *archive) {
  return archive == NULL ? NULL : archive->data;
}

size_t FUNC(ds_history_archive_size)(const ds_history_archive_t *archive) {
  return archive == NULL ? 0U : archive->size;
}

bool FUNC(ds_history_archive_write)(const void *data, size_t size, void *user) {
  ds_history_archive_t *archive;
  unsigned char *next;
  size_t capacity;

  archive = (ds_history_archive_t *)user;
  if (archive == NULL || (data == NULL && size != 0U)) {
    return false;
  }
  if (size > ((size_t)-1) - archive->size) {
    return false;
  }
  if (archive->size + size > archive->capacity) {
    capacity = archive->capacity == 0U ? 64U : archive->capacity;
    while (capacity < archive->size + size) {
      if (capacity > ((size_t)-1) / 2U) {
        return false;
      }
      capacity *= 2U;
    }
    next = (unsigned char *)archive->allocator(capacity);
    if (next == NULL) {
      return false;
    }
    if (archive->data != NULL && archive->size != 0U) {
      memcpy(next, archive->data, archive->size);
    }
    if (archive->data != NULL) {
      archive->deallocator(archive->data);
    }
    archive->data = next;
    archive->capacity = capacity;
  }
  if (size != 0U) {
    memcpy(archive->data + archive->size, data, size);
  }
  archive->size += size;
  return true;
}

bool FUNC(ds_history_archive_read)(void *data, size_t size, void *user) {
  ds_history_archive_t *archive;

  archive = (ds_history_archive_t *)user;
  if (archive == NULL || (data == NULL && size != 0U)) {
    return false;
  }
  if (size > archive->size - archive->offset) {
    return false;
  }
  if (size != 0U) {
    memcpy(data, archive->data + archive->offset, size);
  }
  archive->offset += size;
  return true;
}

bool FUNC(ds_history_serialize_portable)(
    ds_history_t *history, ds_history_write_func_t write,
    ds_history_payload_write_func_t write_payload, void *io_user,
    void *payload_user) {
  static const unsigned char magic[8] = {'D', 'S', 'H', 'I', 'S', 'T', '4', 0};
  size_t branch_count;
  bool ok;

  if (history == NULL || write == NULL || write_payload == NULL) {
    return false;
  }

  LOCK(history);
  branch_count = history_branch_count(history);
  ok = write(magic, sizeof(magic), io_user) &&
       FUNC(ds_history_write_size)(write, io_user, branch_count) &&
       FUNC(ds_history_write_ulong)(write, io_user,
                                    history->next_operation_id) &&
       FUNC(ds_history_write_ulong)(write, io_user, history->next_branch_id) &&
       FUNC(ds_history_write_size)(write, io_user,
                                   history->checkpoint_interval) &&
       history_serialize_branch_recursive_portable(
           history->main_branch, write, write_payload, io_user, payload_user);
  UNLOCK(history);
  return ok;
}

ds_history_t *FUNC(ds_history_deserialize_portable)(
    const ds_history_ops_t *ops, void *config, ds_history_read_func_t read,
    ds_history_payload_read_func_t read_payload, void *io_user,
    void *payload_user) {
  static const unsigned char expected_magic[8] = {'D', 'S', 'H', 'I', 'S', 'T',
                                                  '4', 0};
  unsigned char magic[8];
  ds_history_t *history;
  ds_history_branch_t *branch;
  ds_history_branch_t *parent;
  size_t branch_count;
  size_t i;
  unsigned long next_operation_id;
  unsigned long next_branch_id;
  size_t checkpoint_interval;
  unsigned long id;
  unsigned long parent_id;
  unsigned long fork_time;
  size_t name_len;
  char *name;

  if (ops == NULL || read == NULL || read_payload == NULL) {
    return NULL;
  }
  if (!read(magic, sizeof(magic), io_user) ||
      memcmp(magic, expected_magic, sizeof(magic)) != 0 ||
      !FUNC(ds_history_read_size)(read, io_user, &branch_count) ||
      !FUNC(ds_history_read_ulong)(read, io_user, &next_operation_id) ||
      !FUNC(ds_history_read_ulong)(read, io_user, &next_branch_id) ||
      !FUNC(ds_history_read_size)(read, io_user, &checkpoint_interval)) {
    return NULL;
  }
  if (branch_count == 0U) {
    return NULL;
  }

  history = FUNC(ds_history_create)(ops, config);
  if (history == NULL) {
    return NULL;
  }
  history->checkpoint_interval = checkpoint_interval;

  for (i = 0U; i < branch_count; i++) {
    if (!FUNC(ds_history_read_ulong)(read, io_user, &id) ||
        !FUNC(ds_history_read_ulong)(read, io_user, &parent_id) ||
        !FUNC(ds_history_read_ulong)(read, io_user, &fork_time) ||
        !FUNC(ds_history_read_size)(read, io_user, &name_len)) {
      FUNC(ds_history_destroy)(history);
      return NULL;
    }
    name = history_read_name(history, read, io_user, name_len);
    if (name_len != 0U && name == NULL) {
      FUNC(ds_history_destroy)(history);
      return NULL;
    }

    if (parent_id == 0UL) {
      branch = history->main_branch;
      if (branch == NULL) {
        if (name != NULL) {
          history->deallocator(name);
        }
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
      if (branch->name != NULL) {
        history->deallocator(branch->name);
      }
      branch->name = name;
      branch->id = id;
      branch->fork_time = fork_time;
    } else {
      parent = history_find_branch_by_id(history, parent_id);
      if (parent == NULL) {
        if (name != NULL) {
          history->deallocator(name);
        }
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
      branch = history_branch_alloc(history, parent, fork_time, name);
      if (name != NULL) {
        history->deallocator(name);
      }
      if (branch == NULL) {
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
      branch->id = id;
      if (!history_branch_register(history, branch)) {
        history_branch_destroy(branch);
        FUNC(ds_history_destroy)(history);
        return NULL;
      }
    }

    if (history->next_branch_id <= id) {
      history->next_branch_id = id + 1UL;
    }
    if (!history_read_branch_ops_portable(branch, read, read_payload, io_user,
                                          payload_user)) {
      FUNC(ds_history_destroy)(history);
      return NULL;
    }
  }

  if (history->next_operation_id < next_operation_id) {
    history->next_operation_id = next_operation_id;
  }
  if (history->next_branch_id < next_branch_id) {
    history->next_branch_id = next_branch_id;
  }
  return history;
}
