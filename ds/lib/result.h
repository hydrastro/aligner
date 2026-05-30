#ifndef DS_RESULT_H
#define DS_RESULT_H

#include "status.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_result_ptr {
  ds_status_t status;
  void *value;
} ds_result_ptr_t;

typedef struct ds_result_size {
  ds_status_t status;
  size_t value;
} ds_result_size_t;


#ifdef __cplusplus
}
#endif

#endif /* DS_RESULT_H */
