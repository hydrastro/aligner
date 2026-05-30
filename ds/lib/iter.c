#include "iter.h"
#include <stddef.h>

ds_status_t ds_iter_next(ds_iter_t *iter) {
  if (iter == NULL || iter->next == NULL) {
    return DS_ERR_NULL;
  }
  return iter->next(iter);
}

void ds_iter_destroy(ds_iter_t *iter) {
  if (iter == NULL) {
    return;
  }
  if (iter->destroy != NULL) {
    iter->destroy(iter);
  }
  iter->container = NULL;
  iter->state = NULL;
  iter->key = NULL;
  iter->value = NULL;
  iter->node = NULL;
  iter->next = NULL;
  iter->destroy = NULL;
}
