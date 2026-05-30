#define _POSIX_C_SOURCE 200112L
#include "error.h"
#include <stdio.h>

void ds_error_clear(ds_error_t *error) {
  if (error == NULL) {
    return;
  }
  error->status = DS_OK;
  error->id = NULL;
  error->module = NULL;
  error->message = NULL;
  error->file = NULL;
  error->line = 0UL;
  error->function = NULL;
}

ds_status_t ds_error_set(ds_error_t *error, ds_status_t status,
                         const char *id, const char *module,
                         const char *message, const char *file,
                         unsigned long line, const char *function) {
  if (error != NULL) {
    error->status = status;
    error->id = id;
    error->module = module;
    error->message = message;
    error->file = file;
    error->line = line;
    error->function = function;
  }
  return status;
}

ds_status_t ds_error_set_desc(ds_error_t *error, const ds_error_desc_t *desc,
                              const char *file, unsigned long line,
                              const char *function) {
  if (desc == NULL) {
    return ds_error_set(error, DS_ERR_INTERNAL, "ds.error/null_desc", "error",
                        "error descriptor is NULL", file, line, function);
  }
  return ds_error_set(error, desc->status, desc->id, desc->module,
                      desc->message, file, line, function);
}

int ds_error_format(const ds_error_t *error, char *buffer,
                    size_t buffer_size) {
  const char *id;
  const char *module;
  const char *message;
  const char *file;
  const char *function;

  if (buffer == NULL || buffer_size == 0U) {
    return -1;
  }
  if (error == NULL || error->status == DS_OK) {
    return snprintf(buffer, buffer_size, "[OK]");
  }

  id = error->id != NULL ? error->id : "ds.error/unknown";
  module = error->module != NULL ? error->module : "unknown";
  message = error->message != NULL ? error->message : "unknown error";
  file = error->file != NULL ? error->file : "?";
  function = error->function != NULL ? error->function : "?";

  return snprintf(buffer, buffer_size, "[%s] %s (%s): %s at %s:%lu:%s",
                  ds_status_name(error->status), id, module, message, file,
                  error->line, function);
}
