#ifndef DS_ERROR_H
#define DS_ERROR_H

#include "status.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DS_FUNCTION
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define DS_FUNCTION __func__
#else
#define DS_FUNCTION NULL
#endif
#endif

typedef struct ds_error {
  ds_status_t status;
  const char *id;
  const char *module;
  const char *message;
  const char *file;
  unsigned long line;
  const char *function;
} ds_error_t;

typedef struct ds_error_desc {
  ds_status_t status;
  const char *id;
  const char *module;
  const char *message;
} ds_error_desc_t;

void ds_error_clear(ds_error_t *error);
ds_status_t ds_error_set(ds_error_t *error, ds_status_t status,
                         const char *id, const char *module,
                         const char *message, const char *file,
                         unsigned long line, const char *function);
ds_status_t ds_error_set_desc(ds_error_t *error, const ds_error_desc_t *desc,
                              const char *file, unsigned long line,
                              const char *function);
int ds_error_format(const ds_error_t *error, char *buffer,
                    size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* DS_ERROR_H */
