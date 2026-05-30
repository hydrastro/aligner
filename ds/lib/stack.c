#include "stack.h"
#include <stdlib.h>

ds_stack_t *FUNC(stack_create)(void) {
  return FUNC(stack_create_alloc)(malloc, free);
}

ds_stack_t *FUNC(stack_create_alloc)(void *(*allocator)(size_t),
                                     void (*deallocator)(void *)) {
  ds_stack_t *stack = (ds_stack_t *)allocator(sizeof(ds_stack_t));
  stack->allocator = allocator;
  stack->deallocator = deallocator;
  stack->nil = (ds_stack_node_t *)allocator(sizeof(ds_stack_node_t));
  stack->nil->next = stack->nil;
  stack->top = stack->nil;
  stack->size = 0;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(stack);
#endif
  return stack;
}

void FUNC(stack_push)(ds_stack_t *stack, ds_stack_node_t *node) {
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif
  if (node == stack->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(stack);
#endif
    return;
  }
  node->next = stack->top;
  stack->top = node;
  stack->size += 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

ds_stack_node_t *FUNC(stack_pop)(ds_stack_t *stack) {
  ds_stack_node_t *node;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif
  if (FUNC(stack_is_empty)(stack)) {
#ifdef DS_THREAD_SAFE
    UNLOCK(stack);
#endif
    return stack->nil;
  }
  node = stack->top;
  stack->top = node->next;
  stack->size -= 1;
#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
  return node;
}

ds_stack_node_t *FUNC(stack_peek)(ds_stack_t *stack) {
  ds_stack_node_t *result;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif
  result = stack->top;
#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
  return result;
}

bool FUNC(stack_is_empty)(ds_stack_t *stack) {
  bool result;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif
  result = stack->top == stack->nil;
#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
  return result;
}

void FUNC(stack_destroy)(ds_stack_t *stack,
                         void (*destroy)(ds_stack_node_t *)) {
  ds_stack_node_t *node = stack->top;
  while (node != stack->nil) {
    ds_stack_node_t *next = node->next;
    destroy(node);
    node = next;
  }
  stack->deallocator(stack->nil);
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(stack);
#endif
  stack->deallocator(stack);
}

void FUNC(stack_delete)(ds_stack_t *stack) {
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  stack->top = stack->nil;
  stack->size = 0;

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

void FUNC(stack_delete_node)(ds_stack_t *stack, ds_stack_node_t *node) {
  ds_stack_node_t *current;
  ds_stack_node_t *previous;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  current = stack->top;
  previous = stack->nil;

  while (current != stack->nil && current != node) {
    previous = current;
    current = current->next;
  }

  if (current == node) {
    if (previous == stack->nil) {
      stack->top = current->next;
    } else {
      previous->next = current->next;
    }
    stack->size -= 1;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

void FUNC(stack_destroy_node)(ds_stack_t *stack, ds_stack_node_t *node,
                              void (*destroy)(ds_stack_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  FUNC(stack_delete_node)(stack, node);
  if (destroy != NULL) {
    destroy(node);
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

void FUNC(stack_pop_destroy)(ds_stack_t *stack,
                             void (*destroy_node)(ds_stack_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  if (!FUNC(stack_is_empty)(stack)) {
    ds_stack_node_t *node = FUNC(stack_pop)(stack);
    destroy_node(node);
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

ds_stack_node_t *FUNC(stack_search)(ds_stack_t *stack, ds_stack_node_t *node,
                                    int (*compare)(ds_stack_node_t *,
                                                   ds_stack_node_t *)) {
  ds_stack_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  current = stack->top;

  while (current != stack->nil) {
    if (compare(current, node) == 0) {
#ifdef DS_THREAD_SAFE
      UNLOCK(stack);
#endif
      return current;
    }
    current = current->next;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif

  return stack->nil;
}

void FUNC(stack_walk_forward)(ds_stack_t *stack, ds_stack_node_t *start_node,
                              void (*callback)(ds_stack_node_t *)) {
  ds_stack_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  current = start_node != stack->nil ? start_node : stack->top;

  while (current != stack->nil) {
    callback(current);
    current = current->next;
  }

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

void FUNC(stack_walk_backwards)(ds_stack_t *stack, ds_stack_node_t *current,
                                void (*callback)(ds_stack_node_t *)) {
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif

  if (current == stack->nil) {
#ifdef DS_THREAD_SAFE
    UNLOCK(stack);
#endif
    return;
  }

  FUNC(stack_walk_backwards)(stack, current->next, callback);
  callback(current);

#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif
}

ds_stack_t *
FUNC(stack_clone)(ds_stack_t *stack,
                  ds_stack_node_t *(*clone_node)(ds_stack_node_t *)) {
  ds_stack_t *temp_stack;
  ds_stack_t *new_stack;
  ds_stack_node_t *current;
#ifdef DS_THREAD_SAFE
  LOCK(stack);
#endif
  temp_stack = FUNC(stack_create_alloc)(stack->allocator, stack->deallocator);
  new_stack = FUNC(stack_create_alloc)(stack->allocator, stack->deallocator);
  current = stack->top;
  while (current != stack->nil) {
    ds_stack_node_t *cloned_node = clone_node(current);
    FUNC(stack_push)(temp_stack, cloned_node);
    current = current->next;
  }
  while (!FUNC(stack_is_empty)(temp_stack)) {
    ds_stack_node_t *node = FUNC(stack_pop)(temp_stack);
    FUNC(stack_push)(new_stack, node);
  }
  FUNC(stack_destroy)(temp_stack, NULL);
#ifdef DS_THREAD_SAFE
  UNLOCK(stack);
#endif

  return new_stack;
}
