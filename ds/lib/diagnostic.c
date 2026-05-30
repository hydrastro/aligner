#define _POSIX_C_SOURCE 200112L
#include "diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *diag_default_alloc(size_t size) { return malloc(size); }
static void diag_default_free(void *ptr) { free(ptr); }
static void diagnostics_collector_emit(ds_diagnostic_sink_t *sink,
                                       const ds_diagnostic_t *diagnostic);

const char *ds_diagnostic_level_name(ds_diagnostic_level_t level) {
  switch (level) {
  case DS_DIAG_DEBUG:
    return "DEBUG";
  case DS_DIAG_INFO:
    return "INFO";
  case DS_DIAG_NOTICE:
    return "NOTICE";
  case DS_DIAG_WARNING:
    return "WARNING";
  case DS_DIAG_ERROR:
    return "ERROR";
  case DS_DIAG_CRITICAL:
    return "CRITICAL";
  case DS_DIAG_ALERT:
    return "ALERT";
  case DS_DIAG_EMERGENCY:
    return "EMERGENCY";
  default:
    return "UNKNOWN";
  }
}

void ds_diagnostic_sink_emit(ds_diagnostic_sink_t *sink,
                             const ds_diagnostic_t *diagnostic) {
  if (sink == NULL || sink->emit == NULL || diagnostic == NULL) {
    return;
  }
  sink->emit(sink, diagnostic);
}

void ds_diagnostic_emit_desc(ds_diagnostic_sink_t *sink,
                             const ds_diagnostic_desc_t *desc,
                             const ds_diagnostic_arg_t *args,
                             size_t arg_count, const char *file,
                             unsigned long line, const char *function) {
  ds_diagnostic_t diagnostic;

  if (desc == NULL) {
    return;
  }

  diagnostic.id = desc->id;
  diagnostic.level = desc->level;
  diagnostic.module = desc->module;
  diagnostic.message = desc->message;
  diagnostic.file = file;
  diagnostic.line = line;
  diagnostic.function = function;
  diagnostic.args = args;
  diagnostic.arg_count = arg_count;

  ds_diagnostic_sink_emit(sink, &diagnostic);
}

int ds_diagnostic_format(const ds_diagnostic_t *diagnostic, char *buffer,
                         size_t buffer_size) {
  const char *id;
  const char *module;
  const char *message;
  int n;

  if (buffer == NULL || buffer_size == 0U) {
    return -1;
  }
  if (diagnostic == NULL) {
    return snprintf(buffer, buffer_size, "[DIAGNOSTIC NULL]");
  }

  id = diagnostic->id != NULL ? diagnostic->id : "ds.diagnostic/unknown";
  module = diagnostic->module != NULL ? diagnostic->module : "unknown";
  message = diagnostic->message != NULL ? diagnostic->message : "";
  n = snprintf(buffer, buffer_size, "[%s] %s (%s): %s",
               ds_diagnostic_level_name(diagnostic->level), id, module,
               message);
  return n;
}

void ds_diagnostics_init(ds_diagnostics_t *diagnostics) {
  ds_diagnostics_init_alloc(diagnostics, diag_default_alloc, diag_default_free);
}

void ds_diagnostics_init_alloc(ds_diagnostics_t *diagnostics,
                               void *(*allocator)(size_t),
                               void (*deallocator)(void *)) {
  if (diagnostics == NULL) {
    return;
  }
  diagnostics->items = NULL;
  diagnostics->size = 0U;
  diagnostics->capacity = 0U;
  diagnostics->allocator = allocator != NULL ? allocator : diag_default_alloc;
  diagnostics->deallocator = deallocator != NULL ? deallocator : diag_default_free;
  diagnostics->sink.emit = diagnostics_collector_emit;
  diagnostics->sink.user = diagnostics;
}

void ds_diagnostics_destroy(ds_diagnostics_t *diagnostics) {
  if (diagnostics == NULL) {
    return;
  }
  if (diagnostics->items != NULL) {
    diagnostics->deallocator(diagnostics->items);
  }
  diagnostics->items = NULL;
  diagnostics->size = 0U;
  diagnostics->capacity = 0U;
}

void ds_diagnostics_clear(ds_diagnostics_t *diagnostics) {
  if (diagnostics == NULL) {
    return;
  }
  diagnostics->size = 0U;
}

ds_diagnostic_sink_t *ds_diagnostics_sink(ds_diagnostics_t *diagnostics) {
  if (diagnostics == NULL) {
    return NULL;
  }
  return &diagnostics->sink;
}

static void diagnostics_collector_emit(ds_diagnostic_sink_t *sink,
                                       const ds_diagnostic_t *diagnostic) {
  ds_diagnostics_t *diagnostics;
  ds_diagnostic_t *new_items;
  size_t new_capacity;

  if (sink == NULL || diagnostic == NULL) {
    return;
  }
  diagnostics = (ds_diagnostics_t *)sink->user;
  if (diagnostics == NULL) {
    return;
  }

  if (diagnostics->size == diagnostics->capacity) {
    new_capacity = diagnostics->capacity == 0U ? 8U : diagnostics->capacity * 2U;
    new_items = (ds_diagnostic_t *)diagnostics->allocator(new_capacity * sizeof(*new_items));
    if (new_items == NULL) {
      return;
    }
    if (diagnostics->items != NULL) {
      memcpy(new_items, diagnostics->items,
             diagnostics->size * sizeof(*new_items));
      diagnostics->deallocator(diagnostics->items);
    }
    diagnostics->items = new_items;
    diagnostics->capacity = new_capacity;
  }

  diagnostics->items[diagnostics->size] = *diagnostic;
  diagnostics->size++;
}
