#ifndef DS_ITER_H
#define DS_ITER_H

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ds_iter;
typedef struct ds_iter ds_iter_t;
typedef ds_status_t (*ds_iter_next_func_t)(ds_iter_t *iter);
typedef void (*ds_iter_destroy_func_t)(ds_iter_t *iter);

struct ds_iter {
  void *container;
  void *state;
  void *key;
  void *value;
  void *node;
  ds_iter_next_func_t next;
  ds_iter_destroy_func_t destroy;
};

ds_status_t ds_iter_next(ds_iter_t *iter);
void ds_iter_destroy(ds_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif /* DS_ITER_H */
