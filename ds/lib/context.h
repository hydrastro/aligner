#ifndef DS_CONTEXT_H
#define DS_CONTEXT_H

#include "diagnostic.h"
#include "error.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_context {
  void *(*alloc)(size_t size, void *user);
  void *(*calloc)(size_t count, size_t size, void *user);
  void *(*realloc)(void *ptr, size_t size, void *user);
  void (*free)(void *ptr, void *user);
  void *user;
  ds_diagnostic_sink_t *diagnostic_sink;
  ds_error_t last_error;
  unsigned int flags;
} ds_context_t;

void ds_context_init(ds_context_t *context);
ds_context_t *ds_default_context(void);
void ds_context_set_diagnostic_sink(ds_context_t *context,
                                    ds_diagnostic_sink_t *sink);
void *ds_context_alloc(ds_context_t *context, size_t size);
void *ds_context_calloc(ds_context_t *context, size_t count, size_t size);
void *ds_context_realloc(ds_context_t *context, void *ptr, size_t size);
void ds_context_free(ds_context_t *context, void *ptr);
void ds_context_clear_error(ds_context_t *context);
const ds_error_t *ds_context_last_error(const ds_context_t *context);
ds_status_t ds_context_set_error(ds_context_t *context, ds_status_t status,
                                 const char *id, const char *module,
                                 const char *message, const char *file,
                                 unsigned long line, const char *function);
ds_status_t ds_context_set_error_desc(ds_context_t *context,
                                      const ds_error_desc_t *desc,
                                      const char *file, unsigned long line,
                                      const char *function);
void ds_context_emit_diagnostic(ds_context_t *context,
                                const ds_diagnostic_desc_t *desc,
                                const ds_diagnostic_arg_t *args,
                                size_t arg_count, const char *file,
                                unsigned long line, const char *function);

#define DS_CONTEXT_ERROR(ctx, status, id, module, message)                    \
  ds_context_set_error((ctx), (status), (id), (module), (message), __FILE__,  \
                       (unsigned long)__LINE__, DS_FUNCTION)

#define DS_CONTEXT_ERROR_DESC(ctx, desc)                                      \
  ds_context_set_error_desc((ctx), (desc), __FILE__,                          \
                            (unsigned long)__LINE__, DS_FUNCTION)

#define DS_CONTEXT_DIAG(ctx, desc, args, arg_count)                           \
  ds_context_emit_diagnostic((ctx), (desc), (args), (arg_count), __FILE__,    \
                             (unsigned long)__LINE__, DS_FUNCTION)

#ifdef __cplusplus
}
#endif

#endif /* DS_CONTEXT_H */
