#ifndef DS_DIAGNOSTIC_H
#define DS_DIAGNOSTIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ds_diagnostic_level {
  DS_DIAG_DEBUG = 0,
  DS_DIAG_INFO = 1,
  DS_DIAG_NOTICE = 2,
  DS_DIAG_WARNING = 3,
  DS_DIAG_ERROR = 4,
  DS_DIAG_CRITICAL = 5,
  DS_DIAG_ALERT = 6,
  DS_DIAG_EMERGENCY = 7
} ds_diagnostic_level_t;

typedef enum ds_diagnostic_arg_type {
  DS_ARG_CSTR,
  DS_ARG_INT,
  DS_ARG_UINT,
  DS_ARG_SIZE,
  DS_ARG_PTR
} ds_diagnostic_arg_type_t;

typedef struct ds_diagnostic_arg {
  const char *name;
  ds_diagnostic_arg_type_t type;
  union {
    const char *cstr;
    long i;
    unsigned long u;
    size_t size;
    const void *ptr;
  } value;
} ds_diagnostic_arg_t;

typedef struct ds_diagnostic {
  const char *id;
  ds_diagnostic_level_t level;
  const char *module;
  const char *message;
  const char *file;
  unsigned long line;
  const char *function;
  const ds_diagnostic_arg_t *args;
  size_t arg_count;
} ds_diagnostic_t;

typedef struct ds_diagnostic_desc {
  const char *id;
  ds_diagnostic_level_t level;
  const char *module;
  const char *message;
} ds_diagnostic_desc_t;

struct ds_diagnostic_sink;
typedef struct ds_diagnostic_sink ds_diagnostic_sink_t;

struct ds_diagnostic_sink {
  void (*emit)(ds_diagnostic_sink_t *sink, const ds_diagnostic_t *diagnostic);
  void *user;
};

typedef struct ds_diagnostics {
  ds_diagnostic_t *items;
  size_t size;
  size_t capacity;
  void *(*allocator)(size_t);
  void (*deallocator)(void *);
  ds_diagnostic_sink_t sink;
} ds_diagnostics_t;

const char *ds_diagnostic_level_name(ds_diagnostic_level_t level);
void ds_diagnostic_sink_emit(ds_diagnostic_sink_t *sink,
                             const ds_diagnostic_t *diagnostic);
void ds_diagnostic_emit_desc(ds_diagnostic_sink_t *sink,
                             const ds_diagnostic_desc_t *desc,
                             const ds_diagnostic_arg_t *args,
                             size_t arg_count, const char *file,
                             unsigned long line, const char *function);
int ds_diagnostic_format(const ds_diagnostic_t *diagnostic, char *buffer,
                         size_t buffer_size);

void ds_diagnostics_init(ds_diagnostics_t *diagnostics);
void ds_diagnostics_init_alloc(ds_diagnostics_t *diagnostics,
                               void *(*allocator)(size_t),
                               void (*deallocator)(void *));
void ds_diagnostics_destroy(ds_diagnostics_t *diagnostics);
void ds_diagnostics_clear(ds_diagnostics_t *diagnostics);
ds_diagnostic_sink_t *ds_diagnostics_sink(ds_diagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* DS_DIAGNOSTIC_H */
